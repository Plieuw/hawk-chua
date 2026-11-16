#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""HAWK CHUA on FM1 Open: move the UI out of the firmware into HAWK CHUA's data (the user flash, step 2 of the install).

  ovl_split.py OBJ SRC_DIR [--list]

OBJ: hc_open.c compiled with -ffunction-sections -fdata-sections (native, not LTO). The seed is every function and
every const table defined in the UI's sources (SRC_DIR/ui.c, ui_style.c, fx_art.c) and hc_sdk.c's two ways in
(hc_ui_boot, hc_ui_round); then, until nothing changes: in, every other code or const section that only overlay
sections refer to (helpers and tables only the UI uses); out, every overlay section something outside refers to (the
firmware keeps what it calls itself, as ui_say). The one exception: hc_ovl_entry (hc_sdk.c), the pointers the firmware
calls the two ways in through, only once the overlay checked out. A section that defines a global symbol (the SDK
calls it) never moves; nor does any writable data. The UI runs in the control task only, never in the audio interrupt,
so nothing the audio interrupt reaches can move.

The moved sections are renamed in place (the names' prefixes, the same length, so OBJ keeps its layout):
  .text.X -> .hcot.X    .rodata.X -> .hcorod.X    .data.rel.ro.X -> .hcod_rel_ro.X
and the linker places them in .hc_ovl (tools/build_open.py), at the user flash's address."""
import re
import struct
import sys
from pathlib import Path

MOVE = [(".text.", ".hcot."), (".rodata.", ".hcorod."), (".data.rel.ro.local.", ".hcod_rel_ro_local."),
        (".data.rel.ro.", ".hcod_rel_ro.")]
UI_FILES = ("ui.c", "ui_style.c", "fx_art.c")
ENTRY = ("hc_ui_boot", "hc_ui_round")           # (hc_sdk.c: the overlay's ways in, through hc_ovl_entry only)


def ui_names(src):
    fns, consts = set(), set()
    for f in UI_FILES:
        t = (Path(src) / f).read_text(errors="replace")
        for m in re.finditer(r"(?m)^static\b[^=;{(]*?[\s*]([A-Za-z_]\w*)\s*\([^;]*?\)\s*(?:/\*.*?\*/\s*)?\{", t, re.S):
            fns.add(m.group(1))
        for m in re.finditer(r"(?m)^static\s+(?:volatile\s+)?const\b[^=;]*?[\s*]([A-Za-z_]\w*)\s*(?:\[[^\]=;]*\])*\s*=", t):
            consts.add(m.group(1))
    return fns, consts


class Elf:
    def __init__(self, data):
        self.d = bytearray(data)
        if self.d[:4] != b"\x7fELF" or self.d[5] != 1:
            raise SystemExit("not a little-endian ELF")
        self.c64 = self.d[4] == 2
        if self.c64:
            shoff, = struct.unpack_from("<Q", self.d, 0x28)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", self.d, 0x3A)
        else:
            shoff, = struct.unpack_from("<I", self.d, 0x20)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", self.d, 0x2E)
        self.sh = []
        for i in range(shnum):
            o = shoff + i * shentsize
            if self.c64:
                name, typ, flags, addr, off, size, link, info, align, ent = struct.unpack_from("<IIQQQQIIQQ", self.d, o)
            else:
                name, typ, flags, addr, off, size, link, info, align, ent = struct.unpack_from("<IIIIIIIIII", self.d, o)
            self.sh.append(dict(name=name, type=typ, flags=flags, off=off, size=size, link=link, info=info, ent=ent))
        self.stroff = self.sh[shstrndx]["off"]
        for s in self.sh:
            s["n"] = self.cstr(self.stroff + s["name"])

    def cstr(self, o):
        e = self.d.index(0, o)
        return self.d[o:e].decode("latin-1")

    def symbols(self):
        """[(name, shndx, global)] of the symbol table"""
        st = next(s for s in self.sh if s["type"] == 2)
        strt = self.sh[st["link"]]["off"]
        out = []
        for i in range(st["size"] // st["ent"]):
            o = st["off"] + i * st["ent"]
            if self.c64:
                name, info, other, shndx = struct.unpack_from("<IBBH", self.d, o)
            else:
                name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", self.d, o)
            out.append((self.cstr(strt + name), shndx, (info >> 4) in (1, 2)))
        return out

    def refs(self):
        """{section index: set of section indexes that refer to it}"""
        syms = self.symbols()
        r = {}
        for s in self.sh:
            if s["type"] not in (4, 9) or not s["ent"]:   # RELA, REL
                continue
            src = s["info"]
            for i in range(s["size"] // s["ent"]):
                o = s["off"] + i * s["ent"]
                info = struct.unpack_from("<Q" if self.c64 else "<I", self.d, o + (8 if self.c64 else 4))[0]
                sym = info >> 32 if self.c64 else info >> 8
                if sym < len(syms):
                    tgt = syms[sym][1]
                    if 0 < tgt < len(self.sh) and tgt != src:
                        r.setdefault(tgt, set()).add(src)
        return r


def split(obj, src):
    e = Elf(Path(obj).read_bytes())
    fns, consts = ui_names(src)
    pinned = {sh for name, sh, g in e.symbols() if g and 0 < sh < len(e.sh)}
    movable = {}
    for i, s in enumerate(e.sh):
        for a, b in MOVE:
            if s["n"].startswith(a) and s["type"] == 1:      # PROGBITS
                movable[i] = (a, b)
    ovl = set()
    fns |= set(ENTRY)
    for i, (a, _) in movable.items():
        base = e.sh[i]["n"][len(a):]
        stem = base.split(".str")[0] if ".str" in base else base
        if i in pinned:
            continue
        if (a == ".text." and base in fns) or (a != ".text." and (base in consts or stem in fns)):
            ovl.add(i)
    refs = e.refs()
    exempt = {i for i, s in enumerate(e.sh) if "hc_ovl_entry" in s["n"]}
    changed = True
    while changed:
        changed = False
        for i in movable:                        # in: only the overlay refers to it
            if i not in ovl and i not in pinned and refs.get(i) and refs[i] <= ovl:
                ovl.add(i)
                changed = True
        for i in list(ovl):                      # out: something the firmware keeps refers to it
            if refs.get(i, set()) - ovl - exempt:
                ovl.discard(i)
                changed = True
    for i in sorted(ovl):
        a, b = movable[i]
        o = e.stroff + e.sh[i]["name"]
        assert e.d[o:o + len(a)] == a.encode()
        e.d[o:o + len(b)] = b.encode()
    return e, ovl


def main():
    obj, src = sys.argv[1], sys.argv[2]
    e, ovl = split(obj, src)
    size = sum(e.sh[i]["size"] for i in ovl)
    print(f"overlay: {len(ovl)} sections, {size} B (the UI: {len(ui_names(src)[0])} functions seeded)")
    if "--list" in sys.argv:
        for i in sorted(ovl, key=lambda i: -e.sh[i]["size"])[:40]:
            print(f"  {e.sh[i]['size']:7d} {e.cstr(e.stroff + e.sh[i]['name'])}")
    Path(obj).write_bytes(e.d)


if __name__ == "__main__":
    main()
