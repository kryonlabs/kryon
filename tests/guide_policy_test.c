#include <assert.h>
#include <math.h>

#include "runtime/guide.h"

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

static void
check_vec(Vector2 got, float x, float y)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
}

int
main(void)
{
    GuideMetrics metrics = GuideMetricsFor(1.0f);
    GuidePolicy policy;
    GuideScrim scrim;
    GuideLayout layout;
    GuideArrow arrow;
    Rectangle tip;

    assert(metrics.margin == 12);
    assert(metrics.pad == 12);
    assert(metrics.button_size == 34);
    assert(metrics.default_line_gap == 6);
    assert(GuideStepFor(-2, 4) == 0);
    assert(GuideStepFor(8, 4) == 3);
    assert(GuideTipWidth(640, 0, metrics) == 300);
    assert(GuideTipWidth(120, 80, metrics) == 80);
    assert(GuideMaxTipHeight(480, 20, 30, metrics) == 406);
    assert(GuideChromeHeight(metrics) == 114);
    assert(GuideTipHeight(30, 300, metrics) == 144);
    assert(GuideTipHeight(600, 300, metrics) == 300);

    policy = GuidePolicyFor(0, 3, false, true, false);
    assert(policy.step == 1 && policy.changed && !policy.finished);
    policy = GuidePolicyFor(2, 3, false, true, false);
    assert(policy.step == 2 && !policy.changed && policy.finished);
    policy = GuidePolicyFor(2, 3, true, false, false);
    assert(policy.step == 1 && policy.changed && !policy.closed);
    policy = GuidePolicyFor(1, 3, false, false, true);
    assert(policy.step == 1 && policy.closed);

    tip = GuideTipBounds((Rectangle){100, 100, 50, 40}, 200, 120, 640, 480,
                         0, 0, metrics);
    check_rect(tip, 25, 160, 200, 120);

    scrim = GuideScrimFor(320, 240, (Rectangle){40, 50, 60, 30}, metrics);
    check_rect(scrim.top, 0, 0, 320, 46);
    check_rect(scrim.bottom, 0, 84, 320, 156);
    check_rect(scrim.left, 0, 46, 36, 38);
    check_rect(scrim.right, 104, 46, 216, 38);

    arrow = GuideArrowFor((Rectangle){100, 100, 80, 40},
                          (Rectangle){120, 20, 20, 20}, metrics);
    check_vec(arrow.line_start, 130, 40);
    check_vec(arrow.line_end, 130, 100);
    check_vec(arrow.tip0, 130, 100);
    check_vec(arrow.tip1, 120, 90);
    check_vec(arrow.tip2, 140, 90);
    assert(fabsf(arrow.stroke_width - 2.0f) < 0.001f);

    arrow = GuideArrowFor((Rectangle){100, 100, 80, 40},
                          (Rectangle){120, 200, 20, 20}, metrics);
    check_vec(arrow.line_start, 130, 200);
    check_vec(arrow.line_end, 130, 140);
    check_vec(arrow.tip1, 140, 150);

    arrow = GuideArrowFor((Rectangle){100, 100, 80, 40},
                          (Rectangle){20, 105, 20, 20}, metrics);
    check_vec(arrow.line_start, 40, 115);
    check_vec(arrow.line_end, 100, 115);
    check_vec(arrow.tip1, 90, 105);

    arrow = GuideArrowFor((Rectangle){100, 100, 80, 40},
                          (Rectangle){220, 105, 20, 20}, metrics);
    check_vec(arrow.line_start, 220, 115);
    check_vec(arrow.line_end, 180, 115);
    check_vec(arrow.tip1, 190, 105);

    layout = GuideLayoutFor((Rectangle){25, 160, 200, 120}, 176, 30, 1, 3,
                            metrics);
    check_rect(layout.close_button, 185, 172, 28, 28);
    check_rect(layout.back_button, 137, 234, 34, 34);
    check_rect(layout.next_button, 179, 234, 34, 34);
    check_rect(layout.text, 37, 208, 176, 14);
    assert(layout.controls_y == 234);
    assert(layout.text_clip_height == 14);
    assert(layout.text_clipped);
    assert(!layout.finish);

    layout = GuideLayoutFor((Rectangle){25, 160, 200, 180}, 176, 30, 2, 3,
                            metrics);
    assert(layout.finish);
    assert(!layout.text_clipped);
    return 0;
}
