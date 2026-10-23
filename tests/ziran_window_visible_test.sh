#!/bin/sh
# A window on a private Xvfb display is visible; without a reachable display
# SDL falls back to its offscreen driver, which WindowVisible refuses.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SDL_VIDEODRIVER
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$ziran_root/build/bin/zi2c" --no-main --define NATIVE_WINDOW_HAVE_SDL --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/ziran_window_visible_test.zi"
if pkg-config --exists sdl2; then
    # shellcheck disable=SC2046
    "${CC:-cc}" -std=c11 -O2 -I"$ziran_root/include" -iquote "$work/c" "$work"/c/*.c \
        $(pkg-config --libs sdl2) -o "$work/run"
    WINDOW_VISIBLE_EXPECTED=1 YUE_DESKTOP_RECOVERY=0 \
        xvfb-run -a -n 200 -s '-noreset -screen 0 640x480x24' "$work/run"
    WINDOW_VISIBLE_EXPECTED=0 XDG_RUNTIME_DIR="$work" "$work/run"
else
    echo 'SDL2 unavailable; window visibility checks skipped'
fi
for define in _WIN32 PLATFORM_WEB ANDROID_BUILD; do
    "$ziran_root/build/bin/zi2c" --no-main --define "$define" --root "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$work/$define" "$repo/src/backend/window_visible.zi"
done
echo 'Window visible: a private display is visible; the offscreen fallback without one is not'
