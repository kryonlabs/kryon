#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_segmented_control_widget_test.zi

widget_host=ziran_clip_bounds_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_clip_bounds_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_clip_bounds_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
