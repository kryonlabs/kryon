#include "kryon.h"
#include "kry_inject.h"

#include <stdio.h>
#include <stdlib.h>

static UISwipeGesture gesture;
static UISwipeSpec spec = {
    .bounds = {0.0f, 0.0f, 320.0f, 480.0f},
    .directions = UI_SWIPE_HORIZONTAL,
    .min_distance = 48.0f,
    .axis_bias = 1.25f
};

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static UISwipeResult
swipe_frame(float x, float y, int down)
{
    UISwipeResult result;

    InjectMousePosition(x, y);
    InjectMouseButton(MOUSE_BUTTON_LEFT, down);
    InjectPump();
    BeginUIFrame(320, 480, 1.0f);
    result = UpdateUISwipe(&gesture, spec);
    EndUIFrame();
    return result;
}

static void
reset_test(void)
{
    InjectReset();
    ResetUISwipe(&gesture);
}

static void
test_horizontal_swipes(void)
{
    UISwipeResult result;

    reset_test();
    result = swipe_frame(240.0f, 200.0f, 1);
    check_int("left press active", result.active, 1);
    result = swipe_frame(170.0f, 204.0f, 1);
    check_int("left drag claimed", result.dragging, 1);
    check_int("left drag progress", result.progress == 1.0f, 1);
    result = swipe_frame(170.0f, 204.0f, 0);
    check_int("left direction", result.direction, UI_SWIPE_LEFT);
    check_int("left release consumed", UIReleaseConsumed(), 1);

    reset_test();
    swipe_frame(80.0f, 200.0f, 1);
    swipe_frame(145.0f, 197.0f, 1);
    result = swipe_frame(145.0f, 197.0f, 0);
    check_int("right direction", result.direction, UI_SWIPE_RIGHT);
}

static void
test_threshold_and_axis_lock(void)
{
    UISwipeResult result;

    reset_test();
    swipe_frame(200.0f, 200.0f, 1);
    swipe_frame(170.0f, 201.0f, 1);
    result = swipe_frame(170.0f, 201.0f, 0);
    check_int("short drag has no direction", result.direction, UI_SWIPE_NONE);
    check_int("short claimed drag consumes release", UIReleaseConsumed(), 1);

    reset_test();
    swipe_frame(160.0f, 120.0f, 1);
    result = swipe_frame(163.0f, 190.0f, 1);
    check_int("vertical motion cancels horizontal gesture", result.cancelled, 1);
    result = swipe_frame(163.0f, 190.0f, 0);
    check_int("cancelled gesture has no direction", result.direction,
              UI_SWIPE_NONE);
    check_int("cancelled release remains available", UIReleaseConsumed(), 0);
}

static void
test_bounds_and_allowed_directions(void)
{
    UISwipeResult result;

    reset_test();
    swipe_frame(400.0f, 200.0f, 1);
    swipe_frame(300.0f, 200.0f, 1);
    result = swipe_frame(300.0f, 200.0f, 0);
    check_int("outside press ignored", result.direction, UI_SWIPE_NONE);

    reset_test();
    spec.directions = UI_SWIPE_LEFT;
    swipe_frame(80.0f, 200.0f, 1);
    swipe_frame(150.0f, 200.0f, 1);
    result = swipe_frame(150.0f, 200.0f, 0);
    check_int("disabled right direction ignored", result.direction,
              UI_SWIPE_NONE);
    spec.directions = UI_SWIPE_HORIZONTAL;
}

static void
test_vertical_swipes(void)
{
    UISwipeResult result;

    reset_test();
    spec.directions = UI_SWIPE_VERTICAL;
    swipe_frame(160.0f, 220.0f, 1);
    swipe_frame(157.0f, 150.0f, 1);
    result = swipe_frame(157.0f, 150.0f, 0);
    check_int("up direction", result.direction, UI_SWIPE_UP);

    reset_test();
    swipe_frame(160.0f, 120.0f, 1);
    swipe_frame(164.0f, 190.0f, 1);
    result = swipe_frame(164.0f, 190.0f, 0);
    check_int("down direction", result.direction, UI_SWIPE_DOWN);
    spec.directions = UI_SWIPE_HORIZONTAL;
}

int
main(void)
{
    InitUI(320, 480, 1.0f);
    test_horizontal_swipes();
    test_threshold_and_axis_lock();
    test_bounds_and_allowed_directions();
    test_vertical_swipes();
    reset_test();
    return 0;
}
