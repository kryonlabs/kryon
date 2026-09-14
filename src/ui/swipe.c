#include "ui_internal.h"
#include "runtime/swipe.h"

void
ResetSwipe(SwipeGesture *gesture)
{
    if(gesture == NULL)
        return;
    memset(gesture, 0, sizeof(*gesture));
    if(g_ui_pointer_owner == POINTER_OWNER_SWIPE)
        g_ui_pointer_owner = POINTER_OWNER_NONE;
}

SwipeResult
UpdateSwipe(SwipeGesture *gesture, SwipeSpec spec)
{
    SwipeResult result = {0};
    Vector2 pointer = ui_mouse_world();
    Vector2 delta = {0};
    unsigned int directions = SwipeDirectionsFor(spec.directions);
    float min_distance = SwipeMinDistanceFor(GetScale(), spec.min_distance);
    float axis_bias = SwipeAxisBiasFor(spec.axis_bias);
    float decision_distance = SwipeDecisionDistanceFor(GetScale());
    double now = GetTime();

    if(gesture == NULL || spec.bounds.width <= 0.0f ||
       spec.bounds.height <= 0.0f)
        return result;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ResetSwipe(gesture);
        if(SwipeCanBegin(spec.bounds,
                         CheckCollisionPointRec(pointer, spec.bounds),
                         g_ui_pointer_owner == POINTER_OWNER_NONE &&
                         !ui_input_captures_click_internal(pointer, 0))) {
            gesture->active = 1;
            gesture->start = pointer;
            gesture->started_at = now;
        }
    }

    if(!gesture->active) {
        result.cancelled = gesture->cancelled;
        return result;
    }

    delta.x = pointer.x - gesture->start.x;
    delta.y = pointer.y - gesture->start.y;
    result.delta = delta;
    result.active = 1;

    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        SwipeDragState drag;
        SwipeDragLifecycle lifecycle;

        drag = SwipeDragStateFor(delta, directions, axis_bias,
                                 decision_distance, min_distance,
                                 gesture->dragging != 0);
        lifecycle = SwipeDragLifecycleFor(
            g_ui_pointer_owner == POINTER_OWNER_NONE ||
                g_ui_pointer_owner == POINTER_OWNER_SWIPE,
            drag.cancelled, gesture->dragging != 0, drag.dragging);
        if(lifecycle.cancel_active) {
            gesture->active = 0;
            gesture->cancelled = 1;
            result.active = 0;
            result.cancelled = 1;
            return result;
        }
        if(lifecycle.claim_pointer_owner) {
            gesture->dragging = 1;
            g_ui_pointer_owner = POINTER_OWNER_SWIPE;
        }

        result.dragging = lifecycle.dragging;
        if(lifecycle.capture_input) {
            result.progress = drag.progress;
            PushInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
        return result;
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        SwipeReleaseState release =
            SwipeReleaseStateFor(delta, directions, axis_bias, min_distance,
                                 now - gesture->started_at,
                                 spec.max_duration,
                                 gesture->dragging != 0);
        SwipeReleaseLifecycle lifecycle =
            SwipeReleaseLifecycleFor(gesture->dragging != 0);

        if(lifecycle.consume_release)
            ConsumeRelease();
        if(lifecycle.capture_input) {
            PushInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
        result.direction = release.direction;
        result.progress = release.progress;
        result.dragging = 0;
        result.active = 0;
        ResetSwipe(gesture);
        return result;
    }

    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        ResetSwipe(gesture);
    return result;
}
