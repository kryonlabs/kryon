#include <assert.h>

#include "runtime/paragraph.h"

int
main(void)
{
    ParagraphMetrics metrics = ParagraphResolveMetrics(
        0, 16, 0, 4, 0, 0, 240, 0, 30);
    ParagraphLayoutPolicy policy;
    ParagraphLineStep step;

    assert(metrics.font == 16);
    assert(metrics.line_gap == 4);
    assert(metrics.icon_size == 16);
    assert(metrics.width == 240);
    assert(metrics.height == 20);
    assert(metrics.next_y == 50);
    assert(ParagraphCanLayout(metrics.width));

    metrics = ParagraphResolveMetrics(18, 16, 6, 4, 22, 180, 240, 72, 12);
    assert(metrics.font == 18);
    assert(metrics.line_gap == 6);
    assert(metrics.icon_size == 22);
    assert(metrics.width == 180);
    assert(metrics.height == 72);
    assert(metrics.next_y == 84);

    metrics = ParagraphResolveMetrics(14, 16, 3, 4, 0, -10, -20, -1, 5);
    assert(metrics.width == 0);
    assert(metrics.height == 17);
    assert(!ParagraphCanLayout(metrics.width));
    assert(ParagraphDefaultLineGap(1.0f) == 4);
    assert(ParagraphDefaultLineGap(2.0f) == 8);
    assert(ParagraphDefaultLineGap(0.0f) == 4);

    policy = ParagraphLayoutPolicyFor(7, 1, 0, 4, 2.0f);
    assert(policy.space_width == 9);
    assert(policy.icon_spacing == 8);
    assert(policy.line_gap == 4);

    policy = ParagraphLayoutPolicyFor(5, -4, 6, 4, 1.0f);
    assert(policy.space_width == 0);
    assert(policy.icon_spacing == 4);
    assert(policy.line_gap == 6);
    assert(ParagraphLayoutTotalHeight(3, 18, 4) == 62);
    assert(ParagraphLayoutTotalHeight(0, 18, 4) == 0);
    step = ParagraphLineStepFor(20, 4, 30, 60);
    assert(!step.wrap);
    assert(step.width == 54);
    step = ParagraphLineStepFor(40, 4, 30, 60);
    assert(step.wrap);
    assert(step.width == 30);
    step = ParagraphLineStepFor(-10, -4, -30, 60);
    assert(!step.wrap);
    assert(step.width == 0);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignStart) == 10);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignCenter) == 30);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignEnd) == 50);
    assert(ParagraphLineXFor(10, 50, 60, TextAlignEnd) == 10);
    assert(ParagraphNextLineY(20, 18, 4, true) == 42);
    assert(ParagraphNextLineY(20, 18, 4, false) == 38);
    return 0;
}
