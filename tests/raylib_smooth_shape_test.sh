#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran_dir=${ZIRAN_DIR:-"$root/../ziran"}
compiler=${ZI2C_BIN:-"$ziran_dir/build/bin/zi2c"}
standard=${ZIRAN_STD:-"$ziran_dir/std"}
ziran=${ZIRAN_BIN:-"$ziran_dir/build/bin/ziran"}
raylib_source=${RAYLIB_SOURCE:-$("$ziran" pkg path raylib)}
raylib=${RAYLIB_A:-"$raylib_source/src/libraylib.a"}
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
mkdir -p "$root/build"
work=$(mktemp -d "$root/build/raylib-smooth-shape.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
"$compiler" --no-main --root "$root/tests" \
    --module-path "$root/src/backend" --module-path "$root/src/ui" \
    --module-path "$standard" -o "$work/c" \
    "$root/tests/raylib_smooth_shape_behavior.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
    -Wl,--gc-sections -I"$ziran_dir/include" -iquote "$work/c" \
    "$work/c"/*.c "$raylib" $raylib_libs \
    -o "$work/test"
cd "$work"
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY \
    timeout 20s xvfb-run -a -n 301 -s '-screen 0 640x480x24' ./test
echo 'Raylib rounded fills and outlines anti-aliased, square fills crisp: passed'
