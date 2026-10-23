#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_context_menu_widget_test.zi

widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set --
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_context_menu_widget_test:main -o "$work/menu.zx" "$source"
test "$("$ziran" run "$work/menu.zx")" = 0
