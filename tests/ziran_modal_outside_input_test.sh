#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export HARMONY_DESKTOP_RECOVERY=0
export YUE_DESKTOP_RECOVERY=0
source=$repo/tests/ziran_modal_outside_input_test.zi
set -- \
    --bind kryon/raster_shape:RasterRoundedRectangle=ziran_overlay_widget_host:RasterRoundedRectangle
matrix_forms=both matrix_c=cc matrix_bundles=after-native \
    matrix_std="$ziran_root/std" . "$repo/tests/main_matrix.sh"
echo 'Modal scrim, popup stacking, capture, focus and Escape input passed'
