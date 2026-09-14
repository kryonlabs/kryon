#include <assert.h>

#include "runtime/spinbox.h"

int
main(void)
{
    SpinboxStepResult step;
    SpinboxLayout layout;

    assert(SpinboxEffectiveStep(0) == 1);
    assert(SpinboxEffectiveStep(-3) == 1);
    assert(SpinboxEffectiveStep(4) == 4);
    assert(SpinboxClampValue(-2, 0, 9) == 0);
    assert(SpinboxClampValue(12, 0, 9) == 9);
    assert(SpinboxClampValue(4, 0, 9) == 4);

    layout = SpinboxLayoutFor((Rectangle){10, 20, 90, 30}, 28);
    assert((int)layout.left.x == 10 && (int)layout.left.width == 28);
    assert((int)layout.text.x == 38 && (int)layout.text.width == 34);
    assert((int)layout.right.x == 72 && (int)layout.right.width == 28);

    layout = SpinboxLayoutFor((Rectangle){10, 20, 40, 30}, 28);
    assert(layout.button_width == 20);
    assert((int)layout.text.width == 0);

    step = SpinboxStepValue(4, 0, 9, 2, -1, false);
    assert(step.value == 2 && step.changed);
    step = SpinboxStepValue(8, 0, 9, 2, 1, false);
    assert(step.value == 9 && step.changed);
    step = SpinboxStepValue(0, 0, 9, 2, -1, true);
    assert(step.value == 9 && step.changed);
    step = SpinboxStepValue(9, 0, 9, 2, 1, true);
    assert(step.value == 0 && step.changed);

    step = SpinboxStepButtonsValue(5, 0, 9, 2, true, false, false);
    assert(step.value == 3 && step.changed);
    step = SpinboxStepButtonsValue(5, 0, 9, 2, false, true, false);
    assert(step.value == 7 && step.changed);
    step = SpinboxStepButtonsValue(5, 0, 9, 2, true, true, false);
    assert(step.value == 5 && step.changed);
    step = SpinboxStepButtonsValue(5, 0, 9, 2, false, false, false);
    assert(step.value == 5 && !step.changed);

    return 0;
}
