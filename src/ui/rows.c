#include "ui_internal.h"

void
DrawUIInfoRows(InfoRowsProps rows)
{
    Color background = rows.background.a != 0
                           ? rows.background
                           : DarkenUIColor(c_bg, 6);
    Color separator = rows.separator.a != 0
                          ? rows.separator
                          : DarkenUIColor(c_bg, 30);
    Color default_text = rows.default_text.a != 0 ? rows.default_text : c_text;
    int row_h = rows.row_height > 0 ? rows.row_height : ScaleUIPx(32);
    int padding_x = rows.padding_x > 0 ? rows.padding_x : ScaleUIPx(10);

    if(rows.rows == NULL || rows.row_count <= 0 || rows.width <= 0 || row_h <= 0)
        return;

    DrawRectangle(rows.x, rows.y, rows.width, row_h * rows.row_count,
                  background);
    for(int i = 0; i < rows.row_count; i++) {
        const UIInfoRow *row = &rows.rows[i];
        int y = rows.y + i * row_h;
        int font = row->font > 0 ? row->font : GetUIFontSize();
        Color text = row->color.a != 0 ? row->color : default_text;

        if(i > 0)
            DrawLine(rows.x, y, rows.x + rows.width, y, separator);
        DrawLeftUIControlTextInRect(row->text ? row->text : "",
                                        (Rectangle){(float)(rows.x + padding_x),
                                                    (float)y,
                                                    (float)(rows.width - padding_x * 2),
                                                    (float)row_h},
                                        font, text);
    }
}

int
ui_label_text_field_height(LabelTextFieldProps row)
{
    int label_h = row.label_h > 0 ? row.label_h : ScaleUIPx(22);
    int field_h = row.field_h > 0 ? row.field_h : ScaleUIPx(40);
    int gap = row.gap > 0 ? row.gap : 0;
    int bottom_gap = row.bottom_gap > 0 ? row.bottom_gap : ScaleUIPx(24);

    return label_h + gap + field_h + bottom_gap;
}

int
GetUILabelTextFieldHeight(LabelTextFieldProps row)
{
    return ui_label_text_field_height(row);
}

int
DrawUILabelTextField(LabelTextFieldProps row, int x, int y, int w)
{
    int label_font = row.label_font > 0 ? row.label_font : GetUISmallFontSize();
    int label_h = row.label_h > 0 ? row.label_h : ScaleUIPx(22);
    int field_h = row.field_h > 0 ? row.field_h : ScaleUIPx(40);
    int gap = row.gap > 0 ? row.gap : 0;
    Color label_color = row.label_color.a != 0 ? row.label_color : DarkenUIColor(c_text, 34);
    TextFieldProps field = row.field;

    DrawUIText(row.label != NULL ? row.label : "", x, y, label_font, label_color);
    field.bounds = (Rectangle){(float)x, (float)(y + label_h + gap), (float)w, (float)field_h};
    return RenderTextField(field);
}

int
ui_section_label_height(SectionLabelProps label)
{
    return label.height > 0 ? label.height : ScaleUIPx(24);
}

int
DrawUISectionLabel(SectionLabelProps label, int x, int y)
{
    int font = label.font > 0 ? label.font : GetUISmallFontSize();
    int icon_d = label.icon_diameter > 0 ? label.icon_diameter : ScaleUIPx(18);
    Color color = label.color.a != 0 ? label.color : DarkenUIColor(c_text, 34);
    const char *text = label.label != NULL ? label.label : "";
    int label_w;

    DrawUIText(text, x, y, font, color);
    if(!label.info_button)
        return 0;
    label_w = TextWidth(text, font);
    return DrawUIInfoButton(x + label_w + ScaleUIPx(16),
                               y + font / 2 + ScaleUIPx(1), icon_d);
}

int
ui_checkbox_row_height(CheckboxRowProps row)
{
    return row.height > 0 ? row.height : ScaleUIPx(42);
}

int
DrawUICheckboxRow(CheckboxRowProps row, int x, int y)
{
    if(row.disabled)
        return DrawDisabledUICheckboxToggle(x, y, row.label, row.value, 1);
    return DrawUICheckboxToggle(x, y, row.label, row.value);
}

int
DrawUIOverlayButton(OverlayButtonProps button)
{
    Vector2 mouse;
    int mouse_inside;
    int captured;
    int active;
    int hovered;
    int font;
    int text_w;
    Color background;
    Color border;
    Color text;

    if(button.bounds.width <= 0 || button.bounds.height <= 0)
        return 0;

    mouse = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse, button.bounds);
    captured = UIInputCapturesClick(mouse);
    active = !button.disabled && !captured && mouse_inside;
    hovered = active && UIHoverEffectsEnabled();
    font = button.font > 0 ? button.font : GetUIFontSize();
    background = hovered && button.hover_background.a != 0
                     ? button.hover_background
                     : button.background;
    border = hovered && button.hover_border.a != 0
                 ? button.hover_border
                 : button.border;
    text = button.text.a != 0 ? button.text : c_text;

    if(background.a != 0)
        DrawRectangleRec(button.bounds, background);
    if(border.a != 0)
        DrawRectangleLinesEx(button.bounds, ScaleUIPx(1), border);
    if(button.label != NULL) {
        text_w = TextWidth(button.label, font);
        DrawUIText(button.label,
                        (int)(button.bounds.x + (button.bounds.width - text_w) / 2),
                        GetUIControlTextY(button.label, (int)button.bounds.y,
                                        (int)button.bounds.height, font),
                        font, text);
    }

    if(button.disabled && !captured && mouse_inside)
        MarkUIDisabled();
    if(active)
        MarkUIClickable();

    return active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int
GetUIButtonRowHeight(ButtonRowProps row)
{
    int height = row.height > 0 ? row.height : ScaleUIPx(30);
    int gap = row.gap > 0 ? row.gap : ScaleUIPx(6);
    int width = row.width;
    int row_w = 0;
    int rows = 1;
    int font = GetUISmallFontSize();

    if(row.items == NULL || row.count <= 0)
        return height;
    if(width <= 0)
        return height;

    for(int i = 0; i < row.count; i++) {
        int item_w = TextWidth(row.items[i].label != NULL ? row.items[i].label : "",
                                   font) + ScaleUIPx(20);
        int min_w = ScaleUIPx(76);
        int max_w = ScaleUIPx(144);
        int next_w;

        if(item_w < min_w)
            item_w = min_w;
        if(item_w > max_w)
            item_w = max_w;
        next_w = row_w > 0 ? row_w + gap + item_w : item_w;
        if(row_w > 0 && next_w > width) {
            rows++;
            row_w = item_w;
        } else {
            row_w = next_w;
        }
    }

    return rows * height + (rows - 1) * gap;
}

int
GetUISpinboxRowHeight(SpinboxRowProps row)
{
    return row.row_height > 0 ? row.row_height : ScaleUIPx(54);
}

UIForm
UIFormBegin(int x, int y, int width)
{
    UIForm form;

    memset(&form, 0, sizeof(form));
    form.x = x;
    form.y = y;
    form.width = width;
    form.cursor_y = y;
    form.gap = 0;
    return form;
}

int
UIFormY(const UIForm *form)
{
    return form != NULL ? form->cursor_y : 0;
}

int
UIFormAdvance(UIForm *form, int height)
{
    int y;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    if(height > 0)
        form->cursor_y += height;
    if(form->gap > 0)
        form->cursor_y += form->gap;
    return y;
}

Rectangle
UIFormTakeRect(UIForm *form, int height)
{
    Rectangle bounds = {0};

    if(form == NULL)
        return bounds;
    bounds = (Rectangle){(float)form->x, (float)form->cursor_y,
                         (float)form->width, (float)(height > 0 ? height : 0)};
    form->last_bounds = bounds;
    UIFormAdvance(form, height);
    return bounds;
}

void
UIFormNoteFocus(UIForm *form, int focus_id, Rectangle bounds)
{
    if(form == NULL || focus_id <= 0)
        return;
    if(IsUIFocusActive(focus_id)) {
        form->focused_rect = bounds;
        form->focused_rect_valid = 1;
    }
}

int
UIFormEnsureFocusedVisible(UIForm *form, UIScrollArea area, int margin)
{
    if(form == NULL || !form->focused_rect_valid)
        return 0;
    EnsureUIScrollRectVisible(area, form->focused_rect, margin);
    form->focused_rect_valid = 0;
    return 1;
}

int
UIFormSectionLabel(UIForm *form, SectionLabelProps label)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_section_label_height(label);
    UIFormTakeRect(form, height);
    return SectionLabel(label, form->x, y);
}

int
UIFormLabelTextField(UIForm *form, LabelTextFieldProps row)
{
    int y;
    int height;
    int result;
    Rectangle field_bounds;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_label_text_field_height(row);
    UIFormTakeRect(form, height);
    result = LabelTextField(row, form->x, y, form->width);

    field_bounds = row.field.bounds;
    if(field_bounds.width <= 0 || field_bounds.height <= 0) {
        int label_h = row.label_h > 0 ? row.label_h : ScaleUIPx(22);
        int field_h = row.field_h > 0 ? row.field_h : ScaleUIPx(40);
        int gap = row.gap > 0 ? row.gap : 0;
        field_bounds = (Rectangle){(float)form->x,
                                   (float)(y + label_h + gap),
                                   (float)form->width,
                                   (float)field_h};
    }
    UIFormNoteFocus(form, row.field.focus_id, field_bounds);
    return result;
}

int
UIFormCheckboxRow(UIForm *form, CheckboxRowProps row)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_checkbox_row_height(row);
    UIFormTakeRect(form, height);
    return CheckboxRow(row, form->x, y);
}

int
UIFormSpinboxRow(UIForm *form, SpinboxRowProps row)
{
    int y;
    int height;
    int label_font;
    int control_w;
    int label_w;
    Color label_color;
    SpinboxProps spinbox;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = GetUISpinboxRowHeight(row);
    UIFormTakeRect(form, height);

    label_font = row.label_font > 0 ? row.label_font : GetUIFontSize();
    control_w = row.control_width > 0 ? row.control_width : ScaleUIPx(156);
    if(control_w > form->width)
        control_w = form->width;
    label_w = row.label_width > 0
                  ? row.label_width
                  : form->width - control_w - ScaleUIPx(12);
    if(label_w < 0)
        label_w = 0;
    label_color = row.label_color.a != 0 ? row.label_color : c_text;

    DrawLeftUIControlTextInRect(row.label != NULL ? row.label : "",
                                (Rectangle){(float)form->x, (float)y,
                                            (float)label_w, (float)height},
                                label_font, label_color);
    spinbox = row.spinbox;
    if(spinbox.bounds.width <= 0)
        spinbox.bounds.width = (float)control_w;
    if(spinbox.bounds.height <= 0)
        spinbox.bounds.height = (float)(height - ScaleUIPx(14));
    spinbox.bounds.x = (float)(form->x + form->width - (int)spinbox.bounds.width);
    spinbox.bounds.y = (float)(y + (height - (int)spinbox.bounds.height) / 2);
    return Spinbox(spinbox);
}

int
UIFormButtonRow(UIForm *form, ButtonRowProps row)
{
    int height;

    if(form == NULL)
        return -1;
    row.x = form->x;
    row.y = form->cursor_y;
    row.width = form->width;
    height = GetUIButtonRowHeight(row);
    UIFormTakeRect(form, height);
    return ButtonRow(row);
}

int
UIFormSection(UIForm *form, const char *label)
{
    return UIFormSectionLabel(form, (SectionLabelProps){.label = label});
}

int
UIFormSectionEx(UIForm *form, SectionLabelProps label)
{
    return UIFormSectionLabel(form, label);
}

int
UIFormTextField(UIForm *form, const char *label, char *text,
                size_t text_size, int *cursor_position, int *focused,
                int focus_id)
{
    LabelTextFieldProps row;

    memset(&row, 0, sizeof(row));
    row.label = label;
    row.field.text = text;
    row.field.text_size = text_size;
    row.field.cursor_position = cursor_position;
    row.field.focused = focused;
    row.field.focus_id = focus_id;
    return UIFormLabelTextField(form, row);
}

int
UIFormTextFieldEx(UIForm *form, LabelTextFieldProps row)
{
    return UIFormLabelTextField(form, row);
}

int
UIFormCheckbox(UIForm *form, const char *label, int *value)
{
    return UIFormCheckboxRow(form, (CheckboxRowProps){
        .label = label,
        .value = value
    });
}

int
UIFormCheckboxEx(UIForm *form, CheckboxRowProps row)
{
    return UIFormCheckboxRow(form, row);
}

int
UIFormSpinbox(UIForm *form, const char *label, int id, int min, int max,
              int step, int *value)
{
    SpinboxRowProps row;

    memset(&row, 0, sizeof(row));
    row.label = label;
    row.spinbox.id = id;
    row.spinbox.min = min;
    row.spinbox.max = max;
    row.spinbox.step = step;
    row.spinbox.value = value;
    return UIFormSpinboxRow(form, row);
}

int
UIFormSpinboxEx(UIForm *form, SpinboxRowProps row)
{
    return UIFormSpinboxRow(form, row);
}

int
UIFormButtons(UIForm *form, const UIButtonRowItem *items, int count)
{
    return UIFormButtonRow(form, (ButtonRowProps){
        .items = items,
        .count = count
    });
}

int
UIFormButtonsEx(UIForm *form, ButtonRowProps row)
{
    return UIFormButtonRow(form, row);
}

int
DrawUIButtonRow(ButtonRowProps row)
{
    int clicked = -1;
    int gap = row.gap > 0 ? row.gap : ScaleUIPx(6);
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;
    int y = row.y;
    int font = GetUISmallFontSize();

    if(row.height <= 0)
        row.height = ScaleUIPx(30);
    if(row.items == NULL || row.count <= 0 || row.width <= 0)
        return -1;

    for(int i = 0; i <= row.count; i++) {
        int end_row = i == row.count;
        int item_w = 0;
        int next_w;

        if(!end_row) {
            item_w = TextWidth(row.items[i].label != NULL ? row.items[i].label : "",
                                   font) + ScaleUIPx(20);
            if(item_w < ScaleUIPx(76))
                item_w = ScaleUIPx(76);
            if(item_w > ScaleUIPx(144))
                item_w = ScaleUIPx(144);
        }
        next_w = row_w > 0 ? row_w + gap + item_w : item_w;

        if(!end_row && (row_w == 0 || next_w <= row.width)) {
            row_w = next_w;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            int button_w = (row.width - gap * (row_count - 1)) / row_count;
            int x = row.x + (row.width - (button_w * row_count + gap * (row_count - 1))) / 2;

            if(button_w <= 0)
                return clicked;
            for(int j = 0; j < row_count; j++) {
                int hover = 0;
                int item_index = row_start + j;

                if(RenderStyledButton(x, y, button_w, row.height,
                                          row.items[item_index].label,
                                          row.items[item_index].style,
                                          row.items[item_index].disabled,
                                          &hover))
                    clicked = item_index;
                x += button_w + gap;
            }
            y += row.height + gap;
        }

        row_start = i;
        row_w = item_w;
        row_count = end_row ? 0 : 1;
    }

    return clicked;
}
