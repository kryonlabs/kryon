. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_button_theme_raster_test.zi

widget_host=ziran_button_theme_raster_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_button_theme_raster_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_button_theme_raster_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
widget_native=1
. "$repo/tests/widget_matrix.sh"
