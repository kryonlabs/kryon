#include "ui_internal.h"

static float
ui_swipe_abs(float value)
{
    return value < 0.0f ? -value : value;
}

static unsigned int
ui_swipe_directions(UISwipeSpec spec)
{
    return spec.directions != 0 ? spec.directions : UI_SWIPE_ALL;
}

static UISwipeDirection
ui_swipe_direction(Vector2 delta, unsigned int directions, float axis_bias)
{
    float dx = ui_swipe_abs(delta.x);
    float dy = ui_swipe_abs(delta.y);
    UISwipeDirection direction;

    if(dx >= dy * axis_bias)
        direction = delta.x < 0.0f ? UI_SWIPE_LEFT : UI_SWIPE_RIGHT;
    else if(dy >= dx * axis_bias)
        direction = delta.y < 0.0f ? UI_SWIPE_UP : UI_SWIPE_DOWN;
    else
        return UI_SWIPE_NONE;

    return (directions & (unsigned int)direction) != 0
               ? direction
               : UI_SWIPE_NONE;
}

void
ResetUISwipe(UISwipeGesture *gesture)
{
    if(gesture == NULL)
        return;
    memset(gesture, 0, sizeof(*gesture));
    if(g_ui_pointer_owner == UI_POINTER_OWNER_SWIPE)
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
}

UISwipeResult
UpdateUISwipe(UISwipeGesture *gesture, UISwipeSpec spec)
{
    UISwipeResult result = {0};
    Vector2 pointer = ui_mouse_world();
    Vector2 delta = {0};
    unsigned int directions = ui_swipe_directions(spec);
    float min_distance = spec.min_distance > 0.0f
                             ? spec.min_distance
                             : (float)ScaleUIPx(48);
    float axis_bias = spec.axis_bias >= 1.0f ? spec.axis_bias : 1.25f;
    float decision_distance = (float)ScaleUIPx(8);
    double now = GetTime();

    if(gesture == NULL || spec.bounds.width <= 0.0f ||
       spec.bounds.height <= 0.0f)
        return result;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ResetUISwipe(gesture);
        if(g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           !ui_input_captures_click_internal(pointer, 0) &&
           CheckCollisionPointRec(pointer, spec.bounds)) {
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
        UISwipeDirection direction;
        float distance;

        if(g_ui_pointer_owner != UI_POINTER_OWNER_NONE &&
           g_ui_pointer_owner != UI_POINTER_OWNER_SWIPE) {
            gesture->active = 0;
            gesture->cancelled = 1;
            result.active = 0;
            result.cancelled = 1;
            return result;
        }

        direction = ui_swipe_direction(delta, directions, axis_bias);
        distance = ui_swipe_abs(delta.x) > ui_swipe_abs(delta.y)
                       ? ui_swipe_abs(delta.x)
                       : ui_swipe_abs(delta.y);

        if(!gesture->dragging && distance >= decision_distance) {
            if(direction == UI_SWIPE_NONE) {
                float dx = ui_swipe_abs(delta.x);
                float dy = ui_swipe_abs(delta.y);
                int horizontal_allowed = (directions & UI_SWIPE_HORIZONTAL) != 0;
                int vertical_allowed = (directions & UI_SWIPE_VERTICAL) != 0;

                if((horizontal_allowed && !vertical_allowed && dy >= dx * axis_bias) ||
                   (vertical_allowed && !horizontal_allowed && dx >= dy * axis_bias)) {
                    gesture->active = 0;
                    gesture->cancelled = 1;
                    result.active = 0;
                    result.cancelled = 1;
                }
                return result;
            }
            gesture->dragging = 1;
            g_ui_pointer_owner = UI_POINTER_OWNER_SWIPE;
        }

        result.dragging = gesture->dragging;
        if(gesture->dragging) {
            float primary = direction == UI_SWIPE_LEFT || direction == UI_SWIPE_RIGHT
                                ? ui_swipe_abs(delta.x)
                                : ui_swipe_abs(delta.y);
            result.progress = primary / min_distance;
            if(result.progress > 1.0f)
                result.progress = 1.0f;
            PushUIInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
        return result;
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UISwipeDirection direction =
            ui_swipe_direction(delta, directions, axis_bias);
        float primary = direction == UI_SWIPE_LEFT || direction == UI_SWIPE_RIGHT
                            ? ui_swipe_abs(delta.x)
                            : ui_swipe_abs(delta.y);
        double elapsed = now - gesture->started_at;
        int within_time = spec.max_duration <= 0.0f ||
                          elapsed <= (double)spec.max_duration;

        if(gesture->dragging) {
            UIConsumeRelease();
            PushUIInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
        if(gesture->dragging && within_time && primary >= min_distance)
            result.direction = direction;
        result.progress = min_distance > 0.0f ? primary / min_distance : 0.0f;
        if(result.progress > 1.0f)
            result.progress = 1.0f;
        result.dragging = 0;
        result.active = 0;
        ResetUISwipe(gesture);
        return result;
    }

    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        ResetUISwipe(gesture);
    return result;
}
