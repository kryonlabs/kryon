#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/svg_icon_scale_behavior.zi
# Icons only queue paint; the portable run binds the test raster host.
set -- \
    --bind kryon/raster:RasterLine=ziran_navigation_bar_widget_host:RasterLine \
    --bind kryon/raster_text:RasterText=ziran_navigation_bar_widget_host:RasterText \
    --bind kryon/raster_text:RasterTextClipped=ziran_navigation_bar_widget_host:RasterTextClipped \
    --bind kryon/raster_text:RasterTextRotated=ziran_mesh_noop_host:RasterTextRotated \
    --bind kryon/raster_shape:RasterRoundedRectangle=ziran_navigation_bar_widget_host:RasterRoundedRectangle \
    --bind kryon/raster_shape:RasterRoundedRectangleOutline=ziran_navigation_bar_widget_host:RasterRoundedRectangleOutline \
    --bind kryon/paint_queue:RasterImage=ziran_navigation_bar_widget_host:RasterImage \
    --bind kryon/paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh
"$ziran" bundle --root "$repo/tests" $kryon_project_args  "$@" \
    --entry svg_icon_scale_behavior:main -o "$work/icon.zx" "$source"
test "$("$ziran" run "$work/icon.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    $kryon_project_args  -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -Wl,--gc-sections -lm -o "$work/icon-test"
"$work/icon-test"
