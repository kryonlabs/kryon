#ifndef UI_SWIPE_H
#define UI_SWIPE_H

#include "kryon_compat.generated.h"

typedef enum UISwipeDirection {
    UI_SWIPE_NONE = 0,
    UI_SWIPE_LEFT = 1 << 0,
    UI_SWIPE_RIGHT = 1 << 1,
    UI_SWIPE_UP = 1 << 2,
    UI_SWIPE_DOWN = 1 << 3,
    UI_SWIPE_HORIZONTAL = UI_SWIPE_LEFT | UI_SWIPE_RIGHT,
    UI_SWIPE_VERTICAL = UI_SWIPE_UP | UI_SWIPE_DOWN,
    UI_SWIPE_ALL = UI_SWIPE_HORIZONTAL | UI_SWIPE_VERTICAL
} UISwipeDirection;

typedef struct UISwipeGesture {
    /* Caller-owned state. Zero initialization is valid. */
    int active;
    int dragging;
    int cancelled;
    Vector2 start;
    double started_at;
} UISwipeGesture;

typedef struct UISwipeSpec {
    /* A press must begin inside bounds. */
    Rectangle bounds;
    /* Bitwise UISwipeDirection values; zero enables every direction. */
    unsigned int directions;
    /* Defaults to 48 UI pixels when non-positive. */
    float min_distance;
    /* Dominant-axis ratio; values below 1 default to 1.25. */
    float axis_bias;
    /* Seconds from press to release; non-positive disables the time limit. */
    float max_duration;
} UISwipeSpec;

typedef struct UISwipeResult {
    /* Set for one release frame when a swipe completes. */
    UISwipeDirection direction;
    Vector2 delta;
    /* Dominant distance divided by min_distance, clamped to 0..1. */
    float progress;
    int active;
    int dragging;
    int cancelled;
} UISwipeResult;

UISwipeResult UpdateUISwipe(UISwipeGesture *gesture, UISwipeSpec spec);
void ResetUISwipe(UISwipeGesture *gesture);

#endif
