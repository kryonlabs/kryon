#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_field_menu_test.zi

set -- \
    --bind image_raster:ImageWidth=ziran_widget_noop_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_widget_noop_host:ImageHeight \
    --bind font_metrics:MeasureGlyphWidth=ziran_context_menu_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_context_menu_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_context_menu_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_context_menu_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_context_menu_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_context_menu_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_context_menu_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_context_menu_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_context_menu_widget_host:RasterImageMesh \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_text_field_menu_test:main -o "$work/field.zib" "$source"
test "$("$ziran" run "$work/field.zib")" = 0
