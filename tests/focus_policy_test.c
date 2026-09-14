#include <assert.h>
#include <math.h>

#include "runtime/focus.h"

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
    StyleFrame frame = {0};
    frame.value.padding_x = 5.0f;
    frame.value.border_width = 3.0f;

    FocusPaint paint = FocusPaintFor((Rectangle){10, 20, 30, 40}, 2.0f,
                                     frame);
    check_rect(paint.bounds, 4, 14, 42, 52);
    assert(paint.stroke_width == 4);
    assert(FocusStrokeWidthFor(0.0f) == 2);
    assert(FocusStrokeWidthFor(2.0f) == 4);
    assert(FocusActivationFor(true, true, false, false, true, false, false));
    assert(FocusActivationFor(true, true, false, false, false, true, false));
    assert(!FocusActivationFor(false, true, false, false, true, true, false));
    assert(!FocusActivationFor(true, false, false, false, true, true, false));
    assert(!FocusActivationFor(true, true, true, false, true, true, false));
    assert(!FocusActivationFor(true, true, false, true, true, true, false));
    assert(!FocusActivationFor(true, true, false, false, false, true, true));
    assert(!FocusActivationFor(true, true, false, false, false, false, false));
    check_rect(FocusDefaultOutlineBounds((Rectangle){10, 20, 30, 40}, 1.0f),
               8, 18, 34, 44);
    check_rect(FocusDefaultOutlineBounds((Rectangle){10, 20, 30, 40}, 2.0f),
               6, 16, 38, 48);

    frame.value.fields = StylePaddingX | StyleBorderWidth;
    paint = FocusPaintFor((Rectangle){10, 20, 30, 40}, 2.0f, frame);
    check_rect(paint.bounds, 0, 10, 50, 60);
    assert(paint.stroke_width == 6);

    frame.value.padding_x = 0.0f;
    frame.value.border_width = 0.0f;
    paint = FocusPaintFor((Rectangle){1, 2, 3, 4}, 1.0f, frame);
    check_rect(paint.bounds, 1, 2, 3, 4);
    assert(paint.stroke_width == 0);

    FocusDebugOverlayPaint debug =
        FocusDebugOverlayPaintFor((Rectangle){10, 20, 30, 40}, 14, true);
    check_rect(debug.outline, 10, 20, 30, 40);
    assert(fabsf(debug.label_position.x - 10) < 0.001f);
    assert(fabsf(debug.label_position.y - 6) < 0.001f);
    assert(debug.stroke_width == 1);
    assert(debug.label_visible);

    debug = FocusDebugOverlayPaintFor((Rectangle){10, 20, 30, 40}, -3, false);
    assert(fabsf(debug.label_position.y - 20) < 0.001f);
    assert(!debug.label_visible);

    assert(FocusTabDirectionFor(false, false) == 0);
    assert(FocusTabDirectionFor(false, true) == 0);
    assert(FocusTabDirectionFor(true, false) == 1);
    assert(FocusTabDirectionFor(true, true) == -1);
    return 0;
}
