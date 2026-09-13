#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/modal.h"

static int
ui_modal_icon_button(int x, int y, int size, int padding, Texture2D icon, int *hover)
{
    IconActionSpec props;
    Style normal;
    Style hovered;
    memset(&props, 0, sizeof(props));
    props.bounds = (Rectangle){(float)x, (float)y,
                               (float)(size + padding * 2),
                               (float)(size + padding * 2)};
    props.icon = icon;
    props.icon_size = size;
    props.icon_padding = padding;
    normal = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .icon_only = true},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 15).value);
    hovered = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .icon_only = true},
        ButtonStateHover, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 15).value);
    props.background = normal.background;
    props.hover_background = hovered.background;
    props.icon_color = normal.foreground;
    props.border = normal.border;
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
    ButtonProps props = {.bounds = bounds, .label = label, .font = font,
                         .tone = tone, .emphasis = emphasis,
                         .disabled = disabled};
    ButtonSpec button = {0};
    int active = CheckCollisionPointRec(mouse_world, bounds) &&
                 !InputCapturesClick(mouse_world);

    if(active)
        MarkClickable();

    button.props = props;
    button.props.style.normal = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, disabled ? ButtonStateDisabled : ButtonStateNormal,
            0, 0.0f, 0.0f, 0.0f, StyleKindModal(), 17).value);
    button.props.style.hover = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStateHover, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), 17).value);
    button.props.style.pressed = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStatePressed, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), 17).value);
    button.props.style.disabled = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStateDisabled, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), 17).value);
    button.style_kind = StyleKindModal();
    button.style_resolved = 1;
    if(ui_button_render(button))
        return 1;
    return 0;
}

static int
ui_modal_action_width(const char *label, int font)
{
    ModalMetrics metrics = ModalMetricsFor((float)GetScale());

    return ModalActionWidth(TextWidth(label != NULL ? label : "", font),
                            metrics);
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
        int next_rows = ModalActionRowsStep(row_w, rows, action_w,
                                            content_w, gap);
        row_w = ModalActionRowWidthStep(row_w, action_w, content_w, gap);
        rows = next_rows;
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
    ModalMetrics metrics = ModalMetricsFor((float)GetScale());
    int modal_max_w = modal.max_width > 0 ? Scale(modal.max_width) : 0;
    ModalLayout layout;
    int modal_w;
    int modal_x;
    int modal_y;
    int title_font;
    int msg_font = GetFontSize();
    int btn_font = GetFontSize();
    int btn_h = metrics.button_height;
    int btn_gap = metrics.button_gap;
    int msg_x;
    int msg_y;
    int msg_w;
    int modal_h;
    int btn_y;
    int button_rows;
    int buttons_h;
    int prompt_h = 0;
    int prompt_y = 0;
    int commit_pressed = 0;
    int title_w;
    int result = 0;
    int has_prompt = modal.text != NULL && modal.text_size > 0 &&
                     modal.cursor_position != NULL &&
                     modal.focused != NULL;
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    Style panel_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 2).value);
    Style title_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 16).value);
    Style message_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 20).value);
    Style action_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 17).value);
    Style scrim_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 19).value);

    modal_w = ModalClampWidth(ui_view_width, modal_max_w, metrics);
    msg_w = ModalContentWidth(modal_w, metrics);
    if((message_style.fields & (uint32_t)StyleFontSize) != 0 &&
       message_style.font_size > 0.0f)
        msg_font = (int)(message_style.font_size + 0.5f);
    if((action_style.fields & (uint32_t)StyleFontSize) != 0 &&
       action_style.font_size > 0.0f)
        btn_font = (int)(action_style.font_size + 0.5f);

    TextLayout msg_layout = ParseTextLayout(modal.message, g_ui_gear_icon,
                                                ICON_GEAR, msg_font);
    ReflowTextLayout(&msg_layout, msg_w, msg_font, Scale(4));

    button_rows = ui_modal_measure_action_rows(modal.actions, modal.action_count,
                                               msg_w, btn_gap, btn_font);
    buttons_h = ModalButtonsHeight(button_rows, metrics);
    if(has_prompt) {
        prompt_h = metrics.prompt_height;
    }
    layout = ModalLayoutFor(ui_view_width, ui_view_height, modal_max_w,
                            GetTextLayoutHeight(&msg_layout), button_rows,
                            has_prompt != 0, metrics);
    modal_x = (int)layout.panel.x;
    modal_y = (int)layout.panel.y;
    modal_w = (int)layout.panel.width;
    modal_h = (int)layout.panel.height;
    msg_w = layout.content_width;
    capture.x = (float)modal_x;
    capture.y = (float)modal_y;
    capture.width = (float)modal_w;
    capture.height = (float)modal_h;
    SetModalCapture(capture);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ReleaseConsumed() &&
       !CheckCollisionPointRec(mouse_world, capture)) {
        ConsumeRelease();
        result = -1;
    }
    msg_x = layout.message_x;
    msg_y = layout.message_y;
    btn_y = layout.button_y;
    prompt_y = layout.prompt_y;

    DrawRectangle(0, 0, ui_view_width, ui_view_height,
                  GetColor(Opacity(ColorToInt(scrim_style.background),
                                   scrim_style.opacity)));
    ui_draw_material((Rectangle){modal_x, modal_y, modal_w, modal_h},
                     (Rectangle){0}, panel_style.background,
                     panel_style.border, panel_style.border,
                     panel_style.radius, panel_style.border_width,
                     0.0f, 0.0f, 0, panel_style.focus, 0.0f,
                     panel_style.opacity, ui_style_fill(panel_style),
                     panel_style.material);

    title_font = GetTitleFontSize(modal.title, modal_w - Scale(92));
    if((title_style.fields & (uint32_t)StyleFontSize) != 0 &&
       title_style.font_size > 0.0f)
        title_font = (int)(title_style.font_size + 0.5f);
    title_w = TextWidth(modal.title != NULL ? modal.title : "", title_font);
    RenderText(modal.title != NULL ? modal.title : "",
               modal_x + (modal_w - title_w) / 2,
               modal_y + Scale(14), title_font,
               Fade(title_style.foreground, title_style.opacity));

    DrawTextLayout(&msg_layout, msg_x, &msg_y, msg_font,
                   Fade(message_style.foreground, message_style.opacity));
    FreeTextLayout(&msg_layout);

    if(has_prompt) {
        TextFieldProps field_props;
        memset(&field_props, 0, sizeof(field_props));
        field_props.bounds = (Rectangle){(float)msg_x, (float)prompt_y,
                                         (float)msg_w, (float)prompt_h};
        field_props.text = modal.text;
        field_props.text_size = (size_t)modal.text_size;
        field_props.cursor_position = modal.cursor_position;
        int field_focused = modal.focused != NULL && *modal.focused;
        field_props.focused = &field_focused;
        field_props.max_codepoints = modal.text_size - 1;
        field_props.focus_id = modal.focus_id > 0 ? modal.focus_id : 7301;
        field_props.commit_pressed = &commit_pressed;
        ui_text_field_render(field_props);
        if(modal.focused != NULL)
            *modal.focused = field_focused != 0;
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

UIPanelFrame
RenderModalFrame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    char editor_id[96];
    UIPanelFrame frame = {0};
    Widget widget;
    int title_font;
    int icon_size = Scale(20);
    int icon_padding = Scale(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_w;
    int hover = 0;
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    Style panel_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 2).value);
    Style title_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 16).value);
    Style scrim_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), 19).value);

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
        widget = BeginWidget("modal", editor_id, bounds,
                               WIDGET_MOVABLE |
                               WIDGET_RESIZABLE);
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
        WidgetSetBounds(&widget, bounds);
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
    SetModalCapture(capture);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ReleaseConsumed() &&
       !CheckCollisionPointRec(mouse_world, capture)) {
        ConsumeRelease();
        frame.right_clicked = 1;
    }

    DrawRectangle(0, 0, ui_view_width, ui_view_height,
                  GetColor(Opacity(ColorToInt(scrim_style.background),
                                   scrim_style.opacity)));
    ui_draw_material((Rectangle){frame.x, frame.y, frame.w, frame.h},
                     (Rectangle){0}, panel_style.background,
                     panel_style.border, panel_style.border,
                     panel_style.radius, panel_style.border_width,
                     0.0f, 0.0f, 0, panel_style.focus, 0.0f,
                     panel_style.opacity, ui_style_fill(panel_style),
                     panel_style.material);

    if((title_style.fields & (uint32_t)StyleFontSize) != 0 &&
       title_style.font_size > 0.0f)
        title_font = (int)(title_style.font_size + 0.5f);
    title_w = TextWidth(title, title_font);
    RenderText(title, frame.x + (frame.w - title_w) / 2,
               frame.y + Scale(14), title_font,
               Fade(title_style.foreground, title_style.opacity));

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

    EndWidget(&widget);
    return frame;
}
