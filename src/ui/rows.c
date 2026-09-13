#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/rows.h"

static float
rows_runtime_scale(void)
{
    return (float)Scale(1000) / 1000.0f;
}

static StyleFrame
rows_style_frame(int style_kind, int role)
{
    return ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, style_kind, role);
}

static InfoRowsMetrics
info_rows_metrics(InfoRowsProps rows)
{
    return InfoRowsMetricsFor(rows.row_height, rows.padding_x,
                              rows_runtime_scale(),
                              rows_style_frame(StyleKindText(), 0));
}

static LabelTextFieldMetrics
label_text_field_metrics(LabelTextFieldProps row)
{
    return LabelTextFieldMetricsFor(
        row.label_h, row.field_h, row.gap, row.bottom_gap,
        rows_runtime_scale(), rows_style_frame(StyleKindText(), 1),
        rows_style_frame(StyleKindTextField(), 2));
}

static SectionLabelMetrics
section_label_metrics(SectionLabelProps label)
{
    return SectionLabelMetricsFor(label.height, label.icon_diameter,
                                  rows_runtime_scale(),
                                  rows_style_frame(StyleKindText(), 3));
}

static CheckboxRowMetrics
checkbox_row_metrics(CheckboxRowProps row)
{
    return CheckboxRowMetricsFor(row.height, rows_runtime_scale(),
                                 rows_style_frame(StyleKindCheckbox(), 4));
}

static ButtonRowMetrics
button_row_metrics(ButtonRowProps row)
{
    return ButtonRowMetricsFor(row.height, row.gap, rows_runtime_scale(),
                               rows_style_frame(StyleKindButton(), 5),
                               rows_style_frame(StyleKindButton(), 6));
}

static SpinboxRowMetrics
spinbox_row_metrics(SpinboxRowProps row)
{
    return SpinboxRowMetricsFor(row.row_height, row.control_width,
                                rows_runtime_scale(),
                                rows_style_frame(StyleKindSpinbox(), 7),
                                rows_style_frame(StyleKindSpinbox(), 8));
}

void
RenderInfoRows(InfoRowsProps rows)
{
    Style surface_style = ui_surface_style();
    Style separator_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                         ButtonStateNormal,
                                                         StyleKindSeparator());
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    Color background = rows.background.a != 0
                           ? rows.background
                           : surface_style.background;
    Color separator = rows.separator.a != 0
                          ? rows.separator
                          : separator_style.background;
    Color default_text = rows.default_text.a != 0
                             ? rows.default_text
                             : text_style.foreground;
    InfoRowsMetrics metrics = info_rows_metrics(rows);
    int row_h = metrics.row_height;
    int padding_x = metrics.padding_x;
    int default_font = text_style.font_size > 0.0f
                           ? (int)(text_style.font_size + 0.5f)
                           : GetFontSize();
    int font_token;

    if(rows.rows == NULL || rows.row_count <= 0 || rows.width <= 0 || row_h <= 0)
        return;

    font_token = PushTextFont(text_style.typeface);
    DrawRectangle(rows.x, rows.y, rows.width, row_h * rows.row_count,
                  background);
    for(int i = 0; i < rows.row_count; i++) {
        const UIInfoRow *row = &rows.rows[i];
        int y = rows.y + i * row_h;
        int font = row->font > 0 ? row->font : default_font;
        Color text = row->color.a != 0 ? row->color : default_text;

        if(i > 0)
            DrawLine(rows.x, y, rows.x + rows.width, y, separator);
        DrawLeftControlTextInRect(row->text ? row->text : "",
                                        (Rectangle){(float)(rows.x + padding_x),
                                                    (float)y,
                                                    (float)(rows.width - padding_x * 2),
                                                    (float)row_h},
                                        font, text);
    }
    PopTextFont(font_token);
}

int
ui_label_text_field_height(LabelTextFieldProps row)
{
    LabelTextFieldMetrics metrics = label_text_field_metrics(row);

    return metrics.label_height + metrics.gap + metrics.field_height +
           metrics.bottom_gap;
}

int
GetLabelTextFieldHeight(LabelTextFieldProps row)
{
    return ui_label_text_field_height(row);
}

int
RenderLabelTextField(LabelTextFieldProps row, int x, int y, int w)
{
    LabelTextFieldMetrics metrics = label_text_field_metrics(row);
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    int label_font = row.label_font > 0 ? row.label_font
                     : text_style.font_size > 0.0f
                         ? (int)(text_style.font_size + 0.5f)
                         : GetSmallFontSize();
    Color label_color = row.label_color.a != 0 ? row.label_color
                                               : text_style.foreground;
    int font_token;
    if(row.label_color.a == 0)
        label_color.a = (unsigned char)(label_color.a * 0.72f);
    TextFieldProps field = row.field;

    font_token = PushTextFont(text_style.typeface);
    RenderText(row.label != NULL ? row.label : "", x, y, label_font, label_color);
    PopTextFont(font_token);
    field.bounds = (Rectangle){(float)x,
                               (float)(y + metrics.label_height + metrics.gap),
                               (float)w, (float)metrics.field_height};
    return ui_text_field_render(field);
}

int
ui_section_label_height(SectionLabelProps label)
{
    return section_label_metrics(label).height;
}

int
RenderSectionLabel(SectionLabelProps label, int x, int y)
{
    SectionLabelMetrics metrics = section_label_metrics(label);
    int icon_d = metrics.icon_diameter;
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    int font = label.font > 0 ? label.font
               : text_style.font_size > 0.0f
                   ? (int)(text_style.font_size + 0.5f)
                   : GetSmallFontSize();
    Color color = label.color.a != 0 ? label.color : text_style.foreground;
    int font_token;
    if(label.color.a == 0)
        color.a = (unsigned char)(color.a * 0.72f);
    const char *text = label.label != NULL ? label.label : "";
    int label_w;

    font_token = PushTextFont(text_style.typeface);
    RenderText(text, x, y, font, color);
    if(!label.info) {
        PopTextFont(font_token);
        return 0;
    }
    label_w = TextWidth(text, font);
    PopTextFont(font_token);
    return RenderButtonInfoIndicator(x + label_w + metrics.info_gap,
                               y + font / 2 + metrics.info_y_offset, icon_d);
}

int
ui_checkbox_row_height(CheckboxRowProps row)
{
    return checkbox_row_metrics(row).height;
}

int
RenderCheckboxRow(CheckboxRowProps row, int x, int y)
{
    if(row.disabled)
        return DrawDisabledUICheckboxToggle(x, y, row.label, row.value, 1);
    return RenderCheckboxToggle(x, y, row.label, row.value);
}

int
GetButtonRowHeight(ButtonRowProps row)
{
    ButtonRowMetrics metrics = button_row_metrics(row);
    int height = metrics.height;
    int gap = metrics.gap;
    int width = row.width;
    int row_w = 0;
    int rows = 1;
    int font = GetSmallFontSize();

    if(row.items == NULL || row.count <= 0)
        return height;
    if(width <= 0)
        return height;

    for(int i = 0; i < row.count; i++) {
        int item_w = ButtonRowItemWidth(
            TextWidth(row.items[i].label != NULL ? row.items[i].label : "",
                      font),
            metrics);
        int next_w;
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
GetSpinboxRowHeight(SpinboxRowProps row)
{
    return spinbox_row_metrics(row).row_height;
}

Form
FormBegin(int x, int y, int width)
{
    Form form;

    memset(&form, 0, sizeof(form));
    form.x = x;
    form.y = y;
    form.width = width;
    form.cursor_y = y;
    form.gap = 0;
    return form;
}

int
FormY(const Form *form)
{
    return form != NULL ? form->cursor_y : 0;
}

int
FormAdvance(Form *form, int height)
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
FormTakeRect(Form *form, int height)
{
    Rectangle bounds = {0};

    if(form == NULL)
        return bounds;
    bounds = (Rectangle){(float)form->x, (float)form->cursor_y,
                         (float)form->width, (float)(height > 0 ? height : 0)};
    form->last_bounds = bounds;
    FormAdvance(form, height);
    return bounds;
}

void
FormNoteFocus(Form *form, int focus_id, Rectangle bounds)
{
    if(form == NULL || focus_id <= 0)
        return;
    if(IsFocusActive(focus_id)) {
        form->focused_rect = bounds;
        form->focused_rect_valid = 1;
    }
}

int
FormEnsureFocusedVisible(Form *form, ScrollArea area, int margin)
{
    if(form == NULL || !form->focused_rect_valid)
        return 0;
    EnsureScrollRectVisible(area, form->focused_rect, margin);
    form->focused_rect_valid = 0;
    return 1;
}

int
FormSection(Form *form, SectionLabelProps label)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_section_label_height(label);
    FormTakeRect(form, height);
    return RenderSectionLabel(label, form->x, y);
}

int
FormTextField(Form *form, LabelTextFieldProps row)
{
    int y;
    int height;
    int result;
    Rectangle field_bounds;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_label_text_field_height(row);
    FormTakeRect(form, height);
    result = RenderLabelTextField(row, form->x, y, form->width);

    field_bounds = row.field.bounds;
    if(field_bounds.width <= 0 || field_bounds.height <= 0) {
        LabelTextFieldMetrics metrics = label_text_field_metrics(row);
        field_bounds = (Rectangle){(float)form->x,
                                   (float)(y + metrics.label_height +
                                           metrics.gap),
                                   (float)form->width,
                                   (float)metrics.field_height};
    }
    FormNoteFocus(form, row.field.focus_id, field_bounds);
    return result;
}

int
FormCheckbox(Form *form, CheckboxRowProps row)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_checkbox_row_height(row);
    FormTakeRect(form, height);
    return RenderCheckboxRow(row, form->x, y);
}

int
FormSpinbox(Form *form, SpinboxRowProps row)
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
    SpinboxRowMetrics metrics = spinbox_row_metrics(row);
    height = metrics.row_height;
    FormTakeRect(form, height);

    label_font = row.label_font > 0 ? row.label_font : GetFontSize();
    control_w = metrics.control_width;
    if(control_w > form->width)
        control_w = form->width;
    label_w = row.label_width > 0
                  ? row.label_width
                  : form->width - control_w - metrics.label_gap;
    if(label_w < 0)
        label_w = 0;
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    label_color = row.label_color.a != 0 ? row.label_color
                                         : text_style.foreground;

    DrawLeftControlTextInRect(row.label != NULL ? row.label : "",
                                (Rectangle){(float)form->x, (float)y,
                                            (float)label_w, (float)height},
                                label_font, label_color);
    spinbox = row.spinbox;
    if(spinbox.bounds.width <= 0)
        spinbox.bounds.width = (float)control_w;
    if(spinbox.bounds.height <= 0)
        spinbox.bounds.height = (float)(height - metrics.control_height_inset);
    spinbox.bounds.x = (float)(form->x + form->width - (int)spinbox.bounds.width);
    spinbox.bounds.y = (float)(y + (height - (int)spinbox.bounds.height) / 2);
    return Spinbox(spinbox);
}

int
FormButtons(Form *form, ButtonRowProps row)
{
    int height;

    if(form == NULL)
        return -1;
    row.x = form->x;
    row.y = form->cursor_y;
    row.width = form->width;
    height = GetButtonRowHeight(row);
    FormTakeRect(form, height);
    return RenderButtonRow(row);
}

int
RenderButtonRow(ButtonRowProps row)
{
    int clicked = -1;
    ButtonRowMetrics metrics = button_row_metrics(row);
    int gap = metrics.gap;
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;
    int y = row.y;
    int font = GetSmallFontSize();

    if(row.height <= 0)
        row.height = metrics.height;
    if(row.items == NULL || row.count <= 0 || row.width <= 0)
        return -1;

    for(int i = 0; i <= row.count; i++) {
        int end_row = i == row.count;
        int item_w = 0;
        int next_w;

        if(!end_row) {
            item_w = ButtonRowItemWidth(
                TextWidth(row.items[i].label != NULL ? row.items[i].label : "",
                          font),
                metrics);
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
                int item_index = row_start + j;
                if(Button((ButtonProps){
                       .bounds = {(float)x, (float)y,
                                  (float)button_w, (float)row.height},
                       .label = row.items[item_index].label,
                       .tone = row.items[item_index].tone,
                       .emphasis = row.items[item_index].emphasis,
                       .disabled = row.items[item_index].disabled
                   }))
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
