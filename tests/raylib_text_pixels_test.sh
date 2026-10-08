#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${KRYON_TEXT_TEST_OUTPUT:-"$repo/build/scratch/raylib-text-pixels"}
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
archive=${RAYLIB_A:-$("$ziran" pkg path raylib)/src/libraylib.a}
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/raylib_text_pixels_test.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$work/c" "$work/c"/*.c "$archive" \
    $raylib_libs -o "$work/test"
cd "$repo"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    -u GDK_DISPLAY YUE_DESKTOP_RECOVERY=0 \
    timeout --kill-after=2s 45s xvfb-run -a -n 300 \
    -s '-screen 0 1280x900x24' "$work/test"
echo 'Native glyph pixel coverage matches at 50%–400%, fractional camera scales and atlas eviction'
