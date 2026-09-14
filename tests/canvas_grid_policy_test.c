#include <assert.h>
#include <math.h>

#include "runtime/canvas_grid.h"

int
main(void)
{
    CanvasGridLine line;

    assert(CanvasGridSpacing(30, 10) == 30);
    assert(CanvasGridSpacing(5, 10) == 10);
    assert(CanvasGridSpacing(0, 0) == 1);
    assert(CanvasGridSpacing(-4, -2) == 1);
    assert(CanvasGridSpacing(8, -3) == 8);

    assert(CanvasGridLineCount(0.0f, 10) == 0);
    assert(CanvasGridLineCount(-5.0f, 10) == 0);
    assert(CanvasGridLineCount(100.0f, 25) == 4);
    assert(CanvasGridLineCount(100.0f, 30) == 4);
    assert(CanvasGridLineCount(90.0f, 30) == 3);
    assert(CanvasGridLineCount(1.0f, 100) == 1);
    assert(CanvasGridLineCount(50.0f, 0) == 50);

    line = CanvasGridVerticalLine((Rectangle){10, 20, 200, 100}, 2, 30,
                                  0xFF00FF00);
    assert(fabsf(line.bounds.x - 70.0f) < 0.001f);
    assert(fabsf(line.bounds.y - 20.0f) < 0.001f);
    assert(fabsf(line.bounds.width) < 0.001f);
    assert(fabsf(line.bounds.height - 100.0f) < 0.001f);
    assert(line.color == 0xFF00FF00u);

    line = CanvasGridHorizontalLine((Rectangle){10, 20, 200, 100}, 3, 25,
                                    0x80808080u);
    assert(fabsf(line.bounds.x - 10.0f) < 0.001f);
    assert(fabsf(line.bounds.y - 95.0f) < 0.001f);
    assert(fabsf(line.bounds.width - 200.0f) < 0.001f);
    assert(fabsf(line.bounds.height) < 0.001f);
    assert(line.color == 0x80808080u);

    line = CanvasGridVerticalLine((Rectangle){0, 0, 100, 50}, 1, 0, 0);
    assert(fabsf(line.bounds.x - 1.0f) < 0.001f);

    return 0;
}
