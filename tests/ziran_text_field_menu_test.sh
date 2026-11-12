#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_field_menu_test.zi

widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set -- \
    --bind image_raster:ImageWidth=ziran_widget_noop_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_widget_noop_host:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" bundle --root "$repo/tests" $kryon_project_args \
     "$@" \
    --entry ziran_text_field_menu_test:main -o "$work/field.zx" "$source"
test "$("$ziran" run "$work/field.zx")" = 0
