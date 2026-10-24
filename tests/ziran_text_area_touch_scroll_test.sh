#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_area_touch_scroll_test.zi
widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_text_area_touch_scroll_test:ImageWidth \
    --bind image_raster:ImageHeight=ziran_text_area_touch_scroll_test:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"
matrix_bundles=before
matrix_forms=source
matrix_c=linked
matrix_std="$ziran_root/std"
matrix_report='text area touch scroll'
. "$repo/tests/main_matrix.sh"
