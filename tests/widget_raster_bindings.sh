# Shared raster effects for widget test bundles. The caller supplies its
# host modules and keeps its additional bindings in the positional arguments.
set -- "$@" \
    --bind "font_metrics:MeasureGlyphWidth=$widget_host:MeasureGlyphWidth" \
    --bind "font_metrics:MeasureGlyphLineHeight=$widget_host:MeasureGlyphLineHeight" \
    --bind "raster_text:RasterText=$widget_host:RasterText" \
    --bind "raster_text:RasterTextClipped=$widget_host:RasterTextClipped" \
    --bind "raster_shape:RasterRoundedRectangle=$widget_host:RasterRoundedRectangle" \
    --bind "raster_shape:RasterRoundedRectangleOutline=$widget_host:RasterRoundedRectangleOutline" \
    --bind "raster:RasterLine=$widget_host:RasterLine" \
    --bind "paint_queue:RasterImage=$widget_host:RasterImage" \
    --bind "paint_queue:RasterImageMesh=$widget_mesh_host:RasterImageMesh"
