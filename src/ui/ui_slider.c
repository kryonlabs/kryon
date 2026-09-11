#include "ui_internal.h"
#include "ui_style_internal.h"
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

int
ui_render_slider(int id, int x, int y, int w, const char *label,
                 int min, int max, int *value, const char *suffix,
                 const char *value_text_override)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)Scale(56)};
    UIWidget widget;
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

    widget = BeginUIWidget("slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "slider", id, label),
                           editor_bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
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
    UIWidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(value_text_override != NULL)
        snprintf(value_text, sizeof(value_text), "%s", value_text_override);
    else
        snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    if(can_draw) {
        RenderText(label, x, y, label_font, c_text);
        RenderText(value_text, x + w - TextWidth(value_text, value_font),
                   y, value_font, c_text);
    }

    t = max > min ? (float)(*value - min) / (float)(max - min) : 0.0f;

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
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
        Palette palette;
        Metrics tokens;
        int active = g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !UIInputCapturesClick(mouse_world);

        ui_runtime_theme_values(&palette, &tokens);
        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)x, (float)knob_y, (float)w, (float)knob_h},
            .ratio = t,
            .vertical = false,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .palette = palette,
            .metrics = tokens
        }), hovered, active, UIContentDisabled());
    }

    EndUIWidget(&widget);
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
    UIWidget widget;
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

    widget = BeginUIWidget("vertical_slider",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "vertical_slider", id, NULL),
                           editor_bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
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
    UIWidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
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
        Palette palette;
        Metrics tokens;
        int active = active_visual || g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !UIInputCapturesClick(mouse_world);

        ui_runtime_theme_values(&palette, &tokens);
        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)(x - knob_w / 2), (float)y,
                       (float)knob_w, (float)h},
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .palette = palette,
            .metrics = tokens
        }), hovered, active, UIContentDisabled());
    }

    EndUIWidget(&widget);
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
                                     UIVerticalSliderMarkCallback callback,
                                     void *callback_user_data)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - Scale(18)), (float)y,
                               (float)Scale(36), (float)h};
    UIWidget widget;
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

    widget = BeginUIWidget("vertical_slider_marks",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "vertical_slider_marks", id,
                                                 NULL),
                           editor_bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
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
    UIWidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
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
        Palette palette;
        Metrics tokens;
        int active = g_ui_slider_active_id == id;
        int hovered = CheckCollisionPointRec(mouse_world, hit) &&
                      !UIInputCapturesClick(mouse_world);

        ui_runtime_theme_values(&palette, &tokens);
        ui_draw_slider_paint(SliderPaintFor((SliderSpec){
            .bounds = {(float)(x - knob_w / 2), (float)y,
                       (float)knob_w, (float)h},
            .ratio = t,
            .vertical = true,
            .active = active,
            .hovered = hovered,
            .disabled = UIContentDisabled(),
            .scale = runtime_scale,
            .palette = palette,
            .metrics = tokens
        }), hovered, active, UIContentDisabled());
    }

    EndUIWidget(&widget);
    return changed;
}

int
ToggleSwitch(int x, int y, int w, int h, int *value,
             const char *off_label, const char *on_label, int focused)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)h};
    UIWidget widget;
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ui_touch_target_min();
    int font = GetFontSize();
    int can_draw = IsWindowReady();
    const char *off_text = off_label != NULL ? off_label : "";
    const char *on_text = on_label != NULL ? on_label : "";
    int has_labels = off_text[0] != '\0' || on_text[0] != '\0';
    int off_w = has_labels ? TextWidth(off_text, font) : 0;
    int on_w = has_labels ? TextWidth(on_text, font) : 0;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int min_w = ToggleMinimumWidth(has_labels, off_w, on_w, runtime_scale);
    int min_h = ToggleMinimumHeight(runtime_scale);
    Rectangle bounds;
    int enabled;
    int pressed;
    int hovered;
    int down;
    if(w < min_w)
        w = min_w;
    if(h < min_h)
        h = min_h;

    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    widget = BeginUIWidget("toggle",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "toggle", 0, off_text),
                           editor_bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
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
    UIWidgetSetBounds(&widget, editor_bounds);

    bounds = ui_centered_min_hit_rect(x, y, w, h, min_touch, min_touch);
    enabled = value != NULL && !UIContentDisabled();
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !UIInputCapturesClick(mouse_world);
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
        UIConsumeRelease();
    }
    if(!can_draw) {
        EndUIWidget(&widget);
        return pressed;
    }

    {
        Palette palette;
        Metrics tokens;
        ToggleSpec spec;
        TogglePaint paint;
        StyleFrame track_frame;
        Style track_style;
        StyleFrame active_frame;
        Style active_style;

        ui_runtime_theme_values(&palette, &tokens);
        spec = (ToggleSpec){
            .bounds = editor_bounds,
            .checked = value != NULL && *value,
            .enabled = enabled,
            .hovered = hovered,
            .pressed = down,
            .focused = focused,
            .has_labels = has_labels,
            .off_width = off_w,
            .on_width = on_w,
            .font = font,
            .scale = runtime_scale,
            .palette = palette,
            .metrics = tokens
        };
        paint = TogglePaintFor(spec);
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

    EndUIWidget(&widget);
    return pressed;
}

int
DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                             int *value, int disabled)
{
    char editor_id[96];
    UIWidget widget;
    int font = GetFontSize();
    int box_size = Scale(22);
    int label_gap = Scale(10);
    int label_w = TextWidth(label, font);
    int label_h = TextLineHeight(font);
    int row_h = box_size > label_h ? box_size : label_h;
    Rectangle bounds = {x, y, box_size + label_gap + label_w, row_h};
    Vector2 mouse_world = ui_mouse_world();
    Color box_color = disabled ? DarkenUIColor(c_button, 18) : c_button;
    Color mark_color = disabled ? DarkenUIColor(c_text, 35) : c_text;
    Color label_color = disabled ? DarkenUIColor(c_text, 35) : c_text;
    int pressed;
    int can_draw = IsWindowReady();

    widget = BeginUIWidget("checkbox",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "checkbox", 0, label),
                           bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
    bounds = widget.bounds;
    x = (int)bounds.x;
    y = (int)bounds.y;

    if(CheckCollisionPointRec(mouse_world, bounds) && !UIInputCapturesClick(mouse_world)) {
        if(disabled)
            MarkDisabled();
        else
            MarkClickable();
    }

    pressed = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
              !UIInputCapturesClick(mouse_world) &&
              IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(pressed) {
        *value = !(*value);
        UIConsumeRelease();
    }
    if(!can_draw) {
        EndUIWidget(&widget);
        return pressed;
    }

    if(ui_default_style()) {
        Rectangle box = {x, y + (row_h - box_size) / 2, box_size, box_size};
        int hovered = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
                      !UIInputCapturesClick(mouse_world) &&
                      UIHoverEffectsEnabled();
        ThemeScheme scheme = ui_default_scheme();
        Color fill = *value ? scheme.primary : BLANK;
        Color border = *value ? scheme.primary : scheme.on_surface_variant;
        Color state_color = *value ? scheme.primary : scheme.on_surface_variant;

        mark_color = *value ? scheme.on_primary : mark_color;
        if(disabled) {
            fill = *value ? scheme.disabled_content : BLANK;
            border = scheme.disabled_content;
            mark_color = scheme.disabled_container;
            label_color = scheme.disabled_content;
        }
        ui_default_state_layer((Rectangle){box.x - Scale(12),
                                            box.y - Scale(12),
                                            box.width + Scale(24),
                                            box.height + Scale(24)},
                                state_color, hovered, 0,
                                hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        if(fill.a != 0)
            DrawRectangleRounded(box, 0.12f, 8, fill);
        DrawRectangleRoundedLinesEx(box, 0.12f, 8, Scale(2), border);
    } else if(ui_modern_style()) {
        Rectangle box = {x, y + (row_h - box_size) / 2, box_size, box_size};
        Color border = LightenUIColor(box_color, 22);
        border.a = border.a > 150 ? 150 : border.a;
        float radius = ui_radius_px(box, GetThemeMetrics().control_radius);
        DrawRectangleRounded(box, radius, 8, box_color);
        DrawRectangleRoundedLines(box, radius, 8, border);
    } else {
        DrawRectangle(x, y + (row_h - box_size) / 2, box_size, box_size, box_color);
        RenderBevel(x, y + (row_h - box_size) / 2, box_size, box_size,
                    DarkenUIColor(c_bg, 30), LightenUIColor(c_bg, 20));
    }

    if(*value) {
        int inset = Scale(5);
        float stroke = (float)Scale(2);
        int box_y = y + (row_h - box_size) / 2;
        Vector2 start = {(float)(x + inset), (float)(box_y + box_size / 2)};
        Vector2 middle = {(float)(x + box_size / 2 - Scale(1)),
                          (float)(box_y + box_size - inset)};
        Vector2 end = {(float)(x + box_size - inset),
                       (float)(box_y + inset)};

        DrawLineEx(start, middle, stroke, mark_color);
        DrawLineEx(middle, end, stroke, mark_color);
    }

    RenderText(label, x + box_size + label_gap,
               GetUIControlTextY(label, y, row_h, font),
               font, label_color);

    EndUIWidget(&widget);
    return pressed;
}

int
RenderCheckboxToggle(int x, int y, const char *label, int *value)
{
    return DrawDisabledUICheckboxToggle(x, y, label, value, 0);
}
