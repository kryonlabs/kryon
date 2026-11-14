#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/scene-image-material"
mkdir -p "$out"
source="$repo/tests/scene_image_material_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$out/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$out/ir/scene_image_material_test.zir; root=$out/ir; fi
    "$ziran" build --project --root "$root" --target=c --entry scene_image_material_test:main --exe -o "$out/$form" "$input"
    "$out/$form/scene_image_material_test"
    "$ziran" bundle --project --root "$root" --entry scene_image_material_test:main -o "$out/$form.zx" "$input"
    test "$("$ziran" run "$out/$form.zx")" = 0
done
printf 'Scene image material source/saved native and portable passed\n'
