#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# HAWK CHUA as a VIBE FM firmware: build/vibe/hawkchua.wasm + .zip (the Playground at play.thefirmwarehub.com: drop
# the .wasm onto the device; VIBE FM VST: drop the .zip onto its window). The firmware's own sources, unchanged;
# vibe/hawkchua/main.c in place of the FM-1's hardware.
#   VIBE_SDK=../vibe-fm-sdk sh vibe/build.sh     (the SDK: github.com/Vibe-FM-SDK/vibe-fm-sdk; Zig 0.13.0: zig on the
#                                                 PATH or ZIG=..., or `pip install ziglang==0.13.0` and ZIG="python3 -m ziglang")
set -e
cd "$(dirname "$0")/.."
SDK="${VIBE_SDK:?set VIBE_SDK to the VIBE FM SDK's folder}"
ZIG="${ZIG:-zig}"
ID=hawkchua
if [ ! -f build/gen/ui_fonts.h ]; then           # the generated font / palette headers (as ./build.sh makes them)
    mkdir -p build/gen
    python3 tools/gen_aa_font.py build/gen/ui_fonts.h --preset inter-tight >/dev/null
    python3 tools/gen_ui_palettes.py build/gen/ui_palettes.h >/dev/null
fi
O=build/vibe/obj; rm -rf "$O"; mkdir -p "$O"
for f in vibe/$ID/main.c "$SDK/sdk/src/vibe_sdk.c" "$SDK/sdk/src/vibe_font.c"; do
  $ZIG cc -target wasm32-freestanding -O2 -DFELUCCA_VERSION="\"${FELUCCA_VERSION:-vibe}\"" -ffast-math -fno-builtin-memset -matomics -mbulk-memory \
    -I"$SDK/sdk/include" -I"$SDK/sdk/src" -Ibuild/gen -Ifirmware/src -Wall -Wno-unused-function -Wno-unused-variable \
    -Wno-unused-but-set-variable -c -o "$O/$(basename "$f" .c).o" "$f"
done
$ZIG wasm-ld --no-entry --export-dynamic --export=__heap_base --export=__data_end \
  --shared-memory --max-memory=67108864 -z stack-size=262144 -o "build/vibe/$ID.wasm" "$O"/*.o
cp vibe/$ID/firmware.json build/vibe/$ID.json
[ -f vibe/$ID/icon.png ] && cp vibe/$ID/icon.png build/vibe/$ID.png
(cd build/vibe && rm -f $ID.zip && python3 -c "
import zipfile, os, sys
z = zipfile.ZipFile('$ID.zip', 'w', zipfile.ZIP_DEFLATED)
for f in ('$ID.wasm', '$ID.json', '$ID.png'):
    if os.path.exists(f): z.write(f)
")
echo "built build/vibe/$ID.wasm ($(( $(wc -c < build/vibe/$ID.wasm) / 1024 )) KB) and build/vibe/$ID.zip"
