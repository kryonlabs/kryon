. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_cui_inputs_test.zi

widget_host=cui_inputs_raster_host
widget_mesh_host=ziran_mesh_noop_host
widget_triangles_needed=1
set -- \
    --bind image_raster:ImageWidth=cui_inputs_raster_host:ImageWidth \
    --bind image_raster:ImageHeight=cui_inputs_raster_host:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
widget_native=1
widget_run_saved=1
. "$repo/tests/widget_matrix.sh"
