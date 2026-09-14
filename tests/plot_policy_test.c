#include <assert.h>
#include <math.h>

#include "runtime/plot.h"

int
main(void)
{
    PlotRange range;

    assert(PlotOffset(0, 5) == 0);
    assert(PlotOffset(4, 6) == 2);
    assert(PlotOffset(4, -1) == 3);
    assert(PlotOffset(4, -6) == 2);
    assert(PlotOffset(4, 8) == 0);
    assert(PlotOffset(1, 99) == 0);

    range = PlotRangeFor(0.0f, 10.0f, -1.0f, 20.0f);
    assert(fabsf(range.min_value) < 0.001f);
    assert(fabsf(range.max_value - 10.0f) < 0.001f);
    assert(fabsf(range.range - 10.0f) < 0.001f);

    range = PlotRangeFor(5.0f, 5.0f, -2.0f, 8.0f);
    assert(fabsf(range.min_value + 2.0f) < 0.001f);
    assert(fabsf(range.max_value - 8.0f) < 0.001f);
    assert(fabsf(range.range - 10.0f) < 0.001f);

    range = PlotRangeFor(10.0f, 5.0f, 3.0f, 3.0f);
    assert(fabsf(range.min_value - 2.5f) < 0.001f);
    assert(fabsf(range.max_value - 3.5f) < 0.001f);
    assert(fabsf(range.range - 1.0f) < 0.001f);

    range = PlotRangeFor(0.0f, 10.0f, 0.0f, 0.0f);
    assert(fabsf(range.min_value) < 0.001f);
    assert(fabsf(range.max_value - 10.0f) < 0.001f);
    assert(fabsf(range.range - 10.0f) < 0.001f);

    range = PlotRangeFor(0.0f, 10.0f, 0.0f, 10.0f);
    assert(fabsf(PlotNormalize(5.0f, range) - 0.5f) < 0.001f);
    assert(fabsf(PlotNormalize(-5.0f, range)) < 0.001f);
    assert(fabsf(PlotNormalize(50.0f, range) - 1.0f) < 0.001f);
    assert(fabsf(PlotNormalize(0.0f, range)) < 0.001f);
    assert(fabsf(PlotNormalize(10.0f, range) - 1.0f) < 0.001f);

    return 0;
}
