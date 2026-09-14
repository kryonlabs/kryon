#include <assert.h>
#include <math.h>

#include "runtime/terminal_pane.h"

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
    TerminalPaneMetricsPolicy policy = TerminalPaneMetricsPolicyFor(0.0f);
    assert(policy.font_size == 13);
    assert(policy.line_gap == 2);
    assert(policy.padding == 6);
    assert(policy.min_cols == 8);
    assert(policy.min_rows == 4);

    policy = TerminalPaneMetricsPolicyFor(2.0f);
    assert(policy.font_size == 26);
    assert(policy.line_gap == 4);
    assert(policy.padding == 12);
    assert(TerminalPaneFontSizeFor(0, policy) == 26);
    assert(TerminalPaneFontSizeFor(15, policy) == 15);
    assert(TerminalPaneLineHeightFor(20, 18, policy) == 22);
    assert(TerminalPaneLineHeightFor(24, 18, policy) == 24);
    assert(TerminalPanePaddingFor(-1, policy) == 12);
    assert(TerminalPanePaddingFor(0, policy) == 0);

    check_rect(TerminalPaneContentBoundsFor((Rectangle){10, 20, 120, 80},
                                           -4, 6),
               16, 26, 108, 68);
    check_rect(TerminalPaneContentBoundsFor((Rectangle){10, 20, 8, 7},
                                           4, 6),
               16, 30, 0, 0);

    assert(TerminalPaneClampCols(2, 100, policy) == 8);
    assert(TerminalPaneClampCols(120, 100, policy) == 100);
    assert(TerminalPaneClampCols(42, 100, policy) == 42);
    assert(TerminalPaneClampRows(1, 40, policy) == 4);
    assert(TerminalPaneClampRows(50, 40, policy) == 40);
    assert(TerminalPaneClampRows(12, 40, policy) == 12);

    TerminalPaneScrollIndicatorMetrics indicator =
        TerminalPaneScrollIndicatorMetricsFor(1.0f);
    assert(indicator.font_size == 13);
    assert(indicator.right_offset == 16);
    assert(indicator.top_offset == 6);
    assert(indicator.width_pad == 10);
    assert(indicator.height == 22);
    assert(indicator.text_x == 5);
    assert(indicator.text_y == 4);

    check_rect(TerminalPaneScrollIndicatorBoundsFor(
                   (Rectangle){10, 20, 200, 90}, 60, indicator),
               134, 26, 70, 22);
    check_rect(TerminalPaneScrollIndicatorBoundsFor(
                   (Rectangle){10, 20, 40, 90}, 60, indicator),
               10, 26, 40, 22);
    check_rect(TerminalPaneScrollIndicatorBoundsFor(
                   (Rectangle){10, 20, 40, 90}, 0, indicator),
               0, 0, 0, 0);
    return 0;
}
