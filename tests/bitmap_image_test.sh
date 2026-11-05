#!/bin/sh
# Invoked through build-queue by the caller/Make target.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/bitmap-image"
mkdir -p "$out"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/bitmap_image_test.zi"
set -- --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind raster_text:RasterTextRotated=pixmap:RasterTextRotated \
    --bind paint_queue:RasterImage=pixmap:RasterImage \
    --bind paint_queue:RasterImageMesh=pixmap:RasterImageMesh \
    --bind image_raster:ImageWidth=pixmap:ImageWidth \
    --bind image_raster:ImageHeight=pixmap:ImageHeight \
    --bind font_metrics:MeasureGlyphWidth=pixmap:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=pixmap:MeasureGlyphLineHeight
"$ziran" ir --project --root "$repo/tests" -o "$out/ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$out/ir/bitmap_image_test.zir; root=$out/ir; fi
    "$ziran" build --project --root "$root" --target=c --entry bitmap_image_test:main --exe -o "$out/pixmap-$form" "$input"
    "$out/pixmap-$form/bitmap_image_test"
    "$ziran" bundle --project --root "$root" --entry bitmap_image_test:main "$@" -o "$out/$form.zx" "$input"
    result=$("$ziran" run "$out/$form.zx")
    printf '%s\n' "$result"
    test "$(printf '%s\n' "$result" | tail -1)" = 0
done
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -ldl -lm -lpthread"
"$ziran" ir --project --root "$repo/tests" --define BITMAP_DESKTOP -o "$out/desktop-ir" "$source"
for form in source saved; do
    input=$source; root=$repo/tests
    if test "$form" = saved; then input=$out/desktop-ir/bitmap_image_test.zir; root=$out/desktop-ir; fi
    LDLIBS="$libs" "$ziran" build --project --root "$root" --target=c --define BITMAP_DESKTOP --entry bitmap_image_test:main --exe -o "$out/desktop-$form" "$input"
    LIBGL_ALWAYS_SOFTWARE=1 timeout 45s xvfb-run -a -s '-screen 0 640x480x24 -nolisten tcp' "$out/desktop-$form/bitmap_image_test"
done
printf 'Managed Image source/saved C, portable pixmap and private-Xvfb Desktop/Cairo passed\n'
