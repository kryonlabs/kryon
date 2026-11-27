#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_field_widget_test.zi

widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind kryon/system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind kryon/system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind kryon/system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
widget_native=1
. "$repo/tests/widget_matrix.sh"
