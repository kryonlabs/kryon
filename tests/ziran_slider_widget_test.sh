#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/slider-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_slider_widget_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_slider_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_slider_widget_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_slider_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_slider_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_slider_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_slider_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_slider_widget_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_slider_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_slider_widget_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
