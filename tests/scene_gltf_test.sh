#!/bin/sh
# The glTF reader and the VRM avatar layer over it, from source and from
# saved IR.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
cd "$repo"
out="$repo/build/scratch/scene-gltf"
mkdir -p "$out"
args="--module-path $repo/src/ui --module-path std=$ziran_root/std"
for test in scene_gltf_test scene_vrm_test; do
    "$ziran_root/build/bin/zi2zir" --root "$repo/tests" $args -o "$out/ir" "$repo/tests/$test.zi"
    for form in source saved; do
        source="$repo/tests/$test.zi"
        root="$repo/tests"
        if test "$form" = saved; then
            source="$out/ir/$test.zir"
            root="$out/ir"
        fi
        "$ziran_root/build/bin/zi2c" --root "$root" $args --entry $test:main --exe -o "$out/$form-$test" "$source"
        "$out/$form-$test/$test"
    done
done
printf 'Scene glTF and VRM source/saved native local tests passed\n'
