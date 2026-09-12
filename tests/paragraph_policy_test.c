#include <assert.h>

#include "runtime/paragraph.h"

int
main(void)
{
    ParagraphMetrics metrics = ParagraphResolveMetrics(
        0, 16, 0, 4, 0, 0, 240, 0, 30);
    assert(metrics.font == 16);
    assert(metrics.line_gap == 4);
    assert(metrics.icon_size == 16);
    assert(metrics.width == 240);
    assert(metrics.height == 20);
    assert(metrics.next_y == 50);
    assert(ParagraphCanLayout(metrics.width));

    metrics = ParagraphResolveMetrics(18, 16, 6, 4, 22, 180, 240, 72, 12);
    assert(metrics.font == 18);
    assert(metrics.line_gap == 6);
    assert(metrics.icon_size == 22);
    assert(metrics.width == 180);
    assert(metrics.height == 72);
    assert(metrics.next_y == 84);

    metrics = ParagraphResolveMetrics(14, 16, 3, 4, 0, -10, -20, -1, 5);
    assert(metrics.width == 0);
    assert(metrics.height == 17);
    assert(!ParagraphCanLayout(metrics.width));
    return 0;
}
