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

# Android's portable app carries the host define too. Compile its flags into
# saved IR and execute the resize test with the portable engine explicitly.
android_ir="$out/android-bundle-ir"
"$ziran" ir --project --root "$repo/tests" --entry shader_scene_test:main \
    --target-os android --target-cpu none --define ANDROID_BUILD --define __arm__ \
    --define KRYON_ANDROID_HOST --define KRYON_ZX_APP --define BUNDLE_NATIVE_RENDERER \
    -o "$android_ir" "$source"
"$ziran" bundle --project --root "$android_ir" --entry shader_scene_test:main \
    -o "$out/android.zx" "$android_ir/shader_scene_test.zir"
test "$("$ziran" run --backend engine "$out/android.zx")" = 0
printf 'Shader scene source/saved native and portable, including Android interpreter, passed\n'
