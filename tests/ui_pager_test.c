#include "kryon.h"
#include "kry_inject.h"

#include <stdio.h>
#include <stdlib.h>

static UISwipeGesture swipe;
static UIGuidePagerProps pager = {
    .content_bounds = {0.0f, 0.0f, 320.0f, 400.0f},
    .footer_bounds = {0.0f, 400.0f, 320.0f, 80.0f},
    .swipe = &swipe,
    .page_count = 4,
    .focus_id = 800,
    .close_label = "Close",
    .back_label = "Back",
    .next_label = "Next",
    .finish_label = "Start"
};

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static UIGuidePagerResult
pager_frame(float x, float y, int down)
{
    UIGuidePagerResult result;

    InjectMousePosition(x, y);
    InjectMouseButton(MOUSE_BUTTON_LEFT, down);
    InjectPump();
    BeginUIFrame(320, 480, 1.0f);
    BeginUIFocus();
    result = DrawUIGuidePager(pager);
    EndUIFocus();
    EndUIFrame();
    return result;
}

static void
reset_test(int page)
{
    InjectReset();
    ResetUISwipe(&swipe);
    ClearUIFocus();
    pager.page = page;
}

static UIGuidePagerResult
click_footer(float x)
{
    pager_frame(x, 440.0f, 1);
    return pager_frame(x, 440.0f, 0);
}

static UIGuidePagerResult
swipe_content(float from_x, float to_x)
{
    pager_frame(from_x, 220.0f, 1);
    pager_frame(to_x, 222.0f, 1);
    return pager_frame(to_x, 222.0f, 0);
}

static void
test_footer_actions(void)
{
    UIGuidePagerResult result;

    reset_test(0);
    result = click_footer(70.0f);
    check_int("first left closes", result.closed, 1);

    reset_test(1);
    result = click_footer(70.0f);
    check_int("middle left changes", result.changed, 1);
    check_int("middle left page", result.page, 0);

    reset_test(1);
    result = click_footer(245.0f);
    check_int("middle right changes", result.changed, 1);
    check_int("middle right page", result.page, 2);

    reset_test(3);
    result = click_footer(245.0f);
    check_int("last right finishes", result.finished, 1);
    check_int("finish keeps page", result.page, 3);
}

static void
test_swipe_actions_and_boundaries(void)
{
    UIGuidePagerResult result;

    reset_test(1);
    result = swipe_content(270.0f, 100.0f);
    check_int("left swipe changes", result.changed, 1);
    check_int("left swipe advances", result.page, 2);

    reset_test(2);
    result = swipe_content(80.0f, 250.0f);
    check_int("right swipe changes", result.changed, 1);
    check_int("right swipe returns", result.page, 1);

    reset_test(0);
    result = swipe_content(80.0f, 250.0f);
    check_int("boundary swipe does not close", result.closed, 0);
    check_int("boundary swipe keeps page", result.page, 0);

    reset_test(3);
    result = swipe_content(270.0f, 100.0f);
    check_int("boundary swipe does not finish", result.finished, 0);
    check_int("finish boundary keeps page", result.page, 3);
}

int
main(void)
{
    InitUI(320, 480, 1.0f);
    test_footer_actions();
    test_swipe_actions_and_boundaries();
    reset_test(0);
    return 0;
}
