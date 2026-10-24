#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/text_touch_behavior.zi
widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set -- \
    --bind image_raster:ImageWidth=text_touch_behavior:ImageWidth \
    --bind image_raster:ImageHeight=text_touch_behavior:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"
matrix_bundles=before
matrix_forms=source
matrix_c=linked
matrix_std="$ziran_root/std"
matrix_report='touch selection'
. "$repo/tests/main_matrix.sh"
