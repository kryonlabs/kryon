#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_frame_transition_test.zi

widget_host=ziran_frame_transition_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind frame_transition:DisposeFrameTexture=ziran_frame_transition_host:DisposeFrameTexture \
    --bind image_raster:ImageWidth=ziran_frame_transition_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_frame_transition_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

matrix_bundles=before
matrix_forms=both
matrix_c=none
matrix_std="$ziran_root/std"
. "$repo/tests/main_matrix.sh"
