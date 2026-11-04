#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_widget_test.zi

widget_host=ziran_text_widget_host
widget_mesh_host=ziran_mesh_noop_host
set --
widget_triangles_needed=1
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
