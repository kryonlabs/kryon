#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_game_controls_test.zi
set -- \
    --bind raster_shape:RasterRoundedRectangle=ziran_keyboard_navigation_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_keyboard_navigation_host:RasterRoundedRectangleOutline
widget_native=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
