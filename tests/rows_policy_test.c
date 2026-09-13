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
    LabelTextFieldLayout field_layout;
    SectionLabelMetrics section;
    CheckboxRowMetrics checkbox;
    ButtonRowMetrics buttons;
    ButtonRowPlacement button_placement;
    ButtonRowWrapDecision button_wrap;
    SpinboxRowMetrics spinbox;
    FormRectResult form_rect;
    SpinboxRowLayout spinbox_layout;
    Rectangle requested;

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
    requested = (Rectangle){0};
    field_layout = LabelTextFieldLayoutFor(8, 20, 160, requested, field);
    assert((int)field_layout.label_bounds.x == 8);
    assert((int)field_layout.label_bounds.y == 20);
    assert((int)field_layout.label_bounds.width == 160);
    assert((int)field_layout.label_bounds.height == 11);
    assert((int)field_layout.field_bounds.x == 8);
    assert((int)field_layout.field_bounds.y == 44);
    assert((int)field_layout.field_bounds.width == 160);
    assert((int)field_layout.field_bounds.height == 12);
    requested = (Rectangle){30, 40, 90, 28};
    field_layout = LabelTextFieldLayoutFor(8, 20, 160, requested, field);
    assert((int)field_layout.field_bounds.x == 30);
    assert((int)field_layout.field_bounds.y == 40);
    assert((int)field_layout.field_bounds.width == 90);
    assert((int)field_layout.field_bounds.height == 28);

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
    button_wrap = ButtonRowWrapFor(0, 76, 160, buttons.gap);
    assert(button_wrap.wraps == 0);
    assert(button_wrap.row_width == 76);
    button_wrap = ButtonRowWrapFor(76, 76, 160, buttons.gap);
    assert(button_wrap.wraps == 0);
    assert(button_wrap.row_width == 158);
    button_wrap = ButtonRowWrapFor(158, 76, 160, buttons.gap);
    assert(button_wrap.wraps == 1);
    assert(button_wrap.row_width == 76);
    button_wrap = ButtonRowWrapFor(20, 30, 40, -4);
    assert(button_wrap.wraps == 1);
    assert(button_wrap.row_width == 30);
    button_placement = ButtonRowPlacementFor(10, 320, 3, 8);
    assert(button_placement.button_width == 101);
    assert(button_placement.total_width == 319);
    assert(button_placement.start_x == 10);
    button_placement = ButtonRowPlacementFor(10, 322, 3, 8);
    assert(button_placement.button_width == 102);
    assert(button_placement.total_width == 322);
    assert(button_placement.start_x == 10);
    button_placement = ButtonRowPlacementFor(10, 5, 3, 8);
    assert(button_placement.button_width == 0);
    assert(button_placement.start_x == 5);
    button_placement = ButtonRowPlacementFor(10, 100, 0, 8);
    assert(button_placement.button_width == 0);
    assert(button_placement.total_width == 0);
    assert(button_placement.start_x == 10);
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

    assert(FormAdvanceY(40, 0, 8) == 48);
    assert(FormAdvanceY(40, -10, 8) == 48);
    assert(FormAdvanceY(40, 30, 8) == 78);
    assert(FormAdvanceY(40, 30, -8) == 70);

    form_rect = FormRectFor(12, 40, 180, 30, 8);
    assert((int)form_rect.bounds.x == 12);
    assert((int)form_rect.bounds.y == 40);
    assert((int)form_rect.bounds.width == 180);
    assert((int)form_rect.bounds.height == 30);
    assert(form_rect.next_cursor_y == 78);
    form_rect = FormRectFor(12, 40, 180, -30, 8);
    assert((int)form_rect.bounds.height == 0);
    assert(form_rect.next_cursor_y == 48);

    spinbox = (SpinboxRowMetrics){.row_height = 54, .control_width = 156,
                                  .label_gap = 12,
                                  .control_height_inset = 14};
    requested = (Rectangle){0};
    spinbox_layout = SpinboxRowLayoutFor(10, 20, 240, 0, requested, spinbox);
    assert((int)spinbox_layout.label_bounds.x == 10);
    assert((int)spinbox_layout.label_bounds.y == 20);
    assert((int)spinbox_layout.label_bounds.width == 72);
    assert((int)spinbox_layout.label_bounds.height == 54);
    assert((int)spinbox_layout.spinbox_bounds.x == 94);
    assert((int)spinbox_layout.spinbox_bounds.y == 27);
    assert((int)spinbox_layout.spinbox_bounds.width == 156);
    assert((int)spinbox_layout.spinbox_bounds.height == 40);
    assert(spinbox_layout.control_width == 156);
    assert(spinbox_layout.label_width == 72);

    requested = (Rectangle){0, 0, 80, 20};
    spinbox_layout = SpinboxRowLayoutFor(10, 20, 120, 30, requested, spinbox);
    assert((int)spinbox_layout.label_bounds.width == 30);
    assert((int)spinbox_layout.spinbox_bounds.x == 50);
    assert((int)spinbox_layout.spinbox_bounds.y == 37);
    assert((int)spinbox_layout.spinbox_bounds.width == 80);
    assert((int)spinbox_layout.spinbox_bounds.height == 20);
    assert(spinbox_layout.control_width == 120);

    return 0;
}
