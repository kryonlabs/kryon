#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/svg_shape_behavior.zi
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --entry svg_shape_behavior:main -o "$work/shape.zib" "$source"
test "$("$ziran" run "$work/shape.zib")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/shape-test"
"$work/shape-test"
