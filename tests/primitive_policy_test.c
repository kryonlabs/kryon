#include <assert.h>
#include <math.h>

#include "runtime/primitive.h"

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
    LinePrimitive line;

    check_rect(PrimitiveBackgroundBounds(640, 480), 0, 0, 640, 480);
    check_rect(PrimitiveRectBounds(10, 20, 30, 40), 10, 20, 30, 40);

    line = PrimitiveLineFor(10, 20, 50, 5);
    assert(line.x1 == 10);
    assert(line.y1 == 20);
    assert(line.x2 == 50);
    assert(line.y2 == 5);
    check_rect(line.bounds, 10, 5, 40, 15);

    line = PrimitiveLineFor(50, 5, 10, 20);
    assert(line.x1 == 50);
    assert(line.y1 == 5);
    assert(line.x2 == 10);
    assert(line.y2 == 20);
    check_rect(line.bounds, 10, 5, 40, 15);
    return 0;
}
