#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/scratch/rotated-text/raylib
mkdir -p "$work"
archive=${RAYLIB_A:-$repo/build/raylib-ziran/libraylib.a}
libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2 libdrm gbm egl glesv2) -ldl -lpthread -lm}
for profile in native android-replay; do
    set --
    if [ "$profile" = android-replay ]; then set -- --define KRYON_FRAME_THREADS; fi
    output=$work/$profile
    "$ziran" build --project --target=c "$@" --root "$repo/tests" --module-path "$repo/tests" \
        -o "$output" "$repo/tests/rotated_text_raylib_test.zi"
    "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$output" "$output"/*.c "$archive" "${WEBP_A:-$repo/build/webp-ziran/libkryonwebp.a}" "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" $libs -o "$output/test"
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
        -u GDK_DISPLAY YUE_DESKTOP_RECOVERY=0 LIBGL_ALWAYS_SOFTWARE=1 \
        timeout --kill-after=2s 45s xvfb-run -a -n 300 -s '-screen 0 300x300x24' "$output/test"
done
echo 'Native and Android shared text/replay renderer: offscreen glyph rotation PASS'
