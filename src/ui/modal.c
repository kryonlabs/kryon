#include "ui_internal.h"

static int
ui_modal_button(int x, int y, int w, int h, const char *label, int font,
                UIButtonStyle style, Vector2 mouse_world)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int active = CheckCollisionPointRec(mouse_world, bounds) &&
                 !UIInputCapturesClick(mouse_world);
    Color background = style == UI_BUTTON_STYLE_PRIMARY ? c_button : c_surface;
    Color hover_background = style == UI_BUTTON_STYLE_PRIMARY ? c_button_hover :
                             LightenUIColor(c_surface, 14);
    Color text = c_text;

    if(active)
        MarkUIClickable();

    if(RenderButton((ButtonSpec){
        .bounds = bounds,
        .label = label,
        .font = font,
        .background = background,
        .hover_background = hover_background,
        .text = text,
        .border = DarkenUIColor(background, 28),
        .radius = 0.08f
    }))
        return 1;
    return 0;
}

static int
ui_modal_action_width(const char *label, int font)
{
    int width = TextWidth(label != NULL ? label : "", font) + ScaleUIPx(24);
    int min_width = ScaleUIPx(88);
    int max_width = ScaleUIPx(150);

    return ui_clampi(width, min_width, max_width);
}

static int
ui_modal_measure_action_rows(const UIModalAction *actions, int count,
                             int content_w, int gap, int font)
{
    int rows = 1;
    int row_w = 0;

    if(actions == NULL || count <= 0)
        return 0;

    for(int i = 0; i < count; i++) {
        int action_w = ui_modal_action_width(actions[i].label, font);
        int next_w = row_w > 0 ? row_w + gap + action_w : action_w;

        if(row_w > 0 && next_w > content_w) {
            rows++;
            row_w = action_w;
        } else {
            row_w = next_w;
        }
    }
    return rows;
}

static int
ui_modal_draw_actions(const UIModalAction *actions, int count,
                      int x, int y, int content_w, int button_h,
                      int gap, int font, Vector2 mouse_world)
{
    int result = 0;
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;

    if(actions == NULL || count <= 0)
        return 0;

    for(int i = 0; i <= count; i++) {
        int end_row = i == count;
        int action_w = !end_row ? ui_modal_action_width(actions[i].label, font) : 0;
        int next_w = row_w > 0 ? row_w + gap + action_w : action_w;

        if(!end_row && (row_w == 0 || next_w <= content_w)) {
            row_w = next_w;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            int equal_w = (content_w - gap * (row_count - 1)) / row_count;
            int draw_x = x + (content_w - (equal_w * row_count + gap * (row_count - 1))) / 2;

            for(int j = 0; j < row_count; j++) {
                int action_index = row_start + j;

                if(ui_modal_button(draw_x, y, equal_w, button_h,
                                   actions[action_index].label, font,
                                   actions[action_index].style,
                                   mouse_world))
                    result = action_index + 1;
                draw_x += equal_w + gap;
            }
            y += button_h + gap;
        }

        row_start = i;
        row_w = action_w;
        row_count = end_row ? 0 : 1;
    }

    return result;
}

int
DrawUIActionModal(ModalProps modal)
{
    int screen_pad = ScaleUIPx(24);
    int modal_min_w = ScaleUIPx(280);
    int modal_max_w = modal.max_width > 0 ? ScaleUIPx(modal.max_width) : ScaleUIPx(420);
    int modal_w;
    int modal_x;
    int modal_y;
    int title_font;
    int msg_font = GetUIFontSize();
    int btn_font = GetUISmallFontSize();
    int btn_h = ClampUIPx(30, 28, 32);
    int btn_gap = ScaleUIPx(8);
    int title_h = ScaleUIPx(48);
    int padding_x = ScaleUIPx(18);
    int padding_bottom = ScaleUIPx(18);
    int msg_x;
    int msg_y;
    int msg_w;
    int msg_gap = ScaleUIPx(18);
    int modal_h;
    int btn_y;
    int button_rows;
    int buttons_h;
    int title_w;
    int result = 0;
    Vector2 mouse_world = ui_mouse_world();

    modal_w = modal_max_w;
    if(modal_w > ui_view_width - screen_pad)
        modal_w = ui_view_width - screen_pad;
    if(modal_w < modal_min_w)
        modal_w = modal_min_w;
    if(modal_w > ui_view_width - ScaleUIPx(8))
        modal_w = ui_view_width - ScaleUIPx(8);
    msg_w = modal_w - padding_x * 2;
    if(msg_w < ScaleUIPx(120))
        msg_w = ScaleUIPx(120);

    TextLayout msg_layout = ParseTextLayout(modal.message, g_ui_gear_icon,
                                                UI_ICON_TYPE_GEAR, msg_font);
    ReflowTextLayout(&msg_layout, msg_w, msg_font, ScaleUIPx(4));

    button_rows = ui_modal_measure_action_rows(modal.actions, modal.action_count,
                                               msg_w, btn_gap, btn_font);
    buttons_h = button_rows > 0 ?
                button_rows * btn_h + (button_rows - 1) * btn_gap : 0;
    modal_h = title_h + GetTextLayoutHeight(&msg_layout) +
              (buttons_h > 0 ? msg_gap + buttons_h : 0) + padding_bottom;
    if(modal_h < ScaleUIPx(160))
        modal_h = ScaleUIPx(160);
    if(modal_h > ui_view_height - ScaleUIPx(24))
        modal_h = ui_view_height - ScaleUIPx(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    SetUIModalCapture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    msg_x = modal_x + padding_x;
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - buttons_h - padding_bottom;

    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});
    if(ui_modern_style()) {
        UIStyleTokens tokens = GetUIStyleTokens();
        Rectangle bounds = {modal_x, modal_y, modal_w, modal_h};
        Color surface = c_surface;
        Color border = LightenUIColor(c_surface, 24);
        if(tokens.panel_alpha < surface.a)
            surface.a = tokens.panel_alpha;
        ui_draw_control_background(bounds, surface, border,
                                   ui_radius_px(bounds, tokens.panel_radius));
    } else {
        DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
        DrawUIBevel(modal_x, modal_y, modal_w, modal_h,
                    LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));
    }

    title_font = GetUITitleFontSize(modal.title, modal_w - ScaleUIPx(92));
    title_w = TextWidth(modal.title != NULL ? modal.title : "", title_font);
    DrawUIText(modal.title != NULL ? modal.title : "",
               modal_x + (modal_w - title_w) / 2,
               modal_y + ScaleUIPx(14), title_font, c_text);

    DrawTextLayout(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    FreeTextLayout(&msg_layout);

    if(modal.close_icon.id != 0) {
        int icon_size = ScaleUIPx(20);
        int icon_padding = ScaleUIPx(8);
        int icon_w = icon_size + icon_padding * 2;
        int hover = 0;

        if(DrawUIPaddedIconBtn(modal_x + modal_w - icon_w - ScaleUIPx(6),
                               modal_y + ScaleUIPx(6), icon_size,
                               icon_padding, modal.close_icon, &hover))
            result = -1;
    }

    if(result == 0 && buttons_h > 0)
        result = ui_modal_draw_actions(modal.actions, modal.action_count,
                                       msg_x, btn_y, msg_w, btn_h, btn_gap,
                                       btn_font, mouse_world);

    return result;
}

int
DrawUIModal(const char *title, const char *message,
               const char *cancel_btn, const char *confirm_btn)
{
    UIModalAction actions[2] = {
        { cancel_btn, UI_BUTTON_STYLE_SECONDARY, 0 },
        { confirm_btn, UI_BUTTON_STYLE_PRIMARY, 0 }
    };

    return DrawUIActionModal((ModalProps){
        .title = title,
        .message = message,
        .actions = actions,
        .action_count = 2,
        .max_width = 360
    });
}

int
DrawUIModal3Button(const char *title, const char *message,
                    const char *left_btn, const char *middle_btn, const char *right_btn)
{
    UIModalAction actions[3] = {
        { left_btn, UI_BUTTON_STYLE_SECONDARY, 0 },
        { middle_btn, UI_BUTTON_STYLE_PRIMARY, 0 },
        { right_btn, UI_BUTTON_STYLE_DANGER, 0 }
    };

    return DrawUIActionModal((ModalProps){
        .title = title,
        .message = message,
        .actions = actions,
        .action_count = 3,
        .max_width = 420
    });
}

int
ui_paragraph_modal_height(ParagraphModalMeasureProps measure)
{
    int width = measure.width > 0 ? measure.width : ScaleUIPx(320);
    int header_h = measure.header_h > 0 ? measure.header_h : ScaleUIPx(58);
    int button_h = measure.button_h > 0 ? measure.button_h : ScaleUIPx(36);
    int line_gap = measure.line_gap > 0 ? measure.line_gap : ScaleUIPx(4);
    int font = measure.font > 0 ? measure.font : GetUIFontSize();
    int extra_lines = measure.extra_lines > 0 ? measure.extra_lines : 0;
    int min_h = measure.min_height > 0 ? measure.min_height : 0;
    int content_w;
    UIParagraphSpec paragraph;
    int height;

    if(width > ui_view_width - ScaleUIPx(24))
        width = ui_view_width - ScaleUIPx(24);
    if(width < ScaleUIPx(160))
        width = ScaleUIPx(160);
    content_w = width - ScaleUIPx(36);
    if(content_w < ScaleUIPx(120))
        content_w = ScaleUIPx(120);
    paragraph = (UIParagraphSpec){
        .text = measure.message,
        .width = content_w,
        .font = font,
        .line_gap = line_gap
    };
    height = header_h +
             ui_paragraph_height(paragraph) +
             extra_lines * (font + line_gap) +
             button_h +
             ScaleUIPx(18);
    if(height < min_h)
        height = min_h;
    return height;
}

UIPanelFrame
DrawUIModalFrame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    char editor_id[96];
    UIPanelFrame frame = {0};
    UIWidget widget;
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
    snprintf(editor_id, sizeof(editor_id), "tmp:modal:%s",
             title != NULL && title[0] != '\0' ? title : "untitled");
    {
        Rectangle bounds = {(float)frame.x, (float)frame.y,
                            (float)frame.w, (float)frame.h};
        widget = BeginUIWidget("modal", editor_id, bounds,
                               UI_WIDGET_MOVABLE |
                               UI_WIDGET_RESIZABLE);
        bounds = widget.bounds;
        frame.x = (int)bounds.x;
        frame.y = (int)bounds.y;
        frame.w = (int)bounds.width;
        frame.h = (int)bounds.height;
        if(frame.w < ScaleUIPx(120))
            frame.w = ScaleUIPx(120);
        if(frame.h < ScaleUIPx(96))
            frame.h = ScaleUIPx(96);
        bounds = (Rectangle){(float)frame.x, (float)frame.y,
                             (float)frame.w, (float)frame.h};
        UIWidgetSetBounds(&widget, bounds);
    }
    frame.content_x = frame.x + ScaleUIPx(18);
    frame.content_y = frame.y + ScaleUIPx(58);
    frame.content_w = frame.w - ScaleUIPx(36);
    frame.content_h = frame.h - ScaleUIPx(74);
    title_font = GetUITitleFontSize(title, frame.w - icon_w * 2 - ScaleUIPx(24));
    title_w = TextWidth(title, title_font);
    SetUIModalCapture((Rectangle){
        (float)frame.x, (float)frame.y, (float)frame.w, (float)frame.h
    });

    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});
    if(ui_modern_style()) {
        UIStyleTokens tokens = GetUIStyleTokens();
        Rectangle bounds = {frame.x, frame.y, frame.w, frame.h};
        Color surface = c_surface;
        Color border = LightenUIColor(c_surface, 24);
        if(tokens.panel_alpha < surface.a)
            surface.a = tokens.panel_alpha;
        ui_draw_control_background(bounds, surface, border,
                                   ui_radius_px(bounds, tokens.panel_radius));
    } else {
        DrawRectangle(frame.x, frame.y, frame.w, frame.h, c_surface);
        DrawUIBevel(frame.x, frame.y, frame.w, frame.h,
                    LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));
    }

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

    EndUIWidget(&widget);
    return frame;
}
