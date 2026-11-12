#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_area_wheel_test.zi
widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind kryon/image_raster:ImageWidth=ziran_text_area_wheel_test:ImageWidth \
    --bind kryon/image_raster:ImageHeight=ziran_text_area_wheel_test:ImageHeight \
    --bind kryon/system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind kryon/system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind kryon/system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"
matrix_bundles=before
matrix_forms=source
matrix_c=linked
matrix_std="$ziran_root/std"
matrix_report='text area wheel'
. "$repo/tests/main_matrix.sh"
