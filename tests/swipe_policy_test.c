#include <assert.h>
#include <math.h>

#include "runtime/swipe.h"

static void
test_defaults(void)
{
    assert(SwipeDirectionsFor(0) == (unsigned int)SwipeAll);
    assert(SwipeDirectionsFor(SwipeLeft) == (unsigned int)SwipeLeft);
    assert(fabsf(SwipeMinDistanceFor(2.0f, 0.0f) - 96.0f) < 0.001f);
    assert(fabsf(SwipeMinDistanceFor(2.0f, 32.0f) - 32.0f) < 0.001f);
    assert(fabsf(SwipeAxisBiasFor(0.0f) - 1.25f) < 0.001f);
    assert(fabsf(SwipeAxisBiasFor(1.6f) - 1.6f) < 0.001f);
    assert(fabsf(SwipeDecisionDistanceFor(1.5f) - 12.0f) < 0.001f);
}

static void
test_direction(void)
{
    assert(SwipeDirectionFor((Vector2){-40.0f, 2.0f}, SwipeAll, 1.25f) ==
           SwipeLeft);
    assert(SwipeDirectionFor((Vector2){40.0f, 2.0f}, SwipeAll, 1.25f) ==
           SwipeRight);
    assert(SwipeDirectionFor((Vector2){2.0f, -40.0f}, SwipeAll, 1.25f) ==
           SwipeUp);
    assert(SwipeDirectionFor((Vector2){2.0f, 40.0f}, SwipeAll, 1.25f) ==
           SwipeDown);
    assert(SwipeDirectionFor((Vector2){40.0f, 2.0f}, SwipeLeft, 1.25f) ==
           SwipeNone);
    assert(SwipeDirectionFor((Vector2){20.0f, 20.0f}, SwipeAll, 1.25f) ==
           SwipeNone);
}

static void
test_distances(void)
{
    Vector2 delta = {-30.0f, 60.0f};

    assert(fabsf(SwipeAbs(-3.5f) - 3.5f) < 0.001f);
    assert(fabsf(SwipeMaxDistanceFor(delta) - 60.0f) < 0.001f);
    assert(fabsf(SwipePrimaryDistanceFor(delta, SwipeLeft) - 30.0f) <
           0.001f);
    assert(fabsf(SwipePrimaryDistanceFor(delta, SwipeDown) - 60.0f) <
           0.001f);
    assert(fabsf(SwipeProgressFor(20.0f, 100.0f) - 0.2f) < 0.001f);
    assert(fabsf(SwipeProgressFor(200.0f, 100.0f) - 1.0f) < 0.001f);
    assert(fabsf(SwipeProgressFor(20.0f, 0.0f) - 0.0f) < 0.001f);
}

static void
test_axis_cancel(void)
{
    assert(SwipeShouldCancelForAxis((Vector2){3.0f, 70.0f},
                                    SwipeHorizontal, 1.25f));
    assert(SwipeShouldCancelForAxis((Vector2){70.0f, 3.0f},
                                    SwipeVertical, 1.25f));
    assert(!SwipeShouldCancelForAxis((Vector2){70.0f, 3.0f},
                                     SwipeHorizontal, 1.25f));
    assert(!SwipeShouldCancelForAxis((Vector2){70.0f, 3.0f}, SwipeAll,
                                     1.25f));
}

int
main(void)
{
    test_defaults();
    test_direction();
    test_distances();
    test_axis_cancel();
    return 0;
}
