#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_frame_transition_test.zi

widget_host=ziran_frame_transition_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind kryon/frame_transition:CopyFrameTexture=ziran_frame_transition_host:CopyFrameTexture \
    --bind kryon/frame_transition:DisposeFrameTexture=ziran_frame_transition_host:DisposeFrameTexture \
    --bind kryon/image_raster:ImageWidth=ziran_frame_transition_host:ImageWidth \
    --bind kryon/image_raster:ImageHeight=ziran_frame_transition_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

matrix_bundles=before
matrix_forms=both
matrix_c=none
matrix_std="$ziran_root/std"
. "$repo/tests/main_matrix.sh"
