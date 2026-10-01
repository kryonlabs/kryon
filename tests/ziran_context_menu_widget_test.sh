#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_context_menu_widget_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_context_menu_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_context_menu_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=ziran_context_menu_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_context_menu_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_context_menu_widget_host:RasterLine \
    --bind raster_text:RasterText=ziran_context_menu_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_context_menu_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_context_menu_widget_host:RasterImage

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_context_menu_widget_test:main -o "$work/menu.zib" "$source"
test "$("$ziran" run "$work/menu.zib")" = 0
