#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The browser version: web/emu/emu.c (the firmware's sources) to build/emu/emu.wasm.
#   sh web/emu/build.sh        (clang with the wasm32 target, wasm-ld: Debian's clang + lld)
set -e
cd "$(dirname "$0")/../.."
if [ ! -f build/gen/ui_fonts.h ]; then           # the generated font / palette headers (as ./build.sh makes them)
    mkdir -p build/gen
    python3 tools/gen_aa_font.py build/gen/ui_fonts.h --preset inter-tight >/dev/null
    python3 tools/gen_aa_keycaps.py build/gen/ui_keycaps.h >/dev/null
    python3 tools/gen_ui_palettes.py build/gen/ui_palettes.h >/dev/null
fi
CC=${CC:-clang}
LD=$(command -v wasm-ld || ls /usr/bin/wasm-ld-* /usr/lib/llvm-*/bin/wasm-ld 2>/dev/null | head -n 1)
[ -n "$LD" ] || { echo "web/emu/build.sh: no wasm-ld (apt install lld)"; exit 1; }
mkdir -p build/emu
VER=${FELUCCA_VERSION:-web}
$CC --target=wasm32 -O2 -nostdlib -ffreestanding -fno-builtin -Ibuild/gen -DFELUCCA_VERSION="\"$VER\"" \
    -Wall -Wno-unused-function -Wno-unused-variable -c web/emu/emu.c -o build/emu/emu.o
"$LD" --no-entry --allow-undefined -z stack-size=262144 --strip-all build/emu/emu.o -o build/emu/emu.wasm
echo "emu: build/emu/emu.wasm ($(wc -c < build/emu/emu.wasm) B)"
