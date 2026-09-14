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
    ScrollBarDragDecision drag;
    ScrollBarDragReleaseDecision drag_release;
    ScrollContentDragDecision content_drag;
    Rectangle content;
    ScrollClipGeometry clip;
    Rectangle rect;

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
    rect = ScrollScreenBoundsFor((Rectangle){10, 20, 100, 80},
                                 (Vector2){5, 7}, 2.0f);
    assert((int)rect.x == 25);
    assert((int)rect.y == 47);
    assert((int)rect.width == 200);
    assert((int)rect.height == 160);
    rect = ScrollWorldBoundsFor((Rectangle){30, 50, 120, 100},
                                (Vector2){5, 7}, 2.0f);
    assert(rect.x > 12.49f && rect.x < 12.51f);
    assert(rect.y > 21.49f && rect.y < 21.51f);
    assert((int)rect.width == 60);
    assert((int)rect.height == 50);
    clip = ScrollClipGeometryFor((Rectangle){10, 20, 100, 80},
                                 (Rectangle){30, 50, 120, 100}, 8,
                                 (Vector2){5, 7}, 2.0f, 400, 300, 200, 150);
    assert((int)clip.screen_bounds.x == 25);
    assert((int)clip.clipped_world_bounds.width == 60);
    assert((int)clip.visual_screen_bounds.x == 9);
    assert((int)clip.visual_screen_bounds.y == 31);
    assert((int)clip.visual_screen_bounds.width == 232);
    assert((int)clip.visual_screen_bounds.height == 192);
    assert(clip.visual_bleed == 16);
    clip = ScrollClipGeometryFor((Rectangle){-5, -8, 20, 20},
                                 (Rectangle){0, 0, 10, 10}, 10,
                                 (Vector2){0, 0}, 1.0f, 50, 40, 50, 40);
    assert((int)clip.visual_screen_bounds.x == 0);
    assert((int)clip.visual_screen_bounds.y == 0);
    assert((int)clip.visual_screen_bounds.width == 25);
    assert((int)clip.visual_screen_bounds.height == 22);
    clip = ScrollClipGeometryFor((Rectangle){0, 0, 80, 60},
                                 (Rectangle){2, 4, 20, 30}, 0,
                                 (Vector2){3, 5}, 0.0f, 0, 0, 50, 40);
    assert((int)clip.screen_bounds.x == 3);
    assert((int)clip.screen_bounds.y == 5);
    assert((int)clip.clipped_world_bounds.x == -1);
    assert((int)clip.visual_screen_bounds.width == 51);
    assert((int)clip.visual_screen_bounds.height == 41);
    assert(clip.visual_bleed == 1);
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
    drag = ScrollBarDragFor(1, 0, 1, 1, 0, 0, 40, 40, 0, 160, paint);
    assert(drag.start_drag);
    assert(drag.claim_scroll_owner);
    assert(!drag.continue_drag);
    assert(!drag.cancel_drag);
    assert(drag.scroll_offset == 40);
    drag = ScrollBarDragFor(1, 0, 0, 0, 1, 1, 40, 40, 10, 160, paint);
    assert(!drag.start_drag);
    assert(drag.continue_drag);
    assert(drag.claim_scroll_owner);
    assert(!drag.cancel_drag);
    assert(drag.scroll_offset == 65);
    drag = ScrollBarDragFor(0, 0, 0, 0, 1, 1, 40, 40, 10, 160, paint);
    assert(!drag.start_drag);
    assert(!drag.continue_drag);
    assert(drag.cancel_drag);
    assert(drag.scroll_offset == 40);
    drag = ScrollBarDragFor(1, 1, 1, 1, 0, 0, 40, 40, 0, 160, paint);
    assert(!drag.start_drag);
    assert(!drag.continue_drag);
    assert(!drag.cancel_drag);
    drag_release = ScrollBarDragReleaseFor(1, 1, 0);
    assert(drag_release.clear_drag);
    assert(drag_release.consume_release);
    drag_release = ScrollBarDragReleaseFor(1, 0, 1);
    assert(drag_release.clear_drag);
    assert(!drag_release.consume_release);
    drag_release = ScrollBarDragReleaseFor(0, 1, 0);
    assert(!drag_release.clear_drag);
    assert(!drag_release.consume_release);
    content_drag = ScrollContentDragFor(160, 1, 1, 1, 0, 0, 1, 0, 0,
                                        0, 0, 40, 40, 0, 5);
    assert(content_drag.start_drag);
    assert(content_drag.active);
    assert(content_drag.gesture_pending);
    assert(!content_drag.dragging);
    assert(content_drag.scroll_offset == 40);
    content_drag = ScrollContentDragFor(160, 0, 1, 1, 0, 0, 1, 0, 0,
                                        1, 0, 40, 40, 3, 5);
    assert(content_drag.active);
    assert(!content_drag.dragging);
    assert(!content_drag.claim_scroll_owner);
    assert(!content_drag.capture_input);
    content_drag = ScrollContentDragFor(160, 0, 1, 1, 0, 0, 1, 0, 0,
                                        1, 0, 40, 40, 8, 5);
    assert(content_drag.active);
    assert(content_drag.dragging);
    assert(content_drag.claim_scroll_owner);
    assert(content_drag.capture_input);
    assert(content_drag.scroll_offset == 32);
    content_drag = ScrollContentDragFor(160, 0, 0, 1, 0, 0, 0, 1, 0,
                                        1, 1, 32, 40, 8, 5);
    assert(!content_drag.active);
    assert(!content_drag.dragging);
    assert(content_drag.capture_input);
    content_drag = ScrollContentDragFor(160, 1, 1, 1, 0, 0, 1, 0, 1,
                                        1, 1, 32, 40, 8, 5);
    assert(!content_drag.active);
    assert(!content_drag.dragging);
    assert(!content_drag.start_drag);
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
