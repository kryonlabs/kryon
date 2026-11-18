#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/shader-prepared"
mkdir -p "$out"
"$ziran" build --project --root "$repo/tests" --target=c --entry shader_prepared_test:main --exe -o "$out/c" "$repo/tests/shader_prepared_test.zi"
"$out/c/shader_prepared_test"
"$ziran" bundle --project --root "$repo/tests" --entry shader_prepared_test:main -o "$out/test.zx" "$repo/tests/shader_prepared_test.zi"
test "$("$ziran" run "$out/test.zx")" = 0
printf 'Prepared shaders: workloads, mutable bindings, overwrites, discard and native/portable parity passed\n'
