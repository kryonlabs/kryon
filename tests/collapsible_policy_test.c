#include <assert.h>
#include <math.h>

#include "runtime/collapsible.h"

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
    CollapsibleMetrics metrics = CollapsibleMetricsFor(2.0f);
    assert(metrics.header_height == 64);
    assert(metrics.depth_indent == 40);
    assert(metrics.close_width == 56);
    assert(metrics.icon_offset == 16);
    assert(metrics.text_offset == 56);

    CollapsibleLayout layout = CollapsibleLayoutFor(
        (Rectangle){10, 20, 180, 120}, true, 2, true, metrics);
    assert(layout.has_close);
    check_rect(layout.header, 90, 20, 100, 64);
    check_rect(layout.body, 90, 20, 44, 64);
    check_rect(layout.close_bounds, 134, 20, 56, 64);

    layout = CollapsibleLayoutFor((Rectangle){0, 0, 32, 99},
                                  true, 4, true, metrics);
    check_rect(layout.header, 32, 0, 0, 64);
    check_rect(layout.body, 32, 0, 0, 64);
    check_rect(layout.close_bounds, 32, 0, 0, 64);

    layout = CollapsibleLayoutFor((Rectangle){3, 4, 50, 70},
                                  false, 9, false, metrics);
    assert(!layout.has_close);
    check_rect(layout.header, 3, 4, 50, 64);
    check_rect(layout.body, 3, 4, 50, 64);

    assert(CollapsibleMarkerFor(false, false) == CollapsibleMarkerClosed);
    assert(CollapsibleMarkerFor(true, false) == CollapsibleMarkerOpen);
    assert(CollapsibleMarkerFor(true, true) == CollapsibleMarkerLeaf);
    assert(CollapsibleMetricsFor(0.0f).header_height == 32);
    return 0;
}
