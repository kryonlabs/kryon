#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/system_palette_buttons.zi"
"${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -lm -o "$work/palette-test"
"$work/palette-test"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry system_palette_buttons:main \
    -o "$work/palette.zx" "$repo/tests/system_palette_buttons.zi"
test "$("$ziran" run "$work/palette.zx" | tail -n 1)" = 0
