#include <assert.h>
#include <math.h>

#include "runtime/input.h"

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
    InputCellLayout layout;
    InputContinuousStep continuous;
    InputDiscreteStep discrete;
    InputPointerInteraction pointer;
    InputStep precise;

    assert(InputDefaultStepButtonWidth(1.0f) == 24);
    assert(InputDoubleClickSlopFor(2.0f) == 12);
    assert(InputPointerDragThresholdFor(2.0f) == 10);
    assert(!InputPointerDragShouldStart(5, 0, 5));
    assert(InputPointerDragShouldStart(6, 0, 5));
    assert(InputPointerDragShouldStart(0, -6, 5));
    assert(InputPointerDragShouldStart(0, -1, -1));
    assert(InputPointerDragIsHorizontal(7, -6));
    assert(InputPointerDragIsHorizontal(-6, 6));
    assert(!InputPointerDragIsHorizontal(5, -6));
    pointer = InputPointerInteractionFor(true, false, false, true, true,
                                         false, true);
    assert(pointer.active);
    assert(pointer.hovered);
    assert(!pointer.disabled_marker);
    assert(pointer.activated);
    assert(pointer.consume_release);
    pointer = InputPointerInteractionFor(true, false, true, true, true,
                                         false, true);
    assert(!pointer.active);
    assert(!pointer.hovered);
    assert(pointer.disabled_marker);
    assert(!pointer.activated);
    assert(!pointer.consume_release);
    pointer = InputPointerInteractionFor(true, true, false, true, true,
                                         false, true);
    assert(!pointer.active);
    assert(!pointer.disabled_marker);
    assert(!pointer.activated);
    pointer = InputPointerInteractionFor(true, false, false, true, true,
                                         true, true);
    assert(pointer.active);
    assert(!pointer.activated);
    assert(!pointer.consume_release);
    pointer = InputPointerInteractionFor(true, false, false, true, true,
                                         false, false);
    assert(pointer.active);
    assert(!pointer.activated);

    assert(InputTempEditActivationFor(1, 1, 0, 0, 0, 99.0f,
                                      99.0f, 99.0f, 6));
    assert(InputTempEditActivationFor(1, 0, 1, 1, 1, 0.20f,
                                      4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(0, 1, 1, 1, 1, 0.20f,
                                       4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(1, 0, 0, 1, 1, 0.20f,
                                       4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(1, 0, 1, 0, 1, 0.20f,
                                       4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(1, 0, 1, 1, 0, 0.20f,
                                       4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(1, 0, 1, 1, 1, 0.40f,
                                       4.0f, -5.0f, 6));
    assert(!InputTempEditActivationFor(1, 0, 1, 1, 1, 0.20f,
                                       7.0f, -5.0f, 6));

    layout = InputCellLayoutFor((Rectangle){10, 20, 120, 30}, 3, 1, 16, 1);
    check_rect(layout.cell, 50, 20, 40, 30);
    check_rect(layout.field, 50, 20, 8, 30);
    check_rect(layout.minus, 58, 20, 16, 30);
    check_rect(layout.plus, 74, 20, 16, 30);
    assert(layout.has_step_buttons);

    assert(InputKindIsFloat(NumericFloat));
    assert(InputKindIsInt(NumericInt));
    assert(InputKindIsDouble(NumericDouble));
    assert(InputDefaultFormat(NumericFloat)[1] == '.');
    assert(InputRoundValueForKind(NumericInt, 2.6) == 3.0);
    assert(InputRoundValueForKind(NumericInt, -2.6) == -3.0);

    continuous = InputContinuousStepValue(1.0f, 0.5f, 2.0f, 1, 1);
    assert(continuous.changed);
    assert(fabsf(continuous.value - 3.0f) < 0.001f);
    discrete = InputDiscreteStepValue(4, 1, 10, -1, 1);
    assert(discrete.changed);
    assert(discrete.value == -6);
    precise = InputStepValueForKind(NumericDouble, 2.0, 0.25, 1.0, 1, 0);
    assert(precise.changed);
    assert(fabs(precise.value - 2.25) < 0.001);

    return 0;
}
