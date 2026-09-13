#include <assert.h>

#include "runtime/rows.h"
#include "runtime/style.h"

int
main(void)
{
    StyleFrame row = {0};
    StyleFrame item = {0};
    InfoRowsMetrics info;
    LabelTextFieldMetrics field;
    SectionLabelMetrics section;
    CheckboxRowMetrics checkbox;
    ButtonRowMetrics buttons;
    SpinboxRowMetrics spinbox;

    info = InfoRowsMetricsFor(0, 0, 1.0f, row);
    assert(info.row_height == 32);
    assert(info.padding_x == 10);
    row.value.fields = StyleContentOffset | StylePaddingX;
    row.value.offset_y = 28.0f;
    row.value.padding_x = 0.0f;
    info = InfoRowsMetricsFor(0, 0, 2.0f, row);
    assert(info.row_height == 56);
    assert(info.padding_x == 0);
    info = InfoRowsMetricsFor(17, 9, 2.0f, row);
    assert(info.row_height == 17);
    assert(info.padding_x == 9);

    row = (StyleFrame){0};
    item = (StyleFrame){0};
    field = LabelTextFieldMetricsFor(0, 0, 0, 0, 1.0f, row, item);
    assert(field.label_height == 22);
    assert(field.field_height == 40);
    assert(field.gap == 0);
    assert(field.bottom_gap == 24);
    row.value.fields = StyleContentOffset | StyleGap;
    row.value.offset_y = 18.0f;
    row.value.gap = 3.0f;
    item.value.fields = StyleContentOffset | StyleGap;
    item.value.offset_y = 31.0f;
    item.value.gap = 7.0f;
    field = LabelTextFieldMetricsFor(0, 0, 0, 0, 2.0f, row, item);
    assert(field.label_height == 36);
    assert(field.field_height == 62);
    assert(field.gap == 6);
    assert(field.bottom_gap == 14);
    field = LabelTextFieldMetricsFor(11, 12, 13, 14, 2.0f, row, item);
    assert(field.label_height == 11);
    assert(field.field_height == 12);
    assert(field.gap == 13);
    assert(field.bottom_gap == 14);

    row = (StyleFrame){0};
    section = SectionLabelMetricsFor(0, 0, 1.0f, row);
    assert(section.height == 24);
    assert(section.icon_diameter == 18);
    assert(section.info_gap == 16);
    assert(section.info_y_offset == 1);
    row.value.fields = StyleContentOffset | StyleIconSize |
                       StylePaddingX | StylePaddingY;
    row.value.offset_y = 19.0f;
    row.value.icon_size = 12.0f;
    row.value.padding_x = 4.0f;
    row.value.padding_y = 0.0f;
    section = SectionLabelMetricsFor(0, 0, 2.0f, row);
    assert(section.height == 38);
    assert(section.icon_diameter == 24);
    assert(section.info_gap == 8);
    assert(section.info_y_offset == 0);

    row = (StyleFrame){0};
    checkbox = CheckboxRowMetricsFor(0, 1.0f, row);
    assert(checkbox.height == 42);
    row.value.fields = StyleContentOffset;
    row.value.offset_y = 21.0f;
    checkbox = CheckboxRowMetricsFor(0, 2.0f, row);
    assert(checkbox.height == 42);
    checkbox = CheckboxRowMetricsFor(33, 2.0f, row);
    assert(checkbox.height == 33);

    row = (StyleFrame){0};
    item = (StyleFrame){0};
    buttons = ButtonRowMetricsFor(0, 0, 1.0f, row, item);
    assert(buttons.height == 30);
    assert(buttons.gap == 6);
    assert(buttons.item_padding_x == 10);
    assert(buttons.min_width == 76);
    assert(buttons.max_width == 144);
    assert(ButtonRowItemWidth(20, buttons) == 76);
    assert(ButtonRowItemWidth(100, buttons) == 120);
    assert(ButtonRowItemWidth(200, buttons) == 144);
    row.value.fields = StyleContentOffset | StyleGap;
    row.value.offset_y = 20.0f;
    row.value.gap = 0.0f;
    item.value.fields = StylePaddingX | StyleContentOffset | StyleIconSize;
    item.value.padding_x = 2.0f;
    item.value.offset_x = 40.0f;
    item.value.icon_size = 80.0f;
    buttons = ButtonRowMetricsFor(0, 0, 2.0f, row, item);
    assert(buttons.height == 40);
    assert(buttons.gap == 0);
    assert(buttons.item_padding_x == 4);
    assert(buttons.min_width == 80);
    assert(buttons.max_width == 160);

    row = (StyleFrame){0};
    item = (StyleFrame){0};
    spinbox = SpinboxRowMetricsFor(0, 0, 1.0f, row, item);
    assert(spinbox.row_height == 54);
    assert(spinbox.control_width == 156);
    assert(spinbox.label_gap == 12);
    assert(spinbox.control_height_inset == 14);
    row.value.fields = StyleContentOffset | StyleGap;
    row.value.offset_y = 22.0f;
    row.value.gap = 5.0f;
    item.value.fields = StyleContentOffset | StylePaddingY;
    item.value.offset_x = 90.0f;
    item.value.padding_y = 3.0f;
    spinbox = SpinboxRowMetricsFor(0, 0, 2.0f, row, item);
    assert(spinbox.row_height == 44);
    assert(spinbox.control_width == 180);
    assert(spinbox.label_gap == 10);
    assert(spinbox.control_height_inset == 6);

    return 0;
}
