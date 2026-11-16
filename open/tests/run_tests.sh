#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Host tests of HAWK CHUA on FM1 Open (open/app with ../firmware/src) against a stand-in for the SDK.
# Run from the repo root: sh open/tests/run_tests.sh   (screens and a WAV in open/build/host)
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
rm -f "$OUT"/*.rgb "$OUT"/*.png
INC="-Iapp -Iapp/board -Iapp/synth -Isdk/include -Isdk/config -I../firmware/src"
cc -std=gnu11 -O1 -g -w -DFM1_HOST_TEST $INC -o "$OUT/open_test" tests/open_test.c -lm
"$OUT/open_test" "$OUT"
echo "== the same under ASan / UBSan"
cc -std=gnu11 -O1 -g -w -fsanitize=address,undefined -fno-sanitize-recover=all -DFM1_HOST_TEST $INC \
    -o "$OUT/open_test_san" tests/open_test.c -lm
"$OUT/open_test_san" "$OUT" > "$OUT/san.log" 2>&1 || { tail -30 "$OUT/san.log"; exit 1; }
tail -1 "$OUT/san.log"
python3 tests/rgb_png.py "$OUT"
