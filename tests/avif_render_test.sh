#!/bin/sh
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
out="$repo/build/scratch/avif-render"
mkdir -p "$out/assets"
cp "$repo/tests/fixtures/avif/rgba8.avif" "$out/assets/image"
"$ziran" bundle --project --define KRYON_HOSTED_UI --root "$repo/tests" \
    --asset-dir "assets=$out/assets" \
    --entry avif_bundle_guest:main -o "$out/app.zx" "$repo/tests/avif_bundle_guest.zi"
for host in cairo raylib; do
    set --
    libs="$(pkg-config --libs cairo freetype2 pangocairo fontconfig glib-2.0) -ldl -lm -lpthread"
    if test "$host" = raylib; then
        set -- --define AVIF_RAYLIB --define BUNDLE_NATIVE_RENDERER
        libs="$repo/build/raylib-ziran/libraylib.a $repo/build/webp-ziran/libkryonwebp.a $repo/build/avif-ziran/avif/libavif.a $repo/build/avif-ziran/aom/libaom.a $repo/build/zx-raylib/runtime/libziran.a $(pkg-config --libs sdl2 libdrm gbm egl glesv2) $(pkg-config --libs libcurl 2>/dev/null || echo -l:libcurl.so.4) -lopus -ldl -lpthread -lm"
    fi
    "$ziran" build --project --target=c "$@" --root "$repo/tests" \
        -o "$out/$host" "$repo/tests/avif_render_test.zi"
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections -Wno-main \
        -I"$ziran_root/include" -I"$out/$host" "$out/$host"/*.c $libs -o "$out/$host/test"
    env KRYON_AVIF_LIBRARY="$repo/build/avif-ziran/libkryonavif.so" KRYON_APP_DATA="$out/data" LIBGL_ALWAYS_SOFTWARE=1 \
        timeout 40s xvfb-run -a -s '-screen 0 640x480x24 -nolisten tcp' "$out/$host/test" \
        "$out/assets/image" "$repo/tests/fixtures/avif/rgba8.rgba" "$out/app.zx"
done
"$ziran" build --project --target=c --root "$repo/tests" \
    -o "$out/pixmap" "$repo/tests/avif_pixmap_test.zi"
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$out/pixmap" "$out/pixmap"/*.c -ldl -lm -o "$out/pixmap/test"
env KRYON_AVIF_LIBRARY="$repo/build/avif-ziran/libkryonavif.so" "$out/pixmap/test" \
    "$out/assets/image" "$repo/tests/fixtures/avif/rgba8.rgba"
