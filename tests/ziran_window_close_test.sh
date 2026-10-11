#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran_root/build/bin/zi2c" --no-main --define NATIVE_WINDOW_HAVE_SDL --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_window_close_test.zi"
if pkg-config --exists sdl2; then
    # shellcheck disable=SC2046
    "${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -iquote "$work/c" "$work"/c/*.c \
        $(pkg-config --libs sdl2) -o "$work/run"
    "$work/run"
else
    echo 'SDL2 unavailable; window close event checks skipped'
fi
for define in _WIN32 PLATFORM_WEB ANDROID_BUILD; do
    "$ziran_root/build/bin/zi2c" --no-main --define "$define" --root "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$work/$define" "$repo/src/backend/window_close.zi"
done
echo 'Window close: only the watched window, only close events, reported once'
