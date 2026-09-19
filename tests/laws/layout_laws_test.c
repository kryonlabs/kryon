#include <limits.h>

#include "lawcheck.h"
#include "runtime/input.h"
#include "runtime/layout.h"
#include "runtime/paned_view.h"
#include "runtime/style_sheet.h"

static int
abs_i32(int value)
{
    return value < 0 ? -value : value;
}

int
main(void)
{
    LawCheck law = {0};

    LAW_BEGIN(&law, "layout.metrics.content.nonnegative");
    FOR_INT(width, -8, 64) {
        FOR_INT(height, -8, 64) {
            FOR_INT(padding, -4, 40) {
                Rectangle bounds = {3.0f, 5.0f, (float)width, (float)height};
                LayoutMetrics metrics = LayoutMetricsFor(bounds, -2, padding);
                REQUIRE(&law, metrics.gap >= 0);
                REQUIRE(&law, metrics.padding >= 0);
                REQUIRE(&law, metrics.content.width >= 0.0f);
                REQUIRE(&law, metrics.content.height >= 0.0f);
            }
        }
    }

    LAW_BEGIN(&law, "input.drag.monotonic.with.distance");
    FOR_INT(threshold, -4, 12) {
        FOR_INT(dx, -16, 16) {
            FOR_INT(dy, -16, 16) {
                int starts = InputPointerDragShouldStart(dx, dy, threshold);
                int farther_x = dx < 0 ? dx - 1 : dx + 1;
                int farther_y = dy < 0 ? dy - 1 : dy + 1;
                if(starts && abs_i32(dx) >= abs_i32(dy))
                    REQUIRE(&law, InputPointerDragShouldStart(farther_x, dy, threshold));
                if(starts && abs_i32(dy) >= abs_i32(dx))
                    REQUIRE(&law, InputPointerDragShouldStart(dx, farther_y, threshold));
            }
        }
    }

    LAW_BEGIN(&law, "paned_view.split.clamped");
    FOR_INT(min_first, -8, 32) {
        FOR_INT(limit, min_first, 64) {
            FOR_INT(split, -16, 80) {
                int clamped = PanedViewClampSplit(split, min_first, limit);
                REQUIRE(&law, clamped >= min_first);
                REQUIRE(&law, clamped <= limit);
            }
        }
    }

    LAW_BEGIN(&law, "style.priority.deterministic.ordering");
    FOR_INT(layer_a, -2, 2) {
        FOR_INT(layer_b, -2, 2) {
            FOR_INT(spec_a, 0, 4) {
                FOR_INT(spec_b, 0, 4) {
                    FOR_INT(order_a, 0, 4) {
                        FOR_INT(order_b, 0, 4) {
                            StylePriority a = {true, layer_a, spec_a, order_a};
                            StylePriority b = {true, layer_b, spec_b, order_b};
                            int a_wins = StylePriorityWins(a, b);
                            int b_wins = StylePriorityWins(b, a);
                            if(layer_a > layer_b ||
                               (layer_a == layer_b && spec_a > spec_b) ||
                               (layer_a == layer_b && spec_a == spec_b &&
                                order_a > order_b)) {
                                REQUIRE(&law, a_wins);
                                REQUIRE(&law, !b_wins);
                            }
                            if(layer_a == layer_b && spec_a == spec_b &&
                               order_a == order_b) {
                                REQUIRE(&law, a_wins);
                                REQUIRE(&law, b_wins);
                            }
                        }
                    }
                }
            }
        }
    }

    LAW_BEGIN(&law, "style.priority.absent.never.wins");
    {
        StylePriority absent = {false, INT_MAX, INT_MAX, INT_MAX};
        StylePriority present = {true, INT_MIN, 0, 0};
        REQUIRE(&law, !StylePriorityWins(absent, present));
        REQUIRE(&law, StylePriorityWins(present, absent));
    }

    return lawcheck_finish(&law);
}
