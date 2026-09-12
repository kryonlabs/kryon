#include <assert.h>
#include <math.h>

#include "runtime/canvas.h"

static void
check_vec(Vector2 got, float x, float y)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
}

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

int
main(void)
{
    Rectangle bounds = {40, 50, 200, 100};
    CanvasPolicyResult result;

    assert(fabsf(CanvasZoom(0.0f) - 1.0f) < 0.001f);
    assert(fabsf(CanvasZoom(2.0f) - 2.0f) < 0.001f);
    assert(CanvasContains(bounds, (Vector2){40, 50}));
    assert(CanvasContains(bounds, (Vector2){240, 150}));
    assert(!CanvasContains(bounds, (Vector2){241, 150}));

    check_vec(CanvasPointToScreen(bounds, (Vector2){50, 70}, 10, 20, 2.0f),
              40, 50);
    check_vec(CanvasPointFromScreen(bounds, (Vector2){40, 50}, 10, 20, 2.0f),
              50, 70);
    check_rect(CanvasRectToScreenBounds(bounds, (Rectangle){50, 70, 20, 10},
                                        10, 20, 2.0f),
               40, 50, 40, 20);

    result = CanvasBeginResultFor(bounds, (Vector2){60, 80}, 10, 20, 2.0f,
                                  true);
    assert(result.active);
    assert(result.dragging);
    check_vec(result.world, 60, 85);
    result = CanvasBeginResultFor(bounds, (Vector2){300, 80}, 10, 20, 2.0f,
                                  true);
    assert(!result.active);
    assert(!result.dragging);

    assert(CanvasHitTestStep((Vector2){15, 15}, (Rectangle){10, 10, 20, 20},
                             1, -1) == 1);
    assert(CanvasHitTestStep((Vector2){15, 15}, (Rectangle){10, 10, 20, 20},
                             0, 2) == 2);
    assert(CanvasHitTestStep((Vector2){80, 80}, (Rectangle){10, 10, 20, 20},
                             1, -1) == -1);
    return 0;
}
