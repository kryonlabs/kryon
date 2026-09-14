#include <assert.h>
#include <math.h>

#include "runtime/inspect.h"

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
    Rectangle bounds = {10, 20, 2, 3};
    InspectEdit edit;

    check_rect(InspectClampBounds(bounds, 4.0f), 10, 20, 4, 4);
    check_rect(InspectResizeHandleBounds((Rectangle){10, 20, 80, 40}, 12),
               78, 48, 12, 12);
    check_rect(InspectResizeHandleBounds((Rectangle){10, 20, 80, 40}, 0),
               78, 48, 12, 12);

    edit = InspectEditForDelta((Rectangle){10, 20, 80, 40},
                               (Rectangle){100, 120, 160, 80},
                               20, 10, 2.0f, false, 4.0f);
    check_rect(edit.bounds, 20, 25, 80, 40);
    check_rect(edit.screen_bounds, 120, 130, 160, 80);

    edit = InspectEditForDelta((Rectangle){10, 20, 3, 4},
                               (Rectangle){100, 120, 6, 8},
                               -4, -6, 2.0f, true, 4.0f);
    check_rect(edit.bounds, 10, 20, 4, 4);
    check_rect(edit.screen_bounds, 100, 120, 2, 2);

    assert(fabsf(InspectKeyboardStep(false) - 1.0f) < 0.001f);
    assert(fabsf(InspectKeyboardStep(true) - 8.0f) < 0.001f);

    return 0;
}
