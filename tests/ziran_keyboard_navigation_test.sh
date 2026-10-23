#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_keyboard_navigation_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_keyboard_navigation_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_keyboard_navigation_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_keyboard_navigation_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_keyboard_navigation_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_keyboard_navigation_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_keyboard_navigation_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_keyboard_navigation_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_keyboard_navigation_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_keyboard_navigation_host:RasterImageMesh \
    --bind image_raster:ImageWidth=ziran_keyboard_navigation_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_keyboard_navigation_host:ImageHeight

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
