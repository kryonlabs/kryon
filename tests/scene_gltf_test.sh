#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
cd "$repo"
out="$repo/build/scratch/scene-gltf"
mkdir -p "$out"
args="--module-path $repo/src/ui --module-path std=$ziran_root/std"
"$ziran_root/build/bin/zi2zir" --root "$repo/tests" $args -o "$out/ir" "$repo/tests/scene_gltf_test.zi"
for form in source saved; do
    source="$repo/tests/scene_gltf_test.zi"
    root="$repo/tests"
    if test "$form" = saved; then
        source="$out/ir/scene_gltf_test.zir"
        root="$out/ir"
    fi
    "$ziran_root/build/bin/zi2c" --root "$root" $args --entry scene_gltf_test:main --exe -o "$out/$form" "$source"
    "$out/$form/scene_gltf_test"
done
printf 'Scene glTF source/saved native local tests passed\n'
