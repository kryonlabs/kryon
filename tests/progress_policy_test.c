#include <assert.h>
#include <math.h>

#include "runtime/progress.h"

int
main(void)
{
    ProgressLayout layout;

    assert(fabsf(ProgressRatio(0, 10, 5) - 0.5f) < 0.001f);
    assert(fabsf(ProgressRatio(0, 10, -3)) < 0.001f);
    assert(fabsf(ProgressRatio(0, 10, 30) - 1.0f) < 0.001f);
    assert(fabsf(ProgressRatio(5, 5, 3)) < 0.001f);
    assert(fabsf(ProgressRatio(5, 4, 3)) < 0.001f);
    assert(fabsf(ProgressRatio(5, 4, 6) - 1.0f) < 0.001f);

    {
        Rectangle fill = ProgressFillBounds((Rectangle){0, 0, 100, 10}, 0.25f);
        assert(fabsf(fill.width - 25.0f) < 0.001f);
        fill = ProgressFillBounds((Rectangle){0, 0, 100, 10}, -1.0f);
        assert(fabsf(fill.width) < 0.001f);
        fill = ProgressFillBounds((Rectangle){0, 0, 100, 10}, 2.0f);
        assert(fabsf(fill.width - 100.0f) < 0.001f);
    }

    assert(fabsf(ProgressDrawRadius((Rectangle){0, 0, 100, 10}, 5.0f) -
                 0.5f) < 0.001f);
    assert(fabsf(ProgressDrawRadius((Rectangle){0, 0, 100, 10}, -3.0f)) <
           0.001f);
    assert(fabsf(ProgressDrawRadius((Rectangle){0, 0, 100, 10}, 20.0f) -
                 1.0f) < 0.001f);
    assert(fabsf(ProgressDrawRadius((Rectangle){0, 0, 100, 0}, 5.0f)) <
           0.001f);

    assert(fabsf(ProgressLabelX((Rectangle){0, 0, 100, 10}, 60.0f, 10.0f,
                                4.0f) - 64.0f) < 0.001f);
    assert(fabsf(ProgressLabelX((Rectangle){0, 0, 100, 10}, 20.0f, 10.0f,
                                4.0f) - 24.0f) < 0.001f);
    assert(fabsf(ProgressLabelX((Rectangle){0, 0, 100, 10}, 30.0f, 40.0f,
                                4.0f) - 34.0f) < 0.001f);

    assert(fabsf(ProgressLabelY((Rectangle){0, 0, 100, 10}, 6.0f) - 2.0f) <
           0.001f);

    assert(!ProgressLabelOnFill(10.0f, 6.0f, 4.0f));
    assert(ProgressLabelOnFill(20.0f, 6.0f, 4.0f));

    layout = ProgressLayoutFor((Rectangle){0, 0, 100, 10}, 0, 10, 2, 10.0f,
                               6.0f, 4.0f);
    assert(fabsf(layout.ratio - 0.2f) < 0.001f);
    assert(fabsf(layout.fill_bounds.width - 20.0f) < 0.001f);
    assert(layout.label_on_fill);
    assert(fabsf(layout.label_x - 24.0f) < 0.001f);
    assert(fabsf(layout.label_y - 2.0f) < 0.001f);

    layout = ProgressLayoutFor((Rectangle){0, 0, 100, 10}, 0, 10, 10, 10.0f,
                               6.0f, 4.0f);
    assert(layout.label_on_fill);
    assert(fabsf(layout.label_x - 86.0f) < 0.001f);

    assert(fabsf(ProgressLabelPaddingForStyle((StyleFrame){0}, 1.0f) -
                 6.0f) < 0.001f);
    assert(fabsf(ProgressLabelPaddingForStyle((StyleFrame){0}, 2.0f) -
                 12.0f) < 0.001f);
    {
        StyleFrame label_frame = {0};
        label_frame.value.fields = StylePaddingX;
        label_frame.value.padding_x = 3.0f;
        assert(fabsf(ProgressLabelPaddingForStyle(label_frame, 1.0f) -
                     3.0f) < 0.001f);
        label_frame.value.padding_x = 0.0f;
        assert(fabsf(ProgressLabelPaddingForStyle(label_frame, 1.0f)) <
               0.001f);
    }

    return 0;
}
