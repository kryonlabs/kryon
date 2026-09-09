#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/button.h"
#include "runtime/style.h"
#include "runtime/surface.h"
#include "toolkit_store.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;

static StyleFrame resolve_button_frame(ButtonProps button, ButtonState state,
                                      int automatic, float h, float p, float f);


static void
ui_draw_button_content(const ButtonSpec *button, Rectangle bounds,
                       int font, Color color)
{
    const char *label = button->label != NULL ? button->label : "";
    int has_icon = button->disclosure || button->icon.id != 0 ||
                   (button->icon_type > UI_ICON_TYPE_NONE &&
                    button->icon_type < UI_ICON_TYPE_COUNT);
    float scale = (float)Scale(1000) / 1000.0f;
    if(scale <= 0.0f) scale = 1.0f;
    ButtonContent content = ContentLayout(bounds.width / scale,
        bounds.height / scale, TextWidth(label, font) / scale,
        button->icon_size, button->gap, has_icon, button->icon_only,
        button->icon_placement == IconPlacementTrailing,
        button->content_offset.x, button->content_offset.y);
    float icon_size = content.icon_size * scale;
    if(button->loading) {
        Ring ring = LoadingRing(bounds.width / scale, bounds.height / scale,
            content.icon_size, GetTime() * 1000.0, ColorToInt(color), ColorToInt(GetThemeSurface()));
        Vector2 center = {bounds.x + ring.x * scale, bounds.y + ring.y * scale};
        ring.inner_radius *= scale;
        ring.outer_radius *= scale;
        ring.glow_blur *= scale;
        float paint_radius = LoadingPaintRadius(ring);
        for(int y = (int)floorf(center.y - paint_radius); y < (int)ceilf(center.y + paint_radius); y++) {
            for(int x = (int)floorf(center.x - paint_radius); x < (int)ceilf(center.x + paint_radius); x++) {
                RingSample sample = LoadingSample(ring, x - center.x, y - center.y);
                if((sample.glow & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.glow));
                if((sample.track & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.track));
                if((sample.arc & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.arc));
                if((sample.tip & 255) != 0)
                    DrawRectangle(x, y, 1, 1, GetColor(sample.tip));
            }
        }
        return;
    }
    if(has_icon) {
        Rectangle icon_bounds = {bounds.x + content.icon_x * scale,
                                 bounds.y + content.icon_y * scale,
                                 icon_size, icon_size};
        if(button->disclosure) {
            for(int y = (int)floorf(icon_bounds.y); y < (int)ceilf(icon_bounds.y + icon_size); y++) {
                for(int x = (int)floorf(icon_bounds.x); x < (int)ceilf(icon_bounds.x + icon_size); x++) {
                    float coverage = ChevronCoverage((float)x - icon_bounds.x,
                        (float)y - icon_bounds.y, icon_size);
                    if(coverage > 0.0f)
                        DrawRectangle(x, y, 1, 1, GetColor(Opacity(ColorToInt(color), coverage)));
                }
            }
        } else if(button->icon.id != 0) {
            Rectangle source = {0, 0, (float)button->icon.width,
                                (float)button->icon.height};
            DrawTexturePro(button->icon, source, icon_bounds,
                           kryon_zero_vector2, 0.0f, color);
        } else {
            DrawIcon(button->icon_type, icon_bounds, color);
        }
    }
    if(!button->icon_only && label[0] != '\0') {
        Rectangle label_bounds = bounds;
        label_bounds.x += content.text_x * scale;
        label_bounds.y += content.text_y * scale;
        label_bounds.width = content.text_width * scale;
        label_bounds.height = content.text_height * scale;
        int baseline = TextBaselineY(label, (int)label_bounds.y,
            (int)label_bounds.height, font);
        DrawUIText(label, (int)label_bounds.x, baseline, font, color);
    }
}

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


ButtonProps
ui_button_style_props(ButtonSpec button)
{
    return (ButtonProps){
        .state = button.state, .size = button.size,
        .tone = button.tone, .emphasis = button.emphasis, .style = button.style,
        .disabled = button.disabled, .loading = button.loading,
        .selected = button.selected
    };
}

static int
ui_render_button(ButtonSpec button, int handle_input, int paint,
                 int retained_hovered, int retained_pressed, Color *foreground)
{
    char editor_id[96];
    UIWidget widget;
    int hovered;
    int focused;
    int clicked = 0;
    int font = button.font > 0 ? button.font : GetFontSize();
    const char *typeface = NULL;
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color text = button.text.a != 0 ? button.text : c_text;
    Color border = button.border.a != 0 ? button.border : LightenUIColor(background, 32);
    float radius = button.radius > 0.0f ? button.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;
    float hover_amount = 0.0f;
    float press_amount = 0.0f;
    float focus_amount = 0.0f;
    Rectangle draw_bounds;
    int termi_button = ui_termi_backend();
    int default_controls = ui_default_style() && !termi_button;

    StateFlags flags = ResolveFlags(button.state, button.disabled, button.loading, button.selected);
    button.disabled = flags.disabled;
    button.loading = flags.loading;
    button.selected = flags.selected;
    int interactive = CanActivate(button.disabled, button.loading);

    memset(&widget, 0, sizeof(widget));
    if(handle_input) {
        widget = BeginUIWidget("button",
                               ui_inspect_control_id(editor_id,
                                                     sizeof(editor_id),
                                                     "button",
                                                     button.focus_id,
                                                     button.label),
                               button.bounds,
                               UI_WIDGET_MOVABLE |
                               UI_WIDGET_RESIZABLE);
        button.bounds = widget.bounds;
        UIWidgetSetAction(&widget, button.label);
        clicked = UIHandleClick(button.bounds, !interactive, &hovered);
        focused = interactive && button.focus_id > 0 &&
                  RegisterUIFocus(button.focus_id, button.bounds);
        retained_pressed = hovered &&
                           IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    } else {
        hovered = interactive && retained_hovered;
        focused = interactive && button.focus_id > 0 &&
                  IsUIFocusActive(button.focus_id);
    }
    if(focused && IsUIFocusActivatePressed(button.focus_id))
        retained_pressed = 1;
    InteractionState interaction = ResolveInteraction(button.state, button.disabled,
        button.loading, retained_pressed, hovered, focused, button.selected);
    ButtonState state = (ButtonState)interaction.state;
    hovered = interaction.hovered;
    retained_pressed = interaction.pressed;
    focused = interaction.focused;
    draw_bounds = button.bounds;
    if(!paint || !IsWindowReady()) {
        if(focused)
            SetUIFocusTextInputActive(0);
        if(handle_input)
            EndUIWidget(&widget);
        return handle_input
            ? interactive && (clicked || IsUIFocusActivatePressed(button.focus_id)) : 0;
    }

    if(default_controls) {
        unsigned int key = 2166136261u;
        const char *label = button.label != NULL ? button.label : "";

        key = (key ^ (unsigned int)button.focus_id) * 16777619u;
        if(button.focus_id == 0) {
            key = (key ^ (unsigned int)(int)button.bounds.x) * 16777619u;
            key = (key ^ (unsigned int)(int)button.bounds.y) * 16777619u;
            while(*label != '\0')
                key = (key ^ (unsigned char)*label++) * 16777619u;
        }
        InteractionMotion *motion = toolkit_button_motion(key);
        {
            float dt = GetFrameTime();
            ThemeMetrics metrics = GetThemeMetrics();
            *motion = AdvanceInteractionMotion(*motion, hovered, retained_pressed, focused,
                cues, button.state != ButtonStateAuto, button.disabled, button.loading,
                dt * 1000.0f, metrics.transition_normal_ms, metrics.transition_fast_ms);
            hover_amount = motion->hover.value;
            press_amount = motion->press.value;
            focus_amount = motion->focus.value;
            if(motion->active)
                InvalidateTree(UI_INVALIDATE_PAINT);
        }
    } else {
        hover_amount = hovered ? 1.0f : 0.0f;
        press_amount = retained_pressed ? 1.0f : 0.0f;
        focus_amount = focused ? 1.0f : 0.0f;
    }

    if(default_controls) {
        if(button.loading && !button.disabled)
            InvalidateTree(UI_INVALIDATE_PAINT);
        ThemeScheme scheme = ui_default_scheme();
        FillStates fill_states = {0};

        border = scheme.outline;
        border.a = GetThemeMetrics().border_alpha;
        if(button.style_resolved) {
            ButtonProps props;
            Style resolved;

            props = ui_button_style_props(button);
            props.pill = radius >= 0.5f;
            StyleFrame frame = resolve_button_frame(props, state,
                button.state == ButtonStateAuto, hover_amount, press_amount, focus_amount);
            resolved = ui_unpack_style(frame.value);
            typeface = resolved.typeface;
            fill_states = frame.fill;
            background = resolved.background;
            font = ResolveFont(button.font, Scale(resolved.font_size), GetFontSize());
            text = resolved.foreground;
            border = resolved.border;
            radius = resolved.radius;
            button.focus = resolved.focus;
            button.border_width = resolved.border_width;
            button.opacity = resolved.opacity;
            button.gap = resolved.gap;
            button.icon_size = resolved.icon_size;
            button.content_offset = resolved.content_offset;
            button.material = resolved.material;
        } else if(button.disabled) {
            background = scheme.disabled_container;
            text = scheme.disabled_content;
        } else {
            background = button.background.a != 0 ? button.background
                                                  : scheme.surface_variant;
            text = button.text.a != 0 ? button.text : scheme.on_surface_variant;
        }
        if(!button.style_resolved) {
            radius = GetThemeMetrics().control_radius;
            button.opacity = 1.0f;
            button.border_width = GetThemeMetrics().border_width;
            button.focus = GetTheme().colors.focus;
        }
        draw_bounds = ui_draw_material(
            draw_bounds, button.surface_bounds, background, border, border, radius,
            button.border_width, hover_amount, press_amount, button.disabled,
            button.focus, focus_amount, button.opacity, fill_states, button.material);
        text = GetColor(Opacity(ColorToInt(text), button.opacity));
        if(foreground != NULL)
            *foreground = text;
        if(focused)
            SetUIFocusTextInputActive(0);
        int typeface_token = PushUIFont(typeface);
        ui_draw_button_content(&button, draw_bounds, font, text);
        PopUIFont(typeface_token);
        if(handle_input)
            EndUIWidget(&widget);
        return handle_input
            ? interactive && (clicked || IsUIFocusActivatePressed(button.focus_id)) : 0;
    }

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        text.a = text.a > 150 ? 150 : text.a;
    }
    draw_background = ColorLerp(background, hover_background, hover_amount);
    draw_border = ColorLerp(border, LightenUIColor(hover_background, cues ? 54 : 40),
                            hover_amount);
    if(termi_button && !button.disabled && retained_pressed)
        draw_background = DarkenUIColor(draw_background, 18);
    if(cues && hovered)
        draw_background = LightenUIColor(draw_background, 6);
    if(termi_button)
        draw_border = hovered ? LightenUIColor(hover_background, 78)
                              : LightenUIColor(background, 58);

    ui_draw_control_background(draw_bounds, draw_background, draw_border, radius);
    if(termi_button)
        ui_draw_termi_button_outline(draw_bounds, draw_border, hovered,
                                     retained_pressed,
                                     button.disabled);
    if(focused) {
        SetUIFocusTextInputActive(0);
        DrawUIFocus(draw_bounds);
    }

    if(foreground != NULL)
        *foreground = text;
    ui_draw_button_content(&button, draw_bounds, font, text);
    if(handle_input)
        EndUIWidget(&widget);
    return handle_input
        ? interactive && (clicked || IsUIFocusActivatePressed(button.focus_id)) : 0;
}

int
RenderButton(ButtonSpec button)
{
    return ui_render_button(button, 1, 1, 0, 0, NULL);
}

int
HandleButton(ButtonSpec button)
{
    return ui_render_button(button, 1, 0, 0, 0, NULL);
}

Color
ui_paint_button(ButtonSpec button, int hovered, int pressed)
{
    Color foreground = button.text;
    (void)ui_render_button(button, 0, 1, hovered, pressed, &foreground);
    return foreground;
}

int
DrawUIIconButton(IconButtonProps button)
{
    char editor_id[96];
    UIWidget widget;
    int hovered;
    int focused;
    int clicked = 0;
    int icon_padding = button.icon_padding > 0 ? button.icon_padding : Scale(3);
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
    int default_controls = ui_default_style() && !termi_button;

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

    if(default_controls) {
        int pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ThemeScheme scheme = ui_default_scheme();

        if(button.background.a == 0) {
            background = BLANK;
            border = BLANK;
        } else {
            border = scheme.outline;
            border.a = GetThemeMetrics().border_alpha;
        }
        if(button.icon_color.a != 0)
            icon_tint = button.icon_color;
        else
            icon_tint = scheme.on_surface_variant;
        if(button.disabled) {
            background = BLANK;
            icon_tint = scheme.disabled_content;
        }
        if(background.a != 0) {
            radius = ui_radius_px(button.bounds,
                                  GetThemeMetrics().control_radius);
            ui_draw_control_background(button.bounds, background, border, radius);
        }
        if(!button.disabled) {
            Rectangle state = ui_centered_min_hit_rect((int)button.bounds.x,
                                                       (int)button.bounds.y,
                                                       (int)button.bounds.width,
                                                       (int)button.bounds.height,
                                                       ui_touch_target_min(),
                                                       ui_touch_target_min());
            ui_default_state_layer(state, icon_tint, hovered, focused, pressed);
        }
        if(focused) {
            SetUIFocusTextInputActive(0);
            ui_default_focus(button.bounds);
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
        } else if(button.icon_type > UI_ICON_TYPE_NONE &&
                  button.icon_type < UI_ICON_TYPE_COUNT)
            DrawIcon(button.icon_type,
                     (Rectangle){(float)icon_x, (float)icon_y,
                                 (float)draw_size, (float)draw_size},
                     icon_tint);
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
    int font = GetFontSize();
    const char *text = label != NULL ? label : "";
    int w = (int)TextWidth(text, font) + Scale(16);
    int h = TextLineHeight(font) + Scale(8);
    int min_w = Scale(34);
    int min_h = Scale(34);
    Rectangle bounds;
    int hovered;
    ButtonSpec spec;

    if(w < min_w)
        w = min_w;
    if(h < min_h)
        h = min_h;
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

Style
ResolveButtonStyle(ButtonProps button, ButtonState state)
{
    return ui_unpack_style(resolve_button_frame(button, state, 0, 0, 0, 0).value);
}

static StyleFrame
resolve_button_frame(ButtonProps button, ButtonState state,
                     int automatic, float h, float p, float f)
{
    ThemeScheme scheme = ui_default_scheme();
    const Theme *theme = GetThemeRef();
    Palette defaults = DefaultPalette(GetEffectiveThemeDarkMode());
    Color surface = theme != NULL ? theme->colors.surface : scheme.surface;
    Color neutral = theme != NULL ? theme->colors.surface_raised : scheme.surface_variant;
    Color accent = theme != NULL ? theme->colors.accent : scheme.primary;
    Color accent_hover = theme != NULL ? theme->colors.accent_hover
                                        : GetThemeButtonHover();
    Color accent_pressed = theme != NULL ? theme->colors.accent_pressed
                                          : DarkenUIColor(accent, 14);
    Color danger = theme != NULL ? theme->colors.danger : GetColor(defaults.danger);
    Color success = theme != NULL ? theme->colors.success
        : GetColor(defaults.success);
    Color warning = theme != NULL ? theme->colors.warning
        : GetColor(defaults.warning);
    Color disabled_text = theme != NULL
        ? theme->colors.text_disabled
        : GetColor(defaults.text_disabled);

    Palette palette = {
        .surface = ColorToInt(surface),
        .surface_raised = ColorToInt(neutral),
        .accent = ColorToInt(accent),
        .accent_hover = ColorToInt(accent_hover),
        .accent_pressed = ColorToInt(accent_pressed),
        .on_accent = ColorToInt(theme != NULL ? theme->colors.on_accent : scheme.on_primary),
        .text = ColorToInt(theme != NULL ? theme->colors.text : scheme.on_surface),
        .text_disabled = ColorToInt(disabled_text),
        .danger = ColorToInt(danger),
        .on_danger = ColorToInt(theme != NULL ? theme->colors.on_danger : GetColor(defaults.on_danger)),
        .success = ColorToInt(success),
        .on_success = ColorToInt(theme != NULL ? theme->colors.on_success : GetColor(defaults.on_success)),
        .warning = ColorToInt(warning),
        .on_warning = ColorToInt(theme != NULL ? theme->colors.on_warning : GetColor(defaults.on_warning)),
        .link = ColorToInt(theme != NULL ? theme->colors.link : GetThemeLink()),
        .focus = ColorToInt(theme != NULL ? theme->colors.focus : GetThemeLink())
    };
    ThemeMetrics metrics = GetThemeMetrics();
    Metrics tokens = {
        .radius_medium = metrics.radius_medium,
        .radius_large = metrics.radius_large,
        .radius_pill = metrics.radius_pill,
        .border_width = metrics.border_width,
        .control_padding_small = metrics.control_padding_small,
        .control_padding_medium = metrics.control_padding_medium,
        .control_padding_large = metrics.control_padding_large,
        .control_gap = metrics.control_gap,
        .font_size_small = metrics.font_size_small,
        .font_size_medium = metrics.font_size_medium,
        .font_size_large = metrics.font_size_large,
        .icon_size_small = metrics.icon_size_small,
        .icon_size_medium = metrics.icon_size_medium,
        .icon_size_large = metrics.icon_size_large
    };
    return ResolveFrame(button.tone, button.emphasis,
        state, button.size, button.pill, button.circle, button.disabled,
        button.loading, button.selected, palette, tokens,
        ui_pack_style_states(button.style), automatic, h, p, f);
}

static int
segmented_item_width(const SegmentOption *option, int font,
                     int min_item_width, int max_item_width)
{
    int label_w = TextWidth(option != NULL && option->label != NULL
                                ? option->label
                                : "",
                            font);
    int item_w = label_w + Scale(20);

    if(min_item_width <= 0)
        min_item_width = Scale(72);
    if(max_item_width <= 0)
        max_item_width = Scale(180);
    if(item_w < min_item_width)
        item_w = min_item_width;
    if(max_item_width > 0 && item_w > max_item_width)
        item_w = max_item_width;
    return item_w;
}

int
GetSegmentedControlHeight(SegmentedControlProps control)
{
    int font = control.font > 0 ? control.font : GetSmallFontSize();
    int gap = control.gap > 0 ? control.gap : Scale(6);
    int row_h = control.height > 0 ? control.height : Scale(30);
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
    int font = control.font > 0 ? control.font : GetSmallFontSize();
    int gap = control.gap > 0 ? control.gap : Scale(6);
    int row_h = control.height > 0 ? control.height : Scale(30);
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
                ButtonProps props;
                Style paint;
                int focus_id = control.id > 0 ? control.id * 100 + item_index + 1
                                              : 0;
                ButtonSpec button;

                memset(&button, 0, sizeof(button));
                memset(&props, 0, sizeof(props));
                button.bounds = (Rectangle){(float)x, (float)y,
                                            (float)button_w, (float)row_h};
                button.label = option->label;
                button.font = font;
                button.focus_id = focus_id;
                button.disabled = option->disabled;
                props.tone = item_index == selected
                    ? ButtonToneAccent : ButtonToneNeutral;
                props.emphasis = ButtonEmphasisSoft;
                props.selected = item_index == selected;
                paint = ResolveButtonStyle(props, ButtonStateNormal);
                button.background = paint.background;
                button.hover_background = paint.background;
                button.text = paint.foreground;
                button.border = paint.border;
                button.radius = paint.radius;
                button.style_resolved = 1;
                if(RenderButton(button)) {
                    result.clicked_index = item_index;
                    if(control.selected_index != NULL &&
                       *control.selected_index != item_index) {
                        *control.selected_index = item_index;
                        result.changed = 1;
                    }
                    result.selected_index = item_index;
                }
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
    int gap = control.gap > 0 ? control.gap : Scale(6);
    int row_h = control.height > 0 ? control.height : Scale(34);
    int item_w = control.min_item_width > 0
                     ? control.min_item_width
                     : Scale(42);
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
    int font = control.font > 0 ? control.font : GetSmallFontSize();
    int gap = control.gap > 0 ? control.gap : Scale(6);
    int row_h = control.height > 0 ? control.height : Scale(34);
    int item_w = control.min_item_width > 0
                     ? control.min_item_width
                     : Scale(42);
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
            ButtonProps props;
            Style paint;

            score_label(label, sizeof(label), value);
            memset(&button, 0, sizeof(button));
            memset(&props, 0, sizeof(props));
            button.bounds = (Rectangle){(float)x, (float)y,
                                        (float)button_w, (float)row_h};
            button.label = label;
            button.font = font;
            button.focus_id = focus_id;
            props.tone = selected_value ? ButtonToneAccent : ButtonToneNeutral;
            props.emphasis = ButtonEmphasisSoft;
            props.selected = selected_value;
            paint = ResolveButtonStyle(props, ButtonStateNormal);
            button.background = paint.background;
            button.hover_background = paint.background;
            button.text = paint.foreground;
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
            button.style_resolved = 1;
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
    int min_touch = Scale(32);
    int radius;
    int active = 0;
    int hover = 0;
    Rectangle hit;
    Color fill;
    Color stroke;
    Color text;
    int font;

    if(diameter <= 0)
        diameter = Scale(18);
    radius = diameter / 2;
    hit = ui_centered_min_hit_rect(center_x - radius, center_y - radius,
                                  diameter, diameter, min_touch, min_touch);

    active = CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world);
    if(active) {
        hover = UIHoverEffectsEnabled();
        MarkUIClickable();
    }

    if(ui_default_style()) {
        ThemeScheme scheme = ui_default_scheme();

        fill = BLANK;
        stroke = scheme.outline;
        text = scheme.primary;
        ui_default_state_layer(hit, text, hover, 0,
                                active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    } else {
        fill = hover ? c_button_hover : DarkenUIColor(c_bg, 8);
        stroke = c_text;
        text = c_text;
    }
    DrawCircle(center_x, center_y, radius, fill);
    DrawCircleLines(center_x, center_y, radius, stroke);
    font = GetSmallFontSize();
    ui_paint_text_box("i",
                      (Rectangle){(float)(center_x - radius),
                                  (float)(center_y - radius),
                                  (float)diameter, (float)diameter},
                      font, text, TextWrapNone, TextAlignCenter,
                      TextAlignCenter, ui_active_font_token(), 0);

    if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        UIConsumeRelease();
        return 1;
    }
    return 0;
}
