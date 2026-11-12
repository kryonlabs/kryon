#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_toast_widget_test.zi

widget_triangles_needed=1
widget_host=ziran_toast_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind kryon/image_raster:ImageWidth=ziran_toast_widget_host:ImageWidth \
    --bind kryon/image_raster:ImageHeight=ziran_toast_widget_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
