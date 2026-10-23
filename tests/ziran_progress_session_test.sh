#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_progress_session_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_progress_session_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_progress_session_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=ziran_progress_session_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_progress_session_host:RasterTextClipped \
    --bind raster_shape:RasterRoundedRectangle=ziran_progress_session_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=ziran_progress_session_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=ziran_progress_session_host:RasterLine \
    --bind paint_queue:RasterImage=ziran_progress_session_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_progress_session_host:RasterImageMesh

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
