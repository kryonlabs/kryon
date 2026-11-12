#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_area_menu_test.zi

widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set -- \
    --bind image_raster:ImageWidth=ziran_text_area_menu_test:ImageWidth \
    --bind image_raster:ImageHeight=ziran_text_area_menu_test:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" bundle --root "$repo/tests" $kryon_project_args \
     "$@" \
    --entry ziran_text_area_menu_test:main -o "$work/area.zx" "$source"
test "$("$ziran" run "$work/area.zx")" = 0
