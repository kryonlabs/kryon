#include <assert.h>
#include <math.h>

#include "runtime/layout.h"

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
    Rectangle bounds = {10, 20, 100, 80};
    Rectangle child = {0, 0, 0, 18};
    LayoutMetrics metrics = LayoutMetricsFor(bounds, 6, 4);
    float cursor;

    assert(metrics.gap == 6);
    assert(metrics.padding == 4);
    check_rect(metrics.content, 14, 24, 92, 72);
    cursor = LayoutCursorForChild(metrics, false, 2, 48);
    assert(fabsf(cursor - 84.0f) < 0.001f);
    check_rect(LayoutChildBounds(child, child, metrics, false, cursor),
               14, 84, 92, 18);

    child = (Rectangle){0, 0, 30, 0};
    cursor = LayoutCursorForChild(metrics, true, 1, 30);
    assert(fabsf(cursor - 50.0f) < 0.001f);
    check_rect(LayoutChildBounds(child, child, metrics, true, cursor),
               50, 24, 30, 72);

    child = (Rectangle){5, 0, 30, 10};
    check_rect(LayoutChildBounds(child, child, metrics, true, 50),
               5, 0, 30, 10);

    child = (Rectangle){0, 0, 0, 0};
    check_rect(StackChildBounds(child, child, metrics), 14, 24, 92, 72);

    metrics = LayoutMetricsFor(bounds, -1, -2);
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);
    check_rect(metrics.content, 10, 20, 100, 80);

    check_rect(LayoutScopeBounds((Rectangle){2, 3, 0, -1}, 320, 180),
               2, 3, 320, 180);
    check_rect(LayoutScopeBounds((Rectangle){2, 3, 40, 50}, 320, 180),
               2, 3, 40, 50);
    return 0;
}
