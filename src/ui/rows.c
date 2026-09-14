#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/style.h"
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
                              rows_style_frame(StyleKindText(),
                                               InfoRowsTextRole()));
}

static LabelTextFieldMetrics
label_text_field_metrics(LabelTextFieldProps row)
{
    return LabelTextFieldMetricsFor(
        row.label_h, row.field_h, row.gap, row.bottom_gap,
        rows_runtime_scale(), rows_style_frame(StyleKindText(),
                                               LabelTextFieldLabelRole()),
        rows_style_frame(StyleKindTextField(), LabelTextFieldFieldRole()));
}

static SectionLabelMetrics
section_label_metrics(SectionLabelProps label)
{
    return SectionLabelMetricsFor(label.height, label.icon_diameter,
                                  rows_runtime_scale(),
                                  rows_style_frame(StyleKindText(),
                                                   SectionLabelRole()));
}

static CheckboxRowMetrics
checkbox_row_metrics(CheckboxRowProps row)
{
    return CheckboxRowMetricsFor(row.height, rows_runtime_scale(),
                                 rows_style_frame(StyleKindCheckbox(),
                                                  CheckboxRowRole()));
}

static ButtonRowMetrics
button_row_metrics(ButtonRowProps row)
{
    return ButtonRowMetricsFor(row.height, row.gap, rows_runtime_scale(),
                               rows_style_frame(StyleKindButton(),
                                                ButtonRowPrimaryRole()),
                               rows_style_frame(StyleKindButton(),
                                                ButtonRowSecondaryRole()));
}

static SpinboxRowMetrics
spinbox_row_metrics(SpinboxRowProps row)
{
    return SpinboxRowMetricsFor(row.row_height, row.control_width,
                                rows_runtime_scale(),
                                rows_style_frame(StyleKindSpinbox(),
                                                 SpinboxRowLabelRole()),
                                rows_style_frame(StyleKindSpinbox(),
                                                 SpinboxRowControlRole()));
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
    InfoRowsLayout layout;
    int row_h = metrics.row_height;
    int default_font = ResolveFont(0, StyleFontValue(text_style.fields,
                                                     text_style.font_size),
                                   GetFontSize());
    int font_token;

    if(rows.rows == NULL || rows.row_count <= 0 || rows.width <= 0 || row_h <= 0)
        return;

    layout = InfoRowsLayoutFor(rows.x, rows.y, rows.width, rows.row_count,
                               metrics);
    font_token = PushTextFont(text_style.typeface);
    DrawRectangle((int)layout.background.x, (int)layout.background.y,
                  (int)layout.background.width,
                  (int)layout.background.height, background);
    for(int i = 0; i < rows.row_count; i++) {
        const InfoRow *row = &rows.rows[i];
        InfoRowLayout row_layout =
            InfoRowLayoutFor(rows.x, rows.y, rows.width, i, metrics);
        int font = ResolveFont(row->font, 0, default_font);
        Color text = row->color.a != 0 ? row->color : default_text;

        if(i > 0)
            DrawLine(rows.x, row_layout.separator_y, rows.x + rows.width,
                     row_layout.separator_y, separator);
        RenderControlTextInRect(row->text ? row->text : "",
                                row_layout.text_bounds, font, text);
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
    LabelTextFieldLayout layout;
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    int label_font = ResolveFont(
        row.label_font, StyleFontValue(text_style.fields, text_style.font_size),
        GetSmallFontSize());
    Color label_color = row.label_color.a != 0 ? row.label_color
                                               : text_style.foreground;
    int font_token;
    if(row.label_color.a == 0)
        label_color.a = RowsMutedAlpha(label_color.a);
    TextFieldProps field = row.field;

    layout = LabelTextFieldLayoutFor(x, y, w, (Rectangle){0}, metrics);
    font_token = PushTextFont(text_style.typeface);
    RenderText(row.label != NULL ? row.label : "", (int)layout.label_bounds.x,
               (int)layout.label_bounds.y, label_font, label_color);
    PopTextFont(font_token);
    field.bounds = layout.field_bounds;
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
    int font = ResolveFont(label.font,
                           StyleFontValue(text_style.fields,
                                          text_style.font_size),
                           GetSmallFontSize());
    Color color = label.color.a != 0 ? label.color : text_style.foreground;
    int font_token;
    if(label.color.a == 0)
        color.a = RowsMutedAlpha(color.a);
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
        return DrawDisabledCheckboxToggle(x, y, row.label, row.value, 1);
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
        ButtonRowWrapDecision wrap =
            ButtonRowWrapFor(row_w, item_w, width, gap);
        if(wrap.wraps)
            rows++;
        row_w = wrap.row_width;
    }

    return ButtonRowTotalHeight(rows, height, gap);
}

int
GetSpinboxRowHeight(SpinboxRowProps row)
{
    return spinbox_row_metrics(row).row_height;
}

RowForm
RowFormBegin(int x, int y, int width)
{
    RowForm form;

    memset(&form, 0, sizeof(form));
    form.x = x;
    form.y = y;
    form.width = width;
    form.cursor_y = y;
    form.gap = 0;
    return form;
}

int
RowFormY(const RowForm *form)
{
    return form != NULL ? form->cursor_y : 0;
}

int
RowFormAdvance(RowForm *form, int height)
{
    int y;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    form->cursor_y = FormAdvanceY(form->cursor_y, height, form->gap);
    return y;
}

Rectangle
RowFormTakeRect(RowForm *form, int height)
{
    FormRectResult result;

    if(form == NULL)
        return (Rectangle){0};
    result = FormRectFor(form->x, form->cursor_y, form->width, height,
                         form->gap);
    form->last_bounds = result.bounds;
    form->cursor_y = result.next_cursor_y;
    return result.bounds;
}

void
RowFormNoteFocus(RowForm *form, int focus_id, Rectangle bounds)
{
    if(form == NULL || focus_id <= 0)
        return;
    if(IsFocusActive(focus_id)) {
        form->focused_rect = bounds;
        form->focused_rect_valid = 1;
    }
}

int
RowFormEnsureFocusedVisible(RowForm *form, ScrollArea area, int margin)
{
    if(form == NULL || !form->focused_rect_valid)
        return 0;
    EnsureScrollRectVisible(area, form->focused_rect, margin);
    form->focused_rect_valid = 0;
    return 1;
}

int
RowFormSection(RowForm *form, SectionLabelProps label)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_section_label_height(label);
    RowFormTakeRect(form, height);
    return RenderSectionLabel(label, form->x, y);
}

int
RowFormTextField(RowForm *form, LabelTextFieldProps row)
{
    int y;
    int height;
    int result;
    Rectangle field_bounds;
    LabelTextFieldMetrics metrics;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_label_text_field_height(row);
    RowFormTakeRect(form, height);
    result = RenderLabelTextField(row, form->x, y, form->width);

    metrics = label_text_field_metrics(row);
    field_bounds = LabelTextFieldLayoutFor(form->x, y, form->width,
                                           row.field.bounds, metrics)
                       .field_bounds;
    RowFormNoteFocus(form, row.field.focus_id, field_bounds);
    return result;
}

int
RowFormCheckbox(RowForm *form, CheckboxRowProps row)
{
    int y;
    int height;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    height = ui_checkbox_row_height(row);
    RowFormTakeRect(form, height);
    return RenderCheckboxRow(row, form->x, y);
}

int
RowFormSpinbox(RowForm *form, SpinboxRowProps row)
{
    int y;
    int height;
    int label_font;
    Color label_color;
    SpinboxProps spinbox;
    SpinboxRowLayout layout;

    if(form == NULL)
        return 0;
    y = form->cursor_y;
    SpinboxRowMetrics metrics = spinbox_row_metrics(row);
    height = metrics.row_height;
    RowFormTakeRect(form, height);

    label_font = ResolveFont(row.label_font, 0, GetFontSize());
    layout = SpinboxRowLayoutFor(form->x, y, form->width, row.label_width,
                                 row.spinbox.bounds, metrics);
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    label_color = row.label_color.a != 0 ? row.label_color
                                         : text_style.foreground;

    RenderControlTextInRect(row.label != NULL ? row.label : "",
                            layout.label_bounds, label_font, label_color);
    spinbox = row.spinbox;
    spinbox.bounds = layout.spinbox_bounds;
    return Spinbox(spinbox);
}

int
RowFormButtons(RowForm *form, ButtonRowProps row)
{
    int height;

    if(form == NULL)
        return -1;
    row.x = form->x;
    row.y = form->cursor_y;
    row.width = form->width;
    height = GetButtonRowHeight(row);
    RowFormTakeRect(form, height);
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
        ButtonRowWrapDecision wrap = {0};

        if(!end_row) {
            item_w = ButtonRowItemWidth(
                TextWidth(row.items[i].label != NULL ? row.items[i].label : "",
                          font),
                metrics);
            wrap = ButtonRowWrapFor(row_w, item_w, row.width, gap);
        }

        if(!end_row && !wrap.wraps) {
            row_w = wrap.row_width;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            ButtonRowPlacement placement =
                ButtonRowPlacementFor(row.x, row.width, row_count, gap);
            int button_w = placement.button_width;
            int x = placement.start_x;

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
            y = ButtonRowNextY(y, row.height, gap);
        }

        row_start = i;
        row_w = end_row ? 0 : wrap.row_width;
        row_count = end_row ? 0 : 1;
    }

    return clicked;
}
