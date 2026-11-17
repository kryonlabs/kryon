#!/bin/sh
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
out="$repo/build/scratch/avif-render"
mkdir -p "$out/assets"
cp "$repo/tests/fixtures/avif/rgba8.avif" "$out/assets/image"
for host in cairo; do
    set --
    libs="$(pkg-config --libs cairo freetype2 pangocairo fontconfig glib-2.0) -ldl -lm -lpthread"
    "$ziran" build --project --target=c "$@" --root "$repo/tests" \
        -o "$out/$host" "$repo/tests/avif_render_test.zi"
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections -Wno-main \
        -I"$ziran_root/include" -I"$out/$host" "$out/$host"/*.c $libs -o "$out/$host/test"
    env KRYON_AVIF_LIBRARY="$repo/build/avif-ziran/libkryonavif.so" KRYON_APP_DATA="$out/data" LIBGL_ALWAYS_SOFTWARE=1 \
        timeout 40s xvfb-run -a -s '-screen 0 640x480x24 -nolisten tcp' "$out/$host/test" \
        "$out/assets/image" "$repo/tests/fixtures/avif/rgba8.rgba"
done
"$ziran" build --project --target=c --root "$repo/tests" \
    -o "$out/pixmap" "$repo/tests/avif_pixmap_test.zi"
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$out/pixmap" "$out/pixmap"/*.c -ldl -lm -o "$out/pixmap/test"
env KRYON_AVIF_LIBRARY="$repo/build/avif-ziran/libkryonavif.so" "$out/pixmap/test" \
    "$out/assets/image" "$repo/tests/fixtures/avif/rgba8.rgba"
