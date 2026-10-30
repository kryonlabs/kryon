#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_carousel_widget_test.zi

widget_host=ziran_image_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_image_widget_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_image_widget_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_native=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"

# Keep selection behavior covered alongside the rendered cover regression.
(
    work=$work/selection
    mkdir "$work"
    source=$repo/tests/carousel_behavior.zi
    set --
    . "$repo/tests/widget_matrix.sh"
)
