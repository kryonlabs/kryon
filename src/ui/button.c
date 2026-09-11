#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_paint_internal.h"
#include "runtime/button.h"
#include "runtime/style.h"
#include "runtime/surface.h"

static void ui_button_theme_values(Palette *palette, Metrics *metrics);

static void
ui_draw_button_content(const ButtonSpec *button, Rectangle bounds,
                       int font, Color color)
{
    ButtonProps props = button->props;
    StyleData paint = {.icon_size = button->paint.icon_size, .gap = button->paint.gap,
                       .offset_x = button->paint.content_offset.x, .offset_y = button->paint.content_offset.y};
    ContentDrawing content = PaintContent(props, bounds, paint, font,
        TextWidth(props.label != NULL ? props.label : "", font), ColorToInt(color),
        ColorToInt(GetThemeSurface()), (float)Scale(1000) / 1000.0f,
        GetTime() * 1000.0, button->disclosure);
    ui_draw(content.mark);
    ui_draw(content.label);
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


static int
ui_render_button(ButtonSpec button, int handle_input, int paint,
                 int retained_hovered, int retained_pressed, Color *foreground)
{
    char editor_id[96];
    UIWidget widget;
    int hovered;
    int focused;
    int font = button.props.font > 0 ? button.props.font : GetFontSize();
    Color background = button.paint.background.a != 0 ? button.paint.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color text = button.paint.foreground.a != 0 ? button.paint.foreground : c_text;
    Color border = button.paint.border.a != 0 ? button.paint.border : LightenUIColor(background, 32);
    float radius = button.paint.radius > 0.0f ? button.paint.radius : 0.06f;
    int cues = UITransitionCuesEnabled();
    Color draw_background;
    Color draw_border;
    float hover_amount = 0.0f;
    float press_amount = 0.0f;
    float focus_amount = 0.0f;
    InteractionMotion motion = {0};
    Rectangle draw_bounds;
    int termi_button = ui_termi_backend();
    int default_controls = ui_default_style() && !termi_button;

    memset(&widget, 0, sizeof(widget));
    if(handle_input) {
        widget = BeginUIWidget("button",
                               ui_inspect_control_id(editor_id,
                                                     sizeof(editor_id),
                                                     "button",
                                                     button.props.id,
                                                     button.props.label),
                               button.props.bounds,
                               UI_WIDGET_MOVABLE |
                               UI_WIDGET_RESIZABLE);
        button.props.bounds = widget.bounds;
        UIWidgetSetAction(&widget, button.props.label);
    }
    ButtonProps props = button.props;
    ButtonInput input;
    if(handle_input) {
        input = ReadButtonInput(props);
    } else {
        Activation sample = {
            .hovered = retained_hovered,
            .pressed = retained_pressed,
            .focused = button.props.id > 0 && IsUIFocusActive(button.props.id)
        };
        if(sample.focused && IsUIFocusActivatePressed(button.props.id))
            sample.pressed = true;
        input = ResolveButtonInput(props, sample);
    }
    button.props.disabled = input.flags.disabled;
    button.props.loading = input.flags.loading;
    button.props.selected = input.flags.selected;
    ButtonState state = (ButtonState)input.interaction.state;
    hovered = input.interaction.hovered;
    retained_pressed = input.interaction.pressed;
    focused = input.interaction.focused;
    draw_bounds = button.props.bounds;
    if(!paint || !IsWindowReady()) {
        if(focused)
            SetUIFocusTextInputActive(0);
        if(handle_input)
            EndUIWidget(&widget);
        return handle_input
            ? input.activated : 0;
    }

    if(default_controls) {
        unsigned int key = 2166136261u;
        const char *label = button.props.label != NULL ? button.props.label : "";

        key = (key ^ (unsigned int)button.props.id) * 16777619u;
        if(button.props.id == 0) {
            key = (key ^ (unsigned int)(int)button.props.bounds.x) * 16777619u;
            key = (key ^ (unsigned int)(int)button.props.bounds.y) * 16777619u;
            while(*label != '\0')
                key = (key ^ (unsigned char)*label++) * 16777619u;
        }
        if(button.style_resolved) {
            props = button.props;
            Palette palette;
            Metrics tokens;
            ui_button_theme_values(&palette, &tokens);
            ButtonFrame frame = AdvanceFrame(key, props, input, palette, tokens,
                ui_pack_style_states(props.style), cues, GetFrameTime() * 1000.0f,
                button.surface_bounds, ColorToInt(GetThemeSurface()), GetUIScale(), GetFontSize());
            frame.appearance = ui_style_apply_effects_frame(frame.appearance);
            frame.material.value = frame.appearance.value;
            frame.material.fill = ui_style_apply_effects_fill(frame.material.fill);
            if(frame.repaint)
                InvalidateTree(UI_INVALIDATE_PAINT);
            text = GetColor(frame.foreground);
            if(foreground != NULL)
                *foreground = text;
            if(focused)
                SetUIFocusTextInputActive(0);
            int typeface_token = PushUIFont(frame.appearance.value.typeface.data);
            PaintButton(frame, TextWidth(frame.props.label != NULL ? frame.props.label : "", frame.font),
                GetTime() * 1000.0, button.disclosure, ui_surface_painter, ui_painter);
            PopUIFont(typeface_token);
            if(handle_input)
                EndUIWidget(&widget);
            return handle_input ? input.activated : 0;
        }
        ThemeMetrics metrics = GetThemeMetrics();
        motion = AdvanceButtonMotion(key, props, input, cues,
            GetFrameTime() * 1000.0f, metrics.transition_normal_ms, metrics.transition_fast_ms);
        hover_amount = motion.hover.value;
        press_amount = motion.press.value;
        focus_amount = motion.focus.value;
    } else {
        hover_amount = hovered ? 1.0f : 0.0f;
        press_amount = retained_pressed ? 1.0f : 0.0f;
        focus_amount = focused ? 1.0f : 0.0f;
    }

    if(default_controls) {
        if(motion.active || (button.props.loading && !button.props.disabled))
            InvalidateTree(UI_INVALIDATE_PAINT);
        ThemeScheme scheme = ui_default_scheme();
        FillStates fill_states = {0};
        border = scheme.outline;
        border.a = GetThemeMetrics().border_alpha;
        if(button.props.disabled) {
            background = scheme.disabled_container;
            text = scheme.disabled_content;
        } else {
            background = button.paint.background.a != 0 ? button.paint.background
                                                  : scheme.surface_variant;
            text = button.paint.foreground.a != 0 ? button.paint.foreground : scheme.on_surface_variant;
        }
        radius = GetThemeMetrics().control_radius;
        button.paint.opacity = 1.0f;
        button.paint.border_width = GetThemeMetrics().border_width;
        button.paint.focus = GetTheme().colors.focus;
        draw_bounds = ui_draw_material(
            draw_bounds, button.surface_bounds, background, border, border, radius,
            button.paint.border_width, hover_amount, press_amount, button.props.disabled,
            button.paint.focus, focus_amount, button.paint.opacity, fill_states, button.paint.material);
        text = GetColor(Opacity(ColorToInt(text), button.paint.opacity));
        if(foreground != NULL)
            *foreground = text;
        if(focused)
            SetUIFocusTextInputActive(0);
        int typeface_token = PushUIFont(NULL);
        ui_draw_button_content(&button, draw_bounds, font, text);
        PopUIFont(typeface_token);
        if(handle_input)
            EndUIWidget(&widget);
        return handle_input
            ? input.activated : 0;
    }

    if(button.props.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        text.a = text.a > 150 ? 150 : text.a;
    }
    draw_background = ColorLerp(background, hover_background, hover_amount);
    draw_border = ColorLerp(border, LightenUIColor(hover_background, cues ? 54 : 40),
                            hover_amount);
    if(termi_button && !button.props.disabled && retained_pressed)
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
                                     button.props.disabled);
    if(focused) {
        SetUIFocusTextInputActive(0);
        RenderFocus(draw_bounds);
    }

    if(foreground != NULL)
        *foreground = text;
    ui_draw_button_content(&button, draw_bounds, font, text);
    if(handle_input)
        EndUIWidget(&widget);
    return handle_input
        ? input.activated : 0;
}

int
ui_button_render(ButtonSpec button)
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
    Color foreground = button.paint.foreground;
    (void)ui_render_button(button, 0, 1, hovered, pressed, &foreground);
    return foreground;
}

int
RenderIconAction(IconActionSpec button)
{
    ButtonSpec spec = {0};
    float scale = (float)Scale(1000) / 1000.0f;
    int padding = button.icon_padding > 0 ? button.icon_padding : Scale(3);
    int icon_size = button.icon_size;

    if(scale <= 0.0f)
        scale = 1.0f;
    if(icon_size <= 0) {
        float available = fminf(button.bounds.width, button.bounds.height);
        icon_size = (int)available - padding * 2;
    }
    if(icon_size < 1)
        icon_size = 1;

    spec.props.bounds = button.bounds;
    spec.props.id = button.focus_id;
    spec.props.disabled = button.disabled;
    spec.props.icon = button.icon;
    spec.props.icon_type = button.icon_type;
    spec.props.icon_only = 1;
    spec.paint.icon_size = (float)icon_size / scale;
    spec.props.tone = ButtonToneNeutral;
    spec.props.emphasis = button.background.a != 0
        ? ButtonEmphasisSoft : ButtonEmphasisGhost;
    spec.style_resolved = 1;
    spec.props.style.normal.fields = StyleIconSize;
    spec.props.style.normal.icon_size = spec.paint.icon_size;
    if(button.background.a != 0) {
        spec.props.style.normal.fields |= StyleBackground;
        spec.props.style.normal.background = button.background;
    }
    if(button.hover_background.a != 0) {
        spec.props.style.hover.fields |= StyleBackground;
        spec.props.style.hover.background = button.hover_background;
    }
    if(button.icon_color.a != 0) {
        spec.props.style.normal.fields |= StyleForeground;
        spec.props.style.normal.foreground = button.icon_color;
    }
    if(button.border.a != 0) {
        spec.props.style.normal.fields |= StyleBorder;
        spec.props.style.normal.border = button.border;
    }
    if(button.radius > 0.0f) {
        spec.props.style.normal.fields |= StyleRadius;
        spec.props.style.normal.radius = button.radius *
            fminf(button.bounds.width, button.bounds.height) / (2.0f * scale);
    }
    spec.paint.background = button.background;
    spec.hover_background = button.hover_background;
    spec.paint.foreground = button.icon_color;
    spec.paint.border = button.border;
    spec.paint.radius = button.radius;
    return ui_button_render(spec);
}

int
ui_text_button_render(int x, int y, const char *label, int *hover)
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
    spec.props.bounds = bounds;
    spec.props.label = text;
    spec.props.font = font;
    spec.paint.background = c_button;
    spec.hover_background = c_button_hover;
    spec.paint.foreground = c_text;
    spec.paint.border = LightenUIColor(c_button, 32);
    spec.paint.radius = 0.06f;
    return ui_button_render(spec);
}

Style
ResolveButtonStyle(ButtonProps button, ButtonState state)
{
    return ui_unpack_style(ui_button_style_frame(button, state, 0, 0, 0, 0).value);
}

static void
ui_button_theme_values(Palette *palette_out, Metrics *metrics_out)
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
        .transition_normal_ms = metrics.transition_normal_ms,
        .transition_fast_ms = metrics.transition_fast_ms,
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
    *palette_out = palette;
    *metrics_out = tokens;
}

StyleFrame
ui_button_style_frame(ButtonProps button, ButtonState state,
                      int automatic, float h, float p, float f)
{
    Palette palette;
    Metrics tokens;
    ui_button_theme_values(&palette, &tokens);
    return ui_style_apply_effects_frame(ResolveFrame(button.tone, button.emphasis,
        state, button.size, button.pill, button.circle, button.disabled,
        button.loading, button.selected, palette, tokens,
        ui_pack_style_states(button.style), automatic, h, p, f));
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
                button.props.bounds = (Rectangle){(float)x, (float)y,
                                            (float)button_w, (float)row_h};
                button.props.label = option->label;
                button.props.font = font;
                button.props.id = focus_id;
                button.props.disabled = option->disabled;
                props.tone = item_index == selected
                    ? ButtonToneAccent : ButtonToneNeutral;
                props.emphasis = ButtonEmphasisSoft;
                props.selected = item_index == selected;
                paint = ResolveButtonStyle(props, ButtonStateNormal);
                button.paint.background = paint.background;
                button.hover_background = paint.background;
                button.paint.foreground = paint.foreground;
                button.paint.border = paint.border;
                button.paint.radius = paint.radius;
                button.style_resolved = 1;
                if(ui_button_render(button)) {
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
            button.props.bounds = (Rectangle){(float)x, (float)y,
                                        (float)button_w, (float)row_h};
            button.props.label = label;
            button.props.font = font;
            button.props.id = focus_id;
            props.tone = selected_value ? ButtonToneAccent : ButtonToneNeutral;
            props.emphasis = ButtonEmphasisSoft;
            props.selected = selected_value;
            paint = ResolveButtonStyle(props, ButtonStateNormal);
            button.paint.background = paint.background;
            button.hover_background = paint.background;
            button.paint.foreground = paint.foreground;
            if(value < 0 && !selected_value) {
                button.paint.background = DarkenUIColor(c_bg, 10);
                button.hover_background = DarkenUIColor(c_bg, 4);
                button.paint.foreground = Fade(c_text, 0.78f);
            } else if(value > 0 && !selected_value) {
                button.paint.background = c_surface;
                button.hover_background = LightenUIColor(c_surface, 8);
                button.paint.foreground = c_text;
            }
            button.paint.border = selected_value ? c_button : Fade(c_text, 0.30f);
            button.paint.radius = 0.08f;
            button.style_resolved = 1;
            if(ui_button_render(button)) {
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
RenderInfoButton(int center_x, int center_y, int diameter)
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
        MarkClickable();
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
