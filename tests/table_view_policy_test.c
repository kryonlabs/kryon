#include <assert.h>
#include <math.h>

#include "runtime/table_view.h"

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
    Rectangle bounds = {10, 20, 300, 160};
    TableViewMetrics metrics = TableViewMetricsFor(2.0f);
    TableViewLayout layout;
    TableViewScrollLayout scroll;
    Rectangle row;

    assert(metrics.default_row_height == 56);
    assert(metrics.min_header_height == 60);
    assert(metrics.default_min_column_width == 64);
    assert(metrics.header_text_pad_x == 12);
    assert(metrics.resize_tolerance == 10);
    assert(TableViewRowHeight(0, 2.0f, metrics) == 56);
    assert(TableViewRowHeight(18, 2.0f, metrics) == 36);
    assert(TableViewHeaderHeight(20, 2.0f, metrics) == 60);
    assert(TableViewHeaderHeight(44, 2.0f, metrics) == 88);
    assert(TableViewDefaultColumnWidth(301, 3) == 100);
    assert(TableViewColumnWidth(0, 100) == 100);
    assert(TableViewColumnWidth(72, 100) == 72);
    assert(TableViewMinimumColumnWidth(0, 2.0f, metrics) == 64);
    assert(TableViewMinimumColumnWidth(40, 2.0f, metrics) == 80);

    metrics = TableViewMetricsFor(1.0f);
    layout = TableViewLayoutFor(bounds, 8, 28, 30, 2, 1.0f, metrics);
    assert(layout.row_height == 28);
    assert(layout.header_height == 30);
    check_rect(layout.body, 10, 50, 300, 130);
    assert(layout.frozen_rows == 2);
    assert(layout.scroll_body_height == 74);
    assert(layout.max_scroll == 94);

    scroll = TableViewScrollFor(37, layout.frozen_rows, layout.row_height,
                                layout.scroll_body_height);
    assert(scroll.first == 3);
    assert(scroll.y_offset == 9);
    assert(scroll.visible_rows == 3);

    row = TableViewRowBounds(bounds, layout, 1, 1, scroll, false);
    check_rect(row, 10, 78, 300, 28);
    row = TableViewRowBounds(bounds, layout, 3, 2, scroll, true);
    check_rect(row, 10, 97, 300, 28);
    check_rect(TableViewHeaderBounds(bounds, 40, 90, layout.header_height),
               40, 20, 90, 30);
    check_rect(TableViewCellBounds(row, 40, 90), 40, 97, 90, 28);
    check_rect(TableViewViewport(bounds, layout, false), 10, 50, 300, 56);
    check_rect(TableViewViewport(bounds, layout, true), 10, 106, 300, 74);
    return 0;
}
