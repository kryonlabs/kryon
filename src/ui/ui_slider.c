#include "ui_internal.h"
#include "ui_style_internal.h"

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
    int track_y = y + Scale(28);
    int track_h = Scale(8);
    int knob_w = Scale(12);
    int knob_h = Scale(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int min_touch_h = ui_touch_target_min();
    int changed = 0;
    int can_draw = IsWindowReady();
    char value_text[48];
    Rectangle hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);
    float t;
    int knob_x;

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
    if(w < Scale(32))
        w = Scale(32);
    track_y = y + Scale(28);
    knob_y = track_y - (knob_h - track_h) / 2;
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

    t = (float)(*value - min) / (float)(max - min);
    knob_x = x + (int)(t * (float)w) - knob_w / 2;

    if(can_draw) {
        if(!ui_default_style() && ui_modern_style()) {
            DrawRectangleRounded((Rectangle){x, track_y, w, track_h},
                                 0.5f, 8, DarkenUIColor(c_bg, 20));
        } else if(!ui_default_style()) {
            DrawRectangle(x, track_y, w, track_h, DarkenUIColor(c_bg, 28));
            RenderBevel(x, track_y, w, track_h,
                        DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
        }
    }

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

    t = (float)(*value - min) / (float)(max - min);
    knob_x = x + (int)(t * (float)w) - knob_w / 2;

    if(can_draw) {
        if(ui_default_style()) {
            int active_w = (int)(t * (float)w);
            Color inactive = ui_default_surface_container();
            Color outline = ui_default_outline();

            DrawRectangleRounded((Rectangle){x, track_y, w, Scale(4)},
                                 0.5f, 8, inactive);
            DrawRectangleRounded((Rectangle){x, track_y, active_w, Scale(4)},
                                 0.5f, 8, c_circle);
            if(g_ui_slider_active_id == id)
                ui_default_state_layer((Rectangle){knob_x - Scale(10),
                                                    knob_y - Scale(5),
                                                    knob_w + Scale(20),
                                                    knob_h + Scale(10)},
                                        c_circle, 0, 0, 1);
            DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                       (float)Scale(10), c_circle);
            DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                            (float)Scale(10), outline);
        } else if(ui_modern_style()) {
            DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                       (float)(knob_h / 2), c_button);
            DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                            (float)(knob_h / 2), LightenUIColor(c_button, 24));
        } else {
            DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
            RenderBevel(knob_x, knob_y, knob_w, knob_h,
                        LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
        }
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
    int track_w = Scale(8);
    int knob_w = Scale(20);
    int knob_h = Scale(12);
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
    if(h < Scale(32))
        h = Scale(32);
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

    if(ui_modern_style()) {
        Color track = active_visual ? DarkenUIColor(c_button, 72)
                                    : DarkenUIColor(c_bg, 20);
        if(active_visual) {
            DrawRectangleRounded((Rectangle){track_x - Scale(5), y - Scale(5),
                                             track_w + Scale(10), h + Scale(10)},
                                 0.5f, 12, ui_alpha(c_button, 34));
        }
        DrawRectangleRounded((Rectangle){track_x, y, track_w, h},
                             0.5f, 8, track);
    } else {
        DrawRectangle(track_x, y, track_w, h, DarkenUIColor(c_bg, 28));
        RenderBevel(track_x, y, track_w, h,
                    DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
    }

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
        float t = (float)(*value - min) / (float)(max - min);
        int position_y = y + h - (int)(t * (float)h);
        int knob_y;
        int knob_x = track_x - (knob_w - track_w) / 2;

        if(position_y < y)
            position_y = y;
        if(position_y > y + h)
            position_y = y + h;
        knob_y = position_y - knob_h / 2;
        if(knob_y < y)
            knob_y = y;
        if(knob_y + knob_h > y + h)
            knob_y = y + h - knob_h;

        if(ui_modern_style()) {
            Color active = active_visual ? c_button : c_button_hover;
            Color thumb = active_visual ? c_button_hover : c_button;
            DrawRectangleRounded((Rectangle){track_x, position_y, track_w,
                                             y + h - position_y},
                                 0.5f, 8, active);
            if(active_visual) {
                DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                           (float)(knob_w / 2 + Scale(7)), ui_alpha(c_button, 54));
            }
            DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                       (float)(knob_w / 2), thumb);
            DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                            (float)(knob_w / 2), LightenUIColor(thumb, 36));
        } else {
            DrawRectangle(track_x, position_y, track_w, y + h - position_y,
                          c_button_hover);
            RenderBevel(track_x, position_y, track_w, y + h - position_y,
                        LightenUIColor(c_button_hover, 35),
                        DarkenUIColor(c_button_hover, 35));
            DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
            RenderBevel(knob_x, knob_y, knob_w, knob_h,
                        LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
        }
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
    int track_w = Scale(8);
    int knob_w = Scale(20);
    int knob_h = Scale(12);
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
    if(h < Scale(32))
        h = Scale(32);
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

    if(ui_modern_style()) {
        DrawRectangleRounded((Rectangle){track_x, y, track_w, h},
                             0.5f, 8, DarkenUIColor(c_bg, 20));
    } else {
        DrawRectangle(track_x, y, track_w, h, DarkenUIColor(c_bg, 28));
        RenderBevel(track_x, y, track_w, h,
                    DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
    }

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
        float t = (float)(*value - min) / (float)(max - min);
        int position_y = y + h - (int)(t * (float)h);
        int knob_y;
        int knob_x = track_x - (knob_w - track_w) / 2;

        if(position_y < y)
            position_y = y;
        if(position_y > y + h)
            position_y = y + h;
        knob_y = position_y - knob_h / 2;
        if(knob_y < y)
            knob_y = y;
        if(knob_y + knob_h > y + h)
            knob_y = y + h - knob_h;

        if(ui_modern_style()) {
            DrawRectangleRounded((Rectangle){track_x, position_y, track_w,
                                             y + h - position_y},
                                 0.5f, 8, c_button_hover);
            DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                       (float)(knob_w / 2), c_button);
            DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                            (float)(knob_w / 2), LightenUIColor(c_button, 24));
        } else {
            DrawRectangle(track_x, position_y, track_w, y + h - position_y,
                          c_button_hover);
            RenderBevel(track_x, position_y, track_w, y + h - position_y,
                        LightenUIColor(c_button_hover, 35),
                        DarkenUIColor(c_button_hover, 35));
            DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
            RenderBevel(knob_x, knob_y, knob_w, knob_h,
                        LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
        }
    }

    EndUIWidget(&widget);
    return changed;
}

int
RenderToggleSwitch(int x, int y, int w, int h, int *value,
                   const char *off_label, const char *on_label)
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
    int min_half_w = (off_w > on_w ? off_w : on_w) + Scale(16);
    int min_w = has_labels ? min_half_w * 2 + Scale(6) : Scale(54);
    Rectangle bounds;
    int enabled;
    int pressed;
    int hovered;
    int down;
    if(w < min_w)
        w = min_w;
    if(h < Scale(34))
        h = Scale(34);

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
    if(h < Scale(34))
        h = Scale(34);
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
        int checked = value != NULL && *value;
        ButtonProps track_props = {.tone = checked ? ButtonToneAccent
                                                   : ButtonToneNeutral,
            .emphasis = checked ? ButtonEmphasisFilled : ButtonEmphasisSoft,
            .disabled = !enabled, .pill = 1};
        Style track_style = ui_style_apply_effects(ResolveButtonStyle(
            track_props,
            down ? ButtonStatePressed
                 : hovered ? ButtonStateHover : ButtonStateNormal));
        Rectangle track_bounds;
        int track_w = has_labels ? w : Scale(54);
        int track_h = has_labels ? h : Scale(32);
        int track_x;
        int track_y;

        if(track_w > w)
            track_w = w;
        if(track_h > h)
            track_h = h;
        track_x = x + (w - track_w) / 2;
        track_y = y + (h - track_h) / 2;
        track_bounds = (Rectangle){track_x, track_y, track_w, track_h};
        ui_draw_material(track_bounds, (Rectangle){0}, track_style.background,
                         track_style.border, track_style.border,
                         track_style.radius, track_style.border_width,
                         hovered ? 1.0f : 0.0f, down ? 1.0f : 0.0f,
                         !enabled, track_style.focus, 0.0f,
                         track_style.opacity, ui_style_fill(track_style),
                         track_style.material);

        if(has_labels) {
            ButtonProps active_props = {.tone = ButtonToneAccent,
                .emphasis = ButtonEmphasisFilled, .disabled = !enabled,
                .pill = 1};
            Style active_style = ui_style_apply_effects(
                ResolveButtonStyle(active_props, ButtonStateNormal));
            int active_w = (track_w - Scale(6)) / 2;
            int active_x = checked ? track_x + track_w - active_w - Scale(3)
                                   : track_x + Scale(3);
            Color label_color = enabled ? c_text : DarkenUIColor(c_text, 42);

            ui_draw_material((Rectangle){active_x, track_y + Scale(3),
                                         active_w, track_h - Scale(6)},
                             track_bounds, active_style.background,
                             active_style.border, active_style.border,
                             active_style.radius, active_style.border_width,
                             0.0f, 0.0f, !enabled, active_style.focus, 0.0f,
                             active_style.opacity, ui_style_fill(active_style),
                             active_style.material);
            RenderText(off_text, x + w / 4 - off_w / 2,
                       GetUIControlTextY(off_text, y, h, font),
                       font, label_color);
            RenderText(on_text, x + w * 3 / 4 - on_w / 2,
                       GetUIControlTextY(on_text, y, h, font),
                       font, label_color);
        } else {
            ButtonProps thumb_props = {.tone = checked ? ButtonToneAccent
                                                       : ButtonToneNeutral,
                .emphasis = checked ? ButtonEmphasisFilled
                                    : ButtonEmphasisSoft,
                .disabled = !enabled, .circle = 1};
            Style thumb_style = ui_style_apply_effects(ResolveButtonStyle(
                thumb_props,
                down ? ButtonStatePressed
                     : hovered ? ButtonStateHover : ButtonStateNormal));
            int thumb_size = checked ? Scale(24) : Scale(20);
            int thumb_x;
            int thumb_y;
            Rectangle thumb_bounds;

            if(down)
                thumb_size += Scale(2);
            if(thumb_size > track_h - Scale(6))
                thumb_size = track_h - Scale(6);
            thumb_x = checked ? track_x + track_w - thumb_size - Scale(4)
                              : track_x + Scale(4);
            thumb_y = track_y + (track_h - thumb_size) / 2;
            thumb_bounds = (Rectangle){thumb_x, thumb_y,
                                       thumb_size, thumb_size};
            if(hovered && enabled) {
                Color glow = checked ? track_style.background
                                     : track_style.border;
                glow.a = glow.a > 92 ? 92 : glow.a;
                DrawCircle(thumb_x + thumb_size / 2,
                           thumb_y + thumb_size / 2,
                           (float)(thumb_size / 2 + Scale(5)), glow);
            }
            ui_draw_material(thumb_bounds, track_bounds,
                             checked ? ui_default_on_color(track_style.background)
                                     : thumb_style.background,
                             thumb_style.border, thumb_style.border,
                             1.0f, thumb_style.border_width,
                             hovered ? 1.0f : 0.0f,
                             down ? 1.0f : 0.0f,
                             !enabled, thumb_style.focus, 0.0f,
                             thumb_style.opacity, ui_style_fill(thumb_style),
                             thumb_style.material);
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
        int padding = Scale(4);
        int box_y = y + (row_h - box_size) / 2;
        DrawLine(x + padding, box_y + padding, x + box_size / 2,
                 box_y + box_size - padding, mark_color);
        DrawLine(x + box_size / 2, box_y + box_size - padding,
                 x + box_size - padding, box_y + padding, mark_color);
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
