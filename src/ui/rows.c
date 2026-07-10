#include "ui.h"

void
DrawUIInfoRows(UIInfoRows rows)
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
GetUILabelTextFieldHeight(UILabelTextField row)
{
    int label_h = row.label_h > 0 ? row.label_h : ScaleUIPx(22);
    int field_h = row.field_h > 0 ? row.field_h : ScaleUIPx(40);
    int gap = row.gap > 0 ? row.gap : 0;
    int bottom_gap = row.bottom_gap > 0 ? row.bottom_gap : ScaleUIPx(24);

    return label_h + gap + field_h + bottom_gap;
}

int
DrawUILabelTextField(UILabelTextField row, int x, int y, int w)
{
    int label_font = row.label_font > 0 ? row.label_font : GetUISmallFontSize();
    int label_h = row.label_h > 0 ? row.label_h : ScaleUIPx(22);
    int field_h = row.field_h > 0 ? row.field_h : ScaleUIPx(40);
    int gap = row.gap > 0 ? row.gap : 0;
    Color label_color = row.label_color.a != 0 ? row.label_color : DarkenUIColor(c_text, 34);
    UITextField field = row.field;

    DrawUIText(row.label != NULL ? row.label : "", x, y, label_font, label_color);
    field.bounds = (Rectangle){(float)x, (float)(y + label_h + gap), (float)w, (float)field_h};
    return DrawUITextField(field);
}

int
GetUISectionLabelHeight(UISectionLabel label)
{
    return label.height > 0 ? label.height : ScaleUIPx(24);
}

int
DrawUISectionLabel(UISectionLabel label, int x, int y)
{
    int font = label.font > 0 ? label.font : GetUISmallFontSize();
    int icon_d = label.icon_diameter > 0 ? label.icon_diameter : ScaleUIPx(18);
    Color color = label.color.a != 0 ? label.color : DarkenUIColor(c_text, 34);
    const char *text = label.label != NULL ? label.label : "";
    int label_w;

    DrawUIText(text, x, y, font, color);
    if(!label.info_button)
        return 0;
    label_w = MeasureUIText(text, font);
    return DrawUIInfoButton(x + label_w + ScaleUIPx(16),
                               y + font / 2 + ScaleUIPx(1), icon_d);
}

int
GetUICheckboxRowHeight(UICheckboxRow row)
{
    return row.height > 0 ? row.height : ScaleUIPx(42);
}

int
DrawUICheckboxRow(UICheckboxRow row, int x, int y)
{
    if(row.disabled)
        return DrawDisabledUICheckboxToggle(x, y, row.label, row.value, 1);
    return DrawUICheckboxToggle(x, y, row.label, row.value);
}

int
DrawUIOverlayButton(UIOverlayButton button)
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
        text_w = MeasureUIText(button.label, font);
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
GetUIButtonRowHeight(UIButtonRow row)
{
    return row.height > 0 ? row.height : ScaleUIPx(40);
}

int
DrawUIButtonRow(UIButtonRow row)
{
    int clicked = -1;
    int gap = row.gap > 0 ? row.gap : ScaleUIPx(10);
    int button_w;

    if(row.items == NULL || row.count <= 0 || row.width <= 0 || row.height <= 0)
        return -1;

    button_w = (row.width - gap * (row.count - 1)) / row.count;
    if(button_w <= 0)
        return -1;

    for(int i = 0; i < row.count; i++) {
        int hover = 0;
        int x = row.x + i * (button_w + gap);

        if(DrawUIGenericButton(x, row.y, button_w, row.height,
                                  row.items[i].label,
                                  row.items[i].style,
                                  row.items[i].disabled,
                                  &hover))
            clicked = i;
    }

    return clicked;
}
