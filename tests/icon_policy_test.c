#include <assert.h>
#include <math.h>

#include "runtime/icon.h"

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
    IconLayout layout = IconLayoutFor(12, 18, 24);
    assert(layout.size == 24);
    assert(layout.drawable);
    check_rect(layout.bounds, 12, 18, 24, 24);

    layout = IconLayoutFor(3, 4, -10);
    assert(layout.size == 0);
    assert(!layout.drawable);
    check_rect(layout.bounds, 3, 4, 0, 0);

    assert(IconButtonSizeFor(0, 1.0f) == 12);
    assert(IconButtonSizeFor(1, 1.0f) == 14);
    assert(IconButtonSizeFor(2, 2.0f) == 32);
    assert(IconButtonSizeFor(3, 1.5f) == 30);
    assert(IconButtonSizeFor(99, 1.0f) == 14);
    assert(IconButtonPaddingFor(0, 2.0f) == 8);
    assert(IconButtonPaddingFor(1, 2.0f) == 10);
    assert(IconButtonPaddingFor(2, 2.0f) == 12);
    assert(IconButtonPaddingFor(3, 2.0f) == 14);
    assert(IconButtonPaddingFor(99, 1.0f) == 5);
    return 0;
}
