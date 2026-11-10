#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
output=$repo/build/scratch/rotated-text/core
mkdir -p "$output"
"$ziran" build --target=c --root "$repo/tests" $kryon_ui_args \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$output" "$repo/tests/rotated_text_test.zi"
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$output" "$output"/*.c -lm -o "$output/test"
"$output/test"
echo 'Rotated text: real pixmap glyphs, arbitrary angles, pivot, font size and link hits PASS'

set -- --bind font_metrics:MeasureGlyphWidth=pixmap:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=pixmap:MeasureGlyphLineHeight \
    --bind raster_triangles:RasterTriangles=pixmap:RasterTriangles \
    --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind raster_text:RasterTextRotated=pixmap:RasterTextRotated \
    --bind paint_queue:RasterImage=pixmap:RasterImage \
    --bind paint_queue:RasterImageMesh=pixmap:RasterImageMesh
"$ziran" bundle --root "$repo/tests" $kryon_ui_args \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" "$@" \
    --entry rotated_text_test:main -o "$output/test.zx" "$repo/tests/rotated_text_test.zi"
test "$("$ziran" run "$output/test.zx")" = 0
echo 'Rotated text: portable ZX glyph rotation PASS'
