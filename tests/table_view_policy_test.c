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
    StyleFrame table = {.value = {.fields = StyleContentOffset,
                                   .offset_y = 28.0f}};
    StyleFrame header = {.value = {.fields = StyleContentOffset | StylePaddingX,
                                    .offset_y = 30.0f, .padding_x = 6.0f}};
    StyleFrame cell = {.value = {.fields = StyleContentOffset,
                                  .offset_x = 32.0f}};
    StyleFrame divider = {.value = {.fields = StylePaddingX | StyleContentOffset,
                                     .padding_x = 5.0f, .offset_y = 8.0f}};
    TableViewMetrics metrics = TableViewMetricsFor(2.0f, table, header,
                                                   cell, divider);
    TableViewLayout layout;
    TableViewScrollLayout scroll;
    Rectangle row;

    assert(metrics.default_row_height == 56);
    assert(metrics.min_header_height == 60);
    assert(metrics.default_min_column_width == 64);
    assert(metrics.header_text_pad_x == 12);
    assert(metrics.resize_tolerance == 10);
    assert(metrics.scrollbar_width == 16);
    assert(TableViewRowHeight(0, 2.0f, metrics) == 56);
    assert(TableViewRowHeight(18, 2.0f, metrics) == 36);
    assert(TableViewHeaderHeight(20, 2.0f, metrics) == 60);
    assert(TableViewHeaderHeight(44, 2.0f, metrics) == 88);
    assert(TableViewDefaultColumnWidth(301, 3) == 100);
    assert(TableViewColumnWidth(0, 100) == 100);
    assert(TableViewColumnWidth(72, 100) == 72);
    assert(TableViewMinimumColumnWidth(0, 2.0f, metrics) == 64);
    assert(TableViewMinimumColumnWidth(40, 2.0f, metrics) == 80);

    table.value.offset_y = 26.0f;
    header.value.offset_y = 34.0f;
    header.value.padding_x = 9.0f;
    cell.value.offset_x = 44.0f;
    divider.value.padding_x = 7.0f;
    divider.value.offset_y = 10.0f;
    metrics = TableViewMetricsFor(1.0f, table, header, cell, divider);
    assert(metrics.default_row_height == 26);
    assert(metrics.min_header_height == 34);
    assert(metrics.default_min_column_width == 44);
    assert(metrics.header_text_pad_x == 9);
    assert(metrics.resize_tolerance == 7);
    assert(metrics.scrollbar_width == 10);

    table.value.offset_y = 0.0f;
    header.value.offset_y = 0.0f;
    header.value.padding_x = 0.0f;
    cell.value.offset_x = 0.0f;
    divider.value.padding_x = 0.0f;
    divider.value.offset_y = 0.0f;
    metrics = TableViewMetricsFor(1.0f, table, header, cell, divider);
    assert(metrics.default_row_height == 28);
    assert(metrics.min_header_height == 30);
    assert(metrics.default_min_column_width == 32);
    assert(metrics.header_text_pad_x == 0);
    assert(metrics.resize_tolerance == 0);
    assert(metrics.scrollbar_width == 0);

    table.value.offset_y = 28.0f;
    header.value.offset_y = 30.0f;
    header.value.padding_x = 6.0f;
    cell.value.offset_x = 32.0f;
    divider.value.padding_x = 5.0f;
    divider.value.offset_y = 8.0f;
    metrics = TableViewMetricsFor(1.0f, table, header, cell, divider);
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
    check_rect(TableViewScrollbarBoundsFor(bounds, layout, 8),
               302, 106, 8, 74);
    return 0;
}
