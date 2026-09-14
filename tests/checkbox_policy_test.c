#include <assert.h>
#include <math.h>

#include "runtime/checkbox.h"

int
main(void)
{
    CheckboxLayout layout;
    CheckboxValueResult value;
    CheckboxFlagResult flag;

    assert(CheckboxSlotSize(1.0f, (StyleFrame){0}) == 22);
    assert(CheckboxSlotSize(2.0f, (StyleFrame){0}) == 44);
    assert(CheckboxBoxSize(1.0f, (StyleFrame){0}) == 20);
    assert(CheckboxBoxSize(2.0f, (StyleFrame){0}) == 40);

    {
        StyleFrame box = {0};
        box.value.fields = StyleIconSize;
        box.value.icon_size = 30.0f;
        assert(CheckboxBoxSize(1.0f, box) == 30);
        box.value.icon_size = 0.0f;
        assert(CheckboxBoxSize(1.0f, box) == 0);
    }
    {
        StyleFrame box = {0};
        box.value.fields = StylePaddingX;
        box.value.padding_x = 0.0f;
        assert(CheckboxSlotSize(1.0f, box) == 22);
    }

    layout = CheckboxLayoutForText(10.0f, 20.0f, 50.0f, 14.0f, 1.0f,
                                   (StyleFrame){0}, (StyleFrame){0});
    assert(fabsf(layout.bounds.x - 10.0f) < 0.001f);
    assert(fabsf(layout.bounds.width - 82.0f) < 0.001f);
    assert(fabsf(layout.bounds.height - 22.0f) < 0.001f);
    assert(fabsf(layout.slot_bounds.width - 22.0f) < 0.001f);
    assert(fabsf(layout.label_x - 42.0f) < 0.001f);
    assert(fabsf(layout.label_y - 24.0f) < 0.001f);

    layout = CheckboxLayoutForText(0.0f, 0.0f, 0.0f, 40.0f, 1.0f,
                                   (StyleFrame){0}, (StyleFrame){0});
    assert(fabsf(layout.bounds.height - 40.0f) < 0.001f);

    assert(fabsf(CheckboxLabelYFor((Rectangle){0, 0, 100, 40}, 14.0f) -
                 13.0f) < 0.001f);

    value = CheckboxValueApply(0, 1, 1);
    assert(value.checked);
    assert(value.changed);
    value = CheckboxValueApply(1, 1, 1);
    assert(!value.checked);
    assert(value.changed);
    value = CheckboxValueApply(1, 1, 0);
    assert(value.checked);
    assert(!value.changed);
    value = CheckboxValueApply(1, 0, 1);
    assert(value.checked);
    assert(!value.changed);

    flag = CheckboxFlagApply(0, 4, 1);
    assert(flag.flags == 4);
    assert(flag.checked);
    assert(flag.changed);
    flag = CheckboxFlagApply(4, 4, 1);
    assert(flag.flags == 0);
    assert(!flag.checked);
    assert(flag.changed);
    flag = CheckboxFlagApply(7, 4, 0);
    assert(flag.flags == 7);
    assert(flag.checked);
    assert(!flag.changed);

    assert(CheckboxBoxRoleForTone(ButtonToneAccent) == 10);
    assert(CheckboxBoxRoleForTone(ButtonToneNeutral) == 9);
    assert(CheckboxLabelRole() == 6);

    return 0;
}
