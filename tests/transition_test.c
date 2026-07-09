#include "ui_transition.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

void
DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    (void)color;
}

static void
check_true(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void
check_float_near(const char *name, float actual, float expected)
{
    if(fabsf(actual - expected) > 0.0001f) {
        fprintf(stderr, "FAIL: %s got %.6f want %.6f\n", name, actual, expected);
        failures++;
    }
}

static void
test_smooth_fade_alpha(void)
{
    UITransition transition;

    BeginUITransition(&transition, 1.0f);
    check_float_near("fade out starts transparent", GetUITransitionAlpha(&transition), 0.0f);
    check_true("half step does not complete", StepUITransition(&transition, 0.5f) == UI_TRANSITION_NONE);
    check_float_near("fade out smooth midpoint", GetUITransitionAlpha(&transition), 0.5f);
    check_true("fade out completes once", StepUITransition(&transition, 0.5f) == UI_TRANSITION_OUT);
    check_true("phase changes to fade in", transition.phase == UI_TRANSITION_IN);
    check_float_near("fade in starts opaque", GetUITransitionAlpha(&transition), 1.0f);
    check_true("fade in completes once", StepUITransition(&transition, 1.0f) == UI_TRANSITION_IN);
    check_true("transition resets after fade in", !transition.active);
}

static void
test_large_delta_and_clamp(void)
{
    UITransition transition;

    BeginUITransition(&transition, 0.0f);
    check_true("zero duration clamps active", transition.active);
    check_true("large delta completes only fade out phase",
               StepUITransition(&transition, 100.0f) == UI_TRANSITION_OUT);
    check_true("still active after large fade out step", transition.active);
    check_true("large delta completes fade in phase",
               StepUITransition(&transition, 100.0f) == UI_TRANSITION_IN);
    check_true("inactive after large fade in step", !transition.active);
}

static void
test_reverse_from_fade_in(void)
{
    UITransition transition;

    BeginUITransition(&transition, 1.0f);
    check_true("enter fade in", StepUITransition(&transition, 1.0f) == UI_TRANSITION_OUT);
    check_true("advance fade in", StepUITransition(&transition, 0.25f) == UI_TRANSITION_NONE);
    ReverseUITransitionToOut(&transition);
    check_true("reverse changes phase", transition.phase == UI_TRANSITION_OUT);
    check_float_near("reverse preserves eased visual alpha", GetUITransitionAlpha(&transition), 0.84375f);
}

int
main(void)
{
    test_smooth_fade_alpha();
    test_large_delta_and_clamp();
    test_reverse_from_fade_in();
    if(failures != 0)
        return 1;
    printf("transition tests passed\n");
    return 0;
}
