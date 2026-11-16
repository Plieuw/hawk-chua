#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""The host test's screens (240 x 240 RGB565, .rgb) as PNGs, and sheet.png with all of them."""
import struct, sys
from pathlib import Path
from PIL import Image

d = Path(sys.argv[1])
ims = []
for f in sorted(d.glob("*.rgb")):
    px = struct.unpack("<57600H", f.read_bytes())
    im = Image.new("RGB", (240, 240))
    im.putdata([((v >> 11) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31) for v in px])
    im.save(f.with_suffix(".png"))
    ims.append(im)
if ims:
    cols = 5
    sheet = Image.new("RGB", (cols * 244, (len(ims) + cols - 1) // cols * 244), "white")
    for i, im in enumerate(ims):
        sheet.paste(im, (i % cols * 244 + 2, i // cols * 244 + 2))
    sheet.save(d / "sheet.png")
    print(f"{len(ims)} screens: {d / 'sheet.png'}")
