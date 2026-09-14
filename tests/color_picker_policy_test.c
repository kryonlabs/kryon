#include <assert.h>
#include <math.h>

#include "runtime/color_picker.h"

int
main(void)
{
    ColorPickerLayout layout;
    Rectangle row;

    assert(ColorPickerChannelIdFor(5, 0) == 41);
    assert(ColorPickerChannelIdFor(5, 3) == 44);
    assert(ColorPickerChannelIdFor(0, 1) == 0);
    assert(ColorPickerChannelIdFor(5, -1) == 0);

    layout = ColorPickerLayoutFor((Rectangle){0, 0, 100, 200}, 3, 1.0f,
                                  (StyleFrame){0});
    assert(fabsf(layout.row_height - (200.0f - 36.0f - 4.0f) / 3.0f) < 0.01f);
    assert(fabsf(layout.swatch_bounds.height - 36.0f) < 0.001f);
    assert(fabsf(layout.swatch_bounds.y -
                 (layout.row_height * 3.0f + 4.0f)) < 0.01f);
    assert(fabsf(layout.swatch_bounds.width - 100.0f) < 0.001f);

    layout = ColorPickerLayoutFor((Rectangle){0, 0, 100, 40}, 3, 1.0f,
                                  (StyleFrame){0});
    assert(fabsf(layout.row_height - 28.0f) < 0.001f);

    layout = ColorPickerLayoutFor((Rectangle){0, 0, 100, 200}, 0, 1.0f,
                                  (StyleFrame){0});
    assert(fabsf(layout.row_height - (200.0f - 36.0f - 4.0f)) < 0.001f);

    row = ColorPickerChannelBounds((Rectangle){10, 0, 100, 200}, 1, 3, 1.0f,
                                   (StyleFrame){0});
    assert(fabsf(row.x - 10.0f) < 0.001f);
    assert(fabsf(row.y - (200.0f - 40.0f) / 3.0f) < 0.01f);
    assert(fabsf(row.width - 100.0f) < 0.001f);
    assert(row.height > 0.0f);

    assert(fabsf(ColorPickerClampChannel(-0.5f)) < 0.001f);
    assert(fabsf(ColorPickerClampChannel(0.25f) - 0.25f) < 0.001f);
    assert(fabsf(ColorPickerClampChannel(1.5f) - 1.0f) < 0.001f);

    assert(ColorPickerChannelByte(0.0f) == 0);
    assert(ColorPickerChannelByte(1.0f) == 255);
    assert(ColorPickerChannelByte(-1.0f) == 0);
    assert(ColorPickerChannelByte(2.0f) == 255);
    assert(ColorPickerChannelByte(0.5f) == 128);

    {
        Color color = ColorPickerColorFor(1.0f, 0.0f, 0.5f, 0.5f, 3);
        assert(color.r == 255);
        assert(color.g == 0);
        assert(color.b == 128);
        assert(color.a == 255);

        color = ColorPickerColorFor(1.0f, 0.0f, 0.5f, 0.5f, 4);
        assert(color.a == 128);

        color = ColorPickerColorFor(2.0f, -1.0f, 0.0f, 2.0f, 4);
        assert(color.r == 255);
        assert(color.g == 0);
        assert(color.b == 0);
        assert(color.a == 255);
    }

    return 0;
}
