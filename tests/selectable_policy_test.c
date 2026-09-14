#include <assert.h>
#include <math.h>
#include <string.h>

#include "runtime/selectable.h"

int
main(void)
{
    SelectableSpec spec;
    SelectablePaint paint;
    SelectableToggleResult toggle;

    assert(fabsf(SelectableLabelInset(1.0f, (StyleFrame){0}) - 8.0f) <
           0.001f);
    assert(fabsf(SelectableLabelInset(2.0f, (StyleFrame){0}) - 16.0f) <
           0.001f);
    {
        StyleFrame face = {0};
        face.value.fields = StylePaddingX;
        face.value.padding_x = 12.0f;
        assert(fabsf(SelectableLabelInset(1.0f, face) - 12.0f) < 0.001f);
        face.value.padding_x = 0.0f;
        assert(fabsf(SelectableLabelInset(1.0f, face)) < 0.001f);
    }

    memset(&spec, 0, sizeof(spec));
    spec.bounds = (Rectangle){10, 20, 100, 30};
    spec.label_inset = 8.0f;
    paint = SelectablePaintFor(spec);
    assert(!paint.draw_fill);
    assert(fabsf(paint.label_x - 18.0f) < 0.001f);
    assert(fabsf(paint.bounds.x - 10.0f) < 0.001f);

    spec.selected = 1;
    paint = SelectablePaintFor(spec);
    assert(paint.draw_fill);

    spec.selected = 0;
    spec.hovered = 1;
    paint = SelectablePaintFor(spec);
    assert(paint.draw_fill);

    spec.hovered = 0;
    spec.pressed = 1;
    paint = SelectablePaintFor(spec);
    assert(paint.draw_fill);

    spec.pressed = 0;
    spec.disabled = 1;
    spec.hovered = 1;
    paint = SelectablePaintFor(spec);
    assert(!paint.draw_fill);

    toggle = SelectableToggleFor(0, 1, 1);
    assert(toggle.selected);
    assert(toggle.changed);
    toggle = SelectableToggleFor(1, 1, 1);
    assert(!toggle.selected);
    assert(toggle.changed);
    toggle = SelectableToggleFor(1, 1, 0);
    assert(toggle.selected);
    assert(!toggle.changed);
    toggle = SelectableToggleFor(1, 0, 1);
    assert(toggle.selected);
    assert(!toggle.changed);

    return 0;
}
