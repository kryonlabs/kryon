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
    TrianglePrimitive triangle;
    Color styled = {0x12, 0x34, 0x56, 0xff};
    Color fallback = {0xaa, 0xbb, 0xcc, 0xff};
    Color transparent = {0};
    Color picked;

    check_rect(PrimitiveBackgroundBounds(640, 480), 0, 0, 640, 480);
    picked = PrimitiveAppBackgroundColor(styled, fallback);
    assert(picked.r == 0x12 && picked.g == 0x34 && picked.b == 0x56 &&
           picked.a == 0xff);
    picked = PrimitiveAppBackgroundColor(transparent, fallback);
    assert(picked.r == 0xaa && picked.g == 0xbb && picked.b == 0xcc &&
           picked.a == 0xff);
    check_rect(PrimitiveRectBounds(10, 20, 30, 40), 10, 20, 30, 40);
    check_rect(PrimitiveCircleBounds(40, 50, 8), 32, 42, 16, 16);
    check_rect(PrimitiveRingBounds(40, 50, 12), 28, 38, 24, 24);

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

    triangle = PrimitiveTriangleFor(20, 10, -5, 30, 12, -4);
    assert(triangle.x1 == 20);
    assert(triangle.y1 == 10);
    assert(triangle.x2 == -5);
    assert(triangle.y2 == 30);
    assert(triangle.x3 == 12);
    assert(triangle.y3 == -4);
    check_rect(triangle.bounds, -5, -4, 25, 34);
    return 0;
}
