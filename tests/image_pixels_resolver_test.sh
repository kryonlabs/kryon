#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/image-pixels-resolver"
mkdir -p "$out"
python3 "$repo/tests/image_pixels_fixtures.py"
"$ziran" ir --project --root "$repo/tests" -o "$out/ir" "$repo/tests/image_pixels_resolver_test.zi"
for form in source saved; do
    source="$repo/tests/image_pixels_resolver_test.zi"
    input_root="$repo/tests"
    if test "$form" = saved; then
        source="$out/ir/image_pixels_resolver_test.zir"
        input_root="$out/ir"
    fi
    LDLIBS='-ldl -lm' "$ziran" build --project --root "$input_root" --target=c \
        --entry image_pixels_resolver_test:main --exe -o "$out/$form" "$source"
    (cd "$repo" && "$out/$form/image_pixels_resolver_test")
done
printf 'Embedded image path resolution source/saved native local tests passed\n'
