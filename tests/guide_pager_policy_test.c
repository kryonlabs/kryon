#include <assert.h>
#include <math.h>

#include "runtime/guide_pager.h"
#include "runtime/style.h"

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
    StyleFrame bar = {0};
    GuidePagerMetrics metrics;
    GuidePagerLayout layout;
    GuidePagerPolicy policy;
    GuidePagerInput input;

    bar.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                       StyleContentOffset;
    bar.value.padding_x = 12.0f;
    bar.value.gap = 12.0f;
    bar.value.icon_size = 48.0f;
    bar.value.offset_x = 48.0f;
    metrics = GuidePagerMetricsFor(2.0f, bar);

    assert(metrics.pad == 24);
    assert(metrics.gap == 24);
    assert(metrics.button_height == 96);
    assert(fabsf(metrics.swipe_min_distance - 96.0f) < 0.001f);
    assert(fabsf(metrics.swipe_axis_bias - 1.25f) < 0.001f);
    assert(fabsf(metrics.swipe_max_duration - 0.8f) < 0.001f);

    assert(GuidePagerPageCount(0) == 1);
    assert(GuidePagerPageFor(-3, 5) == 0);
    assert(GuidePagerPageFor(7, 5) == 4);
    assert(GuidePagerPageFor(2, 5) == 2);

    metrics = GuidePagerMetricsFor(1.0f, bar);
    layout = GuidePagerLayoutFor((Rectangle){10, 400, 300, 80}, metrics);
    assert(layout.valid);
    assert(layout.button_height == 48);
    check_rect(layout.left_button, 22, 416, 132, 48);
    check_rect(layout.right_button, 166, 416, 132, 48);

    layout = GuidePagerLayoutFor((Rectangle){0, 0, 40, 10}, metrics);
    assert(!layout.valid);

    bar.value.padding_x = 0.0f;
    bar.value.gap = 0.0f;
    bar.value.icon_size = 0.0f;
    bar.value.offset_x = 0.0f;
    metrics = GuidePagerMetricsFor(1.0f, bar);
    assert(metrics.pad == 0);
    assert(metrics.gap == 0);
    assert(metrics.button_height == 0);
    assert(fabsf(metrics.swipe_min_distance - 48.0f) < 0.001f);
    layout = GuidePagerLayoutFor((Rectangle){10, 400, 300, 80}, metrics);
    assert(!layout.valid);

    policy = GuidePagerPolicyFor(0, 3, false, true, false, false, false);
    assert(policy.page == 1 && policy.changed && !policy.finished);

    policy = GuidePagerPolicyFor(2, 3, false, true, false, false, true);
    assert(policy.page == 2 && !policy.changed && policy.finished);

    policy = GuidePagerPolicyFor(2, 3, true, false, false, false, false);
    assert(policy.page == 1 && policy.changed && !policy.closed);

    policy = GuidePagerPolicyFor(0, 3, true, false, true, false, false);
    assert(policy.page == 0 && policy.closed);

    policy = GuidePagerPolicyFor(2, 3, false, false, false, true, false);
    assert(policy.page == 2 && policy.finished);

    input = GuidePagerInputFor(true, false, false, false, false, false);
    assert(input.previous_requested && !input.next_requested &&
           !input.close_requested && !input.keyboard_finish);
    input = GuidePagerInputFor(false, true, false, false, false, false);
    assert(!input.previous_requested && input.next_requested &&
           !input.close_requested && input.keyboard_finish);
    input = GuidePagerInputFor(false, false, true, false, false, true);
    assert(!input.previous_requested && input.next_requested &&
           !input.close_requested && !input.keyboard_finish);
    input = GuidePagerInputFor(false, false, false, true, false, false);
    assert(!input.previous_requested && !input.next_requested &&
           input.close_requested && !input.keyboard_finish);
    input = GuidePagerInputFor(false, false, false, false, true, false);
    assert(!input.previous_requested && !input.next_requested &&
           input.close_requested && !input.keyboard_finish);
    return 0;
}
