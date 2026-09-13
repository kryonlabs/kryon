#include <assert.h>
#include <math.h>

#include "runtime/guide.h"
#include "runtime/style.h"

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

static void
check_default_metrics(GuideMetrics metrics)
{
    assert(metrics.margin == 12);
    assert(metrics.anchor_padding == 4);
    assert(metrics.anchor_stroke == 2);
    assert(metrics.gap == 20);
    assert(metrics.pad == 12);
    assert(metrics.button_size == 34);
    assert(metrics.close_size == 28);
    assert(metrics.close_icon_size == 16);
    assert(metrics.close_icon_padding == 6);
    assert(metrics.nav_icon_size == 19);
    assert(metrics.nav_icon_padding == 7);
    assert(metrics.nav_gap == 8);
    assert(metrics.text_gap == 8);
    assert(metrics.default_line_gap == 6);
    assert(metrics.controls_gap == 12);
    assert(metrics.text_guard == 8);
    assert(metrics.min_tip_height == 112);
    assert(metrics.default_tip_width == 300);
}

static void
check_zero_metric_policy(StyleFrame panel, StyleFrame anchor, StyleFrame label,
                         StyleFrame action, StyleFrame close)
{
    GuideMetrics metrics;

    panel.value.padding_x = 0.0f;
    panel.value.padding_y = 0.0f;
    panel.value.gap = 0.0f;
    panel.value.offset_x = 0.0f;
    panel.value.offset_y = 0.0f;
    anchor.value.padding_x = 0.0f;
    anchor.value.border_width = 0.0f;
    label.value.padding_x = 0.0f;
    label.value.padding_y = 0.0f;
    label.value.gap = 0.0f;
    action.value.padding_x = 0.0f;
    action.value.gap = 0.0f;
    action.value.icon_size = 0.0f;
    action.value.offset_y = 0.0f;
    close.value.padding_x = 0.0f;
    close.value.gap = 0.0f;
    close.value.icon_size = 0.0f;
    close.value.offset_y = 0.0f;

    metrics = GuideMetricsFor(1.0f, panel, anchor, label, action, close);
    assert(metrics.margin == 0);
    assert(metrics.anchor_padding == 0);
    assert(metrics.anchor_stroke == 0);
    assert(metrics.gap == 0);
    assert(metrics.pad == 0);
    assert(metrics.button_size == 34);
    assert(metrics.close_size == 28);
    assert(metrics.close_icon_size == 0);
    assert(metrics.close_icon_padding == 0);
    assert(metrics.nav_icon_size == 0);
    assert(metrics.nav_icon_padding == 0);
    assert(metrics.nav_gap == 0);
    assert(metrics.text_gap == 0);
    assert(metrics.default_line_gap == 0);
    assert(metrics.controls_gap == 0);
    assert(metrics.text_guard == 0);
    assert(metrics.min_tip_height == 112);
    assert(metrics.default_tip_width == 300);
}

int
main(void)
{
    StyleFrame panel = {0};
    StyleFrame anchor = {0};
    StyleFrame label = {0};
    StyleFrame action = {0};
    StyleFrame close = {0};
    GuideMetrics metrics;
    GuidePolicy policy;
    GuideScrim scrim;
    GuideLayout layout;
    GuideArrow arrow;
    Rectangle tip;

    panel.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                         StyleContentOffset;
    panel.value.padding_x = 12.0f;
    panel.value.padding_y = 12.0f;
    panel.value.gap = 20.0f;
    panel.value.offset_x = 300.0f;
    panel.value.offset_y = 112.0f;
    anchor.value.fields = StylePaddingX | StyleBorderWidth;
    anchor.value.padding_x = 4.0f;
    anchor.value.border_width = 2.0f;
    label.value.fields = StylePaddingX | StylePaddingY | StyleGap;
    label.value.padding_x = 8.0f;
    label.value.padding_y = 6.0f;
    label.value.gap = 8.0f;
    action.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                          StyleContentOffset;
    action.value.padding_x = 7.0f;
    action.value.gap = 8.0f;
    action.value.icon_size = 19.0f;
    action.value.offset_y = 34.0f;
    close.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                         StyleContentOffset;
    close.value.padding_x = 6.0f;
    close.value.gap = 12.0f;
    close.value.icon_size = 16.0f;
    close.value.offset_y = 28.0f;
    metrics = GuideMetricsFor(1.0f, panel, anchor, label, action, close);

    check_default_metrics(metrics);
    check_default_metrics(GuideMetricsFor(1.0f, (StyleFrame){0},
                                         (StyleFrame){0}, (StyleFrame){0},
                                         (StyleFrame){0}, (StyleFrame){0}));
    check_zero_metric_policy(panel, anchor, label, action, close);
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
