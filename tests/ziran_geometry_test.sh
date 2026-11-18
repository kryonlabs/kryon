#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/geometry_behavior.zi
"$ziran" bundle --root "$repo/tests" $kryon_ui_args --module-path "$ziran_root/std" \
    --entry geometry_behavior:main -o "$work/geometry.zx" "$source"
bundle_result=$("$ziran" run "$work/geometry.zx")
test "$bundle_result" = 0 || { printf '%s\n' "$bundle_result"; exit 1; }

"$ziran" build --target=c --root "$repo/tests" \
    $kryon_ui_args --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/geometry-test" -lm
"$work/geometry-test" || exit $?
