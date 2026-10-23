#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_area_menu_test.zi

widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set -- \
    --bind image_raster:ImageWidth=ziran_text_area_menu_test:ImageWidth \
    --bind image_raster:ImageHeight=ziran_text_area_menu_test:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_text_area_menu_test:main -o "$work/area.zib" "$source"
test "$("$ziran" run "$work/area.zib")" = 0
