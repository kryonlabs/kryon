#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_page_text_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_page_text_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_page_text_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_page_text_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_page_text_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_page_text_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_page_text_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_page_text_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_page_text_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_page_text_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
