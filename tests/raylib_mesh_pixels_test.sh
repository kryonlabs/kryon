#!/bin/sh
# Warped images through raylib: a plain, a mirrored and a bent grid drawn
# into a private Xvfb window and read back, then solid triangles in both
# windings.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/raylib-mesh-pixels"
mkdir -p "$work"
exec 9>"$work/lock"
flock 9
# The archive Kryon's raylib projects build (mk/ziran-project.mk).
archive=${RAYLIB_A:-"$repo/build/raylib-ziran/libraylib.a"}
test -f "$archive" || { echo "build a raylib project first, or set RAYLIB_A" >&2; exit 1; }
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/c" \
    "$repo/tests/raylib_mesh_pixels_test.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$work/c" "$work/c"/*.c "$archive" \
    $raylib_libs -o "$work/test"
cd "$repo"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
    -u GDK_DISPLAY YUE_DESKTOP_RECOVERY=0 \
    timeout --kill-after=2s 45s xvfb-run -a -n 301 \
    -s '-screen 0 640x480x24' dbus-run-session -- "$work/test"
echo 'Warped images and solid triangles draw through raylib'
