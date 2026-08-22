#include "ui_internal.h"

#define UI_BUTTON_ANIM_MAX 128

typedef struct UIButtonAnimState {
    unsigned int key;
    float hover;
    float press;
    unsigned long frame_seen;
} UIButtonAnimState;

static UIButtonAnimState g_ui_button_anim[UI_BUTTON_ANIM_MAX];

int
RenderButton(ButtonSpec button)
{
    char editor_id[96];
    UIWidget widget;
    int hovered;
    int focused;
    int clicked = 0;
    int font = button.font > 0 ? button.font : GetUIFontSize();
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color text = button.text.a != 0 ? button.text : c_text;
    Color border = button.border.a != 0 ? button.border : LightenUIColor(background, 32);
    float radius = button.radius > 0.0f ? button.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;
    UIButtonAnimState *anim = NULL;
    float hover_amount = 0.0f;
    float press_amount = 0.0f;
    Rectangle draw_bounds;

    widget = BeginUIWidget("button",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "button", button.focus_id,
                                                 button.label),
                           button.bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
    button.bounds = widget.bounds;
    UIWidgetSetAction(&widget, button.label);

    clicked = UIHandleClick(button.bounds, button.disabled, &hovered);
    focused = !button.disabled && button.focus_id > 0 &&
              RegisterUIFocus(button.focus_id, button.bounds);
    draw_bounds = button.bounds;

    if(ui_material_style() && !button.disabled) {
        unsigned int key = 2166136261u;
        const char *label = button.label != NULL ? button.label : "";

        key = (key ^ (unsigned int)button.focus_id) * 16777619u;
        key = (key ^ (unsigned int)(int)button.bounds.x) * 16777619u;
        key = (key ^ (unsigned int)(int)button.bounds.y) * 16777619u;
        key = (key ^ (unsigned int)(int)button.bounds.width) * 16777619u;
        key = (key ^ (unsigned int)(int)button.bounds.height) * 16777619u;
        while(*label != '\0')
            key = (key ^ (unsigned char)*label++) * 16777619u;
        anim = &g_ui_button_anim[key % UI_BUTTON_ANIM_MAX];
        if(anim->key != key || g_ui_frame_serial - anim->frame_seen > 12) {
            memset(anim, 0, sizeof(*anim));
            anim->key = key;
        }
        anim->frame_seen = g_ui_frame_serial;
        {
            float dt = GetFrameTime();
            float hover_target = hovered ? 1.0f : 0.0f;
            float press_target = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1.0f : 0.0f;
            float hover_step = dt * 10.0f;
            float press_step = dt * 16.0f;

            if(hover_step > 1.0f)
                hover_step = 1.0f;
            if(press_step > 1.0f)
                press_step = 1.0f;
            anim->hover += (hover_target - anim->hover) * hover_step;
            anim->press += (press_target - anim->press) * press_step;
            hover_amount = anim->hover;
            press_amount = anim->press;
        }
        draw_bounds.y += (float)ScaleUIPx(2) * press_amount;
    } else {
        hover_amount = hovered ? 1.0f : 0.0f;
    }

    if(ui_material_style()) {
        int pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        int ripple_key = button.focus_id != 0 ? button.focus_id :
                         (int)(button.bounds.x * 3 + button.bounds.y * 5 +
                               button.bounds.width * 7 + button.bounds.height * 11);

        radius = 0.50f;
        border = BLANK;
        if(button.disabled) {
            UIMaterialScheme scheme = ui_material_scheme();
            background = scheme.disabled_container;
            text = scheme.disabled_content;
        } else {
            background = button.background.a != 0 ? button.background : c_circle;
            text = button.text.a != 0 ? button.text : ui_material_on_color(background);
        }
        ui_draw_control_background(draw_bounds, background, border, radius);
        if(!button.disabled) {
            ui_material_state_layer(draw_bounds, text, hovered, focused, pressed);
            ui_material_ripple(draw_bounds, text, ripple_key, pressed);
        }
        if(focused) {
            SetUIFocusTextInputActive(0);
            ui_material_focus(draw_bounds);
        }
        DrawCenteredUIControlText(button.label ? button.label : "",
                                  (int)(draw_bounds.x + draw_bounds.width * 0.5f),
                                  (int)(draw_bounds.y + draw_bounds.height * 0.5f),
                                  font, text);
        EndUIWidget(&widget);
        return clicked || IsUIFocusActivatePressed(button.focus_id);
    }

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        text.a = text.a > 150 ? 150 : text.a;
    }
    draw_background = ColorLerp(background, hover_background, hover_amount);
    draw_border = ColorLerp(border, LightenUIColor(hover_background, cues ? 54 : 40),
                            hover_amount);
    if(cues && hovered)
        draw_background = LightenUIColor(draw_background, 6);

    ui_draw_control_background(draw_bounds, draw_background, draw_border, radius);
    if(cues && hovered && button.bounds.width > 4 && button.bounds.height > 4) {
        Color cue = LightenUIColor(draw_background, 42);
        cue.a = cue.a > 170 ? 170 : cue.a;
        DrawRectangle((int)draw_bounds.x + 2, (int)draw_bounds.y + 1,
                      (int)button.bounds.width - 4, ScaleUIPx(1), cue);
    }

    if(focused) {
        SetUIFocusTextInputActive(0);
        DrawUIFocus(draw_bounds);
    }

    DrawCenteredUIControlText(button.label ? button.label : "",
                              (int)(draw_bounds.x + draw_bounds.width * 0.5f),
                              (int)(draw_bounds.y + draw_bounds.height * 0.5f),
                              font, text);
    EndUIWidget(&widget);
    return clicked || IsUIFocusActivatePressed(button.focus_id);
}

int
DrawUIIconButton(IconButtonProps button)
{
    char editor_id[96];
    UIWidget widget;
    int hovered;
    int focused;
    int clicked = 0;
    int icon_padding = button.icon_padding > 0 ? button.icon_padding : ScaleUIPx(3);
    int draw_size = button.icon_size;
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color icon_tint = WHITE;
    Color border = button.border.a != 0 ? button.border : DarkenUIColor(background, 35);
    float radius = button.radius > 0.0f ? button.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;

    widget = BeginUIWidget("icon_button",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "icon_button",
                                                 button.focus_id, NULL),
                           button.bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
    button.bounds = widget.bounds;

    clicked = UIHandleClick(button.bounds, button.disabled, &hovered);
    focused = !button.disabled && button.focus_id > 0 &&
              RegisterUIFocus(button.focus_id, button.bounds);

    if(draw_size <= 0) {
        int available_w = (int)button.bounds.width - icon_padding * 2;
        int available_h = (int)button.bounds.height - icon_padding * 2;
        draw_size = available_w < available_h ? available_w : available_h;
    }
    if(draw_size < 1)
        draw_size = 1;

    if(ui_material_style()) {
        int pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        int ripple_key = button.focus_id != 0 ? button.focus_id :
                         (int)(button.bounds.x * 13 + button.bounds.y * 17);
        UIMaterialScheme scheme = ui_material_scheme();

        radius = 0.50f;
        if(button.background.a == 0) {
            background = BLANK;
            border = BLANK;
        } else {
            border = BLANK;
        }
        if(button.icon_color.a != 0)
            icon_tint = button.icon_color;
        else
            icon_tint = scheme.on_surface_variant;
        if(button.disabled) {
            background = BLANK;
            icon_tint = scheme.disabled_content;
        }
        if(background.a != 0)
            ui_draw_control_background(button.bounds, background, border, radius);
        if(!button.disabled) {
            Rectangle state = ui_centered_min_hit_rect((int)button.bounds.x,
                                                       (int)button.bounds.y,
                                                       (int)button.bounds.width,
                                                       (int)button.bounds.height,
                                                       ui_touch_target_min(),
                                                       ui_touch_target_min());
            ui_material_state_layer(state, icon_tint, hovered, focused, pressed);
            ui_material_ripple(state, icon_tint, ripple_key, pressed);
        }
        if(focused) {
            SetUIFocusTextInputActive(0);
            ui_material_focus(button.bounds);
        }
    } else {
        if(button.disabled) {
            background.a = background.a > 120 ? 120 : background.a;
            icon_tint.a = 150;
        }
        draw_background = hovered ? hover_background : background;
        draw_border = hovered ? LightenUIColor(hover_background, cues ? 54 : 40) : border;
        if(cues && hovered)
            draw_background = LightenUIColor(draw_background, 6);
        ui_draw_control_background(button.bounds, draw_background, draw_border, radius);
        if(cues && hovered && button.bounds.width > 4 && button.bounds.height > 4) {
            Color cue = LightenUIColor(draw_background, 42);
            cue.a = cue.a > 170 ? 170 : cue.a;
            DrawRectangle((int)button.bounds.x + 2, (int)button.bounds.y + 1,
                          (int)button.bounds.width - 4, ScaleUIPx(1), cue);
        }
        if(focused) {
            SetUIFocusTextInputActive(0);
            DrawUIFocus(button.bounds);
        }
    }

    {
        int icon_x = (int)(button.bounds.x + (button.bounds.width - (float)draw_size) * 0.5f);
        int icon_y = (int)(button.bounds.y + (button.bounds.height - (float)draw_size) * 0.5f);

        if(button.icon.id != 0) {
            Rectangle src = {0, 0, (float)button.icon.width, (float)button.icon.height};
            Rectangle dst = {(float)icon_x, (float)icon_y, (float)draw_size, (float)draw_size};
            DrawTexturePro(button.icon, src, dst, (Vector2){0}, 0, icon_tint);
        }
    }
    EndUIWidget(&widget);
    return clicked || IsUIFocusActivatePressed(button.focus_id);
}

int
DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover)
{
    int btn_size = GetUIIconButtonSize(size);
    int padding = GetUIIconButtonPadding(size);
    int w = btn_size + padding * 2;
    int h = btn_size + padding * 2;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    Vector2 mouse_world = ui_mouse_world();
    int hovered = CheckCollisionPointRec(mouse_world, bounds) &&
                  !UIInputCapturesClick(mouse_world) &&
                  UIHoverEffectsEnabled();

    if(hover != NULL)
        *hover = hovered;
    return DrawUIIconButton((IconButtonProps){
        .bounds = bounds,
        .icon = icon,
        .icon_size = btn_size,
        .icon_padding = padding,
        .background = c_button,
        .hover_background = c_button_hover,
        .icon_color = WHITE,
        .border = DarkenUIColor(c_button, 35),
        .radius = 0.12f
    });
}

int
DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int w = size + padding * 2;
    int h = size + padding * 2;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int hovered = CheckCollisionPointRec(mouse_world, bounds) &&
                  !UIInputCapturesClick(mouse_world) &&
                  UIHoverEffectsEnabled();

    if(hover != NULL)
        *hover = hovered;
    return DrawUIIconButton((IconButtonProps){
        .bounds = bounds,
        .icon = icon,
        .icon_size = size,
        .icon_padding = padding,
        .background = c_button,
        .hover_background = c_button_hover,
        .icon_color = WHITE,
        .border = DarkenUIColor(c_button, 35),
        .radius = 0.12f
    });
}

int
RenderTextButton(int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int font = GetUISmallFontSize();
    const char *text = label != NULL ? label : "";
    int w = (int)TextWidth(text, font) + ScaleUIPx(16);
    int h = TextLineHeight(font) + ScaleUIPx(8);
    Rectangle bounds;
    int hovered;

    x = x - w / 2;
    bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !UIInputCapturesClick(mouse_world) &&
              UIHoverEffectsEnabled();
    if(hover != NULL)
        *hover = hovered;
    return RenderButton((ButtonSpec){
        .bounds = bounds,
        .label = text,
        .font = font,
        .background = c_button,
        .hover_background = c_button_hover,
        .text = c_text,
        .border = LightenUIColor(c_button, 32),
        .radius = 0.06f
    });
}

static void
ui_button_style_colors(ButtonStyle style, Color *bg, Color *hover_bg,
                       Color *text_color)
{
    switch(style) {
    case ButtonStyleSecondary:
        *bg = DarkenUIColor(c_bg, 14);
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case ButtonStyleDanger:
        *bg = (Color){180, 70, 70, 255};
        *hover_bg = (Color){200, 90, 90, 255};
        *text_color = c_text;
        return;
    case ButtonStyleTab:
        *bg = DarkenUIColor(c_bg, 10);
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case ButtonStyleTabSelected:
        *bg = c_button;
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case ButtonStylePrimary:
    default:
        *bg = c_button;
        *hover_bg = c_button_hover;
        *text_color = c_text;
        return;
    }
}

int
RenderStyledButton(int x, int y, int w, int h, const char *label,
                       ButtonStyle style, int disabled, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int font = GetUISmallFontSize();
    Rectangle bounds = {x, y, w, h};
    int mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    int captured = UIInputCapturesClick(mouse_world);
    int hovered = mouse_inside && !disabled && !captured && UIHoverEffectsEnabled();
    int clicked;
    Color bg;
    Color hover_bg;
    Color text_color;

    ui_button_style_colors(style, &bg, &hover_bg, &text_color);
    if(ui_material_style()) {
        UIMaterialScheme scheme = ui_material_scheme();

        switch(style) {
        case ButtonStyleSecondary:
            bg = scheme.surface_variant;
            hover_bg = scheme.surface_variant;
            text_color = scheme.on_surface_variant;
            break;
        case ButtonStyleDanger:
            bg = scheme.error;
            hover_bg = scheme.error;
            text_color = scheme.on_error;
            break;
        case ButtonStyleTab:
            bg = BLANK;
            hover_bg = scheme.surface_variant;
            text_color = scheme.on_surface_variant;
            break;
        case ButtonStyleTabSelected:
            bg = scheme.secondary;
            hover_bg = scheme.secondary;
            text_color = scheme.on_secondary;
            break;
        case ButtonStylePrimary:
        default:
            bg = scheme.primary;
            hover_bg = scheme.primary;
            text_color = scheme.on_primary;
            break;
        }
    }
    if(disabled) {
        bg = DarkenUIColor(bg, 22);
        text_color = DarkenUIColor(text_color, 70);
    }

    if(hover != NULL)
        *hover = hovered;

    clicked = RenderButton((ButtonSpec){
        .bounds = bounds,
        .label = label,
        .font = font,
        .disabled = disabled,
        .background = bg,
        .hover_background = hover_bg,
        .text = text_color,
        .border = LightenUIColor(bg, 32),
        .radius = 0.08f
    });

    if(!ui_material_style() && UITransitionCuesEnabled() &&
       style == ButtonStyleTabSelected &&
       !disabled && w > ScaleUIPx(18)) {
        int cue_h = ScaleUIPx(2);
        if(cue_h < 1)
            cue_h = 1;
        DrawRectangle(x + ScaleUIPx(9), y + h - cue_h, w - ScaleUIPx(18),
                      cue_h, LightenUIColor(c_button_hover, 18));
    }

    return clicked;
}

int
DrawUIInfoButton(int center_x, int center_y, int diameter)
{
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ScaleUIPx(32);
    int radius;
    int active = 0;
    int hover = 0;
    Rectangle hit;
    Color fill;
    Color stroke;
    Color text;
    int font;

    if(diameter <= 0)
        diameter = ScaleUIPx(18);
    radius = diameter / 2;
    hit = ui_centered_min_hit_rect(center_x - radius, center_y - radius,
                                  diameter, diameter, min_touch, min_touch);

    active = CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world);
    if(active) {
        hover = UIHoverEffectsEnabled();
        MarkUIClickable();
    }

    if(ui_material_style()) {
        UIMaterialScheme scheme = ui_material_scheme();

        fill = BLANK;
        stroke = scheme.outline;
        text = scheme.primary;
        ui_material_state_layer(hit, text, hover, 0,
                                active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    } else {
        fill = hover ? c_button_hover : DarkenUIColor(c_bg, 8);
        stroke = c_text;
        text = c_text;
    }
    DrawCircle(center_x, center_y, radius, fill);
    DrawCircleLines(center_x, center_y, radius, stroke);
    font = GetUISmallFontSize();
    DrawCenteredUIControlText("i", center_x, center_y, font, text);

    if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        return 1;
    }
    return 0;
}
