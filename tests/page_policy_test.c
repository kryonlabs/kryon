#include <assert.h>

#include "runtime/page.h"

int
main(void)
{
    StyleData style = {0};
    PageLayoutMetrics metrics = PageLayoutMetricsFor(style);
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);

    style.fields = StyleGap | StylePaddingX;
    style.gap = 8.4f;
    style.padding_x = 12.5f;
    metrics = PageLayoutMetricsFor(style);
    assert(metrics.gap == 8);
    assert(metrics.padding == 13);

    style.gap = 0.0f;
    style.padding_x = 0.0f;
    metrics = PageLayoutMetricsFor(style);
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);

    style.gap = -1.0f;
    style.padding_x = -2.0f;
    metrics = PageLayoutMetricsFor(style);
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);
    return 0;
}
