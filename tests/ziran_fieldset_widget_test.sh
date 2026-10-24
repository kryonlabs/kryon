#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/fieldset_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphWidth=fieldset_widget_host:MeasureGlyphWidth \
    --bind raster_shape:RasterRoundedRectangle=fieldset_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=fieldset_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=fieldset_widget_host:RasterLine \
    --bind raster_text:RasterText=fieldset_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=fieldset_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=fieldset_widget_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh

widget_native=1
widget_run_saved=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
