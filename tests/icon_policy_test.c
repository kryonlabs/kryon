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
    return 0;
}
