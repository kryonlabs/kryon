#include <assert.h>
#include <math.h>

#include "runtime/toast.h"

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
    ToastMetrics metrics = ToastMetricsFor(2.0f);
    ToastLayout layout;

    assert(metrics.pad_x == 28);
    assert(metrics.pad_y == 20);
    assert(metrics.margin == 36);
    assert(fabsf(metrics.default_seconds - 3.0f) < 0.001f);
    assert(fabsf(ToastDuration(0.0f, metrics) - 3.0f) < 0.001f);
    assert(fabsf(ToastDuration(2.5f, metrics) - 2.5f) < 0.001f);
    assert(ToastMaxWidth(640, metrics) == 568);
    assert(ToastContentWidth(640, metrics) == 512);

    metrics = ToastMetricsFor(1.0f);
    layout = ToastLayoutFor(640, 480, 70, 18, metrics);
    check_rect(layout.bounds, 271, 424, 98, 38);
    check_rect(layout.text_bounds, 285, 434, 70, 18);
    assert(layout.content_width == 576);

    layout = ToastLayoutFor(100, 80, 500, 18, metrics);
    check_rect(layout.bounds, 18, 24, 64, 38);
    assert(layout.text_bounds.width == 36);
    assert(layout.content_width == 36);
    return 0;
}
