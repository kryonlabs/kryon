#include <assert.h>
#include <math.h>

#include "runtime/bevel.h"

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
    BevelLines lines = BevelLinesFor(10, 20, 60, 24);
    check_rect(lines.top, 10, 20, 59, 0);
    check_rect(lines.left, 10, 20, 0, 23);
    check_rect(lines.bottom, 10, 43, 59, 0);
    check_rect(lines.right, 69, 20, 0, 23);
    check_rect(BevelLineBounds(3, 4, 7, 9), 3, 4, 4, 5);
    return 0;
}
