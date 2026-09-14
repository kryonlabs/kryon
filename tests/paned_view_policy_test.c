#include <assert.h>
#include <math.h>

#include "runtime/paned_view.h"

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
    Rectangle bounds = {10, 20, 240, 80};
    StyleFrame handle = {0};
    assert(PanedViewMetricsFor(0.0f, handle).grip == 8);
    assert(PanedViewMetricsFor(0.0f, handle).drop_edge == 46);

    handle.value.fields = StyleIconSize | StylePaddingX;
    handle.value.icon_size = 7.0f;
    handle.value.padding_x = 23.0f;
    PanedViewMetrics metrics = PanedViewMetricsFor(2.0f, handle);
    assert(metrics.grip == 14);
    assert(metrics.drop_edge == 46);
    handle.value.icon_size = 0.0f;
    handle.value.padding_x = 0.0f;
    assert(PanedViewMetricsFor(1.0f, handle).grip == 0);
    assert(PanedViewMetricsFor(1.0f, handle).drop_edge == 0);

    assert(PanedViewSize(bounds, true) == 240);
    assert(PanedViewSize(bounds, false) == 80);
    assert(PanedViewLimit(240, 40, 40) == 200);
    assert(PanedViewLimit(60, 50, 40) == 50);
    assert(PanedViewDefaultSplit(200) == 100);

    assert(PanedViewClampSplit(20, 40, 200) == 40);
    assert(PanedViewClampSplit(220, 40, 200) == 200);
    assert(PanedViewClampSplit(90, 40, 200) == 90);

    assert(PanedViewPointerSplit(bounds, true, 190, 60) == 180);
    assert(PanedViewPointerSplit(bounds, false, 190, 77) == 57);

    handle.value.icon_size = 8.0f;
    metrics = PanedViewMetricsFor(2.0f, handle);
    check_rect(PanedViewHandleFor(bounds, true, 90, metrics),
               92, 20, 16, 80);
    check_rect(PanedViewHandleFor(bounds, false, 50, metrics),
               10, 62, 240, 16);

    metrics.drop_edge = 46;
    assert(PanedViewDropZoneFor(bounds, (Vector2){1, 30}, metrics) ==
           DropNone);
    assert(PanedViewDropZoneFor(bounds, (Vector2){20, 60}, metrics) ==
           DropLeft);
    assert(PanedViewDropZoneFor(bounds, (Vector2){245, 60}, metrics) ==
           DropRight);
    assert(PanedViewDropZoneFor(bounds, (Vector2){120, 25}, metrics) ==
           DropTop);
    assert(PanedViewDropZoneFor(bounds, (Vector2){120, 97}, metrics) ==
           DropBottom);
    assert(PanedViewDropZoneFor((Rectangle){10, 20, 240, 200},
                                (Vector2){120, 120}, metrics) ==
           DropCenter);
    return 0;
}
