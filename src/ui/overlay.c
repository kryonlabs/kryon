#include "ui_internal.h"
#include "runtime/overlay.h"

DismissibleOverlayResult
DismissibleOverlay(DismissibleOverlayProps overlay)
{
    DismissibleOverlayResult result = {0};
    Rectangle bounds = overlay.bounds;
    Vector2 mouse = ui_mouse_world();
    int view_width = OverlayViewExtent(overlay.view_width, ui_view_width);
    int view_height = OverlayViewExtent(overlay.view_height, ui_view_height);
    DismissibleOverlayPolicy policy;

    if(overlay.scrim.a != 0)
        DrawRectangle(0, 0, view_width, view_height, overlay.scrim);

    SetModalCapture(bounds);
    policy = DismissibleOverlayPolicyFor(
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT), ReleaseConsumed(),
        overlay.dismiss_disabled != 0, CheckCollisionPointRec(mouse, bounds));
    if(policy.closed) {
        ConsumeRelease();
    }
    result.closed = policy.closed;
    result.outside_released = policy.outside_released;
    result.release_consumed = policy.release_consumed;

    return result;
}
