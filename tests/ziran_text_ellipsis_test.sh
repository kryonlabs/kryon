#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/test/text_ellipsis.$$
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_ellipsis_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_widget_noop_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_widget_noop_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_widget_noop_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_widget_noop_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_widget_noop_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_widget_noop_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_widget_noop_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_widget_noop_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_widget_noop_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
