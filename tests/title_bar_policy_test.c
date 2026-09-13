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
    StyleFrame bar = {0};
    StyleFrame title = {0};
    StyleFrame action = {0};
    bar.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                       StyleContentOffset;
    bar.value.padding_x = 12.0f;
    bar.value.gap = 4.0f;
    bar.value.icon_size = 32.0f;
    bar.value.offset_x = 60.0f;
    title.value.fields = StylePaddingX | StyleIconSize;
    title.value.padding_x = 16.0f;
    title.value.icon_size = 48.0f;
    action.value.fields = StylePaddingX | StyleIconSize;
    action.value.padding_x = 10.0f;
    action.value.icon_size = 20.0f;
    TitleBarMetrics metrics = TitleBarMetricsFor(2.0f, bar, title, action);
    TitleBarPaint paint;
    TitleBarTitlePaint title_paint;
    assert(metrics.side_margin == 24);
    assert(metrics.leading_reserved == 120);
    assert(metrics.leading_icon_size == 40);
    assert(metrics.leading_padding == 20);
    assert(metrics.dropdown_default_height == 64);
    assert(TitleBarMetricsFor(0.0f, (StyleFrame){0}, (StyleFrame){0},
                              (StyleFrame){0}).side_margin == 12);

    TitleBarLayout layout = TitleBarLayoutFor(360, 88, true, true, 0, 0,
                                              metrics);
    check_rect(layout.bounds, 0, 0, 360, 88);
    check_rect(layout.leading_bounds, 24, 4, 80, 80);
    check_rect(layout.title_bounds, 120, 0, 120, 88);
    check_rect(layout.dropdown_bounds, 112, 12, 224, 64);
    assert(layout.side_reserved == 120);

    layout = TitleBarLayoutFor(100, 44, false, false, 0, 0,
                               TitleBarMetricsFor(1.0f, bar, title, action));
    check_rect(layout.title_bounds, 12, 0, 76, 44);

    layout = TitleBarLayoutFor(90, 32, false, true, 20, 120,
                               TitleBarMetricsFor(1.0f, bar, title, action));
    check_rect(layout.dropdown_bounds, 12, 6, 78, 20);
    bar.value.padding_x = 0.0f;
    bar.value.gap = 0.0f;
    bar.value.icon_size = 0.0f;
    bar.value.offset_x = 0.0f;
    title.value.padding_x = 0.0f;
    title.value.icon_size = 0.0f;
    action.value.padding_x = 0.0f;
    action.value.icon_size = 0.0f;
    metrics = TitleBarMetricsFor(1.0f, bar, title, action);
    assert(metrics.side_margin == 0);
    assert(metrics.leading_reserved == 0);
    assert(metrics.leading_icon_size == 0);
    assert(metrics.leading_padding == 0);
    assert(metrics.leading_x == 0);
    assert(metrics.dropdown_gap == 0);
    assert(metrics.dropdown_default_height == 0);
    assert(metrics.title_min_width == 0);
    assert(metrics.title_horizontal_padding == 0);
    paint = TitleBarPaintFor(360, 88);
    check_rect(paint.bounds, 0, 0, 360, 88);
    check_rect(paint.divider, 0, 87, 360, 1);
    title_paint = TitleBarTitlePaintFor(layout, 40, 16);
    assert(title_paint.x == 25);
    assert(title_paint.y == 8);
    assert(TitleBarShouldShrinkTitleFont(200, 160, 18, 12));
    assert(!TitleBarShouldShrinkTitleFont(160, 160, 18, 12));
    assert(!TitleBarShouldShrinkTitleFont(200, 160, 12, 12));
    assert(TitleBarShrinkTitleFontStep(200, 160, 18, 12) == 17);
    assert(TitleBarShrinkTitleFontStep(160, 160, 18, 12) == 18);
    assert(TitleBarShrinkTitleFontStep(200, 160, 12, 12) == 12);
    return 0;
}
