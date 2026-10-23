#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_swipe_row_test.zi

set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_keyboard_navigation_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_keyboard_navigation_host:RasterRoundedRectangleOutline \
    --bind paint_queue:RasterImage=ziran_keyboard_navigation_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
