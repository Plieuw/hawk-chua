#!/bin/sh
# LEVEL MATCH: measure every built-in preset, write firmware/src/preset_gain.h (after presets or the sound change)
set -e
cd "$(dirname "$0")/.."
gcc -O2 -w -Itests -Ibuild/gen -o build/level_match tests/level_match.c -lm
./build/level_match > firmware/src/preset_gain.h.new 2> build/level_match.txt
mv firmware/src/preset_gain.h.new firmware/src/preset_gain.h
echo "preset_gain.h written ($(wc -l < build/level_match.txt) presets; build/level_match.txt)"
