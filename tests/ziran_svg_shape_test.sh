#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/svg_shape_behavior.zi
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry svg_shape_behavior:main -o "$work/shape.zx" "$source"
test "$("$ziran" run "$work/shape.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/shape-test"
"$work/shape-test"
