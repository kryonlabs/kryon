#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/modal.h"
#include "runtime/style.h"

static int
ui_modal_icon_button(int x, int y, int size, int padding, Texture2D icon,
                     int class_name, int *hover)
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
                      .icon_only = true,
                      .class_name = class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalCloseRole()).value);
    hovered = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .icon_only = true,
                      .class_name = class_name},
        ButtonStateHover, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalCloseRole()).value);
    props.background = normal.background;
    props.hover_background = hovered.background;
    props.icon_color = normal.foreground;
    props.border = normal.border;
    props.radius = StyleLegacyBoxRadius(-1.0f);
    if(hover != NULL)
        *hover = 0;
    return RenderIconAction(props);
}

static int
ui_modal_button(int x, int y, int w, int h, const char *label, int font,
                ButtonTone tone, ButtonEmphasis emphasis, int disabled,
                int class_name, Vector2 mouse_world)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    ButtonProps props = {.bounds = bounds, .label = label,
                         .tone = tone, .emphasis = emphasis,
                         .disabled = disabled,
                         .class_name = class_name};
    ButtonSpec button = {0};
    int active = CheckCollisionPointRec(mouse_world, bounds) &&
                 !InputCapturesClick(mouse_world);

    if(active)
        MarkClickable();

    button.props = props;
    button.style.normal = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, disabled ? ButtonStateDisabled : ButtonStateNormal,
            0, 0.0f, 0.0f, 0.0f, StyleKindModal(), ModalActionRole()).value);
    button.style.normal.font_size = (float)font;
    button.style.normal.fields |= StyleFontSize;
    button.style.hover = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStateHover, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), ModalActionRole()).value);
    button.style.pressed = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStatePressed, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), ModalActionRole()).value);
    button.style.disabled = ui_unpack_style(
        ui_control_style_frame_role_kind(
            props, ButtonStateDisabled, 0, 0.0f, 0.0f, 0.0f,
            StyleKindModal(), ModalActionRole()).value);
    button.style_kind = StyleKindModal();
    button.style_resolved = 1;
    if(ui_button_render(button))
        return 1;
    return 0;
}

static int
ui_modal_action_width(const char *label, int font, ModalMetrics metrics)
{
    return ModalActionWidth(TextWidth(label != NULL ? label : "", font),
                            metrics);
}

static int
ui_modal_measure_action_rows(const ModalAction *actions, int count,
                             int content_w, int gap, int font,
                             ModalMetrics metrics)
{
    int rows = 1;
    int row_w = 0;
    int i;

    if(actions == NULL || count <= 0)
        return 0;

    for(i = 0; i < count; i++) {
        int action_w = ui_modal_action_width(actions[i].label, font, metrics);
        int next_rows = ModalActionRowsStep(row_w, rows, action_w,
                                            content_w, gap);
        row_w = ModalActionRowWidthStep(row_w, action_w, content_w, gap);
        rows = next_rows;
    }
    return rows;
}

static StyleFrame
ui_modal_frame(int class_name, ButtonTone tone, ButtonState state, int role)
{
    return ui_control_style_frame_role_kind(
        (ButtonProps){.tone = tone,
                      .emphasis = tone == ButtonToneAccent ?
                          ButtonEmphasisFilled : ButtonEmphasisSoft,
                      .class_name = class_name},
        state, 0, 0.0f, 0.0f, 0.0f, StyleKindModal(), role);
}

static ModalMetrics
ui_modal_metrics_for_class(int class_name)
{
    return ModalMetricsFor(
        (float)GetScale(),
        ui_modal_frame(class_name, ButtonToneNeutral, ButtonStateNormal,
                       ModalPanelRole()),
        ui_modal_frame(class_name, ButtonToneNeutral, ButtonStateNormal,
                       ModalTitleRole()),
        ui_modal_frame(class_name, ButtonToneNeutral, ButtonStateNormal,
                       ModalMessageRole()),
        ui_modal_frame(class_name, ButtonToneNeutral, ButtonStateNormal,
                       ModalActionRole()),
        ui_modal_frame(class_name, ButtonToneNeutral, ButtonStateNormal,
                       ModalCloseRole()));
}

static int
ui_modal_draw_actions(const ModalAction *actions, int count,
                      int x, int y, int content_w, int button_h,
                      int gap, int font, int class_name,
                      Vector2 mouse_world, ModalMetrics metrics)
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
        int action_w = !end_row ?
            ui_modal_action_width(actions[i].label, font, metrics) : 0;
        int next_rows = !end_row ?
            ModalActionRowsStep(row_w, 1, action_w, content_w, gap) : 0;
        int next_w = !end_row ?
            ModalActionRowWidthStep(row_w, action_w, content_w, gap) : 0;

        if(!end_row && next_rows == 1) {
            row_w = next_w;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            ModalActionPlacement placement =
                ModalActionPlacementFor(x, content_w, row_count, gap);
            int equal_w = placement.action_width;
            int draw_x = placement.start_x;

            for(j = 0; j < row_count; j++) {
                int action_index = row_start + j;

                if(ui_modal_button(draw_x, y, equal_w, button_h,
                                   actions[action_index].label, font,
                                   actions[action_index].tone,
                                   actions[action_index].emphasis,
                                   actions[action_index].disabled,
                                   class_name,
                                   mouse_world))
                    result = action_index + 1;
                draw_x += equal_w + gap;
            }
            y = ModalActionNextY(y, button_h, gap);
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
    ModalMetrics metrics = ui_modal_metrics_for_class(modal.class_name);
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
    int has_prompt = ModalHasPromptFor(modal.text != NULL,
                                       modal.text_size,
                                       modal.cursor_position != NULL,
                                       modal.focused != NULL);
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    ModalDismissal dismissal;
    Style panel_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.class_name = modal.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalPanelRole()).value);
    Style title_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.class_name = modal.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalTitleRole()).value);
    Style message_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.class_name = modal.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalMessageRole()).value);
    Style action_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .class_name = modal.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalActionRole()).value);
    Style scrim_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.class_name = modal.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalScrimRole()).value);

    modal_w = ModalClampWidth(ui_view_width, modal_max_w, metrics);
    msg_w = ModalContentWidth(modal_w, metrics);
    msg_font = ModalFontFor(
        msg_font, StyleFontValue(message_style.fields,
                                 message_style.font_size));
    btn_font = ModalFontFor(
        btn_font, StyleFontValue(action_style.fields,
                                 action_style.font_size));

    TextLayout msg_layout = ParseTextLayout(modal.message, g_ui_gear_icon,
                                                ICON_GEAR, msg_font);
    ReflowTextLayout(&msg_layout, msg_w, msg_font,
                     ModalMessageLineGap((float)GetScale()));

    button_rows = ui_modal_measure_action_rows(modal.actions, modal.action_count,
                                               msg_w, btn_gap, btn_font,
                                               metrics);
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
    dismissal = ModalOutsideDismissalFor(
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
        ReleaseConsumed() != 0,
        CheckCollisionPointRec(mouse_world, capture) != 0);
    if(dismissal.dismissed) {
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

    title_font = ModalFontFor(
        GetTitleFontSize(modal.title, msg_w),
        StyleFontValue(title_style.fields, title_style.font_size));
    title_w = TextWidth(modal.title != NULL ? modal.title : "", title_font);
    RenderText(modal.title != NULL ? modal.title : "",
               modal_x + (modal_w - title_w) / 2,
               modal_y + metrics.frame_title_y, title_font,
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
        field_props.focus_id = ModalPromptFocusIdFor(modal.focus_id);
        field_props.commit_pressed = &commit_pressed;
        ui_text_field_render(field_props);
        if(modal.focused != NULL)
            *modal.focused = field_focused != 0;
    }

    if(result == 0 && modal.close_icon.id != 0) {
        int icon_size = metrics.frame_icon_size;
        int icon_padding = metrics.frame_icon_padding;
        int icon_w = icon_size + icon_padding * 2;
        int hover = 0;

        if(ui_modal_icon_button(modal_x + modal_w - icon_w -
                               metrics.frame_icon_edge_gap,
                               modal_y + metrics.frame_icon_edge_gap, icon_size,
                               icon_padding, modal.close_icon,
                               modal.class_name, &hover))
            result = -1;
    }

    if(result == 0 && buttons_h > 0)
        result = ui_modal_draw_actions(modal.actions, modal.action_count,
                                       msg_x, btn_y, msg_w, btn_h, btn_gap,
                                       btn_font, modal.class_name,
                                       mouse_world, metrics);
    ModalPromptInput prompt_input = ModalPromptInputFor(
        commit_pressed != 0, IsKeyPressed(KEY_ESCAPE) != 0);
    ModalResultDecision result_decision = ModalPromptResultDecisionFor(
        result, has_prompt != 0, prompt_input, modal.action_count);
    result = result_decision.result;

    return result;
}

PanelFrame
RenderModalFrame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    char editor_id[96];
    PanelFrame frame = {0};
    Widget widget;
    ModalMetrics metrics = ui_modal_metrics_for_class(0);
    ModalFrameLayout layout;
    int title_font;
    int title_w;
    int hover = 0;
    Vector2 mouse_world = ui_mouse_world();
    Rectangle capture;
    ModalDismissal dismissal;
    Style panel_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalPanelRole()).value);
    Style title_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalTitleRole()).value);
    Style scrim_style = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindModal(), ModalScrimRole()).value);

    layout = ModalFrameLayoutFor(
        ModalFramePanelFor(ui_view_width, ui_view_height, width, height,
                           metrics),
        metrics);
    frame.w = (int)layout.panel.width;
    frame.h = (int)layout.panel.height;
    frame.x = (int)layout.panel.x;
    frame.y = (int)layout.panel.y;
    snprintf(editor_id, sizeof(editor_id), "tmp:Modal:%s",
             title != NULL && title[0] != '\0' ? title : "untitled");
    {
        Rectangle bounds = layout.panel;
        widget = BeginWidget("Modal", editor_id, bounds,
                               WidgetFlagMovable |
                               WidgetFlagResizable);
        layout = ModalFrameLayoutFor(widget.bounds, metrics);
        WidgetSetBounds(&widget, layout.panel);
    }
    frame.x = (int)layout.panel.x;
    frame.y = (int)layout.panel.y;
    frame.w = (int)layout.panel.width;
    frame.h = (int)layout.panel.height;
    frame.content_x = (int)layout.content.x;
    frame.content_y = (int)layout.content.y;
    frame.content_w = (int)layout.content.width;
    frame.content_h = (int)layout.content.height;
    title_font = GetTitleFontSize(title, layout.title_max_width);
    title_w = TextWidth(title, title_font);
    capture = layout.panel;
    SetModalCapture(capture);
    dismissal = ModalOutsideDismissalFor(
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0,
        ReleaseConsumed() != 0,
        CheckCollisionPointRec(mouse_world, capture) != 0);
    if(dismissal.dismissed) {
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

    title_font = ModalFontFor(
        title_font, StyleFontValue(title_style.fields, title_style.font_size));
    title_w = TextWidth(title, title_font);
    RenderText(title, frame.x + (frame.w - title_w) / 2,
               layout.title_y, title_font,
               Fade(title_style.foreground, title_style.opacity));

    if(left_icon.id != 0) {
        frame.left_clicked = ui_modal_icon_button((int)layout.left_button.x,
                                                     (int)layout.left_button.y,
                                                     layout.icon_size,
                                                     layout.icon_padding,
                                                     left_icon, 0, &hover);
    }
    if(frame.right_clicked == 0 && right_icon.id != 0) {
        frame.right_clicked = ui_modal_icon_button((int)layout.right_button.x,
                                                      (int)layout.right_button.y,
                                                      layout.icon_size,
                                                      layout.icon_padding,
                                                      right_icon, 0, &hover);
    }

    EndWidget(&widget);
    return frame;
}
