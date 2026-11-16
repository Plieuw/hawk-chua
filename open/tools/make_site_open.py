#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The HAWK CHUA (FM1 Open) installer site, a folder of the HAWK CHUA site (GitHub Pages: /open/):

  index.html                         redirect to the installer
  webapp/installer/index.html        open/web/index.html, self-contained (fm1pkg.js, fm1ota.js, hcdata.js inlined)
  firmware/hawk-chua-open-VER.fwsc   step 1: the firmware (FM1 Open + HAWK CHUA's platform layer)
  firmware/hawk-chua-data-VER.bin    step 2: HAWK CHUA's data (the sound's and the UI's code, the presets)
  firmware/FM-1_015.fwsc             M-VAVE's original V15 (FM1 Open's SDK, Apache-2.0): the way back, and the step
                                     from a Felucca-based firmware to FM1 Open
  firmware/licences/                 the licences that travel with them

  make_site_open.py FIRMWARE.fwsc DATA.bin VERSION OUT_DIR
"""
import json
import re
import shutil
import sys
from pathlib import Path

OPEN = Path(__file__).resolve().parents[1]
ROOT = OPEN.parent
WEB = OPEN / "web"
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(fw, data, version, out):
    fw, data, out = Path(fw), Path(data), Path(out)
    raw, dat = fw.read_bytes(), data.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_8\d{6}", product):
        raise SystemExit(f"{fw}: identity {product!r} is not an FM1 Open package (FM-1_8xxxxxx)")
    if dat[:4] != b"HCDA":
        raise SystemExit(f"{data}: not HAWK CHUA's data")
    ident = json.loads((OPEN / "build" / "dx7" / "build-identity.json").read_text())   # tools/build_open.py
    if ident["product"] != product or not re.fullmatch(r"[0-9a-f]{64}", ident["build_id"]) or \
            dat[28:60] != bytes.fromhex(ident["build_id"]):
        raise SystemExit("build-identity.json does not belong to this package and data")
    ver = re.sub(r"[^A-Za-z0-9.-]", "-", version)
    fw_name, data_name = f"hawk-chua-open-{ver}.fwsc", f"hawk-chua-data-{ver}.bin"
    html = (WEB / "index.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((WEB / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js", "hcdata.js"))
    meta = json.dumps({"version": version, "product": product, "buildId": ident["build_id"],
                       "pkg": "../../firmware/" + fw_name, "data": "../../firmware/" + data_name, "dataBytes": len(dat),
                       "stock": "../../firmware/FM-1_015.fwsc"})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index.html must contain {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, fwd, lic = out / "webapp" / "installer", out / "firmware", out / "firmware" / "licences"
    for d in (inst, fwd, lic):
        d.mkdir(parents=True, exist_ok=True)
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(fw, fwd / fw_name)
    shutil.copy(data, fwd / data_name)
    shutil.copy(OPEN / "sdk" / "firmware" / "FM-1_015.fwsc", fwd / "FM-1_015.fwsc")   # M-VAVE's V15, Apache-2.0
    docs = {"LICENSE.md": OPEN / "LICENSE.md", "THIRD_PARTY.md": OPEN / "THIRD_PARTY.md", "GPL-3.0.txt": ROOT / "LICENSE",
            "LICENSING.md": OPEN / "LICENSING.md", "Apache-2.0.txt": ROOT / "LICENSES" / "Apache-2.0.txt",
            "MIT-Rings.txt": ROOT / "LICENSES" / "MIT-Rings.txt", "MIT-DaisySP.txt": ROOT / "LICENSES" / "MIT-DaisySP.txt",
            "OFL-InterTight.txt": ROOT / "LICENSES" / "OFL-InterTight.txt"}
    for n, src in docs.items():
        shutil.copy(src, lic / n)
    (lic / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>HAWK CHUA (FM1 Open) licences</title>'
        '<h1>HAWK CHUA (FM1 Open): licences</h1><ul>'
        '<li><a href="LICENSING.md">LICENSING.md</a>: what is under which licence</li>'
        '<li><a href="LICENSE.md">LICENSE.md</a>: FM1 Open APP, GPL-3.0 with the FM1 SDK linking exception (M-VAVE)</li>'
        '<li><a href="THIRD_PARTY.md">THIRD_PARTY.md</a>: the FM1 SDK (binary) and its components</li>'
        '<li><a href="GPL-3.0.txt">GPL-3.0</a>: HAWK CHUA</li>'
        '<li><a href="MIT-Rings.txt">MIT (Rings)</a>, <a href="MIT-DaisySP.txt">MIT (DaisySP)</a>: the resonators</li>'
        '<li><a href="OFL-InterTight.txt">SIL OFL 1.1</a>: Inter Tight, the UI text</li>'
        '<li><a href="Apache-2.0.txt">Apache-2.0</a>: the boot and update files in the package, and M-VAVE\'s original '
        'V15 firmware (FM-1_015.fwsc, from FM1 Open\'s SDK, unmodified)</li></ul>'
        '<p>Source: <a href="https://github.com/Plieuw/hawk-chua">github.com/Plieuw/hawk-chua</a> (open/)</p>\n',
        encoding="utf-8")
    (out / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>HAWK CHUA (FM1 Open)</title>'
        '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
        '<a href="webapp/installer/">HAWK CHUA (FM1 Open) installer</a>\n', encoding="utf-8")
    print(f"site: {out}: installer ({len(html)} B), firmware/{fw_name} ({len(raw)} B, {product}), "
          f"firmware/{data_name} ({len(dat)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
