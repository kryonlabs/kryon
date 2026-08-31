#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>

static int mouse_released;
static int draw_count;
static Color last_scrim;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

void
__wrap_DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    draw_count++;
    last_scrim = color;
}

bool
__wrap_IsMouseButtonReleased(int button)
{
    return button == MOUSE_BUTTON_LEFT && mouse_released;
}

static DismissibleOverlayResult
overlay_frame(Vector2 mouse, int released)
{
    DismissibleOverlayProps props = {0};
    Color scrim = BLACK;

    draw_count = 0;
    scrim.a = 92;
    mouse_released = 0;
    BeginUIFrame(320, 560, 1.0f);
    SetUIMouseWorldOverride(1, mouse);
    mouse_released = released;
    props.bounds = (Rectangle){80.0f, 0.0f, 240.0f, 560.0f};
    props.view_width = 320;
    props.view_height = 560;
    props.scrim = scrim;
    return DismissibleOverlay(props);
}

static void
finish_frame(void)
{
    SetUIMouseWorldOverride(0, (Vector2){0});
    mouse_released = 0;
    EndUIFrame();
}

static void
test_outside_release_closes_and_consumes(void)
{
    DismissibleOverlayResult result;

    result = overlay_frame((Vector2){24.0f, 120.0f}, 1);
    check_int("outside release closes", result.closed, 1);
    check_int("outside release consumed", result.release_consumed, 1);
    check_int("outside flag", result.outside_released, 1);
    check_int("ui release consumed", UIReleaseConsumed(), 1);
    check_int("scrim drawn", draw_count, 1);
    check_int("scrim alpha", last_scrim.a, 92);
    finish_frame();
}

static void
test_inside_release_keeps_overlay_open(void)
{
    DismissibleOverlayResult result;

    result = overlay_frame((Vector2){120.0f, 120.0f}, 1);
    check_int("inside release stays open", result.closed, 0);
    check_int("inside release not consumed", result.release_consumed, 0);
    check_int("inside point not captured",
              UIInputCapturesClick((Vector2){120.0f, 120.0f}), 0);
    check_int("outside point captured",
              UIInputCapturesClick((Vector2){24.0f, 120.0f}), 1);
    finish_frame();
}

int
main(void)
{
    test_outside_release_closes_and_consumes();
    test_inside_release_keeps_overlay_open();
    return 0;
}
