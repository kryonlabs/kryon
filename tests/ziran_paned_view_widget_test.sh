#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"

source=$repo/tests/paned_view_portable_test.zi
bind_fill=raster_shape:RasterRoundedRectangle=paned_view_host:RasterRoundedRectangle
bind_outline=raster_shape:RasterRoundedRectangleOutline=paned_view_host:RasterRoundedRectangleOutline
bind_line=raster:RasterLine=paned_view_host:RasterLine
bind_text=raster_text:RasterText=paned_view_host:RasterText
bind_clipped=raster_text:RasterTextClipped=paned_view_host:RasterTextClipped
bind_image=paint_queue:RasterImage=paned_view_host:RasterImage
bind_mesh=paint_queue:RasterImageMesh=paned_view_host:RasterImageMesh

set -- --bind "$bind_fill" --bind "$bind_outline" --bind "$bind_line" \
    --bind "$bind_text" --bind "$bind_clipped" --bind "$bind_image" --bind "$bind_mesh"
widget_native=1
widget_run_saved=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"
