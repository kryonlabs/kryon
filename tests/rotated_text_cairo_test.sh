#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
output=$repo/build/scratch/rotated-text/cairo
mkdir -p "$output"
"$ziran" build --project --target=c --root "$repo/tests" \
    -o "$output" "$repo/tests/rotated_text_cairo_test.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$output" "$output"/*.c $(pkg-config --libs pangocairo fontconfig freetype2) -lm -o "$output/test"
"$output/test"
echo 'Cairo rotated text: offscreen arbitrary angle, pivot and font size PASS'
