#!/bin/sh
# Opens real secondary windows next to a raylib window on a private Xvfb
# display: rendered pixels, event ownership, clicks, dragging, keyboard grab
# and restoring the main OpenGL context.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
raylib=${RAYLIB_A:-"$("$ziran" pkg path raylib)/src/libraylib.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran_root/build/bin/zi2c" --no-main --define NATIVE_WINDOW_HAVE_SDL --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/raylib_secondary_window_test.zi"
includes="-I$ziran_root/include"
for header in $(find "$work/c" -name '*.h' | xargs -n1 dirname | sort -u); do
    includes="$includes -I$header"
done
# shellcheck disable=SC2046,SC2086
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections $includes \
    $(find "$work/c" -name '*.c') "$raylib" $(pkg-config --libs sdl2) -lGL -lm -ldl -lpthread -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
    YUE_DESKTOP_RECOVERY=0 xvfb-run -a -n 200 -e /dev/stderr -s '-screen 0 800x600x24' "$work/test"
echo 'raylib secondary windows: pixels, events, clicks, dragging and the main context passed'
