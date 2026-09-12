#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/checkbox.h"
#include "runtime/slider.h"
#include "runtime/toggle.h"

static void
ui_draw_slider_paint(SliderPaint paint, int hovered, int active,
                     int disabled)
{
    StyleFrame track_frame = ui_style_apply_effects_frame(paint.track);
    Style track_style = ui_unpack_style(track_frame.value);
    StyleFrame active_frame = ui_style_apply_effects_frame(paint.active_track);
    Style active_style = ui_unpack_style(active_frame.value);

    ui_draw_material(paint.track_bounds, (Rectangle){0},
                     track_style.background, track_style.border,
                     track_style.border, track_style.radius,
                     track_style.border_width,
                     hovered ? 1.0f : 0.0f, active ? 1.0f : 0.0f,
                     disabled, track_style.focus, 0.0f,
                     track_style.opacity,
                     ui_style_apply_effects_fill(track_frame.fill),
                     track_style.material);
    if(paint.active_bounds.width > 0.0f && paint.active_bounds.height > 0.0f) {
        ui_draw_material(paint.active_bounds, paint.track_bounds,
                         active_style.background, active_style.border,
                         active_style.border, active_style.radius,
                         active_style.border_width,
                         hovered ? 1.0f : 0.0f, active ? 1.0f : 0.0f,
                         disabled, active_style.focus, 0.0f,
                         active_style.opacity,
                         ui_style_apply_effects_fill(active_frame.fill),
                         active_style.material);
    }
    if(FancyEffectsEnabled() && (active || hovered) && paint.glow_radius > 0.0f)
        DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
                   paint.glow_radius, GetColor(paint.glow_color));
    if(FancyEffectsEnabled()) {
        DrawCircle((int)paint.thumb_x, (int)(paint.thumb_y + Scale(2)),
                   paint.thumb_radius + (float)Scale(1),
                   GetColor(paint.thumb_shadow_color));
    }
    DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
               paint.thumb_radius, GetColor(paint.thumb_fill_color));
    if(FancyEffectsEnabled()) {
        DrawCircle((int)(paint.thumb_x - Scale(3)),
                   (int)(paint.thumb_y - Scale(4)),
                   paint.thumb_radius * 0.45f,
                   GetColor(paint.thumb_highlight_color));
    }
    DrawCircleLines((int)paint.thumb_x, (int)paint.thumb_y,
                    paint.thumb_radius, GetColor(paint.thumb_edge_color));
}

static StyleFrame
ui_slider_style_frame_role_kind(ButtonTone tone, ButtonState state, int disabled,
                                int kind, int role);

static StyleFrame
ui_toggle_style_frame_role(ButtonTone tone, ButtonState state, int disabled,
                           int role);

static StyleFrame
ui_slider_style_frame_kind(ButtonTone tone, ButtonState state, int disabled,
                           int kind)
{
    return ui_slider_style_frame_role_kind(tone, state, disabled, kind,
                                           StyleAny());
}

static StyleFrame
ui_slider_style_frame_role_kind(ButtonTone tone, ButtonState state, int disabled,
                                int kind, int role)
{
    ButtonProps props = {0};
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                            kind, role);
}

static StyleFrame
ui_slider_style_frame(ButtonTone tone, ButtonState state, int disabled)
{
    return ui_slider_style_frame_role_kind(tone, state, disabled,
                                           StyleKindSlider(), 4);
}

static StyleFrame
ui_slider_fill_style_frame(ButtonTone tone, ButtonState state, int disabled)
{
    return ui_slider_style_frame_role_kind(tone, state, disabled,
                                           StyleKindSlider(), 5);
}

static StyleFrame
ui_slider_thumb_style_frame(ButtonTone tone, ButtonState state, int disabled)
{
    return ui_slider_style_frame_kind(tone, state, disabled,
                                      StyleKindSliderThumb());
}

static StyleFrame
ui_toggle_style_frame_role(ButtonTone tone, ButtonState state, int disabled,
                           int role)
{
    ButtonProps props = {0};
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                            StyleKindToggle(), role);
}

static StyleFrame
ui_toggle_thumb_style_frame(ButtonTone tone, ButtonState state, int disabled,
                            int selected)
{
    ButtonProps props = {0};
    props.tone = tone;
    props.emphasis = selected ? ButtonEmphasisFilled : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                       StyleKindToggleThumb());
}

static StyleFrame
ui_checkbox_style_frame(ButtonTone tone, ButtonState state, int disabled,
                        int selected)
{
    ButtonProps props = {0};
    props.tone = tone;
    props.emphasis = tone == ButtonToneAccent
                       ? ButtonEmphasisFilled
                       : ButtonEmphasisOutline;
    props.size = ControlSizeMedium;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(
        props, state, 0, 0.0f, 0.0f, 0.0f, StyleKindCheckbox(),
        tone == ButtonToneAccent ? 10 : 9);
}

static StyleFrame
ui_checkbox_label_style_frame(ButtonState state, int disabled, int selected)
{
    ButtonProps props = {0};
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisOutline;
    props.size = ControlSizeMedium;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f,
                                            0.0f, StyleKindCheckbox(), 6);
}

static ButtonState
ui_checkbox_button_state(int hovered, int down, int focused, int disabled)
{
    if(disabled)
        return ButtonStateDisabled;
    if(down)
        return ButtonStatePressed;
    if(focused)
        return ButtonStateFocus;
    if(hovered)
        return ButtonStateHover;
    return ButtonStateNormal;
}

int
ui_render_slider(int id, int x, int y, int w, const char *label,
                 int min, int max, int *value, const char *suffix,
                 const char *value_text_override)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)Scale(56)};
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int label_font = GetFontSize();
    int value_font = GetFontSize();
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int track_y = y + Scale(28);
    int knob_h = SliderThumbSize(runtime_scale);
    int knob_y = track_y - (knob_h - SliderTrackSize(0, runtime_scale)) / 2;
    int min_touch_h = ui_touch_target_min();
    int changed = 0;
    int can_draw = IsWindowReady();
    char value_text[48];
    Rectangle hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);
    float t;

    widget = BeginWidget("slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "slider", id, label),
                           editor_bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    editor_bounds = widget.bounds;
    x = (int)editor_bounds.x;
    y = (int)editor_bounds.y;
    w = (int)editor_bounds.width;
    if(w < SliderMinimumLength(runtime_scale))
        w = SliderMinimumLength(runtime_scale);
    track_y = y + Scale(28);
    knob_y = track_y - (knob_h - SliderTrackSize(0, runtime_scale)) / 2;
    hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)Scale(56)};
    WidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(value_text_override != NULL)
        snprintf(value_text, sizeof(value_text), "%s", value_text_override);
    else
        snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    if(can_draw) {
        StyleFrame label_frame = ui_slider_style_frame_role_kind(
            ButtonToneNeutral, ButtonStateNormal, 0, StyleKindSlider(), 6);
        Style label_style = ui_unpack_style(
            ui_style_apply_effects_frame(label_frame).value);
        if(label_style.font_size > 0.0f) {
            label_font = (int)(label_style.font_size + 0.5f);
            value_font = label_font;
        }
        Color label_color = Fade(label_style.foreground, label_style.opacity);
        RenderText(label, x, y, label_font, label_color);
        RenderText(value_text, x + w - TextWidth(value_text, value_font),
                   y, value_font, label_color);
    }

    t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;

    if(CheckCollisionPointRec(mouse_world, hit) && !InputCapturesClick(mouse_world)) {
        MarkClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = id;
    }

    if(g_ui_slider_active_id == id && g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
       g_ui_pointer_dragging) {
        if(ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;
        else
            g_ui_slider_active_id = 0;
    }

    if(g_ui_slider_active_id == id &&
       ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
         g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        float drag_t = (float)(mx - x) / (float)w;
        if(drag_t < 0.0f)
            drag_t = 0.0f;
        if(drag_t > 1.0f)
            drag_t = 1.0f;
        *value = min + (int)(drag_t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
    if(can_draw) {
        int active = g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !InputCapturesClick(mouse_world);
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)x, (float)knob_y, (float)w, (float)knob_h},
            .ratio = t,
            .vertical = false,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           UIContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       UIContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 UIContentDisabled())
        }), hovered, active, UIContentDisabled());
    }

    EndWidget(&widget);
    return changed;
}

static int
ui_render_vertical_slider_visual(int id, int x, int y, int h,
                                 int min, int max, int *value,
                                 int active_visual)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - Scale(18)), (float)y,
                               (float)Scale(36), (float)h};
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int track_w = SliderTrackSize(1, runtime_scale);
    int knob_w = SliderThumbSize(runtime_scale);
    int knob_h = SliderThumbSize(runtime_scale);
    int track_x = x - track_w / 2;
    int min_touch_w = ui_touch_target_min();
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                             min_touch_w, h);

    widget = BeginWidget("vertical_slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "vertical_slider", id, NULL),
                           editor_bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    editor_bounds = widget.bounds;
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    if(h < SliderMinimumLength(runtime_scale))
        h = SliderMinimumLength(runtime_scale);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                   min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - Scale(18)), (float)y,
                                (float)Scale(36), (float)h};
    WidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(CheckCollisionPointRec(mouse_world, hit) && !InputCapturesClick(mouse_world)) {
        MarkClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    {
        float t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
        int active = active_visual || g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !InputCapturesClick(mouse_world);
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)(x - knob_w / 2), (float)y,
                       (float)knob_w, (float)h},
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           UIContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       UIContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 UIContentDisabled())
        }), hovered, active, UIContentDisabled());
    }

    EndWidget(&widget);
    return changed;
}

int
ui_render_vertical_slider(int id, int x, int y, int h,
                          int min, int max, int *value)
{
    return ui_render_vertical_slider_visual(id, x, y, h, min, max, value, 0);
}

int
ui_render_vertical_slider_active(int id, int x, int y, int h,
                                 int min, int max, int *value)
{
    return ui_render_vertical_slider_visual(id, x, y, h, min, max, value, 1);
}

int
ui_render_vertical_slider_with_marks(int id, int x, int y, int h,
                                     int min, int max, int *value,
                                     SliderMarkCallback callback,
                                     void *callback_user_data)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - Scale(18)), (float)y,
                               (float)Scale(36), (float)h};
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int track_w = SliderTrackSize(1, runtime_scale);
    int knob_w = SliderThumbSize(runtime_scale);
    int knob_h = SliderThumbSize(runtime_scale);
    int track_x = x - track_w / 2;
    int min_touch_w = ui_touch_target_min();
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                             min_touch_w, h);

    widget = BeginWidget("vertical_slider_marks",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "vertical_slider_marks", id,
                                                 NULL),
                           editor_bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    editor_bounds = widget.bounds;
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    if(h < SliderMinimumLength(runtime_scale))
        h = SliderMinimumLength(runtime_scale);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                   min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - Scale(18)), (float)y,
                                (float)Scale(36), (float)h};
    WidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    if(CheckCollisionPointRec(mouse_world, hit) && !InputCapturesClick(mouse_world)) {
        MarkClickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
    }

    {
        float t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
        int active = g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !InputCapturesClick(mouse_world);
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)(x - knob_w / 2), (float)y,
                       (float)knob_w, (float)h},
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           UIContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       UIContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 UIContentDisabled())
        }), hovered, active, UIContentDisabled());
    }

    EndWidget(&widget);
    return changed;
}

int
ToggleSwitch(int x, int y, int w, int h, int *value,
             const char *off_label, const char *on_label, int focused)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)h};
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ui_touch_target_min();
    int font = GetFontSize();
    int can_draw = IsWindowReady();
    const char *off_text = off_label != NULL ? off_label : "";
    const char *on_text = on_label != NULL ? on_label : "";
    int has_labels = off_text[0] != '\0' || on_text[0] != '\0';
    int enabled = value != NULL && !UIContentDisabled();
    StyleFrame label_frame = ui_toggle_style_frame_role(ButtonToneNeutral,
        ButtonStateNormal, !enabled, 6);
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(label_frame).value);
    if(label_style.font_size > 0.0f)
        font = (int)(label_style.font_size + 0.5f);
    int off_w = has_labels ? TextWidth(off_text, font) : 0;
    int on_w = has_labels ? TextWidth(on_text, font) : 0;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int min_w = ToggleMinimumWidth(has_labels, off_w, on_w, runtime_scale);
    int min_h = ToggleMinimumHeight(runtime_scale);
    Rectangle bounds;
    int pressed;
    int hovered;
    int down;
    if(w < min_w)
        w = min_w;
    if(h < min_h)
        h = min_h;

    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    widget = BeginWidget("toggle",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "toggle", 0, off_text),
                           editor_bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    editor_bounds = widget.bounds;
    x = (int)editor_bounds.x;
    y = (int)editor_bounds.y;
    w = (int)editor_bounds.width;
    h = (int)editor_bounds.height;
    if(w < min_w)
        w = min_w;
    if(h < min_h)
        h = min_h;
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    WidgetSetBounds(&widget, editor_bounds);

    bounds = ui_centered_min_hit_rect(x, y, w, h, min_touch, min_touch);
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !InputCapturesClick(mouse_world);
    down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(hovered) {
        if(enabled)
            MarkClickable();
        else
            MarkDisabled();
    }

    pressed = enabled && hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if(pressed) {
        *value = !*value;
        ConsumeRelease();
    }
    if(!can_draw) {
        EndWidget(&widget);
        return pressed;
    }

    {
        ToggleSpec spec;
        TogglePaint paint;
        StyleFrame track_frame;
        Style track_style;
        StyleFrame active_frame;
        Style active_style;
        ButtonState state = down ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);
        int checked = value != NULL && *value;
        ButtonTone track_tone = checked && !has_labels
            ? ButtonToneAccent
            : ButtonToneNeutral;
        int track_role = checked && !has_labels ? 5 : 4;

        spec = (ToggleSpec){
            .bounds = editor_bounds,
            .checked = checked,
            .enabled = enabled,
            .hovered = hovered,
            .pressed = down,
            .focused = focused,
            .has_labels = has_labels,
            .off_width = off_w,
            .on_width = on_w,
            .font = font,
            .scale = runtime_scale,
            .track = ui_toggle_style_frame_role(track_tone, state, !enabled,
                                                track_role),
            .active = ui_toggle_style_frame_role(ButtonToneAccent, state,
                                                 !enabled, 5),
            .thumb = ui_toggle_thumb_style_frame(track_tone, state, !enabled,
                                                 checked)
        };
        paint = TogglePaintFor(spec);
        label_frame = ui_toggle_style_frame_role(ButtonToneNeutral, state,
                                                 !enabled, 6);
        label_style = ui_unpack_style(ui_style_apply_effects_frame(label_frame).value);
        if(paint.has_labels) {
            unsigned int label_color = Opacity(ColorToInt(label_style.foreground),
                                               label_style.opacity);
            if(checked)
                paint.off_label_color = label_color;
            else
                paint.on_label_color = label_color;
        }
        track_frame = ui_style_apply_effects_frame(paint.track);
        track_style = ui_unpack_style(track_frame.value);

        if(paint.show_focus) {
            Color focus = GetColor(track_frame.value.focus);
            Color glow = GetColor(Opacity(track_frame.value.focus, 0.24f));

            DrawRectangleRounded(paint.focus_bounds, 0.5f, 16, glow);
            DrawRectangleRoundedLinesEx(paint.focus_bounds, 0.5f, 16,
                                        (float)Scale(2), focus);
        }
        ui_draw_material(paint.track_bounds, (Rectangle){0},
                         track_style.background, track_style.border,
                         track_style.border, track_style.radius,
                         track_style.border_width,
                         hovered ? 1.0f : 0.0f, down ? 1.0f : 0.0f,
                         !enabled, track_style.focus, 0.0f,
                         track_style.opacity,
                         ui_style_apply_effects_fill(track_frame.fill),
                         track_style.material);

        if(paint.has_labels) {
            active_frame = ui_style_apply_effects_frame(paint.active);
            active_style = ui_unpack_style(active_frame.value);
            ui_draw_material(paint.active_bounds, paint.track_bounds,
                             active_style.background, active_style.border,
                             active_style.border, active_style.radius,
                             active_style.border_width,
                             hovered ? 1.0f : 0.0f, down ? 1.0f : 0.0f,
                             !enabled, active_style.focus, 0.0f,
                             active_style.opacity,
                             ui_style_apply_effects_fill(active_frame.fill),
                             active_style.material);
            RenderNonSelectableText(off_text, (int)paint.off_label_bounds.x,
                                    (int)paint.off_label_bounds.y, font,
                                    GetColor(paint.off_label_color));
            RenderNonSelectableText(on_text, (int)paint.on_label_bounds.x,
                                    (int)paint.on_label_bounds.y, font,
                                    GetColor(paint.on_label_color));
        } else {
            int thumb_cx = (int)paint.thumb_x;
            int thumb_cy = (int)paint.thumb_y;
            int thumb_r = (int)paint.thumb_radius;
            if(FancyEffectsEnabled() && hovered && enabled) {
                DrawCircle(thumb_cx, thumb_cy,
                           paint.thumb_radius + (float)Scale(5),
                           GetColor(paint.thumb_glow_color));
            }
            if(FancyEffectsEnabled()) {
                DrawCircle(thumb_cx, thumb_cy + Scale(2),
                           paint.thumb_radius + (float)Scale(1),
                           GetColor(paint.thumb_shadow_color));
            }
            DrawCircle(thumb_cx, thumb_cy, paint.thumb_radius,
                       GetColor(paint.thumb_fill_color));
            if(FancyEffectsEnabled()) {
                DrawCircle(thumb_cx - Scale(3), thumb_cy - Scale(4),
                           paint.thumb_radius * 0.5f,
                           GetColor(paint.thumb_highlight_color));
            }
            DrawCircleLines(thumb_cx, thumb_cy, (float)thumb_r,
                            GetColor(paint.thumb_edge_color));
        }
    }

    EndWidget(&widget);
    return pressed;
}

int
DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                             int *value, int disabled)
{
    char editor_id[96];
    Widget widget;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int slot_size = CheckboxSlotSize(runtime_scale);
    int label_gap = Scale(10);
    int checked = value != NULL && *value;
    StyleFrame label_frame = ui_checkbox_label_style_frame(
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, checked);
    Style label_style = ui_unpack_style(
        ui_style_apply_effects_frame(label_frame).value);
    int font = label_style.font_size > 0.0f
        ? (int)(label_style.font_size + 0.5f)
        : GetFontSize();
    int label_w = TextWidth(label, font);
    int label_h = TextLineHeight(font);
    int row_h = slot_size > label_h ? slot_size : label_h;
    Rectangle bounds = {x, y, slot_size + label_gap + label_w, row_h};
    Vector2 mouse_world = ui_mouse_world();
    int pressed;
    int can_draw = IsWindowReady();

    widget = BeginWidget("checkbox",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "checkbox", 0, label),
                           bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    bounds = widget.bounds;
    x = (int)bounds.x;
    y = (int)bounds.y;

    if(CheckCollisionPointRec(mouse_world, bounds) && !InputCapturesClick(mouse_world)) {
        if(disabled)
            MarkDisabled();
        else
            MarkClickable();
    }

    pressed = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
              !InputCapturesClick(mouse_world) &&
              IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(pressed) {
        *value = !(*value);
        ConsumeRelease();
    }
    if(!can_draw) {
        EndWidget(&widget);
        return pressed;
    }

    {
        int hovered = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
                      !InputCapturesClick(mouse_world) &&
                      HoverEffectsEnabled();
        int down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ButtonState state = ui_checkbox_button_state(hovered, down, 0,
                                                     disabled);
        CheckboxPaint paint;

        label_frame = ui_checkbox_label_style_frame(state, disabled, checked);
        label_style = ui_unpack_style(
            ui_style_apply_effects_frame(label_frame).value);
        if(label_style.font_size > 0.0f)
            font = (int)(label_style.font_size + 0.5f);
        paint = CheckboxPaintFor((CheckboxSpec){
            .bounds = bounds,
            .checked = checked,
            .enabled = !disabled,
            .hovered = hovered,
            .pressed = down,
            .focused = 0,
            .scale = runtime_scale,
            .box = ui_checkbox_style_frame(ButtonToneNeutral, state, disabled,
                                           checked),
            .active = ui_checkbox_style_frame(ButtonToneAccent, state,
                                              disabled, checked)
        });
        paint.label_color = Opacity(ColorToInt(label_style.foreground),
                                    label_style.opacity);

        if(paint.show_state)
            DrawRectangleRounded(paint.state_bounds, paint.state_radius, 8,
                                 GetColor(paint.state_color));
        if(paint.show_focus)
            DrawRectangleRoundedLinesEx(paint.focus_bounds, paint.focus_radius, 8,
                                        paint.border_width,
                                        GetColor(paint.focus_color));
        if(paint.show_fill)
            DrawRectangleRounded(paint.box_bounds, paint.radius, 8,
                                 GetColor(paint.fill_color));
        DrawRectangleRoundedLinesEx(paint.box_bounds, paint.radius, 8,
                                    paint.border_width,
                                    GetColor(paint.border_color));
        if(paint.show_mark) {
            DrawLineEx(paint.check_start, paint.check_middle, paint.mark_width,
                       GetColor(paint.mark_color));
            DrawLineEx(paint.check_middle, paint.check_end, paint.mark_width,
                       GetColor(paint.mark_color));
        }

        RenderText(label, x + slot_size + label_gap,
                   GetUIControlTextY(label, y, row_h, font),
                   font, GetColor(paint.label_color));
    }

    EndWidget(&widget);
    return pressed;
}

int
RenderCheckboxToggle(int x, int y, const char *label, int *value)
{
    return DrawDisabledUICheckboxToggle(x, y, label, value, 0);
}
