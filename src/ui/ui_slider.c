#include "ui_internal.h"

int
DrawUISlider(int id, int x, int y, int w, const char *label,
             int min, int max, int *value, const char *suffix)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)ScaleUIPx(56)};
    UIWidget widget;
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int label_font = GetUIFontSize();
    int value_font = GetUIFontSize();
    int track_y = y + ScaleUIPx(28);
    int track_h = ScaleUIPx(8);
    int knob_w = ScaleUIPx(12);
    int knob_h = ScaleUIPx(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int min_touch_h = ui_touch_target_min();
    int changed = 0;
    char value_text[32];
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
    if(w < ScaleUIPx(32))
        w = ScaleUIPx(32);
    track_y = y + ScaleUIPx(28);
    knob_y = track_y - (knob_h - track_h) / 2;
    hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)ScaleUIPx(56)};
    UIWidgetSetBounds(&widget, editor_bounds);

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    DrawUIText(label, x, y, label_font, c_text);
    DrawUIText(value_text, x + w - MeasureUIText(value_text, value_font),
               y, value_font, c_text);

    t = (float)(*value - min) / (float)(max - min);
    knob_x = x + (int)(t * (float)w) - knob_w / 2;

    if(!ui_material_style() && ui_modern_style()) {
        DrawRectangleRounded((Rectangle){x, track_y, w, track_h},
                             0.5f, 8, DarkenUIColor(c_bg, 20));
    } else if(!ui_material_style()) {
        DrawRectangle(x, track_y, w, track_h, DarkenUIColor(c_bg, 28));
        DrawUIBevel(x, track_y, w, track_h,
                    DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
    }

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
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

    if(ui_material_style()) {
        int active_w = (int)(t * (float)w);
        Color inactive = ui_material_surface_container();
        Color outline = ui_material_outline();

        DrawRectangleRounded((Rectangle){x, track_y, w, ScaleUIPx(4)},
                             0.5f, 8, inactive);
        DrawRectangleRounded((Rectangle){x, track_y, active_w, ScaleUIPx(4)},
                             0.5f, 8, c_circle);
        if(g_ui_slider_active_id == id)
            ui_material_state_layer((Rectangle){knob_x - ScaleUIPx(10),
                                                knob_y - ScaleUIPx(5),
                                                knob_w + ScaleUIPx(20),
                                                knob_h + ScaleUIPx(10)},
                                    c_circle, 0, 0, 1);
        DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                   (float)ScaleUIPx(10), c_circle);
        DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                        (float)ScaleUIPx(10), outline);
    } else if(ui_modern_style()) {
        DrawCircle(knob_x + knob_w / 2, knob_y + knob_h / 2,
                   (float)(knob_h / 2), c_button);
        DrawCircleLines(knob_x + knob_w / 2, knob_y + knob_h / 2,
                        (float)(knob_h / 2), LightenUIColor(c_button, 24));
    } else {
        DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
        DrawUIBevel(knob_x, knob_y, knob_w, knob_h,
                    LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
    }

    EndUIWidget(&widget);
    return changed;
}

int
DrawUIVerticalSlider(int id, int x, int y, int h,
                     int min, int max, int *value)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - ScaleUIPx(18)), (float)y,
                               (float)ScaleUIPx(36), (float)h};
    UIWidget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = ScaleUIPx(8);
    int knob_w = ScaleUIPx(20);
    int knob_h = ScaleUIPx(12);
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
    if(h < ScaleUIPx(32))
        h = ScaleUIPx(32);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                   min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - ScaleUIPx(18)), (float)y,
                                (float)ScaleUIPx(36), (float)h};
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
        DrawUIBevel(track_x, y, track_w, h,
                    DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
    }

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
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
            DrawUIBevel(track_x, position_y, track_w, y + h - position_y,
                        LightenUIColor(c_button_hover, 35),
                        DarkenUIColor(c_button_hover, 35));
            DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
            DrawUIBevel(knob_x, knob_y, knob_w, knob_h,
                        LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
        }
    }

    EndUIWidget(&widget);
    return changed;
}

int
DrawUIVerticalSliderWithMarks(int id, int x, int y, int h,
                              int min, int max, int *value,
                              UIVerticalSliderMarkCallback callback,
                              void *callback_user_data)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)(x - ScaleUIPx(18)), (float)y,
                               (float)ScaleUIPx(36), (float)h};
    UIWidget widget;
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = ScaleUIPx(8);
    int knob_w = ScaleUIPx(20);
    int knob_h = ScaleUIPx(12);
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
    if(h < ScaleUIPx(32))
        h = ScaleUIPx(32);
    track_x = x - track_w / 2;
    hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h,
                                   min_touch_w, h);
    editor_bounds = (Rectangle){(float)(x - ScaleUIPx(18)), (float)y,
                                (float)ScaleUIPx(36), (float)h};
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
        DrawUIBevel(track_x, y, track_w, h,
                    DarkenUIColor(c_bg, 55), LightenUIColor(c_bg, 35));
    }

    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    if(CheckCollisionPointRec(mouse_world, hit) && !UIInputCapturesClick(mouse_world)) {
        MarkUIClickable();
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
            DrawUIBevel(track_x, position_y, track_w, y + h - position_y,
                        LightenUIColor(c_button_hover, 35),
                        DarkenUIColor(c_button_hover, 35));
            DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
            DrawUIBevel(knob_x, knob_y, knob_w, knob_h,
                        LightenUIColor(c_button, 40), DarkenUIColor(c_button, 40));
        }
    }

    EndUIWidget(&widget);
    return changed;
}

int
DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                   const char *off_label, const char *on_label)
{
    char editor_id[96];
    Rectangle editor_bounds = {(float)x, (float)y, (float)w, (float)h};
    UIWidget widget;
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = ui_touch_target_min();
    int font = GetUIFontSize();
    int material_style = ui_material_style();
    int off_w = material_style ? 0 : MeasureUIText(off_label, font);
    int on_w = material_style ? 0 : MeasureUIText(on_label, font);
    int min_half_w = (off_w > on_w ? off_w : on_w) + ScaleUIPx(16);
    int min_w = material_style ? ScaleUIPx(52) : min_half_w * 2 + ScaleUIPx(6);
    Rectangle bounds;
    int pressed;
    if(w < min_w)
        w = min_w;
    if(h < ScaleUIPx(34))
        h = ScaleUIPx(34);

    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    widget = BeginUIWidget("toggle",
                           ui_inspect_control_id(editor_id, sizeof(editor_id),
                                                 "toggle", 0, off_label),
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
    if(h < ScaleUIPx(34))
        h = ScaleUIPx(34);
    editor_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    UIWidgetSetBounds(&widget, editor_bounds);

    bounds = ui_centered_min_hit_rect(x, y, w, h, min_touch, min_touch);

    if(CheckCollisionPointRec(mouse_world, bounds) && !UIInputCapturesClick(mouse_world))
        MarkUIClickable();

    pressed = CheckCollisionPointRec(mouse_world, bounds) &&
              !UIInputCapturesClick(mouse_world) &&
              IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if(pressed) {
        *value = !(*value);
        UIConsumeRelease();
    }

    if(material_style) {
        int track_w = ScaleUIPx(52);
        int track_h = ScaleUIPx(32);
        int track_x = x + (w - track_w) / 2;
        int track_y = y + (h - track_h) / 2;
        int thumb_r = ScaleUIPx(*value ? 12 : 8);
        int thumb_cx = *value ? track_x + track_w - ScaleUIPx(16)
                              : track_x + ScaleUIPx(16);
        int thumb_cy = track_y + track_h / 2;
        Color track = *value ? c_circle : ui_material_surface_container();
        Color thumb = *value ? ui_material_on_color(c_circle) : ui_material_outline();
        Color outline = *value ? c_circle : ui_material_outline();

        DrawRectangleRounded((Rectangle){track_x, track_y, track_w, track_h},
                             0.50f, 12, track);
        DrawRectangleRoundedLines((Rectangle){track_x, track_y, track_w, track_h},
                                  0.50f, 12, outline);
        if(CheckCollisionPointRec(mouse_world, bounds))
            ui_material_state_layer((Rectangle){track_x - ScaleUIPx(8),
                                                track_y - ScaleUIPx(8),
                                                track_w + ScaleUIPx(16),
                                                track_h + ScaleUIPx(16)},
                                    c_circle, UIHoverEffectsEnabled(), 0,
                                    IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        DrawCircle(thumb_cx, thumb_cy, (float)thumb_r, thumb);
        EndUIWidget(&widget);
        return pressed;
    }

    {
        Color bg = DarkenUIColor(c_bg, 8);
        int track_h = h - 6;
        int track_y = y + 3;
        int active_w = (w - 6) / 2;
        int active_x = *value ? x + w - active_w - 3 : x + 3;
        Color label_color = c_text;
        int off_x = x + w / 4 - off_w / 2;
        int on_x = x + w * 3 / 4 - on_w / 2;

        if(ui_modern_style())
            DrawRectangleRounded((Rectangle){x, y, w, h}, 0.5f, 8, bg);
        else
            DrawRectangle(x, y, w, h, bg);

        DrawRectangleRounded((Rectangle){x + 3, track_y, w - 6, track_h},
                             0.5f, 8, DarkenUIColor(c_bg, 20));
        DrawRectangleRounded((Rectangle){active_x, track_y, active_w, track_h},
                             0.5f, 8, c_button);
        DrawUIText(off_label, off_x, GetUIControlTextY(off_label, y, h, font),
                   font, label_color);
        DrawUIText(on_label, on_x, GetUIControlTextY(on_label, y, h, font),
                   font, label_color);
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
    int font = GetUIFontSize();
    int box_size = ScaleUIPx(22);
    int label_gap = ScaleUIPx(10);
    int label_w = MeasureUIText(label, font);
    int label_h = GetUITextLineHeight(font);
    int row_h = box_size > label_h ? box_size : label_h;
    Rectangle bounds = {x, y, box_size + label_gap + label_w, row_h};
    Vector2 mouse_world = ui_mouse_world();
    Color box_color = disabled ? DarkenUIColor(c_button, 18) : c_button;
    Color mark_color = disabled ? DarkenUIColor(c_text, 35) : c_text;
    Color label_color = disabled ? DarkenUIColor(c_text, 35) : c_text;
    int pressed;

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
            MarkUIDisabled();
        else
            MarkUIClickable();
    }

    pressed = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
              !UIInputCapturesClick(mouse_world) &&
              IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(pressed) {
        *value = !(*value);
        UIConsumeRelease();
    }

    if(ui_material_style()) {
        Rectangle box = {x, y + (row_h - box_size) / 2, box_size, box_size};
        int hovered = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
                      !UIInputCapturesClick(mouse_world) &&
                      UIHoverEffectsEnabled();
        UIMaterialScheme scheme = ui_material_scheme();
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
        ui_material_state_layer((Rectangle){box.x - ScaleUIPx(12),
                                            box.y - ScaleUIPx(12),
                                            box.width + ScaleUIPx(24),
                                            box.height + ScaleUIPx(24)},
                                state_color, hovered, 0,
                                hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        if(fill.a != 0)
            DrawRectangleRounded(box, 0.12f, 8, fill);
        DrawRectangleRoundedLinesEx(box, 0.12f, 8, ScaleUIPx(2), border);
    } else if(ui_modern_style()) {
        Rectangle box = {x, y + (row_h - box_size) / 2, box_size, box_size};
        Color border = LightenUIColor(box_color, 22);
        border.a = border.a > 150 ? 150 : border.a;
        DrawRectangleRounded(box, ui_control_radius(0.06f), 8, box_color);
        DrawRectangleRoundedLines(box, ui_control_radius(0.06f), 8, border);
    } else {
        DrawRectangle(x, y + (row_h - box_size) / 2, box_size, box_size, box_color);
        DrawUIBevel(x, y + (row_h - box_size) / 2, box_size, box_size,
                    DarkenUIColor(c_bg, 30), LightenUIColor(c_bg, 20));
    }

    if(*value) {
        int padding = ScaleUIPx(4);
        int box_y = y + (row_h - box_size) / 2;
        DrawLine(x + padding, box_y + padding, x + box_size / 2,
                 box_y + box_size - padding, mark_color);
        DrawLine(x + box_size / 2, box_y + box_size - padding,
                 x + box_size - padding, box_y + padding, mark_color);
    }

    DrawUIText(label, x + box_size + label_gap,
               GetUIControlTextY(label, y, row_h, font),
               font, label_color);

    EndUIWidget(&widget);
    return pressed;
}

int
DrawUICheckboxToggle(int x, int y, const char *label, int *value)
{
    return DrawDisabledUICheckboxToggle(x, y, label, value, 0);
}
