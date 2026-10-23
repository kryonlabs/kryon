#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_navigation_bar_widget_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_navigation_bar_widget_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_navigation_bar_widget_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_navigation_bar_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_navigation_bar_widget_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_navigation_bar_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_navigation_bar_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_navigation_bar_widget_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_navigation_bar_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_navigation_bar_widget_host:RasterImageMesh \
    --bind image_raster:ImageWidth=ziran_navigation_bar_widget_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_navigation_bar_widget_host:ImageHeight

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
