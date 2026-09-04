#include "ui_internal.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;


#define UI_BUTTON_ANIM_MAX 128

typedef struct UIButtonAnimState {
    unsigned int key;
    float hover;
    float press;
    unsigned long frame_seen;
} UIButtonAnimState;

static UIButtonAnimState g_ui_button_anim[UI_BUTTON_ANIM_MAX];

static int
ui_termi_backend(void)
{
#if defined(KRYON_BACKEND_TERMI)
    return 1;
#else
    return 0;
#endif
}

static void
ui_draw_termi_button_outline(Rectangle bounds, Color border, int hovered,
                             int pressed, int disabled)
{
    Color outline = border;
    float thick = hovered || pressed ? 2.0f : 1.0f;

    if(disabled)
        outline = DarkenUIColor(outline, 45);
    else if(pressed)
        outline = WHITE;
    else if(hovered)
        outline = LightenUIColor(outline, 72);
    else
        outline = LightenUIColor(outline, 36);
    outline.a = 255;
    DrawRectangleLinesEx(bounds, thick, outline);
}

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
    int termi_button = ui_termi_backend();
    int material_controls = ui_material_style() && !termi_button;

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
    if(!IsWindowReady()) {
        if(focused)
            SetUIFocusTextInputActive(0);
        EndUIWidget(&widget);
        return clicked || IsUIFocusActivatePressed(button.focus_id);
    }

    if(material_controls && !button.disabled) {
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

    if(material_controls) {
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
    if(termi_button && !button.disabled && hovered &&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        draw_background = DarkenUIColor(draw_background, 18);
    if(cues && hovered)
        draw_background = LightenUIColor(draw_background, 6);
    if(termi_button)
        draw_border = hovered ? LightenUIColor(hover_background, 78)
                              : LightenUIColor(background, 58);

    ui_draw_control_background(draw_bounds, draw_background, draw_border, radius);
    if(termi_button)
        ui_draw_termi_button_outline(draw_bounds, draw_border, hovered,
                                     hovered &&
                                         IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                     button.disabled);
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
    int termi_button = ui_termi_backend();
    int material_controls = ui_material_style() && !termi_button;

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

    if(material_controls) {
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
        if(termi_button && !button.disabled &&
           hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            draw_background = DarkenUIColor(draw_background, 18);
        if(cues && hovered)
            draw_background = LightenUIColor(draw_background, 6);
        if(termi_button)
            draw_border = hovered ? LightenUIColor(hover_background, 78)
                                  : LightenUIColor(background, 58);
        ui_draw_control_background(button.bounds, draw_background, draw_border, radius);
        if(termi_button)
            ui_draw_termi_button_outline(button.bounds, draw_border, hovered,
                                         hovered &&
                                             IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                         button.disabled);
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
            DrawTexturePro(button.icon, src, dst, kryon_zero_vector2, 0, icon_tint);
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
    IconButtonProps props;

    if(hover != NULL)
        *hover = hovered;
    memset(&props, 0, sizeof(props));
    props.bounds = bounds;
    props.icon = icon;
    props.icon_size = btn_size;
    props.icon_padding = padding;
    props.background = c_button;
    props.hover_background = c_button_hover;
    props.icon_color = WHITE;
    props.border = DarkenUIColor(c_button, 35);
    props.radius = 0.12f;
    return DrawUIIconButton(props);
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
    IconButtonProps props;

    if(hover != NULL)
        *hover = hovered;
    memset(&props, 0, sizeof(props));
    props.bounds = bounds;
    props.icon = icon;
    props.icon_size = size;
    props.icon_padding = padding;
    props.background = c_button;
    props.hover_background = c_button_hover;
    props.icon_color = WHITE;
    props.border = DarkenUIColor(c_button, 35);
    props.radius = 0.12f;
    return DrawUIIconButton(props);
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
    ButtonSpec spec;

    x = x - w / 2;
    bounds.x = (float)x;
    bounds.y = (float)y;
    bounds.width = (float)w;
    bounds.height = (float)h;
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !UIInputCapturesClick(mouse_world) &&
              UIHoverEffectsEnabled();
    if(hover != NULL)
        *hover = hovered;
    memset(&spec, 0, sizeof(spec));
    spec.bounds = bounds;
    spec.label = text;
    spec.font = font;
    spec.background = c_button;
    spec.hover_background = c_button_hover;
    spec.text = c_text;
    spec.border = LightenUIColor(c_button, 32);
    spec.radius = 0.06f;
    return RenderButton(spec);
}

static void
ui_button_style_colors(ButtonStyle style, Color *bg, Color *hover_bg,
                       Color *text_color)
{
    switch(style) {
    case ButtonStyleOutline:
        *bg = c_bg;
        *hover_bg = c_surface;
        *text_color = c_text;
        return;
    case ButtonStyleSecondary:
        *bg = DarkenUIColor(c_bg, 14);
        *hover_bg = c_button;
        *text_color = c_text;
        return;
    case ButtonStyleDanger:
        bg->r = 180;
        bg->g = 70;
        bg->b = 70;
        bg->a = 255;
        hover_bg->r = 200;
        hover_bg->g = 90;
        hover_bg->b = 90;
        hover_bg->a = 255;
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
    ButtonSpec spec;

    ui_button_style_colors(style, &bg, &hover_bg, &text_color);
    if(ui_material_style()) {
        UIMaterialScheme scheme = ui_material_scheme();

        switch(style) {
        case ButtonStyleOutline:
            bg = scheme.surface;
            hover_bg = scheme.surface_variant;
            text_color = scheme.on_surface;
            break;
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

    memset(&spec, 0, sizeof(spec));
    spec.bounds = bounds;
    spec.label = label;
    spec.font = font;
    spec.disabled = disabled;
    spec.background = bg;
    spec.hover_background = hover_bg;
    spec.text = text_color;
    spec.border = style == ButtonStyleOutline
        ? (ui_material_style() ? ui_material_scheme().outline
                               : Fade(text_color, 0.45f))
        : LightenUIColor(bg, 32);
    spec.radius = 0.08f;
    clicked = RenderButton(spec);
    if(style == ButtonStyleOutline && ui_material_style()) {
        DrawRectangleRoundedLinesEx(bounds, 0.50f, 12, ScaleUIPx(1),
                                    ui_material_scheme().outline);
    }

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

static int
segmented_item_width(const SegmentOption *option, int font,
                     int min_item_width, int max_item_width)
{
    int label_w = TextWidth(option != NULL && option->label != NULL
                                ? option->label
                                : "",
                            font);
    int item_w = label_w + ScaleUIPx(20);

    if(min_item_width <= 0)
        min_item_width = ScaleUIPx(72);
    if(max_item_width <= 0)
        max_item_width = ScaleUIPx(180);
    if(item_w < min_item_width)
        item_w = min_item_width;
    if(max_item_width > 0 && item_w > max_item_width)
        item_w = max_item_width;
    return item_w;
}

int
GetSegmentedControlHeight(SegmentedControlProps control)
{
    int font = control.font > 0 ? control.font : GetUISmallFontSize();
    int gap = control.gap > 0 ? control.gap : ScaleUIPx(6);
    int row_h = control.height > 0 ? control.height : ScaleUIPx(30);
    int row_w = 0;
    int rows = 1;

    if(control.options == NULL || control.option_count <= 0 || row_h <= 0)
        return 0;
    if(control.bounds.width <= 0)
        return row_h;
    if(!control.wrap)
        return row_h;

    for(int i = 0; i < control.option_count; i++) {
        int item_w = segmented_item_width(&control.options[i], font,
                                          control.min_item_width,
                                          control.max_item_width);
        int next_w = row_w > 0 ? row_w + gap + item_w : item_w;

        if(row_w > 0 && next_w > (int)control.bounds.width) {
            rows++;
            row_w = item_w;
        } else {
            row_w = next_w;
        }
    }

    return rows * row_h + (rows - 1) * gap;
}

SegmentedControlResult
SegmentedControl(SegmentedControlProps control)
{
    SegmentedControlResult result;
    int font = control.font > 0 ? control.font : GetUISmallFontSize();
    int gap = control.gap > 0 ? control.gap : ScaleUIPx(6);
    int row_h = control.height > 0 ? control.height : ScaleUIPx(30);
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;
    int y = (int)control.bounds.y;
    int selected = control.selected_index != NULL ? *control.selected_index : -1;

    memset(&result, 0, sizeof(result));
    result.selected_index = selected;
    result.clicked_index = -1;
    result.height = GetSegmentedControlHeight(control);

    if(control.options == NULL || control.option_count <= 0 ||
       control.bounds.width <= 0 || row_h <= 0)
        return result;

    for(int i = 0; i <= control.option_count; i++) {
        int end_row = i == control.option_count;
        int item_w = 0;
        int next_w;

        if(!end_row)
            item_w = segmented_item_width(&control.options[i], font,
                                          control.min_item_width,
                                          control.max_item_width);
        next_w = row_w > 0 ? row_w + gap + item_w : item_w;

        if(!end_row &&
           (!control.wrap || row_w == 0 || next_w <= (int)control.bounds.width)) {
            row_w = next_w;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            int available_w = (int)control.bounds.width;
            int button_w = control.wrap
                               ? (available_w - gap * (row_count - 1)) / row_count
                               : row_w / row_count;
            int x = (int)control.bounds.x;

            if(control.wrap)
                x += (available_w -
                      (button_w * row_count + gap * (row_count - 1))) / 2;
            for(int j = 0; j < row_count; j++) {
                int item_index = row_start + j;
                const SegmentOption *option = &control.options[item_index];
                int hover = 0;
                ButtonStyle style = item_index == selected
                                        ? ButtonStyleTabSelected
                                        : ButtonStyleSecondary;
                int focus_id = control.id > 0 ? control.id * 100 + item_index + 1
                                              : 0;
                ButtonSpec button;

                memset(&button, 0, sizeof(button));
                button.bounds = (Rectangle){(float)x, (float)y,
                                            (float)button_w, (float)row_h};
                button.label = option->label;
                button.font = font;
                button.focus_id = focus_id;
                button.disabled = option->disabled;
                ui_button_style_colors(style, &button.background,
                                       &button.hover_background,
                                       &button.text);
                button.border = LightenUIColor(button.background, 32);
                button.radius = 0.08f;
                if(RenderButton(button)) {
                    result.clicked_index = item_index;
                    if(control.selected_index != NULL &&
                       *control.selected_index != item_index) {
                        *control.selected_index = item_index;
                        result.changed = 1;
                    }
                    result.selected_index = item_index;
                }
                (void)hover;
                x += button_w + gap;
            }
            y += row_h + gap;
        }

        row_start = i;
        row_w = item_w;
        row_count = end_row ? 0 : 1;
    }

    return result;
}

static void
score_label(char *buffer, size_t buffer_size, int value)
{
    if(buffer == NULL || buffer_size == 0)
        return;
    if(value > 0)
        snprintf(buffer, buffer_size, "+%d", value);
    else
        snprintf(buffer, buffer_size, "%d", value);
}

static int
score_control_count(ScoreControlProps control)
{
    int min_value = control.min_value;
    int max_value = control.max_value;

    if(min_value == 0 && max_value == 0) {
        min_value = -3;
        max_value = 3;
    }
    if(max_value < min_value)
        return 0;
    return max_value - min_value + 1;
}

int
GetScoreControlHeight(ScoreControlProps control)
{
    int gap = control.gap > 0 ? control.gap : ScaleUIPx(6);
    int row_h = control.height > 0 ? control.height : ScaleUIPx(34);
    int item_w = control.min_item_width > 0
                     ? control.min_item_width
                     : ScaleUIPx(42);
    int count = score_control_count(control);
    int per_row;
    int rows;

    if(count <= 0 || row_h <= 0)
        return 0;
    if(control.bounds.width <= 0 || !control.wrap)
        return row_h;
    per_row = ((int)control.bounds.width + gap) / (item_w + gap);
    if(per_row < 1)
        per_row = 1;
    rows = (count + per_row - 1) / per_row;
    return rows * row_h + (rows - 1) * gap;
}

ScoreControlResult
ScoreControl(ScoreControlProps control)
{
    ScoreControlResult result;
    int font = control.font > 0 ? control.font : GetUISmallFontSize();
    int gap = control.gap > 0 ? control.gap : ScaleUIPx(6);
    int row_h = control.height > 0 ? control.height : ScaleUIPx(34);
    int item_w = control.min_item_width > 0
                     ? control.min_item_width
                     : ScaleUIPx(42);
    int min_value = control.min_value;
    int max_value = control.max_value;
    int count;
    int per_row;
    int selected = control.value != NULL ? *control.value : 0;

    memset(&result, 0, sizeof(result));
    result.value = selected;
    result.clicked_value = selected;
    result.height = GetScoreControlHeight(control);

    if(min_value == 0 && max_value == 0) {
        min_value = -3;
        max_value = 3;
    }
    count = max_value - min_value + 1;
    if(count <= 0 || control.bounds.width <= 0 || row_h <= 0)
        return result;

    per_row = count;
    if(control.wrap) {
        per_row = ((int)control.bounds.width + gap) / (item_w + gap);
        if(per_row < 1)
            per_row = 1;
        if(per_row > count)
            per_row = count;
    }

    for(int row_start = 0, row_index = 0; row_start < count;
        row_start += per_row, row_index++) {
        int row_count = count - row_start;
        int x;
        int y = (int)control.bounds.y + row_index * (row_h + gap);
        int button_w;

        if(row_count > per_row)
            row_count = per_row;
        button_w = control.wrap
                       ? ((int)control.bounds.width - gap * (row_count - 1)) /
                             row_count
                       : item_w;
        if(button_w < 1)
            button_w = 1;
        x = (int)control.bounds.x;
        if(control.wrap)
            x += ((int)control.bounds.width -
                  (button_w * row_count + gap * (row_count - 1))) /
                 2;

        for(int i = 0; i < row_count; i++) {
            int value = min_value + row_start + i;
            int focus_id = control.id > 0 ? control.id * 100 + row_start + i + 1
                                          : 0;
            int selected_value = value == selected;
            char label[16];
            ButtonSpec button;

            score_label(label, sizeof(label), value);
            memset(&button, 0, sizeof(button));
            button.bounds = (Rectangle){(float)x, (float)y,
                                        (float)button_w, (float)row_h};
            button.label = label;
            button.font = font;
            button.focus_id = focus_id;
            ui_button_style_colors(selected_value ? ButtonStyleTabSelected
                                                  : ButtonStyleSecondary,
                                   &button.background,
                                   &button.hover_background,
                                   &button.text);
            if(value < 0 && !selected_value) {
                button.background = DarkenUIColor(c_bg, 10);
                button.hover_background = DarkenUIColor(c_bg, 4);
                button.text = Fade(c_text, 0.78f);
            } else if(value > 0 && !selected_value) {
                button.background = c_surface;
                button.hover_background = LightenUIColor(c_surface, 8);
                button.text = c_text;
            }
            button.border = selected_value ? c_button : Fade(c_text, 0.30f);
            button.radius = 0.08f;
            if(RenderButton(button)) {
                result.clicked = 1;
                result.clicked_value = value;
                if(control.value != NULL && *control.value != value) {
                    *control.value = value;
                    result.changed = 1;
                }
                result.value = value;
            }
            x += button_w + gap;
        }
    }

    return result;
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
