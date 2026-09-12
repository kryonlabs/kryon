#include <assert.h>
#include <math.h>

#include "runtime/grid.h"

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
    GridProps props = {
        .bounds = {10, 20, 660, 100},
        .min_item_width = 200,
        .gap = 12,
        .padding = 4,
    };
    GridMetrics metrics = MeasureGrid(props);
    GridCursor cursor;

    assert(metrics.gap == 12);
    assert(metrics.padding == 4);
    assert(metrics.columns == 3);
    assert(metrics.cell_width == 209);
    check_rect(metrics.content, 14, 24, 652, 92);

    cursor = BeginGridCursor(props);
    cursor = GridStep(cursor, 40, 1);
    check_rect(cursor.item, 14, 24, 209, 40);
    cursor = GridStep(cursor, 50, 2);
    check_rect(cursor.item, 235, 24, 430, 50);
    cursor = GridStep(cursor, 30, 3);
    check_rect(cursor.item, 14, 86, 651, 30);

    metrics = MeasureGrid((GridProps){
        .bounds = {0, 0, 20, 20},
        .columns = -2,
        .min_item_width = -1,
        .max_columns = 2,
        .gap = -4,
        .padding = -3,
    });
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);
    assert(metrics.columns == 1);
    assert(metrics.cell_width == 20);
    return 0;
}
