#include <assert.h>

#include "runtime/scroll.h"

int
main(void)
{
    StyleFrame track = {0};
    StyleFrame thumb = {0};
    ScrollMetrics metrics = ScrollMetricsFor(2.0f, track, thumb);
    ScrollPolicyView view;

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
    assert(ScrollSafeContentWidth(10, 200, 260, 0, metrics) == 200);
    assert(ScrollSafeContentWidth(10, 200, 260, 5, metrics) == 200);
    assert(ScrollSafeContentWidth(10, 300, 260, 5, metrics) == 210);
    assert(ScrollSafeContentWidth(250, 100, 260, 5, metrics) == 0);

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

    return 0;
}
