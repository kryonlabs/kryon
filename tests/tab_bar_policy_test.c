#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "runtime/tab_bar.h"

static StyleFrame
test_frame(uint32_t background, uint32_t foreground, uint32_t border)
{
    StyleFrame frame = {0};
    frame.value.fields = StyleBackground | StyleForeground | StyleBorder;
    frame.value.background = background;
    frame.value.foreground = foreground;
    frame.value.border = border;
    frame.value.focus = 0x778899ff;
    frame.value.radius = 3.0f;
    frame.value.border_width = 2.0f;
    frame.value.opacity = 0.75f;
    return frame;
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
    Rectangle bounds = {10, 20, 240, 32};
    StyleFrame bar_frame = test_frame(0x05060708, 0x01020304,
                                      0x11223344);
    StyleFrame tab_frame = test_frame(0x05060708, 0x01020304,
                                      0x11223344);
    StyleFrame close_frame = test_frame(0x05060708, 0x01020304,
                                        0x11223344);
    TabBarMetrics metrics;
    TabBarPaint paint;
    TabBarContentLayout content;
    TabBarCloseLabelPaint close_label;
    int label_width;
    int close_width;
    int icon_width;
    int total;
    TabBarScroll scroll;

    bar_frame.value.fields |= StylePaddingX | StylePaddingY | StyleGap |
                              StyleIconSize | StyleContentOffset;
    bar_frame.value.padding_x = 80.0f;
    bar_frame.value.padding_y = 7.0f;
    bar_frame.value.gap = 4.0f;
    bar_frame.value.icon_size = 32.0f;
    bar_frame.value.offset_x = 160.0f;
    tab_frame.value.fields |= StylePaddingX | StylePaddingY |
                              StyleIconSize | StyleGap;
    tab_frame.value.padding_x = 8.0f;
    tab_frame.value.padding_y = 10.0f;
    tab_frame.value.icon_size = 44.0f;
    tab_frame.value.gap = 5.0f;
    close_frame.value.fields |= StyleIconSize | StyleGap;
    close_frame.value.icon_size = 24.0f;
    close_frame.value.gap = 9.0f;

    metrics = TabBarDefaultMetrics(80, 160, 1.0f,
                                   bar_frame, tab_frame, close_frame);
    label_width = TabBarTabWidth(42, 1, 0, 0, metrics);
    close_width = TabBarTabWidth(42, 1, 0, 1, metrics);
    icon_width = TabBarTabWidth(0, 0, 1, 0, metrics);
    total = TabBarTotalWidth(label_width + close_width + icon_width,
                             3, metrics.gap);
    scroll = TabBarScrollFor(bounds.width, total, 999);

    assert(TabBarPolicyHeight(1.0f, bar_frame) == 32);
    assert(label_width == 80);
    assert(close_width == 82);
    assert(icon_width == 44);
    assert(total == 214);
    assert(TabBarSelectedIndexFor(-1, 3) == 0);
    assert(TabBarSelectedIndexFor(9, 3) == 0);
    assert(TabBarSelectedIndexFor(2, 3) == 2);
    assert(TabBarSelectedIndexFor(0, 0) == -1);
    assert(TabBarClampedIndexFor(-1, 3) == 0);
    assert(TabBarClampedIndexFor(9, 3) == 2);
    assert(TabBarClampedIndexFor(1, 3) == 1);
    assert(TabBarClampedIndexFor(0, 0) == -1);
    assert(TabBarWrappedIndex(0, -1, 1, 3) == 2);
    assert(TabBarWrappedIndex(2, 1, 1, 3) == 0);
    assert(TabBarWrappedIndex(1, 0, 2, 3) == 0);
    assert(TabBarWrappedIndex(1, 1, 2, 0) == -1);
    assert(scroll.equal_tabs == 1);
    assert(scroll.scroll == 0);
    check_rect(TabBarEqualTabBounds(bounds, 3, 2), 170, 20, 80, 32);
    check_rect(TabBarDragMarkerBounds(40, 80, 20, 32, 1, 1.0f),
               119, 24, 2, 24);

    paint = TabBarPaintFor(bar_frame, tab_frame, close_frame, 1.0f);
    assert(paint.text_padding == 8);
    assert(paint.icon_size == 44);
    assert(paint.icon_gap == 5);
    assert(paint.content_inset_y == 10);
    assert(paint.close_size == 24);
    assert(paint.close_gap == 9);
    assert(paint.reorder_drag_threshold == 7);
    assert(paint.text_color == 0x01020304);
    assert(paint.close_color == 0x01020304);
    content = TabBarContentLayoutFor((Rectangle){20, 30, 120, 32},
                                     42, 1, 1, 1, paint);
    check_rect(content.icon_bounds, 34, 24, 44, 44);
    check_rect(content.text_bounds, 83, 35, 16, 22);
    check_rect(content.close_bounds, 108, 34, 24, 24);
    close_label = TabBarCloseLabelPaintFor(content.close_bounds, 8, 12);
    assert(close_label.x == 116);
    assert(close_label.y == 40);
    content = TabBarContentLayoutFor((Rectangle){20, 30, 120, 32},
                                     0, 0, 1, 0, paint);
    check_rect(content.icon_bounds, 58, 24, 44, 44);
    check_rect(content.text_bounds, 107, 35, 25, 22);

    metrics = TabBarDefaultMetrics(0, 0, 1.0f,
                                   bar_frame, tab_frame, close_frame);
    assert(metrics.min_width == 80);
    assert(metrics.max_width == 160);
    assert(metrics.label_padding == 16);

    bar_frame.value.gap = 0.0f;
    tab_frame.value.padding_x = 0.0f;
    tab_frame.value.gap = 0.0f;
    close_frame.value.gap = 0.0f;
    tab_frame.value.icon_size = 0.0f;
    close_frame.value.icon_size = 0.0f;
    metrics = TabBarDefaultMetrics(0, 0, 1.0f,
                                   bar_frame, tab_frame, close_frame);
    paint = TabBarPaintFor(bar_frame, tab_frame, close_frame, 1.0f);
    assert(metrics.gap == 0);
    assert(metrics.icon_width == 0);
    assert(metrics.close_width == 0);
    assert(metrics.label_padding == 0);
    assert(paint.text_padding == 0);
    assert(paint.icon_size == 0);
    assert(paint.icon_gap == 0);
    assert(paint.close_size == 0);
    assert(paint.close_gap == 0);
    content = TabBarContentLayoutFor((Rectangle){20, 30, 120, 6},
                                     42, 1, 0, 0, paint);
    check_rect(content.text_bounds, 20, 33, 120, 0);
    assert(TabBarTabWidth(0, 0, 1, 0, metrics) == 0);
    assert(TabBarTabWidth(42, 1, 0, 1, metrics) == 80);

    paint = TabBarPaintFor((StyleFrame){0}, (StyleFrame){0},
                           (StyleFrame){0}, 2.0f);
    assert(paint.text_padding == 16);
    assert(paint.icon_size == 32);
    assert(paint.icon_gap == 8);
    assert(paint.content_inset_y == 16);
    assert(paint.close_size == 36);
    assert(paint.close_gap == 12);
    assert(paint.reorder_drag_threshold == 12);
    return 0;
}
