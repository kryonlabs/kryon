#include <assert.h>
#include <math.h>

#include "runtime/separator.h"

int
main(void)
{
    SeparatorLine line;
    SeparatorLabelPaint label;

    assert(SeparatorLineRole() == 7);
    assert(SeparatorLabelRole() == 6);
    assert(SeparatorBulletRole() == 8);

    line = SeparatorLineFor((Rectangle){10, 20, 100, 8}, 0, (StyleFrame){0});
    assert(fabsf(line.line.x - 10.0f) < 0.001f);
    assert(fabsf(line.line.y - 24.0f) < 0.001f);
    assert(fabsf(line.line.width - 100.0f) < 0.001f);
    assert(fabsf(line.line.height) < 0.001f);

    line = SeparatorLineFor((Rectangle){10, 20, 4, 60}, 1, (StyleFrame){0});
    assert(fabsf(line.line.x - 12.0f) < 0.001f);
    assert(fabsf(line.line.y - 20.0f) < 0.001f);
    assert(fabsf(line.line.width) < 0.001f);
    assert(fabsf(line.line.height - 60.0f) < 0.001f);

    label = SeparatorLabelPaintFor((Rectangle){10, 20, 100, 16}, 30.0f, 1, 14,
                                   1.0f, (StyleFrame){0});
    assert(label.show_text);
    assert(label.show_line);
    assert(fabsf(label.line.x - 52.0f) < 0.001f);
    assert(fabsf(label.line.width - 58.0f) < 0.001f);
    assert(fabsf(label.text.x - 10.0f) < 0.001f);
    assert(fabsf(label.text.y - 21.0f) < 0.001f);

    label = SeparatorLabelPaintFor((Rectangle){10, 20, 100, 16}, 30.0f, 0,
                                   14, 1.0f, (StyleFrame){0});
    assert(!label.show_text);
    assert(fabsf(label.line.x - 10.0f) < 0.001f);
    assert(fabsf(label.line.width - 100.0f) < 0.001f);

    label = SeparatorLabelPaintFor((Rectangle){10, 20, 20, 16}, 30.0f, 1, 14,
                                   1.0f, (StyleFrame){0});
    assert(!label.show_line);
    assert(fabsf(label.line.width) < 0.001f);

    {
        StyleFrame frame = {0};
        frame.value.fields = StyleGap;
        frame.value.gap = 20.0f;
        label = SeparatorLabelPaintFor((Rectangle){10, 20, 100, 16}, 30.0f, 1,
                                       14, 1.0f, frame);
        assert(fabsf(label.line.x - 60.0f) < 0.001f);
    }
    {
        StyleFrame frame = {0};
        frame.value.fields = StyleGap;
        frame.value.gap = 0.0f;
        label = SeparatorLabelPaintFor((Rectangle){10, 20, 100, 16}, 30.0f, 1,
                                       14, 1.0f, frame);
        assert(fabsf(label.line.x - 40.0f) < 0.001f);
    }

    return 0;
}
