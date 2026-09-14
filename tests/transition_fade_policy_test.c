#include <assert.h>
#include <math.h>

#include "runtime/transition_fade.h"

static void
check_near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.0001f);
}

int
main(void)
{
    check_near(TransitionClampProgress(-1.0f), 0.0f);
    check_near(TransitionClampProgress(2.0f), 1.0f);
    check_near(TransitionSmoothProgress(0.5f), 0.5f);
    check_near(TransitionDuration(0.0f), 0.001f);
    check_near(TransitionDelta(-3.0f), 0.0f);
    check_near(TransitionReverseElapsed(1.0f, 0.25f), 0.75f);
    check_near(TransitionReverseElapsed(1.0f, 2.0f), 0.0f);

    check_near(TransitionAlpha(true, TransitionOut, 0.5f, 1.0f), 0.5f);
    check_near(TransitionAlpha(true, TransitionIn, 0.25f, 1.0f), 0.84375f);
    check_near(TransitionAlpha(false, TransitionOut, 0.5f, 1.0f), 0.0f);
    assert(TransitionFadeAlphaByte(true, TransitionOut, 1.0f, 1.0f) == 255);
    assert(TransitionApplyAlpha(128, 255) == 128);
    assert(TransitionApplyAlpha(255, 128) == 128);
    assert(TransitionApplyAlpha(300, -2) == 0);
    return 0;
}
