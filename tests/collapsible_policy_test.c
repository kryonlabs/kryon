#include <assert.h>
#include <math.h>
#include <string.h>

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
    StyleFrame header = {0};
    StyleFrame tree_header = {0};
    StyleFrame close = {0};
    header.value.fields = StyleFontSize | StylePaddingX | StylePaddingY |
                          StyleIconSize;
    header.value.font_size = 14.0f;
    header.value.padding_x = 6.0f;
    header.value.padding_y = 10.0f;
    header.value.icon_size = 18.0f;
    tree_header.value.fields = StylePaddingX;
    tree_header.value.padding_x = 17.0f;
    close.value.fields = StyleIconSize;
    close.value.icon_size = 22.0f;
    CollapsibleMetrics metrics = CollapsibleMetricsFor(2.0f, header,
                                                       tree_header, close);
    assert(metrics.header_height == 68);
    assert(metrics.depth_indent == 34);
    assert(metrics.close_width == 44);
    assert(metrics.icon_offset == 12);
    assert(metrics.text_offset == 48);

    CollapsibleLayout layout = CollapsibleLayoutFor(
        (Rectangle){10, 20, 180, 120}, true, 2, true, metrics);
    assert(layout.has_close);
    check_rect(layout.header, 78, 20, 112, 68);
    check_rect(layout.body, 78, 20, 68, 68);
    check_rect(layout.close_bounds, 146, 20, 44, 68);

    layout = CollapsibleLayoutFor((Rectangle){0, 0, 32, 99},
                                  true, 4, true, metrics);
    check_rect(layout.header, 32, 0, 0, 68);
    check_rect(layout.body, 32, 0, 0, 68);
    check_rect(layout.close_bounds, 32, 0, 0, 68);

    layout = CollapsibleLayoutFor((Rectangle){3, 4, 50, 70},
                                  false, 9, false, metrics);
    assert(!layout.has_close);
    check_rect(layout.header, 3, 4, 50, 68);
    check_rect(layout.body, 3, 4, 50, 68);

    assert(CollapsibleMarkerFor(false, false) == CollapsibleMarkerClosed);
    assert(CollapsibleMarkerFor(true, false) == CollapsibleMarkerOpen);
    assert(CollapsibleMarkerFor(true, true) == CollapsibleMarkerLeaf);
    assert(strcmp(CollapsibleMarkerText(CollapsibleMarkerClosed), ">") == 0);
    assert(strcmp(CollapsibleMarkerText(CollapsibleMarkerOpen), "v") == 0);
    assert(strcmp(CollapsibleMarkerText(CollapsibleMarkerLeaf), "•") == 0);
    header.value.fields = 0;
    header.value.font_size = 0.0f;
    header.value.padding_x = 0.0f;
    header.value.padding_y = 0.0f;
    header.value.icon_size = 0.0f;
    tree_header.value.fields = 0;
    tree_header.value.padding_x = 0.0f;
    close.value.fields = 0;
    close.value.icon_size = 0.0f;
    metrics = CollapsibleMetricsFor(0.0f, header, tree_header, close);
    assert(metrics.header_height == 32);
    assert(metrics.depth_indent == 20);
    assert(metrics.close_width == 28);
    assert(metrics.icon_offset == 8);
    assert(metrics.text_offset == 28);
    header.value.fields = StyleFontSize | StylePaddingX | StylePaddingY |
                          StyleIconSize;
    tree_header.value.fields = StylePaddingX;
    close.value.fields = StyleIconSize;
    metrics = CollapsibleMetricsFor(1.0f, header, tree_header, close);
    assert(metrics.header_height == 0);
    assert(metrics.depth_indent == 0);
    assert(metrics.close_width == 0);
    assert(metrics.icon_offset == 0);
    assert(metrics.text_offset == 0);
    return 0;
}
