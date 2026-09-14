#include <assert.h>

#include "runtime/scroll.h"

int
main(void)
{
    StyleFrame track = {0};
    StyleFrame thumb = {0};
    ScrollMetrics metrics = ScrollMetricsFor(2.0f, track, thumb);
    ScrollPolicyView view;
    ScrollBarPaint paint;
    Rectangle content;

    assert(metrics.scrollbar_width == 20);
    assert(metrics.reserved_width == 32);
    assert(metrics.safe_gap == 40);
    assert(metrics.default_wheel_step == 84);
    assert(metrics.drag_threshold == 10);
    assert(metrics.visual_bleed == 16);
    assert(metrics.thumb_min_height == 32);
    assert(metrics.thumb_inset == 4);
    assert(ScrollMax(300, 100) == 200);
    assert(ScrollMax(80, 100) == 0);
    assert(ScrollClamp(-3, 20) == 0);
    assert(ScrollClamp(40, 20) == 20);
    assert(ScrollReservedWidth(0, metrics) == 0);
    assert(ScrollReservedWidth(1, metrics) == 32);
    assert(ScrollContentWidth(30, 10, metrics) == 0);
    assert(ScrollContentWidth(100, 10, metrics) == 68);
    assert(ScrollPageContentWidthFor(320, 0, 0, 24) == 272);
    assert(ScrollPageContentWidthFor(320, 280, 0, 24) == 272);
    assert(ScrollPageContentWidthFor(320, 180, 220, 24) == 220);
    assert(ScrollPageContentWidthFor(40, 0, 0, 30) == 0);
    assert(ScrollSafeContentWidth(10, 200, 260, 0, metrics) == 200);
    assert(ScrollSafeContentWidth(10, 200, 260, 5, metrics) == 200);
    assert(ScrollSafeContentWidth(10, 300, 260, 5, metrics) == 210);
    assert(ScrollSafeContentWidth(250, 100, 260, 5, metrics) == 0);
    content = ScrollScopeContentBounds((Rectangle){10, 20, 120, 80}, 0,
                                       metrics);
    assert((int)content.width == 120);
    content = ScrollScopeContentBounds((Rectangle){10, 20, 120, 80}, 1,
                                       metrics);
    assert((int)content.width == 100);
    assert(ScrollWheelOffsetFor(50, 1.0f, 200,
                                metrics.default_wheel_step) == 0);
    assert(ScrollWheelOffsetFor(50, -1.0f, 200,
                                metrics.default_wheel_step) == 134);
    assert(ScrollRowWheelStepFor(18, 2.0f) == 54);
    assert(ScrollRowWheelStepFor(0, 2.0f) == 180);
    assert(ScrollRowWheelStepFor(0, 0.0f) == 90);

    metrics = ScrollMetricsFor(1.0f, track, thumb);
    view = ScrollMeasure((Rectangle){0, 20, 320, 100}, 260, 12, 280, 40,
                         300, metrics);
    assert(view.content_x == 12);
    assert(view.content_y == -20);
    assert(view.content_w == 268);
    assert(view.viewport_h == 100);
    assert(view.content_h == 260);
    assert(view.max_scroll == 160);
    assert(view.scrollbar_x == 300);

    view = ScrollMeasure((Rectangle){10, 30, 200, 100}, 80, 0, 0, 99, 0,
                         metrics);
    assert(view.content_x == 10);
    assert(view.content_y == 30);
    assert(view.content_w == 200);
    assert(view.max_scroll == 0);
    assert(view.scrollbar_x == 200);

    paint = ScrollBarPaintFor(300, 20, 100, 260, 40, 160, metrics);
    assert((int)paint.track_bounds.x == 300);
    assert((int)paint.track_bounds.y == 20);
    assert((int)paint.track_bounds.width == 10);
    assert((int)paint.track_bounds.height == 100);
    assert((int)paint.thumb_bounds.x == 302);
    assert((int)paint.thumb_bounds.y == 35);
    assert((int)paint.thumb_bounds.width == 6);
    assert((int)paint.thumb_bounds.height == 38);
    assert(paint.track_span == 62);
    assert(paint.scroll_per_pixel > 2.58f && paint.scroll_per_pixel < 2.59f);
    assert(ScrollDragOffsetFor(66.0f, 20.0f, 8.0f, 160, paint) == 98);
    assert(ScrollDragDeltaOffsetFor(80, 12, 160) == 68);
    assert(ScrollDragDeltaOffsetFor(4, 12, 160) == 0);
    assert(ScrollThumbDragDeltaOffsetFor(40, 10, 160, paint) == 65);
    assert(ScrollThumbDragDeltaOffsetFor(150, 20, 160, paint) == 160);
    assert(ScrollRectVisibleOffsetFor(0, 20, 100, 140, 20, 8, 160) == 48);
    assert(ScrollRectVisibleOffsetFor(80, 20, 100, 0, 20, 8, 160) == 52);
    assert(ScrollRectVisibleOffsetFor(0, 20, 100, 140, 20, -8, 160) == 40);

    paint = ScrollBarPaintFor(300, 20, 40, 400, 999, 360, metrics);
    assert((int)paint.thumb_bounds.y == 44);
    assert((int)paint.thumb_bounds.height == 16);
    assert(paint.track_span == 24);

    track.value.fields = StyleIconSize | StylePaddingX | StylePaddingY |
                         StyleGap | StyleContentOffset;
    thumb.value.fields = StyleIconSize | StylePaddingX;
    metrics = ScrollMetricsFor(1.0f, track, thumb);
    assert(metrics.scrollbar_width == 0);
    assert(metrics.reserved_width == 0);
    assert(metrics.safe_gap == 0);
    assert(metrics.default_wheel_step == 0);
    assert(metrics.drag_threshold == 0);
    assert(metrics.visual_bleed == 0);
    assert(metrics.thumb_min_height == 0);
    assert(metrics.thumb_inset == 0);
    paint = ScrollBarPaintFor(20, 30, 12, 100, -20, 88, metrics);
    assert((int)paint.track_bounds.width == 0);
    assert((int)paint.thumb_bounds.x == 20);
    assert((int)paint.thumb_bounds.y == 30);
    assert((int)paint.thumb_bounds.width == 0);

    return 0;
}
