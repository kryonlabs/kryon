#include "ui_internal.h"

static int
ui_modal_icon_button(int x, int y, int size, int padding, Texture2D icon, int *hover)
{
    IconActionSpec props;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){(float)x, (float)y,
                               (float)(size + padding * 2),
                               (float)(size + padding * 2)};
    props.icon = icon;
    props.icon_size = size;
    props.icon_padding = padding;
    props.background = c_button;
    props.hover_background = c_button_hover;
    props.icon_color = GetThemeText();
    props.border = DarkenUIColor(c_button, 35);
    props.radius = 0.12f;
    if(hover != NULL)
        *hover = 0;
    return RenderIconAction(props);
}

static int
ui_modal_button(int x, int y, int w, int h, const char *label, int font,
                ButtonTone tone, ButtonEmphasis emphasis, int disabled,
                Vector2 mouse_world)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int active = CheckCollisionPointRec(mouse_world, bounds) &&
                 !UIInputCapturesClick(mouse_world);

    if(active)
        MarkClickable();

    if(Button((ButtonProps){.bounds= bounds, .label=label, .font=font,
                            .tone=tone, .emphasis=emphasis,
                            .disabled=disabled}))
        return 1;
    return 0;
}

static int
ui_modal_action_width(const char *label, int font)
{
    int width = TextWidth(label != NULL ? label : "", font) + Scale(24);
    int min_width = Scale(88);
    int max_width = Scale(150);

    return ui_clampi(width, min_width, max_width);
}

static int
ui_modal_measure_action_rows(const ModalAction *actions, int count,
                             int content_w, int gap, int font)
{
    int rows = 1;
    int row_w = 0;
    int i;

    if(actions == NULL || count <= 0)
        return 0;

    for(i = 0; i < count; i++) {
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
ui_modal_draw_actions(const ModalAction *actions, int count,
                      int x, int y, int content_w, int button_h,
                      int gap, int font, Vector2 mouse_world)
{
    int result = 0;
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;
    int i;
    int j;

    if(actions == NULL || count <= 0)
        return 0;

    for(i = 0; i <= count; i++) {
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

            for(j = 0; j < row_count; j++) {
                int action_index = row_start + j;

                if(ui_modal_button(draw_x, y, equal_w, button_h,
                                   actions[action_index].label, font,
                                   actions[action_index].tone,
                                   actions[action_index].emphasis,
                                   actions[action_index].disabled,
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
RenderActionModal(ModalProps modal)
{
    int screen_pad = Scale(24);
    int modal_min_w = Scale(280);
    int modal_max_w = modal.max_width > 0 ? Scale(modal.max_width) : Scale(420);
    int modal_w;
    int modal_x;
    int modal_y;
    int title_font;
    int msg_font = GetFontSize();
    int btn_font = GetFontSize();
    int btn_h = Scale(44);
    int btn_gap = Scale(8);
    int title_h = Scale(48);
    int padding_x = Scale(18);
    int padding_bottom = Scale(18);
    int msg_x;
    int msg_y;
    int msg_w;
    int msg_gap = Scale(18);
    int modal_h;
    int btn_y;
    int button_rows;
    int buttons_h;
    int prompt_h = 0;
    int prompt_gap = 0;
    int prompt_y = 0;
    int commit_pressed = 0;
    int title_w;
    int result = 0;
    int has_prompt = modal.text != NULL && modal.text_size > 0 &&
                     modal.cursor_position != NULL &&
                     modal.focused != NULL;
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    Color scrim;

    modal_w = modal_max_w;
    if(modal_w > ui_view_width - screen_pad)
        modal_w = ui_view_width - screen_pad;
    if(modal_w < modal_min_w)
        modal_w = modal_min_w;
    if(modal_w > ui_view_width - Scale(8))
        modal_w = ui_view_width - Scale(8);
    msg_w = modal_w - padding_x * 2;
    if(msg_w < Scale(120))
        msg_w = Scale(120);

    TextLayout msg_layout = ParseTextLayout(modal.message, g_ui_gear_icon,
                                                UI_ICON_TYPE_GEAR, msg_font);
    ReflowTextLayout(&msg_layout, msg_w, msg_font, Scale(4));

    button_rows = ui_modal_measure_action_rows(modal.actions, modal.action_count,
                                               msg_w, btn_gap, btn_font);
    buttons_h = button_rows > 0 ?
                button_rows * btn_h + (button_rows - 1) * btn_gap : 0;
    if(has_prompt) {
        prompt_h = Scale(38);
        prompt_gap = Scale(18);
    }
    modal_h = title_h + GetTextLayoutHeight(&msg_layout) +
              (prompt_h > 0 ? prompt_gap + prompt_h : 0) +
              (buttons_h > 0 ? msg_gap + buttons_h : 0) + padding_bottom;
    if(modal_h < Scale(160))
        modal_h = Scale(160);
    if(modal_h > ui_view_height - Scale(24))
        modal_h = ui_view_height - Scale(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    capture.x = (float)modal_x;
    capture.y = (float)modal_y;
    capture.width = (float)modal_w;
    capture.height = (float)modal_h;
    SetUIModalCapture(capture);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !UIReleaseConsumed() &&
       !CheckCollisionPointRec(mouse_world, capture)) {
        UIConsumeRelease();
        result = -1;
    }
    msg_x = modal_x + padding_x;
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - buttons_h - padding_bottom;
    prompt_y = msg_y + GetTextLayoutHeight(&msg_layout) + prompt_gap;

    scrim.r = 0;
    scrim.g = 0;
    scrim.b = 0;
    scrim.a = 180;
    DrawRectangle(0, 0, ui_view_width, ui_view_height, scrim);
    if(ui_modern_style()) {
        ThemeMetrics tokens = GetThemeMetrics();
        Rectangle bounds = {modal_x, modal_y, modal_w, modal_h};
        Color surface = c_surface;
        Color border = LightenUIColor(c_surface, 24);
        if(tokens.panel_alpha < surface.a)
            surface.a = tokens.panel_alpha;
        ui_draw_control_background(bounds, surface, border,
                                   ui_radius_px(bounds, tokens.panel_radius));
    } else {
        DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
        RenderBevel(modal_x, modal_y, modal_w, modal_h,
                    LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));
    }

    title_font = GetTitleFontSize(modal.title, modal_w - Scale(92));
    title_w = TextWidth(modal.title != NULL ? modal.title : "", title_font);
    RenderText(modal.title != NULL ? modal.title : "",
               modal_x + (modal_w - title_w) / 2,
               modal_y + Scale(14), title_font, c_text);

    DrawTextLayout(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    FreeTextLayout(&msg_layout);

    if(has_prompt) {
        TextFieldProps field_props;
        memset(&field_props, 0, sizeof(field_props));
        field_props.bounds = (Rectangle){(float)msg_x, (float)prompt_y,
                                         (float)msg_w, (float)prompt_h};
        field_props.text = modal.text;
        field_props.text_size = (size_t)modal.text_size;
        field_props.cursor_position = modal.cursor_position;
        field_props.focused = modal.focused;
        field_props.max_codepoints = modal.text_size - 1;
        field_props.font = GetFontSize();
        field_props.focus_id = modal.focus_id > 0 ? modal.focus_id : 7301;
        field_props.commit_pressed = &commit_pressed;
        ui_text_field_render(field_props);
    }

    if(result == 0 && modal.close_icon.id != 0) {
        int icon_size = Scale(20);
        int icon_padding = Scale(8);
        int icon_w = icon_size + icon_padding * 2;
        int hover = 0;

        if(ui_modal_icon_button(modal_x + modal_w - icon_w - Scale(6),
                               modal_y + Scale(6), icon_size,
                               icon_padding, modal.close_icon, &hover))
            result = -1;
    }

    if(result == 0 && buttons_h > 0)
        result = ui_modal_draw_actions(modal.actions, modal.action_count,
                                       msg_x, btn_y, msg_w, btn_h, btn_gap,
                                       btn_font, mouse_world);
    if(result == 0 && has_prompt && commit_pressed)
        result = modal.action_count > 1 ? 2 : 1;
    if(result == 0 && has_prompt && IsKeyPressed(KEY_ESCAPE))
        result = 1;

    return result;
}

int
ui_paragraph_modal_height(ParagraphModalMeasureProps measure)
{
    int width = measure.width > 0 ? measure.width : Scale(320);
    int header_h = measure.header_h > 0 ? measure.header_h : Scale(58);
    int button_h = measure.button_h > 0 ? measure.button_h : Scale(36);
    int line_gap = measure.line_gap > 0 ? measure.line_gap : Scale(4);
    int font = measure.font > 0 ? measure.font : GetFontSize();
    int extra_lines = measure.extra_lines > 0 ? measure.extra_lines : 0;
    int min_h = measure.min_height > 0 ? measure.min_height : 0;
    int content_w;
    ParagraphSpec paragraph;
    int height;

    if(width > ui_view_width - Scale(24))
        width = ui_view_width - Scale(24);
    if(width < Scale(160))
        width = Scale(160);
    content_w = width - Scale(36);
    if(content_w < Scale(120))
        content_w = Scale(120);
    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = measure.message;
    paragraph.width = content_w;
    paragraph.font = font;
    paragraph.line_gap = line_gap;
    height = header_h +
             ui_paragraph_height(paragraph) +
             extra_lines * (font + line_gap) +
             button_h +
             Scale(18);
    if(height < min_h)
        height = min_h;
    return height;
}

UIPanelFrame
RenderModalFrame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    char editor_id[96];
    UIPanelFrame frame = {0};
    UIWidget widget;
    int title_font;
    int icon_size = Scale(20);
    int icon_padding = Scale(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_w;
    int hover = 0;
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    Color scrim;

    if(width > ui_view_width - Scale(24))
        width = ui_view_width - Scale(24);
    if(height > ui_view_height - Scale(24))
        height = ui_view_height - Scale(24);

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
        if(frame.w < Scale(120))
            frame.w = Scale(120);
        if(frame.h < Scale(96))
            frame.h = Scale(96);
        bounds.x = (float)frame.x;
        bounds.y = (float)frame.y;
        bounds.width = (float)frame.w;
        bounds.height = (float)frame.h;
        UIWidgetSetBounds(&widget, bounds);
    }
    frame.content_x = frame.x + Scale(18);
    frame.content_y = frame.y + Scale(58);
    frame.content_w = frame.w - Scale(36);
    frame.content_h = frame.h - Scale(74);
    title_font = GetTitleFontSize(title, frame.w - icon_w * 2 - Scale(24));
    title_w = TextWidth(title, title_font);
    capture.x = (float)frame.x;
    capture.y = (float)frame.y;
    capture.width = (float)frame.w;
    capture.height = (float)frame.h;
    SetUIModalCapture(capture);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !UIReleaseConsumed() &&
       !CheckCollisionPointRec(mouse_world, capture)) {
        UIConsumeRelease();
        frame.right_clicked = 1;
    }

    scrim.r = 0;
    scrim.g = 0;
    scrim.b = 0;
    scrim.a = 180;
    DrawRectangle(0, 0, ui_view_width, ui_view_height, scrim);
    if(ui_modern_style()) {
        ThemeMetrics tokens = GetThemeMetrics();
        Rectangle bounds = {frame.x, frame.y, frame.w, frame.h};
        Color surface = c_surface;
        Color border = LightenUIColor(c_surface, 24);
        if(tokens.panel_alpha < surface.a)
            surface.a = tokens.panel_alpha;
        ui_draw_control_background(bounds, surface, border,
                                   ui_radius_px(bounds, tokens.panel_radius));
    } else {
        DrawRectangle(frame.x, frame.y, frame.w, frame.h, c_surface);
        RenderBevel(frame.x, frame.y, frame.w, frame.h,
                    LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));
    }

    RenderText(title, frame.x + (frame.w - title_w) / 2,
                    frame.y + Scale(14), title_font, c_text);

    if(left_icon.id != 0) {
        frame.left_clicked = ui_modal_icon_button(frame.x + Scale(6),
                                                     frame.y + Scale(6),
                                                     icon_size, icon_padding,
                                                     left_icon, &hover);
    }
    if(frame.right_clicked == 0 && right_icon.id != 0) {
        frame.right_clicked = ui_modal_icon_button(frame.x + frame.w - icon_w - Scale(6),
                                                      frame.y + Scale(6),
                                                      icon_size, icon_padding,
                                                      right_icon, &hover);
    }

    EndUIWidget(&widget);
    return frame;
}
