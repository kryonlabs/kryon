#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_card_widget_test.zi
widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_card_image_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_card_image_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"
widget_native=1
widget_run_saved=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
