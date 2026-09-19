#include <assert.h>
#include <math.h>

#include "runtime/slider.h"

int
main(void)
{
    SliderStep step;
    SliderDiscreteStep discrete;

    assert(fabsf(SliderClampRatio(-1.0f)) < 0.001f);
    assert(fabsf(SliderClampRatio(0.25f) - 0.25f) < 0.001f);
    assert(fabsf(SliderClampRatio(2.0f) - 1.0f) < 0.001f);

    assert(fabsf(SliderPointerRatio(50.0f, 0.0f, 100.0f, 0) - 0.5f) < 0.001f);
    assert(fabsf(SliderPointerRatio(50.0f, 0.0f, 100.0f, 1) - 0.5f) < 0.001f);
    assert(fabsf(SliderPointerRatio(25.0f, 0.0f, 100.0f, 1) - 0.75f) < 0.001f);
    assert(fabsf(SliderPointerRatio(-5.0f, 0.0f, 100.0f, 0)) < 0.001f);
    assert(fabsf(SliderPointerRatio(200.0f, 0.0f, 100.0f, 0) - 1.0f) < 0.001f);
    assert(fabsf(SliderPointerRatio(50.0f, 0.0f, 0.0f, 0)) < 0.001f);

    assert(fabsf(SliderRatio(5.0f, 0.0f, 10.0f) - 0.5f) < 0.001f);
    assert(fabsf(SliderRatio(-5.0f, 0.0f, 10.0f)) < 0.001f);
    assert(fabsf(SliderRatio(15.0f, 0.0f, 10.0f) - 1.0f) < 0.001f);
    assert(fabsf(SliderRatio(5.0f, 5.0f, 5.0f)) < 0.001f);

    assert(fabsf(SliderValue(0.0f, 10.0f, 0.5f) - 5.0f) < 0.001f);
    assert(fabsf(SliderValue(0.0f, 10.0f, -1.0f)) < 0.001f);
    assert(fabsf(SliderValue(0.0f, 10.0f, 2.0f) - 10.0f) < 0.001f);
    assert(fabsf(SliderValue(5.0f, 5.0f, 0.5f) - 5.0f) < 0.001f);

    assert(SliderDiscreteValue(0, 10, 0.5f) == 5);
    assert(SliderDiscreteValue(0, 10, -1.0f) == 0);
    assert(SliderDiscreteValue(0, 10, 2.0f) == 10);
    assert(SliderDiscreteValue(4, 4, 0.5f) == 4);
    assert(fabsf(SliderDiscreteRatio(5, 0, 10) - 0.5f) < 0.001f);
    assert(fabsf(SliderDiscreteRatio(-3, 0, 10)) < 0.001f);

    assert(SliderDiscretePointerValue(75, 0, 100, 0, 10, 0) == 8);
    assert(SliderDiscretePointerValue(25, 0, 100, 0, 10, 1) == 8);

    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 1, 0, 0, 0, 0);
    assert(step.changed);
    assert(fabsf(step.value - 5.1f) < 0.001f);
    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 0, 1, 0, 0, 0);
    assert(step.changed);
    assert(fabsf(step.value) < 0.001f);
    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 0, 0, 1, 0, 0);
    assert(step.changed);
    assert(fabsf(step.value - 10.0f) < 0.001f);
    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 0, 0, 0, 0, 0);
    assert(!step.changed);
    assert(fabsf(step.value - 5.0f) < 0.001f);
    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 1, 0, 0, 1, 0);
    assert(fabsf(step.value - 5.01f) < 0.001f);
    step = SliderKeyboardValue(5.0f, 0.0f, 10.0f, 1, 0, 0, 0, 1);
    assert(fabsf(step.value - 6.0f) < 0.001f);

    discrete = SliderDiscreteKeyboardValue(5, 0, 10, 1, 0, 0, 0, 0);
    assert(discrete.changed);
    assert(discrete.value == 6);
    discrete = SliderDiscreteKeyboardValue(5, 0, 10, 0, 1, 0, 0, 0);
    assert(discrete.changed);
    assert(discrete.value == 0);
    discrete = SliderDiscreteKeyboardValue(5, 0, 10, 0, 0, 0, 0, 0);
    assert(!discrete.changed);
    assert(discrete.value == 5);

    assert(SliderKeyboardDirectionFor(1, 1, 0, 0, 0) == 1);
    assert(SliderKeyboardDirectionFor(1, 0, 1, 0, 0) == -1);
    assert(SliderKeyboardDirectionFor(0, 0, 0, 1, 0) == 1);
    assert(SliderKeyboardDirectionFor(0, 0, 0, 0, 1) == -1);
    assert(SliderKeyboardDirectionFor(1, 0, 0, 1, 1) == 0);
    assert(SliderKeyboardShouldRun(1, 1, 0));
    assert(!SliderKeyboardShouldRun(0, 1, 0));
    assert(!SliderKeyboardShouldRun(1, 0, 0));
    assert(!SliderKeyboardShouldRun(1, 1, 1));

    SliderLayout layout = SliderLayoutFor((Rectangle){20, 30, 360, 0},
        20, 20, 48, 1, false, false, true, true);
    assert(layout.label.x == 20 && layout.label.y == 30);
    assert(layout.value.x + layout.value.width == 380);
    assert(layout.track.y >= layout.label.y + layout.label.height + 12);
    assert(layout.decrement.width == 48 && layout.increment.height == 48);
    assert(layout.track.x > layout.decrement.x + layout.decrement.width);
    assert(layout.limits.y >= layout.track.y + layout.track.height);
    assert(layout.bounds.height >= 104);
    SliderLayout narrow = SliderLayoutFor((Rectangle){20, 30, 160, 0},
        20, 60, 48, 1, false, false, true, true);
    assert(narrow.track.y >= narrow.decrement.y + narrow.decrement.height);
    assert(narrow.bounds.height > layout.bounds.height);
    SliderLayout again = SliderLayoutFor(narrow.bounds, 20, 60, 48, 1,
        false, false, true, true);
    assert(again.bounds.height == narrow.bounds.height);
    assert(again.track.y == narrow.track.y);
    assert(again.track.height == narrow.track.height);

    return 0;
}
