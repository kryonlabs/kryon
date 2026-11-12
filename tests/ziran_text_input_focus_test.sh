#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_input_focus_test.zi

widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_widget_noop_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_widget_noop_host:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
