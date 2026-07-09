#include "ui.h"

static int
ui_modal_button(int x, int y, int w, int h, const char *label, int font,
                Vector2 mouse_world)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int active = CheckCollisionPointRec(mouse_world, bounds);
    int hovered = active && UIHoverEffectsEnabled();
    Color fill = hovered ? c_button_hover : c_button;
    int text_w;

    DrawRectangle(x, y, w, h, fill);
    DrawUIBevel(x, y, w, h, LightenUIColor(fill, 40), DarkenUIColor(fill, 40));
    if(active)
        MarkUIClickable();

    text_w = MeasureUIText(label, font);
    DrawUIText(label, x + (w - text_w) / 2, GetUIControlTextY(label, y, h, font),
                    font, c_text);

    return active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

int
DrawUIModal(const char *title, const char *message,
               const char *cancel_btn, const char *confirm_btn)
{
    int modal_w = ScaleUIPx(280);
    int modal_x;
    int modal_y;
    int title_font = GetUIFontSize();
    int msg_font = GetUIFontSize();
    int btn_font = GetUIFontSize();
    int btn_h = ClampUIPx(36, 32, 40);
    int btn_w = ScaleUIPx(100);
    int btn_gap = ScaleUIPx(12);
    int title_h = ScaleUIPx(32);
    int msg_x;
    int msg_y;
    int msg_w = modal_w - ScaleUIPx(32);
    int msg_gap = ScaleUIPx(18);
    int modal_h;
    int btn_y;
    Vector2 mouse_world = ui_mouse_world();
    UITextLayout msg_layout = ParseUITextLayout(message, g_ui_gear_icon,
                                                          UI_ICON_TYPE_GEAR, msg_font);
    ReflowUITextLayout(&msg_layout, msg_w, msg_font, ScaleUIPx(4));

    modal_h = title_h + GetUITextLayoutHeight(&msg_layout) + msg_gap + btn_h + ScaleUIPx(20);
    if(modal_h < ScaleUIPx(160))
        modal_h = ScaleUIPx(160);
    if(modal_h > ui_view_height - ScaleUIPx(24))
        modal_h = ui_view_height - ScaleUIPx(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    SetUIModalCapture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    msg_x = modal_x + ScaleUIPx(16);
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - btn_h - ScaleUIPx(16);

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    DrawUIBevel(modal_x, modal_y, modal_w, modal_h, LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));

    /* Title */
    int title_w = MeasureUIText(title, title_font);
    DrawUIText(title, modal_x + (modal_w - title_w) / 2, modal_y + ScaleUIPx(12), title_font, c_text);

    /* Draw the layout */
    DrawUITextLayout(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    FreeUITextLayout(&msg_layout);

    /* Buttons */
    int cancel_x = modal_x + (modal_w - btn_w * 2 - btn_gap) / 2;
    int confirm_x = cancel_x + btn_w + btn_gap;
    int result = 0;

    if(ui_modal_button(cancel_x, btn_y, btn_w, btn_h, cancel_btn, btn_font, mouse_world))
        result = 1;
    if(ui_modal_button(confirm_x, btn_y, btn_w, btn_h, confirm_btn, btn_font, mouse_world))
        result = 2;

    return result;
}

int
DrawUIModal3Button(const char *title, const char *message,
                    const char *left_btn, const char *middle_btn, const char *right_btn)
{
    int modal_w = ScaleUIPx(300);
    int modal_x;
    int modal_y;
    int title_font = GetUIFontSize();
    int msg_font = GetUIFontSize();
    int btn_font = GetUIFontSize();
    int btn_h = ClampUIPx(36, 32, 40);
    int btn_w = ScaleUIPx(90);
    int btn_gap = ScaleUIPx(8);
    int title_h = ScaleUIPx(32);
    int msg_x;
    int msg_y;
    int msg_w = modal_w - ScaleUIPx(32);
    int msg_gap = ScaleUIPx(18);
    int modal_h;
    int btn_y;
    Vector2 mouse_world = ui_mouse_world();
    UITextLayout msg_layout = ParseUITextLayout(message, g_ui_gear_icon,
                                                          UI_ICON_TYPE_GEAR, msg_font);
    ReflowUITextLayout(&msg_layout, msg_w, msg_font, ScaleUIPx(4));

    modal_h = title_h + GetUITextLayoutHeight(&msg_layout) + msg_gap + btn_h + ScaleUIPx(20);
    if(modal_h < ScaleUIPx(160))
        modal_h = ScaleUIPx(160);
    if(modal_h > ui_view_height - ScaleUIPx(24))
        modal_h = ui_view_height - ScaleUIPx(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    SetUIModalCapture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    msg_x = modal_x + ScaleUIPx(16);
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - btn_h - ScaleUIPx(16);

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    DrawUIBevel(modal_x, modal_y, modal_w, modal_h, LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));

    /* Title */
    int title_w = MeasureUIText(title, title_font);
    DrawUIText(title, modal_x + (modal_w - title_w) / 2, modal_y + ScaleUIPx(12), title_font, c_text);

    /* Draw the layout */
    DrawUITextLayout(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    FreeUITextLayout(&msg_layout);

    /* Calculate button positions */
    int total_btn_w = btn_w * 3 + btn_gap * 2;
    int left_x = modal_x + (modal_w - total_btn_w) / 2;
    int middle_x = left_x + btn_w + btn_gap;
    int right_x = middle_x + btn_w + btn_gap;

    int result = 0;

    if(ui_modal_button(left_x, btn_y, btn_w, btn_h, left_btn, btn_font, mouse_world))
        result = 1;
    if(ui_modal_button(middle_x, btn_y, btn_w, btn_h, middle_btn, btn_font, mouse_world))
        result = 2;
    if(ui_modal_button(right_x, btn_y, btn_w, btn_h, right_btn, btn_font, mouse_world))
        result = 3;

    return result;
}

int
GetUIParagraphModalHeight(UIParagraphModalMeasure measure)
{
    int width = measure.width > 0 ? measure.width : ScaleUIPx(320);
    int header_h = measure.header_h > 0 ? measure.header_h : ScaleUIPx(58);
    int button_h = measure.button_h > 0 ? measure.button_h : ScaleUIPx(36);
    int line_gap = measure.line_gap > 0 ? measure.line_gap : ScaleUIPx(4);
    int font = measure.font > 0 ? measure.font : GetUIFontSize();
    int extra_lines = measure.extra_lines > 0 ? measure.extra_lines : 0;
    int min_h = measure.min_height > 0 ? measure.min_height : 0;
    int content_w;
    UIParagraph paragraph;
    int height;

    if(width > ui_view_width - ScaleUIPx(24))
        width = ui_view_width - ScaleUIPx(24);
    if(width < ScaleUIPx(160))
        width = ScaleUIPx(160);
    content_w = width - ScaleUIPx(36);
    if(content_w < ScaleUIPx(120))
        content_w = ScaleUIPx(120);
    paragraph = (UIParagraph){
        .text = measure.message,
        .width = content_w,
        .font = font,
        .line_gap = line_gap
    };
    height = header_h +
             GetUIParagraphHeight(paragraph) +
             extra_lines * (font + line_gap) +
             button_h +
             ScaleUIPx(18);
    if(height < min_h)
        height = min_h;
    return height;
}

/* ================================================================
 * SCREEN HEADER (TITLE BAR)
 * ================================================================ */

static void
DrawUITitleBarBackground(int height)
{
    DrawRectangle(0, 0, ui_view_width, height, DarkenUIColor(c_bg, 14));
    DrawLine(0, height - 1, ui_view_width, height - 1,
             DarkenUIColor(c_bg, 42));
}

static int
DrawUITitleBarReturnButton(Texture2D return_icon, int height)
{
    int icon_size = ScaleUIPx(18);
    int padding = ScaleUIPx(5);
    int button_size = icon_size + padding * 2;
    int x = ScaleUIPx(4);
    int y = (height - button_size) / 2;
    int hover = 0;

    if(y < 0)
        y = 0;
    return DrawUIPaddedIconBtn(x, y, icon_size, padding, return_icon, &hover);
}

static void
DrawUITitleBarCenteredTitle(const char *title, int height,
                                       int side_reserved)
{
    int font = GetUIFontSize();
    int title_w;
    int max_w = ui_view_width - side_reserved * 2;

    if(title == NULL)
        title = "";
    if(max_w < ScaleUIPx(48))
        max_w = ui_view_width - ScaleUIPx(16);
    title_w = MeasureUIText(title, font);
    while(font > ScaleUIPx(12) && title_w > max_w) {
        font--;
        title_w = MeasureUIText(title, font);
    }
    DrawUIText(title, (ui_view_width - title_w) / 2,
                    GetUIControlTextY(title, 0, height, font),
                    font, c_text);
}

int
GetUITitleBarHeight(void)
{
    return GetUITabBarHeight();
}

void
DrawUITitleBar(const char *title, int height)
{
    DrawUITitleBarBackground(height);
    DrawUITitleBarCenteredTitle(title, height, ScaleUIPx(12));
}

int
DrawUIReturnTitleBar(Texture2D return_icon, const char *title,
                          int height)
{
    int clicked;

    DrawUITitleBarBackground(height);
    clicked = DrawUITitleBarReturnButton(return_icon, height);
    DrawUITitleBarCenteredTitle(title, height, ScaleUIPx(56));
    return clicked;
}

int
DrawUIReturnDropdownTitleBar(Texture2D return_icon,
                                   UITitleBarDropdown dropdown,
                                   int height)
{
    int icon_size = ScaleUIPx(18);
    int icon_padding = ScaleUIPx(5);
    int back_w = icon_size + icon_padding * 2;
    int gap = ScaleUIPx(4);
    int dropdown_x = ScaleUIPx(4) + back_w + gap;
    int dropdown_h = dropdown.height > 0 ? dropdown.height : ScaleUIPx(32);
    int dropdown_y = (height - dropdown_h) / 2;
    int dropdown_w = ui_view_width - dropdown_x - ScaleUIPx(4);
    int clicked;

    if(dropdown_y < 0)
        dropdown_y = 0;
    if(dropdown.min_width > 0 && dropdown_w < dropdown.min_width)
        dropdown_w = ui_view_width - dropdown_x;
    if(dropdown_w < 1)
        dropdown_w = 1;

    DrawUITitleBarBackground(height);
    clicked = DrawUITitleBarReturnButton(return_icon, height);
    if(!dropdown.disabled)
        DrawUIDropdownButton(dropdown.id, dropdown_x, dropdown_y,
                                dropdown_w, dropdown_h,
                                dropdown.options, dropdown.option_count,
                                dropdown.selected_index);
    return clicked;
}

UIPanelFrame
DrawUIModalFrame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    UIPanelFrame frame = {0};
    int title_font;
    int icon_size = ScaleUIPx(20);
    int icon_padding = ScaleUIPx(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_w;
    int hover = 0;

    if(width > ui_view_width - ScaleUIPx(24))
        width = ui_view_width - ScaleUIPx(24);
    if(height > ui_view_height - ScaleUIPx(24))
        height = ui_view_height - ScaleUIPx(24);

    frame.w = width;
    frame.h = height;
    frame.x = (ui_view_width - width) / 2;
    frame.y = (ui_view_height - height) / 2;
    frame.content_x = frame.x + ScaleUIPx(18);
    frame.content_y = frame.y + ScaleUIPx(58);
    frame.content_w = frame.w - ScaleUIPx(36);
    frame.content_h = frame.h - ScaleUIPx(74);
    title_font = GetUITitleFontSize(title, frame.w - icon_w * 2 - ScaleUIPx(24));
    title_w = MeasureUIText(title, title_font);
    SetUIModalCapture((Rectangle){
        (float)frame.x, (float)frame.y, (float)frame.w, (float)frame.h
    });

    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});
    DrawRectangle(frame.x, frame.y, frame.w, frame.h, c_surface);
    DrawUIBevel(frame.x, frame.y, frame.w, frame.h,
                  LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));

    DrawUIText(title, frame.x + (frame.w - title_w) / 2,
                    frame.y + ScaleUIPx(14), title_font, c_text);

    if(left_icon.id != 0) {
        frame.left_clicked = DrawUIPaddedIconBtn(frame.x + ScaleUIPx(6),
                                                     frame.y + ScaleUIPx(6),
                                                     icon_size, icon_padding,
                                                     left_icon, &hover);
    }
    if(right_icon.id != 0) {
        frame.right_clicked = DrawUIPaddedIconBtn(frame.x + frame.w - icon_w - ScaleUIPx(6),
                                                      frame.y + ScaleUIPx(6),
                                                      icon_size, icon_padding,
                                                      right_icon, &hover);
    }

    return frame;
}

/* ================================================================
 * SCROLLBAR
 * ================================================================ */
