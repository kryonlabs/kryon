#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/system_theme_behavior.zi
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    --entry system_theme_behavior:main -o "$work/theme.zx" "$source"
test "$("$ziran" run "$work/theme.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/theme-test"
"$work/theme-test"
