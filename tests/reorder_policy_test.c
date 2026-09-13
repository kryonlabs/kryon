#include <assert.h>

#include "runtime/reorder.h"

static void
check_rect(Rectangle actual, float x, float y, float w, float h)
{
    assert(actual.x == x);
    assert(actual.y == y);
    assert(actual.width == w);
    assert(actual.height == h);
}

int
main(void)
{
    ReorderMetrics metrics;
    Rectangle handle;
    ReorderHandlePaint handle_paint;
    ReorderPlaceholderPaint placeholder;

    metrics = ReorderMetricsFor(2.0f, 0, 0, 0, 0);
    assert(metrics.handle_width == 72);
    assert(metrics.drag_threshold == 10);
    assert(metrics.auto_scroll_margin == 68);
    assert(metrics.auto_scroll_step == 24);

    metrics = ReorderMetricsFor(2.0f, 44, 7, 11, 13);
    assert(metrics.handle_width == 44);
    assert(metrics.drag_threshold == 7);
    assert(metrics.auto_scroll_margin == 11);
    assert(metrics.auto_scroll_step == 13);

    handle = ReorderHandleBounds((Rectangle){10, 20, 200, 80}, 36, 40);
    assert(handle.x == 10.0f);
    assert(handle.y == 20.0f);
    assert(handle.width == 36.0f);
    assert(handle.height == 40.0f);

    handle = ReorderHandleBounds((Rectangle){10, 20, 200, 30}, 36, 40);
    assert(handle.height == 30.0f);

    handle_paint = ReorderHandlePaintFor((Rectangle){10, 20, 36, 40}, 1.0f);
    assert(handle_paint.dot_count == 6);
    check_rect(handle_paint.dot0, 21.0f, 31.0f, 3.0f, 3.0f);
    check_rect(handle_paint.dot1, 32.0f, 31.0f, 3.0f, 3.0f);
    check_rect(handle_paint.dot2, 21.0f, 38.0f, 3.0f, 3.0f);
    check_rect(handle_paint.dot3, 32.0f, 38.0f, 3.0f, 3.0f);
    check_rect(handle_paint.dot4, 21.0f, 45.0f, 3.0f, 3.0f);
    check_rect(handle_paint.dot5, 32.0f, 45.0f, 3.0f, 3.0f);

    placeholder = ReorderPlaceholderPaintFor((Rectangle){10, 20, 100, 40}, 1.0f);
    assert(placeholder.use_slot == 1);
    check_rect(placeholder.slot_bounds, 13.0f, 23.0f, 94.0f, 34.0f);
    assert(placeholder.stroke_width == 2.0f);
    assert(placeholder.radius == 0.12f);
    assert(placeholder.segments == 10);
    assert(placeholder.fill_alpha == 0.10f);

    placeholder = ReorderPlaceholderPaintFor((Rectangle){10, 20, 100, 24}, 1.0f);
    assert(placeholder.use_slot == 0);
    check_rect(placeholder.line_bounds, 10.0f, 31.0f, 100.0f, 2.0f);

    assert(ReorderDraggedCenterY(90, 20, 40.0f) == 90);
    assert(ReorderTargetIncludesItem(51, (Rectangle){0, 20, 100, 60}));
    assert(!ReorderTargetIncludesItem(50, (Rectangle){0, 20, 100, 60}));
    assert(ReorderTargetIndexFor(-2, 4) == 0);
    assert(ReorderTargetIndexFor(2, 4) == 2);
    assert(ReorderTargetIndexFor(9, 4) == 3);
    assert(ReorderTargetIndexFor(0, 0) == -1);

    return 0;
}
