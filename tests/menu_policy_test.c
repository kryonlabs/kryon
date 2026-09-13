#include <assert.h>
#include <math.h>

#include "runtime/menu.h"
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
    StyleFrame panel = {0};
    StyleFrame item = {0};
    StyleFrame bar = {0};
    MenuMetrics metrics = MenuMetricsFor(2.0f, panel, item, bar);
    Rectangle bounds;
    Rectangle row;
    Rectangle bar_item;
    Vector2 origin;
    MenuLine line;
    int width;

    assert(metrics.row_height == 60);
    assert(metrics.panel_min_width == 360);
    assert(metrics.panel_padding == 24);
    assert(metrics.panel_margin == 8);
    assert(metrics.accelerator_gap == 176);
    assert(metrics.separator_inset == 16);
    assert(metrics.checked_mark_inset == 16);
    assert(metrics.label_inset == 56);
    assert(metrics.submenu_indicator_inset == 36);
    assert(metrics.bar_item_padding == 48);
    assert(metrics.bar_item_gap == 4);
    assert(metrics.bar_item_y_padding == 6);
    assert(metrics.bar_label_inset == 24);

    panel.value.fields = StylePaddingX | StyleGap | StyleContentOffset;
    panel.value.padding_x = 10.0f;
    panel.value.gap = 3.0f;
    panel.value.offset_x = 160.0f;
    item.value.fields = StyleFontSize | StylePaddingX | StylePaddingY |
                        StyleIconSize | StyleContentOffset;
    item.value.font_size = 18.0f;
    item.value.padding_x = 7.0f;
    item.value.padding_y = 6.0f;
    item.value.icon_size = 22.0f;
    item.value.offset_x = 70.0f;
    bar.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                       StyleIconSize;
    bar.value.padding_x = 9.0f;
    bar.value.padding_y = 2.0f;
    bar.value.gap = 5.0f;
    bar.value.icon_size = 11.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.row_height == 30);
    assert(metrics.panel_min_width == 160);
    assert(metrics.panel_padding == 10);
    assert(metrics.panel_margin == 3);
    assert(metrics.accelerator_gap == 70);
    assert(metrics.separator_inset == 7);
    assert(metrics.checked_mark_inset == 7);
    assert(metrics.label_inset == 22);
    assert(metrics.submenu_indicator_inset == 6);
    assert(metrics.bar_item_padding == 18);
    assert(metrics.bar_item_gap == 5);
    assert(metrics.bar_item_y_padding == 2);
    assert(metrics.bar_label_inset == 11);

    item.value.offset_y = 44.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.row_height == 44);
    item.value.offset_y = 0.0f;

    panel.value.padding_x = 0.0f;
    panel.value.gap = 0.0f;
    item.value.offset_x = 0.0f;
    item.value.padding_x = 0.0f;
    item.value.padding_y = 0.0f;
    item.value.icon_size = 0.0f;
    bar.value.gap = 0.0f;
    bar.value.icon_size = 0.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.panel_padding == 0);
    assert(metrics.panel_margin == 0);
    assert(metrics.accelerator_gap == 0);
    assert(metrics.separator_inset == 0);
    assert(metrics.checked_mark_inset == 0);
    assert(metrics.label_inset == 0);
    assert(metrics.submenu_indicator_inset == 0);
    assert(metrics.bar_item_gap == 0);
    assert(metrics.bar_label_inset == 0);

    panel = (StyleFrame){0};
    item = (StyleFrame){0};
    bar = (StyleFrame){0};
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    width = MenuPanelWidthStep(0, 50, 20, true, metrics);
    assert(width == 182);
    bounds = MenuPanelBounds(0, 0, width, 3, metrics);
    check_rect(bounds, 0, 0, 182, 98);
    row = MenuRowBounds(bounds, 1, metrics);
    check_rect(row, 4, 34, 174, 30);
    assert(MenuSubmenuX(row) == 178);
    origin = MenuSubmenuOrigin(row);
    assert(fabsf(origin.x - 178.0f) < 0.001f);
    assert(fabsf(origin.y - 34.0f) < 0.001f);
    line = MenuSeparatorLineFor(row, metrics);
    assert(line.x1 == 12);
    assert(line.y1 == 49);
    assert(line.x2 == 170);
    assert(line.y2 == 49);
    assert(MenuTextY(row, 16) == 41);
    assert(MenuCheckedMarkX(row, metrics) == 12);
    assert(MenuLabelX(row, metrics) == 32);
    assert(MenuAcceleratorX(row, 20, metrics) == 146);
    assert(MenuSubmenuIndicatorX(row, metrics) == 160);
    assert(MenuGroupItemWidth(50, metrics) == 74);
    bar_item = MenuGroupItemBounds(10, (Rectangle){0, 0, 240, 30}, 74,
                                   metrics);
    check_rect(bar_item, 10, 3, 74, 24);
    assert(MenuBarFirstItemX((Rectangle){0, 0, 240, 30}, metrics) == 4);
    assert(MenuBarNextItemX(10, 74, metrics) == 86);
    assert(MenuBarLabelX(bar_item, metrics) == 22);
    assert(MenuBarLabelY(bar_item, 16) == 7);
    return 0;
}
