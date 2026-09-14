#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/checkbox.h"
#include "runtime/focus.h"
#include "runtime/input.h"
#include "runtime/slider.h"
#include "runtime/style.h"
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
        DrawCircle((int)paint.thumb_shadow_x, (int)paint.thumb_shadow_y,
                   paint.thumb_shadow_radius,
                   GetColor(paint.thumb_shadow_color));
    }
    DrawCircle((int)paint.thumb_x, (int)paint.thumb_y,
               paint.thumb_radius, GetColor(paint.thumb_fill_color));
    if(FancyEffectsEnabled()) {
        DrawCircle((int)paint.thumb_highlight_x,
                   (int)paint.thumb_highlight_y,
                   paint.thumb_highlight_radius,
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
                                           StyleKindSlider(),
                                           SliderTrackRole());
}

static StyleFrame
ui_slider_fill_style_frame(ButtonTone tone, ButtonState state, int disabled)
{
    return ui_slider_style_frame_role_kind(tone, state, disabled,
                                           StyleKindSlider(),
                                           SliderFillRole());
}

static StyleFrame
ui_slider_thumb_style_frame(ButtonTone tone, ButtonState state, int disabled)
{
    return ui_slider_style_frame_kind(tone, state, disabled,
                                      StyleKindSliderThumb());
}

static StyleFrame
ui_toggle_style_frame_role_class(ButtonTone tone, ButtonState state,
                                 int disabled, int class_name, int role)
{
    ButtonProps props = {0};
    props.class_name = class_name;
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
ui_toggle_style_frame_role(ButtonTone tone, ButtonState state, int disabled,
                           int role)
{
    return ui_toggle_style_frame_role_class(tone, state, disabled, 0, role);
}

static StyleFrame
ui_toggle_thumb_style_frame_class(ButtonTone tone, ButtonState state,
                                  int disabled, int selected, int class_name)
{
    ButtonProps props = {0};
    props.class_name = class_name;
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
ui_toggle_thumb_style_frame(ButtonTone tone, ButtonState state, int disabled,
                            int selected)
{
    return ui_toggle_thumb_style_frame_class(tone, state, disabled, selected, 0);
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
        CheckboxBoxRoleForTone(tone));
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
                                            0.0f, StyleKindCheckbox(),
                                            CheckboxLabelRole());
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
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    StyleFrame metric_track_frame = ui_slider_style_frame(ButtonToneNeutral,
                                                          ButtonStateNormal,
                                                          ContentDisabled());
    StyleFrame metric_thumb_frame = ui_slider_thumb_style_frame(ButtonToneAccent,
                                                                ButtonStateNormal,
                                                                ContentDisabled());
    SliderEditorLayout layout = SliderHorizontalEditorLayoutFor(
        x, y, w, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    Rectangle editor_bounds = layout.editor_bounds;
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int label_font = GetFontSize();
    int value_font = GetFontSize();
    int changed = 0;
    int can_draw = IsWindowReady();
    char value_text[48];
    Rectangle hit = layout.hit_bounds;
    SliderPointerDecision pointer_decision;
    float t;

    widget = BeginWidget("Slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Slider", id, label),
                           editor_bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    editor_bounds = widget.bounds;
    x = (int)editor_bounds.x;
    y = (int)editor_bounds.y;
    w = (int)editor_bounds.width;
    layout = SliderHorizontalEditorLayoutFor(
        x, y, w, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    editor_bounds = layout.editor_bounds;
    hit = layout.hit_bounds;
    WidgetSetBounds(&widget, editor_bounds);

    pointer_decision = SliderPointerDecisionFor(
        g_ui_slider_active_id == id,
        CheckCollisionPointRec(mouse_world, hit) != 0,
        InputCapturesClick(mouse_world) != 0,
        ui_input_captures_click_internal(mouse_world, 0) != 0,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, false,
        g_ui_pointer_owner == POINTER_OWNER_NONE,
        g_ui_pointer_owner == POINTER_OWNER_HORIZONTAL_SLIDER,
        g_ui_pointer_dragging != 0, ui_pointer_drag_is_horizontal() != 0);
    if(pointer_decision.clear_active)
        g_ui_slider_active_id = 0;

    if(value_text_override != NULL)
        snprintf(value_text, sizeof(value_text), "%s", value_text_override);
    else
        snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    if(can_draw) {
        StyleFrame label_frame = ui_slider_style_frame_role_kind(
            ButtonToneNeutral, ButtonStateNormal, 0, StyleKindSlider(),
            SliderLabelRole());
        Style label_style = ui_unpack_style(
            ui_style_apply_effects_frame(label_frame).value);
        label_font = ResolveFont(0, StyleFontValue(label_style.fields,
                                                   label_style.font_size),
                                 label_font);
        value_font = label_font;
        Color label_color = Fade(label_style.foreground, label_style.opacity);
        RenderText(label, x, y, label_font, label_color);
        RenderText(value_text, x + w - TextWidth(value_text, value_font),
                   y, value_font, label_color);
    }

    t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;

    if(pointer_decision.hovered) {
        MarkClickable();
        if(pointer_decision.start_active)
            g_ui_slider_active_id = id;
    }

    if(pointer_decision.take_horizontal_owner)
        g_ui_pointer_owner = POINTER_OWNER_HORIZONTAL_SLIDER;
    if(pointer_decision.cancel_active)
        g_ui_slider_active_id = 0;

    if(pointer_decision.update_value) {
        int old_value = *value;
        *value = SliderDiscretePointerValue((float)mx, (float)x, (float)w,
                                            min, max, false);
        changed = (*value != old_value);
    }
    if(pointer_decision.finish_active)
        g_ui_slider_active_id = 0;

    t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
    if(can_draw) {
        int active = g_ui_slider_active_id == id;
        int hovered = pointer_decision.hovered;
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = layout.paint_bounds,
            .ratio = t,
            .vertical = false,
            .active = active,
            .hovered = hovered,
            .disabled = ContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           ContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       ContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 ContentDisabled())
        }), hovered, active, ContentDisabled());
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
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    StyleFrame metric_track_frame = ui_slider_style_frame(ButtonToneNeutral,
                                                          ButtonStateNormal,
                                                          ContentDisabled());
    StyleFrame metric_thumb_frame = ui_slider_thumb_style_frame(ButtonToneAccent,
                                                                ButtonStateNormal,
                                                                ContentDisabled());
    SliderEditorLayout layout = SliderVerticalEditorLayoutFor(
        x, y, h, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    Rectangle editor_bounds = layout.editor_bounds;
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int changed = 0;
    Rectangle hit = layout.hit_bounds;
    SliderPointerDecision pointer_decision;

    widget = BeginWidget("Slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Slider", id, NULL),
                           editor_bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    editor_bounds = widget.bounds;
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    layout = SliderVerticalEditorLayoutFor(
        x, y, h, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    editor_bounds = layout.editor_bounds;
    hit = layout.hit_bounds;
    WidgetSetBounds(&widget, editor_bounds);

    pointer_decision = SliderPointerDecisionFor(
        g_ui_slider_active_id == id,
        CheckCollisionPointRec(mouse_world, hit) != 0,
        InputCapturesClick(mouse_world) != 0,
        ui_input_captures_click_internal(mouse_world, 0) != 0,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, true,
        g_ui_pointer_owner == POINTER_OWNER_NONE,
        g_ui_pointer_owner == POINTER_OWNER_VERTICAL_SLIDER,
        g_ui_pointer_dragging != 0, ui_pointer_drag_is_horizontal() != 0);
    if(pointer_decision.clear_active)
        g_ui_slider_active_id = 0;

    if(pointer_decision.hovered) {
        MarkClickable();
        if(pointer_decision.start_active) {
            g_ui_slider_active_id = id;
            if(pointer_decision.set_pointer_owner)
                g_ui_pointer_owner = POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    if(pointer_decision.update_value) {
        int old_value = *value;
        *value = SliderDiscretePointerValue((float)my, (float)y, (float)h,
                                            min, max, true);
        changed = (*value != old_value);
    }
    if(pointer_decision.finish_active)
        g_ui_slider_active_id = 0;

    {
        float t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
        int active = active_visual || g_ui_slider_active_id == id;
        int hovered = pointer_decision.hovered;
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = layout.paint_bounds,
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = ContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           ContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       ContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 ContentDisabled())
        }), hovered, active, ContentDisabled());
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
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    StyleFrame metric_track_frame = ui_slider_style_frame(ButtonToneNeutral,
                                                          ButtonStateNormal,
                                                          ContentDisabled());
    StyleFrame metric_thumb_frame = ui_slider_thumb_style_frame(ButtonToneAccent,
                                                                ButtonStateNormal,
                                                                ContentDisabled());
    SliderEditorLayout layout = SliderVerticalEditorLayoutFor(
        x, y, h, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    Rectangle editor_bounds = layout.editor_bounds;
    Widget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int changed = 0;
    Rectangle hit = layout.hit_bounds;
    SliderPointerDecision pointer_decision;

    widget = BeginWidget("Slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Slider", id,
                                                 NULL),
                           editor_bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    editor_bounds = widget.bounds;
    x = (int)(editor_bounds.x + editor_bounds.width * 0.5f);
    y = (int)editor_bounds.y;
    h = (int)editor_bounds.height;
    layout = SliderVerticalEditorLayoutFor(
        x, y, h, ui_touch_target_min(), runtime_scale, metric_track_frame,
        metric_thumb_frame);
    editor_bounds = layout.editor_bounds;
    hit = layout.hit_bounds;
    WidgetSetBounds(&widget, editor_bounds);

    pointer_decision = SliderPointerDecisionFor(
        g_ui_slider_active_id == id,
        CheckCollisionPointRec(mouse_world, hit) != 0,
        InputCapturesClick(mouse_world) != 0,
        ui_input_captures_click_internal(mouse_world, 0) != 0,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, true,
        g_ui_pointer_owner == POINTER_OWNER_NONE,
        g_ui_pointer_owner == POINTER_OWNER_VERTICAL_SLIDER,
        g_ui_pointer_dragging != 0, ui_pointer_drag_is_horizontal() != 0);
    if(pointer_decision.clear_active)
        g_ui_slider_active_id = 0;

    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    if(pointer_decision.hovered) {
        MarkClickable();
        if(pointer_decision.start_active) {
            g_ui_slider_active_id = id;
            if(pointer_decision.set_pointer_owner)
                g_ui_pointer_owner = POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    if(pointer_decision.update_value) {
        int old_value = *value;
        *value = SliderDiscretePointerValue((float)my, (float)y, (float)h,
                                            min, max, true);
        changed = (*value != old_value);
    }
    if(pointer_decision.finish_active)
        g_ui_slider_active_id = 0;

    {
        float t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;
        int active = g_ui_slider_active_id == id;
        int hovered = pointer_decision.hovered;
        ButtonState state = active ? ButtonStatePressed :
                            (hovered ? ButtonStateHover : ButtonStateNormal);

        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = layout.paint_bounds,
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = ContentDisabled(),
            .scale = runtime_scale,
            .track = ui_slider_style_frame(ButtonToneNeutral, state,
                                           ContentDisabled()),
            .active_track = ui_slider_fill_style_frame(ButtonToneAccent, state,
                                                       ContentDisabled()),
            .thumb = ui_slider_thumb_style_frame(ButtonToneAccent, state,
                                                 ContentDisabled())
        }), hovered, active, ContentDisabled());
    }

    EndWidget(&widget);
    return changed;
}

int
ToggleSwitch(int x, int y, int w, int h, int *value,
             int class_name, const char *off_label, const char *on_label,
             int focused)
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
    int enabled = value != NULL && !ContentDisabled();
    StyleFrame label_frame = ui_toggle_style_frame_role_class(ButtonToneNeutral,
        ButtonStateNormal, !enabled, class_name, ToggleLabelRole());
    Style label_style = ui_unpack_style(ui_style_apply_effects_frame(label_frame).value);
    int checked_for_metrics = value != NULL && *value;
    ButtonTone metric_track_tone = checked_for_metrics && !has_labels
        ? ButtonToneAccent
        : ButtonToneNeutral;
    int metric_track_role = ToggleTrackRoleFor(checked_for_metrics,
                                               has_labels);
    StyleFrame metric_track_frame = ui_toggle_style_frame_role_class(
        metric_track_tone, ButtonStateNormal, !enabled, class_name,
        metric_track_role);
    StyleFrame metric_active_frame = ui_toggle_style_frame_role_class(
        ButtonToneAccent, ButtonStateNormal, !enabled, class_name,
        ToggleFillRole());
    StyleFrame metric_thumb_frame = ui_toggle_thumb_style_frame_class(
        metric_track_tone, ButtonStateNormal, !enabled, checked_for_metrics,
        class_name);
    font = ResolveFont(0, StyleFontValue(label_style.fields,
                                         label_style.font_size), font);
    int off_w = has_labels ? TextWidth(off_text, font) : 0;
    int on_w = has_labels ? TextWidth(on_text, font) : 0;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int min_w = ToggleMinimumWidthForStyle(has_labels, off_w, on_w,
                                           runtime_scale, metric_track_frame,
                                           metric_active_frame, label_frame);
    int min_h = ToggleMinimumHeightForStyle(runtime_scale, metric_track_frame,
                                            metric_thumb_frame);
    Rectangle bounds;
    int pressed;
    int hovered;
    int down;
    InputPointerInteraction interaction;
    if(w < min_w)
        w = min_w;
    if(h < min_h)
        h = min_h;

    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    widget = BeginWidget("Toggle",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Toggle", 0, off_text),
                           editor_bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
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
    interaction = InputPointerInteractionFor(
        CheckCollisionPointRec(mouse_world, bounds) != 0,
        InputCapturesClick(mouse_world) != 0, !enabled,
        HoverEffectsEnabled() != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, false, true);
    hovered = interaction.active;
    down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if(interaction.active)
        MarkClickable();
    if(interaction.disabled_marker)
        MarkDisabled();

    pressed = interaction.activated;

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
        int track_role = ToggleTrackRoleFor(checked, has_labels);

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
            .track = ui_toggle_style_frame_role_class(track_tone, state,
                                                !enabled, class_name,
                                                track_role),
            .active = ui_toggle_style_frame_role_class(ButtonToneAccent, state,
                                                 !enabled, class_name,
                                                 ToggleFillRole()),
            .label = ui_toggle_style_frame_role_class(ButtonToneNeutral, state,
                                                 !enabled, class_name,
                                                 ToggleLabelRole()),
            .thumb = ui_toggle_thumb_style_frame_class(track_tone, state,
                                                 !enabled, checked,
                                                 class_name)
        };
        paint = TogglePaintFor(spec);
        label_frame = ui_toggle_style_frame_role_class(ButtonToneNeutral, state,
                                                 !enabled, class_name,
                                                 ToggleLabelRole());
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
            float scale = (float)Scale(1000) / 1000.0f;
            Color focus = GetColor(track_frame.value.focus);
            Color glow = GetColor(Opacity(track_frame.value.focus, 0.24f));

            DrawRectangleRounded(paint.focus_bounds, 0.5f, 16, glow);
            DrawRectangleRoundedLinesEx(paint.focus_bounds, 0.5f, 16,
                                        (float)FocusStrokeWidthFor(scale),
                                        focus);
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
                           paint.thumb_glow_radius,
                           GetColor(paint.thumb_glow_color));
            }
            if(FancyEffectsEnabled()) {
                DrawCircle((int)paint.thumb_shadow_x,
                           (int)paint.thumb_shadow_y,
                           paint.thumb_shadow_radius,
                           GetColor(paint.thumb_shadow_color));
            }
            DrawCircle(thumb_cx, thumb_cy, paint.thumb_radius,
                       GetColor(paint.thumb_fill_color));
            if(FancyEffectsEnabled()) {
                DrawCircle((int)paint.thumb_highlight_x,
                           (int)paint.thumb_highlight_y,
                           paint.thumb_highlight_radius,
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
DrawDisabledCheckboxToggle(int x, int y, const char *label,
                             int *value, int disabled)
{
    char editor_id[96];
    Widget widget;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int checked = value != NULL && *value;
    StyleFrame label_frame = ui_checkbox_label_style_frame(
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, checked);
    Style label_style = ui_unpack_style(
        ui_style_apply_effects_frame(label_frame).value);
    int font = ResolveFont(0, StyleFontValue(label_style.fields,
                                             label_style.font_size),
                           GetFontSize());
    int label_w = TextWidth(label, font);
    CheckboxLayout layout = CheckboxLayoutForText((float)x, (float)y,
                                                  (float)label_w,
                                                  (float)TextLineHeight(font),
                                                  runtime_scale,
                                                  ui_checkbox_style_frame(
                                                      ButtonToneNeutral,
                                                      disabled ? ButtonStateDisabled : ButtonStateNormal,
                                                      disabled, checked),
                                                  label_frame);
    Rectangle bounds = layout.bounds;
    Vector2 mouse_world = ui_mouse_world();
    InputPointerInteraction interaction;
    int pressed;
    int can_draw = IsWindowReady();

    widget = BeginWidget("Checkbox",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "Checkbox", 0, label),
                           bounds,
                           WidgetFlagMovable |
                           WidgetFlagResizable);
    bounds = widget.bounds;
    x = (int)bounds.x;
    y = (int)bounds.y;
    interaction = InputPointerInteractionFor(
        CheckCollisionPointRec(mouse_world, bounds) != 0,
        InputCapturesClick(mouse_world) != 0, disabled != 0,
        HoverEffectsEnabled() != 0,
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) != 0, false, true);

    if(interaction.disabled_marker)
        MarkDisabled();
    if(interaction.active)
        MarkClickable();

    pressed = interaction.activated;
    if(pressed) {
        *value = !(*value);
        ConsumeRelease();
    }
    if(!can_draw) {
        EndWidget(&widget);
        return pressed;
    }

    {
        int hovered = interaction.hovered;
        int down = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ButtonState state = ui_checkbox_button_state(hovered, down, 0,
                                                     disabled);
        CheckboxPaint paint;

        label_frame = ui_checkbox_label_style_frame(state, disabled, checked);
        label_style = ui_unpack_style(
            ui_style_apply_effects_frame(label_frame).value);
        font = ResolveFont(0, StyleFontValue(label_style.fields,
                                             label_style.font_size), font);
        layout = CheckboxLayoutForText((float)x, (float)y, (float)label_w,
                                       (float)TextLineHeight(font),
                                       runtime_scale,
                                       ui_checkbox_style_frame(
                                           ButtonToneNeutral, state, disabled,
                                           checked),
                                       label_frame);
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
                                              disabled, checked),
            .label = label_frame
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

        RenderText(label, (int)layout.label_x, (int)layout.label_y,
                   font, GetColor(paint.label_color));
    }

    EndWidget(&widget);
    return pressed;
}

int
RenderCheckboxToggle(int x, int y, const char *label, int *value)
{
    return DrawDisabledCheckboxToggle(x, y, label, value, 0);
}
