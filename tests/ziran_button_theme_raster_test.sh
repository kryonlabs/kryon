. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_button_theme_raster_test.zi

widget_host=ziran_button_theme_raster_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
widget_native=1
. "$repo/tests/widget_matrix.sh"
