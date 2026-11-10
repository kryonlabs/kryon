unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_cui_actions_test.zi

widget_host=cui_actions_raster_host
widget_mesh_host=cui_actions_raster_host
widget_triangles_needed=1
set -- \
    --bind image_raster:ImageWidth=cui_actions_raster_host:ImageWidth \
    --bind image_raster:ImageHeight=cui_actions_raster_host:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=cui_actions_raster_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=cui_actions_raster_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=cui_actions_raster_host:SystemClipboardWriteHost
# The native C matrix needs one provider for each foreign symbol.
set -- "$@" \
    --bind font_metrics:MeasureGlyphWidth=cui_actions_raster_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=cui_actions_raster_host:MeasureGlyphLineHeight \
    --bind raster_text:RasterText=cui_actions_raster_host:RasterText \
    --bind raster_text:RasterTextClipped=cui_actions_raster_host:RasterTextClipped \
    --bind raster_text:RasterTextRotated=cui_actions_raster_host:RasterTextRotated \
    --bind raster_shape:RasterRoundedRectangle=cui_actions_raster_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=cui_actions_raster_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=cui_actions_raster_host:RasterLine \
    --bind paint_queue:RasterImage=cui_actions_raster_host:RasterImage \
    --bind paint_queue:RasterImageMesh=cui_actions_raster_host:RasterImageMesh \
    --bind raster_triangles:RasterTriangles=cui_actions_raster_host:RasterTriangles

widget_std="$ziran_root/std"
widget_native=1
widget_run_saved=1
. "$repo/tests/widget_matrix.sh"
