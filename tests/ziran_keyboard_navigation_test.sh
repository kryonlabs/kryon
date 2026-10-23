#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_keyboard_navigation_test.zi

widget_host=ziran_keyboard_navigation_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_keyboard_navigation_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_keyboard_navigation_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
