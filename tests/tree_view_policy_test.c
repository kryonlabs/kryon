#include <assert.h>
#include <math.h>

#include "runtime/style.h"
#include "runtime/tree_view.h"

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
    Rectangle bounds = {10, 20, 180, 90};
    StyleFrame panel = {0};
    StyleFrame item = {0};
    TreeViewMetrics metrics = TreeViewMetricsFor(2.0f, panel, item);
    TreeViewScrollLayout scroll;
    Rectangle row;
    Rectangle marker;
    Rectangle text;
    TreeViewTextPaint paint;
    Rectangle scrollbar;

    assert(metrics.default_row_height == 56);
    assert(metrics.indent_x == 16);
    assert(metrics.depth_indent == 36);
    assert(metrics.marker_width == 32);
    assert(metrics.text_gap == 4);
    assert(TreeViewRowHeight(0, 2.0f, metrics) == 56);
    assert(TreeViewRowHeight(18, 2.0f, metrics) == 36);
    assert(TreeViewContentHeight(5, 28) == 140);
    assert(TreeViewContentHeight(-1, 28) == 0);
    assert(TreeViewMaxScroll(90, 140) == 50);
    assert(TreeViewMaxScroll(160, 140) == 0);
    assert(TreeViewVisibleRows(90, 28) == 4);

    scroll = TreeViewScrollFor(65, 28);
    assert(scroll.first == 2);
    assert(scroll.y_offset == 9);

    panel.value.fields = StylePaddingX;
    panel.value.padding_x = 6.0f;
    item.value.fields = StyleFontSize | StylePaddingY | StyleContentOffset |
        StyleIconSize | StyleGap;
    item.value.font_size = 18.0f;
    item.value.padding_y = 5.0f;
    item.value.offset_x = 14.0f;
    item.value.icon_size = 12.0f;
    item.value.gap = 3.0f;
    metrics = TreeViewMetricsFor(1.0f, panel, item);
    assert(metrics.default_row_height == 28);
    assert(metrics.indent_x == 6);
    assert(metrics.depth_indent == 14);
    assert(metrics.marker_width == 12);
    assert(metrics.text_gap == 3);

    item.value.offset_y = 44.0f;
    metrics = TreeViewMetricsFor(1.0f, panel, item);
    assert(metrics.default_row_height == 44);
    item.value.offset_y = 0.0f;

    panel.value.padding_x = 0.0f;
    item.value.gap = 0.0f;
    metrics = TreeViewMetricsFor(1.0f, panel, item);
    assert(metrics.indent_x == 0);
    assert(metrics.text_gap == 0);

    panel = (StyleFrame){0};
    item = (StyleFrame){0};
    metrics = TreeViewMetricsFor(1.0f, panel, item);
    row = TreeViewRowBounds(bounds, 1, 28, 9);
    check_rect(row, 10, 39, 180, 28);
    assert(TreeViewIndent(-2, metrics) == 8);
    assert(TreeViewIndent(2, metrics) == 44);
    marker = TreeViewMarkerBounds(row, 2, metrics);
    text = TreeViewTextBounds(row, 2, metrics);
    check_rect(marker, 54, 39, 16, 28);
    check_rect(text, 72, 39, 118, 28);
    paint = TreeViewTextPaintFor(marker, text, 18);
    assert(paint.marker_x == 54);
    assert(paint.marker_y == 44);
    assert(paint.text_x == 72);
    assert(paint.text_y == 44);
    scrollbar = TreeViewScrollbarBoundsFor(bounds, 8);
    check_rect(scrollbar, 182, 20, 8, 90);
    return 0;
}
