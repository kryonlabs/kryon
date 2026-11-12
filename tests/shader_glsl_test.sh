#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
cd "$repo"
out="$repo/build/scratch/shader-glsl"
rm -rf "$out/c" "$out/ir"
mkdir -p "$out/c" "$out/ir"
"$ziran" ir --project --root "$repo/tests" --define GLSL_EMIT --entry shader_glsl_test:main -o "$out/ir" "$repo/tests/shader_glsl_test.zi"
"$ziran_root/build/bin/zi2c" --entry shader_glsl_test:main --define GLSL_EMIT --root "$out/ir" -o "$out/c" "$out/ir/"*.zir
"${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" $(find "$out/c" -type d -printf ' -iquote %p') $(find "$out/c" -name '*.c') -lm -o "$out/shader_glsl_test"
"$out/shader_glsl_test" > "$out/fixtures.txt"
"$ziran" bundle --project --root "$repo/tests" --entry shader_glsl_test:main -o "$out/shader_glsl_test.zx" "$repo/tests/shader_glsl_test.zi"
test "$("$ziran" run "$out/shader_glsl_test.zx")" = 0
python3 "$repo/tests/shader_glsl_host_test.py" "$out/fixtures.txt"
printf 'Shader GLSL native, portable and private GLES oracle passed\n'
