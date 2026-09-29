#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_area_menu_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_context_menu_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_context_menu_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_context_menu_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_context_menu_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_context_menu_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_context_menu_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_context_menu_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_context_menu_widget_host:RasterImage \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" "$@" \
    --entry ziran_text_area_menu_test:main -o "$work/area.zib" "$source"
test "$("$ziran" run "$work/area.zib")" = 0
