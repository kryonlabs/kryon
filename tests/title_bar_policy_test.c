#include <assert.h>
#include <math.h>

#include "runtime/title_bar.h"

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
    TitleBarMetrics metrics = TitleBarMetricsFor(2.0f);
    TitleBarPaint paint;
    TitleBarTitlePaint title_paint;
    assert(metrics.side_margin == 24);
    assert(metrics.leading_reserved == 120);
    assert(metrics.leading_icon_size == 40);
    assert(metrics.leading_padding == 20);
    assert(metrics.dropdown_default_height == 64);
    assert(TitleBarMetricsFor(0.0f).side_margin == 12);

    TitleBarLayout layout = TitleBarLayoutFor(360, 88, true, true, 0, 0,
                                              metrics);
    check_rect(layout.bounds, 0, 0, 360, 88);
    check_rect(layout.leading_bounds, 24, 4, 80, 80);
    check_rect(layout.title_bounds, 120, 0, 120, 88);
    check_rect(layout.dropdown_bounds, 112, 12, 224, 64);
    assert(layout.side_reserved == 120);

    layout = TitleBarLayoutFor(100, 44, false, false, 0, 0,
                               TitleBarMetricsFor(1.0f));
    check_rect(layout.title_bounds, 12, 0, 76, 44);

    layout = TitleBarLayoutFor(90, 32, false, true, 20, 120,
                               TitleBarMetricsFor(1.0f));
    check_rect(layout.dropdown_bounds, 12, 6, 78, 20);
    assert(TitleBarTitleX(360, 144) == 108);
    paint = TitleBarPaintFor(360, 88);
    check_rect(paint.bounds, 0, 0, 360, 88);
    check_rect(paint.divider, 0, 87, 360, 1);
    title_paint = TitleBarTitlePaintFor(layout, 40, 16);
    assert(title_paint.x == 25);
    assert(title_paint.y == 8);
    assert(TitleBarShouldShrinkTitleFont(200, 160, 18, 12));
    assert(!TitleBarShouldShrinkTitleFont(160, 160, 18, 12));
    assert(!TitleBarShouldShrinkTitleFont(200, 160, 12, 12));
    return 0;
}
