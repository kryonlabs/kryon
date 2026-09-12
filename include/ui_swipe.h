#ifndef KRYON_SWIPE_H
#define KRYON_SWIPE_H

#include "kryon_compat.generated.h"

typedef enum SwipeDirection {
    SWIPE_NONE = 0,
    SWIPE_LEFT = 1 << 0,
    SWIPE_RIGHT = 1 << 1,
    SWIPE_UP = 1 << 2,
    SWIPE_DOWN = 1 << 3,
    SWIPE_HORIZONTAL = SWIPE_LEFT | SWIPE_RIGHT,
    SWIPE_VERTICAL = SWIPE_UP | SWIPE_DOWN,
    SWIPE_ALL = SWIPE_HORIZONTAL | SWIPE_VERTICAL
} SwipeDirection;

typedef struct SwipeGesture {
    /* Caller-owned state. Zero initialization is valid. */
    int active;
    int dragging;
    int cancelled;
    Vector2 start;
    double started_at;
} SwipeGesture;

typedef struct SwipeSpec {
    /* A press must begin inside bounds. */
    Rectangle bounds;
    /* Bitwise SwipeDirection values; zero enables every direction. */
    unsigned int directions;
    /* Defaults to 48 UI pixels when non-positive. */
    float min_distance;
    /* Dominant-axis ratio; values below 1 default to 1.25. */
    float axis_bias;
    /* Seconds from press to release; non-positive disables the time limit. */
    float max_duration;
} SwipeSpec;

typedef struct SwipeResult {
    /* Set for one release frame when a swipe completes. */
    SwipeDirection direction;
    Vector2 delta;
    /* Dominant distance divided by min_distance, clamped to 0..1. */
    float progress;
    int active;
    int dragging;
    int cancelled;
} SwipeResult;

SwipeResult UpdateSwipe(SwipeGesture *gesture, SwipeSpec spec);
void ResetSwipe(SwipeGesture *gesture);

#endif
