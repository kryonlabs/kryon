#include "ui_internal.h"

DismissibleOverlayResult
DismissibleOverlay(DismissibleOverlayProps overlay)
{
    DismissibleOverlayResult result = {0};
    Rectangle bounds = overlay.bounds;
    Vector2 mouse = ui_mouse_world();
    int view_width = overlay.view_width > 0 ? overlay.view_width : ui_view_width;
    int view_height = overlay.view_height > 0 ? overlay.view_height : ui_view_height;

    if(overlay.scrim.a != 0)
        DrawRectangle(0, 0, view_width, view_height, overlay.scrim);

    SetModalCapture(bounds);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ReleaseConsumed() &&
       !overlay.dismiss_disabled &&
       !CheckCollisionPointRec(mouse, bounds)) {
        ConsumeRelease();
        result.closed = 1;
        result.outside_released = 1;
        result.release_consumed = 1;
    } else {
        result.release_consumed = ReleaseConsumed();
    }

    return result;
}
