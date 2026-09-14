#include <assert.h>
#include <math.h>

#include "runtime/fieldset.h"

int
main(void)
{
    FieldsetPaint paint;

    paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 1, 1.0f,
                             (StyleFrame){0});
    assert(paint.show_title);
    assert(fabsf(paint.title_background.x - 18.0f) < 0.001f);
    assert(fabsf(paint.title_background.y - 42.0f) < 0.001f);
    assert(fabsf(paint.title_background.width - 56.0f) < 0.001f);
    assert(fabsf(paint.title_background.height - 18.0f) < 0.001f);
    assert(fabsf(paint.title_text.x - 26.0f) < 0.001f);
    assert(fabsf(paint.title_text.y - 41.0f) < 0.001f);
    assert(fabsf(paint.title_text.width - 40.0f) < 0.001f);
    assert(fabsf(paint.frame.x - 10.0f) < 0.001f);
    assert(fabsf(paint.border_width) < 0.001f);

    paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 0, 1.0f,
                             (StyleFrame){0});
    assert(!paint.show_title);

    paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 1, 2.0f,
                             (StyleFrame){0});
    assert(fabsf(paint.title_background.x - 26.0f) < 0.001f);
    assert(fabsf(paint.title_background.height - 36.0f) < 0.001f);

    {
        StyleFrame frame = {0};
        frame.value.fields = StylePaddingX | StyleFontSize;
        frame.value.padding_x = 4.0f;
        frame.value.font_size = 20.0f;
        paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 1, 1.0f,
                                 frame);
        assert(fabsf(paint.title_background.x - 14.0f) < 0.001f);
        assert(fabsf(paint.title_background.width - 48.0f) < 0.001f);
        assert(fabsf(paint.title_background.height - 20.0f) < 0.001f);
    }
    {
        StyleFrame frame = {0};
        frame.value.fields = StyleBorderWidth;
        frame.value.border_width = -5.0f;
        paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 1, 1.0f,
                                 frame);
        assert(fabsf(paint.border_width) < 0.001f);
        frame.value.border_width = 2.0f;
        paint = FieldsetPaintFor((Rectangle){10, 50, 100, 60}, 40.0f, 1, 1.0f,
                                 frame);
        assert(fabsf(paint.border_width - 2.0f) < 0.001f);
    }

    return 0;
}
