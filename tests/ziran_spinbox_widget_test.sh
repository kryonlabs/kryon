#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_spinbox_widget_test.zi

widget_host=ziran_spinbox_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_spinbox_widget_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_spinbox_widget_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
