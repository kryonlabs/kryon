#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/svg_icon_cache_behavior.zi
# Icons only queue paint; the portable run binds the test raster host.
set -- \
    --bind raster:RasterLine=ziran_navigation_bar_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_navigation_bar_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_navigation_bar_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_navigation_bar_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_navigation_bar_widget_host:RasterRoundedRectangleOutline \
    --bind paint_queue:RasterImage=ziran_navigation_bar_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" "$@" \
    --entry svg_icon_cache_behavior:main -o "$work/icon-cache.zx" "$source"
test "$("$ziran" run "$work/icon-cache.zx")" = 0

"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -I"$ziran_root/include" -I"$work/c" \
    "$work/c"/*.c -Wl,--gc-sections -lm -o "$work/icon-cache-test"
"$work/icon-cache-test"
