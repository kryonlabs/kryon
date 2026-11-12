# Shared raster effects for widget test bundles. The caller supplies its
# host modules and keeps its additional bindings in the positional arguments.
set -- "$@" \
    --bind "kryon/font_metrics:MeasureGlyphWidth=$widget_host:MeasureGlyphWidth" \
    --bind "kryon/font_metrics:MeasureGlyphLineHeight=$widget_host:MeasureGlyphLineHeight" \
    --bind "kryon/raster_text:RasterText=$widget_host:RasterText" \
    --bind "kryon/raster_text:RasterTextClipped=$widget_host:RasterTextClipped" \
    --bind "kryon/raster_shape:RasterRoundedRectangle=$widget_host:RasterRoundedRectangle" \
    --bind "kryon/raster_shape:RasterRoundedRectangleOutline=$widget_host:RasterRoundedRectangleOutline" \
    --bind "kryon/raster:RasterLine=$widget_host:RasterLine" \
    --bind "kryon/paint_queue:RasterImage=$widget_host:RasterImage" \
    --bind "kryon/paint_queue:RasterImageMesh=$widget_mesh_host:RasterImageMesh"

widget_rotated_host=ziran_mesh_noop_host
case "$widget_host" in
    ziran_link_widget_host|separator_widget_host|ziran_overlay_widget_host|ziran_shape_widget_host|ziran_context_menu_widget_host|ziran_mixed_tree_host|paned_view_host|ziran_test_driver_host)
        widget_rotated_host=$widget_host ;;
esac
widget_triangles_function=RasterTriangles
if test "$widget_rotated_host" = ziran_mesh_noop_host; then widget_triangles_function=NoopRasterTriangles; fi
set -- "$@" --bind "kryon/raster_text:RasterTextRotated=$widget_rotated_host:RasterTextRotated"
if test "${widget_triangles_needed:-0}" = 1; then
    set -- "$@" --bind "kryon/raster_triangles:RasterTriangles=$widget_rotated_host:$widget_triangles_function"
fi
