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
    PanedViewLayout layout;
    PanedViewDragDecision drag;
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
    assert(PanedViewSplitFor(0, false, 240, 40, 40) == 100);
    assert(PanedViewSplitFor(20, true, 240, 40, 40) == 40);
    assert(PanedViewSplitFor(220, true, 240, 40, 40) == 200);

    assert(PanedViewClampSplit(20, 40, 200) == 40);
    assert(PanedViewClampSplit(220, 40, 200) == 200);
    assert(PanedViewClampSplit(90, 40, 200) == 90);

    assert(PanedViewPointerSplit(bounds, true, 190, 60) == 180);
    assert(PanedViewPointerSplit(bounds, false, 190, 77) == 57);
    assert(PanedViewPointerSplitFor(bounds, true, 260, 60, 40, 40) == 200);
    assert(PanedViewPointerSplitFor(bounds, false, 190, 10, 30, 20) == 30);

    handle.value.icon_size = 8.0f;
    metrics = PanedViewMetricsFor(2.0f, handle);
    check_rect(PanedViewHandleFor(bounds, true, 90, metrics),
               92, 20, 16, 80);
    check_rect(PanedViewHandleFor(bounds, false, 50, metrics),
               10, 62, 240, 16);
    layout = PanedViewLayoutFor(bounds, true, 0, false, 40, 40, metrics);
    assert(layout.size == 240);
    assert(layout.limit == 200);
    assert(layout.split == 100);
    check_rect(layout.handle, 102, 20, 16, 80);
    layout = PanedViewLayoutFor(bounds, false, 500, true, 30, 20, metrics);
    assert(layout.size == 80);
    assert(layout.limit == 60);
    assert(layout.split == 60);
    check_rect(layout.handle, 10, 72, 240, 16);
    assert(PanedViewChanged(50, 60, true));
    assert(!PanedViewChanged(50, 50, true));
    assert(!PanedViewChanged(0, 60, false));
    drag = PanedViewDragFor(false, false, false, true, false, true, true,
                            true);
    assert(drag.start_drag);
    assert(drag.drag_active);
    assert(!drag.clear_active);
    drag = PanedViewDragFor(true, true, false, true, false, true, false,
                            false);
    assert(!drag.start_drag);
    assert(drag.drag_active);
    assert(!drag.clear_active);
    drag = PanedViewDragFor(true, true, true, true, false, true, true,
                            true);
    assert(drag.clear_active);
    assert(!drag.drag_active);
    drag = PanedViewDragFor(true, true, false, false, false, true, true,
                            true);
    assert(drag.clear_active);
    assert(!drag.drag_active);
    drag = PanedViewDragFor(true, true, false, true, true, true, true,
                            true);
    assert(drag.clear_active);
    assert(!drag.drag_active);
    drag = PanedViewDragFor(false, false, false, true, false, false, true,
                            true);
    assert(!drag.start_drag);
    assert(!drag.drag_active);

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
