#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/color_picker_widget_portable_test.zi
widget_host=color_picker_widget_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_native=1
widget_run_saved=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
