#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/shader-scene"
mkdir -p "$out"
source="$repo/tests/shader_scene_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$out/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$out/ir/shader_scene_test.zir; root=$out/ir; fi
    "$ziran" build --project --root "$root" --target=c --entry shader_scene_test:main --exe -o "$out/$form" "$input"
    "$out/$form/shader_scene_test"
    "$ziran" bundle --project --root "$root" --entry shader_scene_test:main -o "$out/$form.zx" "$input"
    test "$("$ziran" run "$out/$form.zx")" = 0
done
printf 'Shader scene source/saved native and portable passed\n'
