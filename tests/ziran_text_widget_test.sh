#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_widget_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_text_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_text_widget_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_text_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_text_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_text_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_text_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_text_widget_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_text_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_text_widget_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
