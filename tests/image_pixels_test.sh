#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/image-pixels"
mkdir -p "$out"
python3 "$repo/tests/image_pixels_fixtures.py"
LDLIBS='-ldl -lm' "$ziran" build --project --root "$repo/tests" --target=c --entry image_pixels_test:main --exe -o "$out/native" "$repo/tests/image_pixels_test.zi"
cd "$repo"
"$out/native/image_pixels_test"
printf 'Owned straight RGBA image loading, alpha and invalid files passed\n'
