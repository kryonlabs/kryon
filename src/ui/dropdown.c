#include "ui_internal.h"
#include "dropdown_store.h"
#include "ui_popup_input_internal.h"
#include "ui_style_internal.h"
#include "runtime/dropdown.h"

#include <limits.h>


/* Per-dropdown state to track open/closed and click handling */

typedef struct DropdownState {
    struct DropdownState *next;
    int id;
    int open;
    int just_opened;
    int scroll_offset;
    int x, y, w, h;
    DropdownOption *options;
    int option_count;
    int selected_index;
    int highlight_index;
    int pending_changed;
    int pending_index;
    int touch_pressed;
    int touch_press_start_y;
    int touch_press_scroll;
    int touch_drag_active;
    int scrollbar_pressed;
    int clip_top;
    int clip_bottom;
    UIPopupInputToken input_snapshot;
    unsigned long frame_seen;
    unsigned long opened_frame;
} DropdownState;

struct DropdownStore {
    DropdownState *states;
    int clip_top;
    int clip_bottom;
    int previous_focused;
};

static DropdownStore fallback_store = {.previous_focused = 1};
static DropdownStore *dropdown_store = &fallback_store;
static void dropdown_resize_options(DropdownState *state, int count);

static void
free_dropdown_states(DropdownState *state)
{
    while(state != NULL) {
        DropdownState *next = state->next;
        dropdown_resize_options(state, 0);
        free(state);
        state = next;
    }
}

DropdownStore *
dropdown_store_new(void)
{
    DropdownStore *store = calloc(1, sizeof(*store));
    if(store == NULL)
        abort();
    store->previous_focused = 1;
    return store;
}

void
dropdown_store_free(DropdownStore *store)
{
    if(store == NULL)
        return;
    if(store == dropdown_store || store == &fallback_store)
        abort();
    free_dropdown_states(store->states);
    free(store);
}

DropdownStore *
dropdown_store_swap(DropdownStore *store)
{
    DropdownStore *previous = dropdown_store;
    dropdown_store = store != NULL ? store : &fallback_store;
    return previous;
}

DropdownStore *
dropdown_store_current(void)
{
    return dropdown_store;
}

static DropdownOption *
dropdown_options_alloc(int count)
{
    if(count <= 0) return NULL;
    if((size_t)count > SIZE_MAX / sizeof(DropdownOption)) abort();
    DropdownOption *options = calloc((size_t)count, sizeof(*options));
    if(options == NULL) abort();
    return options;
}

static void
dropdown_copy_text(const char **target, const char *text)
{
    if(text == NULL) text = "";
    if(*target != NULL && strcmp(*target, text) == 0) return;
    char *copy = malloc(strlen(text) + 1);
    if(copy == NULL) abort();
    strcpy(copy, text);
    free((void *)*target);
    *target = copy;
}

static void
dropdown_resize_options(DropdownState *state, int count)
{
    if(state->option_count == count) return;
    DropdownOption *options = dropdown_options_alloc(count);
    int keep = count < state->option_count ? count : state->option_count;
    if(keep > 0) memcpy(options, state->options, (size_t)keep * sizeof(*options));
    for(int i = keep; i < state->option_count; i++) {
        free((void *)state->options[i].label);
        free((void *)state->options[i].font_name);
    }
    free(state->options);
    state->options = options;
    state->option_count = count;
}

static int
dropdown_content_height(int count, int row_height, int padding)
{
    int64_t height = (int64_t)count * row_height + padding;
    return height > INT_MAX ? INT_MAX : (int)height;
}

static Color
dropdown_panel_color(int amount)
{
    int luminance = ((int)c_bg.r + (int)c_bg.g + (int)c_bg.b) / 3;
    return luminance < 96 ? LightenUIColor(c_bg, amount) : DarkenUIColor(c_bg, amount);
}

static Color
dropdown_text_color(Color bg)
{
    int luma = (int)bg.r * 299 + (int)bg.g * 587 + (int)bg.b * 114;

    return luma > 150000 ? (Color){24, 24, 24, 255}
                         : (Color){246, 246, 246, 255};
}

/* Dropdowns use the same neutral face and accent selection as buttons. */
static Style
dropdown_style(int role, int selected, ButtonState state)
{
    ButtonProps props = {0};
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    Style base = ResolveButtonStyle(props, state);
    props.tone = ButtonToneAccent;
    props.emphasis = role == 2
        ? SelectionEmphasis(ColorToInt(GetThemeSurface())) : ButtonEmphasisFilled;
    Style accent = ResolveButtonStyle(props, ButtonStateNormal);
    return ui_unpack_style(Appearance(
        ui_pack_style_states((ControlStyle){.normal = base}).normal,
        ui_pack_style_states((ControlStyle){.normal = accent}).normal,
        ColorToInt(GetThemeSurface()), role, state, selected));
}

static ControlStyle
dropdown_trigger_style(void)
{
    return (ControlStyle){
        .normal = dropdown_style(0, 0, ButtonStateNormal),
        .hover = dropdown_style(0, 0, ButtonStateHover),
        .pressed = dropdown_style(0, 0, ButtonStatePressed),
        .focused = dropdown_style(0, 0, ButtonStateFocus),
        .disabled = dropdown_style(0, 0, ButtonStateDisabled)
    };
}

static void
dropdown_draw_surface(Rectangle bounds, Style paint, int highlighted)
{
    ui_draw_material(bounds, (Rectangle){0}, paint.background, paint.border,
        paint.border, paint.radius, paint.border_width, highlighted, 0, 0,
        paint.focus, 0, paint.opacity, ui_style_fill(paint), paint.material);
}

static void
dropdown_draw_indicator(int center_x, int center_y, int size, int open,
                        Color color)
{
    int half = size / 2;
    int left = center_x - half;
    int right = center_x + half;
    int upper = center_y - half / 2;
    int lower = center_y + half / 2;

    if(open) {
        DrawLine(left, lower, center_x, upper, color);
        DrawLine(center_x, upper, right, lower, color);
        return;
    }
    DrawLine(left, upper, center_x, lower, color);
    DrawLine(center_x, lower, right, upper, color);
}

void
dropdown_store_clip(int top, int bottom)
{
    dropdown_store->clip_top = top > 0 ? top : 0;
    dropdown_store->clip_bottom = bottom > 0 ? bottom : 0;
}

static void
dropdown_menu_layout(const DropdownState *state, int *dropdown_y, int *dropdown_h,
                     int *visible_options, int *open_up)
{
    int option_h;
    int menu_gap;
    int padding_top;
    int padding_bottom;
    int below_y;
    int below_space;
    int above_space;
    int bottom_limit;
    int max_visible_h;
    int total_h;

    if(state == NULL || dropdown_y == NULL || dropdown_h == NULL)
        return;

    option_h = state->h;
    menu_gap = Scale(4);
    padding_top = Scale(4);
    padding_bottom = Scale(4);
    total_h = dropdown_content_height(state->option_count, option_h, padding_top + padding_bottom);
    below_y = state->y + state->h + menu_gap;
    if(below_y < state->clip_top)
        below_y = state->clip_top;
    bottom_limit = state->clip_bottom > 0 ? state->clip_bottom : ui_view_height;
    below_space = bottom_limit - below_y - Scale(16);
    above_space = state->y - state->clip_top - Scale(16);

    if(below_space < 0)
        below_space = 0;
    if(above_space < 0)
        above_space = 0;

    if(open_up != NULL)
        *open_up = (above_space > below_space);

    max_visible_h = (above_space > below_space) ? above_space : below_space;
    if(total_h > max_visible_h) {
        int count = (max_visible_h - Scale(8)) / option_h;
        if(count < 1)
            count = 1;
        total_h = count * option_h + Scale(8);
        if(visible_options != NULL)
            *visible_options = count;
    } else if(visible_options != NULL) {
        *visible_options = state->option_count;
    }

    /* Flip the popup above the button when it does not fit below and
     * there is more room above. Callers used to have to request this via
     * the open_up out-param, but none did - so a tall popup near the
     * bottom of the view was sized for the space above yet placed below,
     * running off-screen with its tail options unclickable. */
    {
        int open_up_local = (below_space < total_h && above_space > below_space);

        if(open_up != NULL)
            *open_up = open_up_local;
        if(open_up_local)
            *dropdown_y = state->y - menu_gap - total_h;
        else
            *dropdown_y = below_y;
    }

    *dropdown_h = total_h;
}

static Rectangle
dropdown_menu_bounds(const DropdownState *state)
{
    int y = 0, height = 0;
    dropdown_menu_layout(state, &y, &height, NULL, NULL);
    int width = ui_clampi(state->w, 0, ui_view_width);
    int x = ui_clampi(state->x, 0, ui_view_width - width);
    return (Rectangle){x, y, width, height};
}

int
dropdown_captures(Vector2 point)
{
    for(DropdownState *state = dropdown_store->states; state != NULL; state = state->next) {
        if(!state->open || state->option_count <= 0)
            continue;
        Rectangle bounds = dropdown_menu_bounds(state);
        if(CheckCollisionPointRec(point, bounds))
            return 1;
    }
    return 0;
}

static void
close_dropdown_state(DropdownState *state)
{
    if(state == NULL)
        return;
    ui_scrollbar_cancel(&state->scroll_offset);
    state->open = 0;
    state->just_opened = 0;
    state->touch_pressed = 0;
    state->touch_drag_active = 0;
    state->scrollbar_pressed = 0;
}

void
dropdown_close(int id)
{
    for(DropdownState *state = dropdown_store->states; state != NULL; state = state->next) {
        if(state->id == id) {
            close_dropdown_state(state);
            return;
        }
    }
}

static void
close_other_dropdowns(int id)
{
    for(DropdownState *state = dropdown_store->states; state != NULL; state = state->next) {
        if(state->id != id)
            close_dropdown_state(state);
    }
}

static DropdownState *
get_or_create_dropdown_state(int id)
{
    for(DropdownState *state = dropdown_store->states; state != NULL; state = state->next) {
        if(state->id == id)
            return state;
    }
    /* State and owned option strings never alias another control's ID. */
    DropdownState *state = calloc(1, sizeof(*state));
    if(state == NULL) abort();
    state->id = id;
    state->next = dropdown_store->states;
    dropdown_store->states = state;
    return state;
}

int
draw_dropdown(int id, int x, int y, int w, int h,
               const char **options, int option_count, int *selected_index)
{
    if(option_count < 0)
        option_count = 0;
    DropdownOption *dropdown_options = dropdown_options_alloc(option_count);

    for(int i = 0; i < option_count; i++) {
        dropdown_options[i].label = options != NULL ? options[i] : NULL;
        dropdown_options[i].font_name = NULL;
    }

    int changed = draw_dropdown_options(id, x, y, w, h, dropdown_options,
                                        option_count, selected_index);
    free(dropdown_options);
    return changed;
}

int
draw_dropdown_options(int id, int x, int y, int w, int h,
                      const DropdownOption *options, int option_count,
                      int *selected_index)
{
    char editor_id[96];
    DropdownState *state = get_or_create_dropdown_state(id);
    UIWidget widget;
    Style content_style = dropdown_style(0, 0, ButtonStateNormal);
    ContentMetrics content = Content(
        ui_pack_style_states((ControlStyle){.normal = content_style}).normal,
        (float)Scale(1000) / 1000.0f);
    int font = ui_default_style() ? (int)content.font : GetFontSize();
    int arrow_pad = Scale(24);
    int arrow_size = Scale(10);
    int changed = 0;
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = ui_mouse_world();
    Color button_bg;
    Color button_text;
    int button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    int active = button_inside &&
                 (state->open
                      ? !ui_base_input_captures_click(mouse, 1)
                      : !UIInputCapturesClick(mouse));
    int hover = active && UIHoverEffectsEnabled();
    int can_draw = IsWindowReady();

    state->frame_seen = g_ui_frame_serial;
    state->input_snapshot = ui_popup_input_snapshot();
    if(UIContentDisabled()) {
        close_dropdown_state(state);
        state->pending_changed = 0;
    }

    snprintf(editor_id, sizeof(editor_id), "dropdown:%d", id);
    widget = BeginUIWidget("dropdown", editor_id, btn_bounds,
                           UI_WIDGET_MOVABLE |
                           UI_WIDGET_RESIZABLE);
    btn_bounds = widget.bounds;
    x = (int)btn_bounds.x;
    y = (int)btn_bounds.y;
    w = (int)btn_bounds.width;
    h = (int)btn_bounds.height;
    if(w < Scale(32))
        w = Scale(32);
    if(h < Scale(24))
        h = Scale(24);
    btn_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    UIWidgetSetBounds(&widget, btn_bounds);
    button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    active = !UIContentDisabled() && button_inside &&
             (state->open
                  ? !ui_base_input_captures_click(mouse, 1)
                  : !UIInputCapturesClick(mouse));
    hover = active && UIHoverEffectsEnabled();
    int focused = !UIContentDisabled() && id > 0 && RegisterUIFocus(id, btn_bounds);
    if(focused) SetUIFocusTextInputActive(0);

    if(state->pending_changed) {
        if(selected_index != NULL)
            *selected_index = state->pending_index;
        state->selected_index = state->pending_index;
        state->pending_changed = 0;
        changed = 1;
    }

    /* Calculate arrow position */
    int arrow_x = x + w - arrow_pad;
    int arrow_y = y + h / 2;

    /* Store state for overlay drawing */
    state->x = x;
    state->y = y;
    state->w = w;
    state->h = h;
    state->clip_top = dropdown_store->clip_top;
    state->clip_bottom = dropdown_store->clip_bottom;
    state->selected_index = selected_index != NULL ? *selected_index : state->selected_index;
    if(option_count < 0)
        option_count = 0;
    dropdown_resize_options(state, option_count);
    for(int i = 0; i < option_count; i++) {
        state->options[i].icon_type = options != NULL ? options[i].icon_type : UI_ICON_TYPE_NONE;
        state->options[i].disabled = options != NULL && options[i].disabled;
        state->options[i].separator_before = options != NULL && options[i].separator_before;
        dropdown_copy_text(&state->options[i].label, options != NULL ? options[i].label : NULL);
        const char *font_name = options != NULL ? options[i].font_name : NULL;
        if(font_name != NULL && font_name[0] != '\0')
            dropdown_copy_text(&state->options[i].font_name, font_name);
        else {
            free((void *)state->options[i].font_name);
            state->options[i].font_name = NULL;
        }
    }

    if(active)
        MarkUIClickable();

    /* Handle click on button */
    int keyboard_open = focused && !state->open && !ui_popup_input_keyboard_captures() &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
         IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_DOWN));
    if((active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || keyboard_open) {
        ClearTextInputFocus();
        if(!keyboard_open) UIConsumeRelease();
        state->open = !state->open;
        if(state->open) {
            close_other_dropdowns(id);
            state->just_opened = 1;
            state->opened_frame = g_ui_frame_serial;
            state->scroll_offset = 0;
            state->highlight_index = state->selected_index;
            state->touch_drag_active = 0;
        }
    }

    /* Draw button background */
    button_bg = state->open ? dropdown_panel_color(28)
                            : (hover ? c_button_hover : dropdown_panel_color(16));
    if(can_draw) {
        if(ui_default_style()) {
            ButtonSpec button = {0};
            button.bounds = btn_bounds;
            button.focus_id = id;
            button.tone = ButtonToneNeutral;
            button.emphasis = ButtonEmphasisSoft;
            button.style_resolved = 1;
            button.style = dropdown_trigger_style();
            button.disabled = UIContentDisabled();
            button_text = ui_paint_button(button, hover || state->open,
                active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        } else if(ui_modern_style()) {
            Color border = LightenUIColor(button_bg, 20);
            ui_draw_control_background(btn_bounds, button_bg, border, 0.06f);
        } else {
            DrawRectangleRec(btn_bounds, button_bg);
            DrawUIBevel(x, y, w, h,
                        state->open ? LightenUIColor(button_bg, 34)
                                    : LightenUIColor(button_bg, 24),
                        state->open ? DarkenUIColor(button_bg, 38)
                                    : DarkenUIColor(button_bg, 30));
        }
    }
    if(!can_draw || !ui_default_style())
        button_text = dropdown_text_color(button_bg);

    /* Draw current selection text, clipped before the chevron. */
    int current_index = state->selected_index;
    if(current_index < 0 || current_index >= option_count)
        current_index = 0;
    const char *current_name = option_count > 0 ? state->options[current_index].label : "";
    const char *current_font = option_count > 0 ? state->options[current_index].font_name : NULL;
    int text_x = x + (int)content.padding;
    if(option_count > 0 && state->options[current_index].icon_type != UI_ICON_TYPE_NONE) {
        if(can_draw)
            DrawIcon(state->options[current_index].icon_type,
                (Rectangle){text_x, y + (h - content.icon) / 2, content.icon, content.icon}, button_text);
        text_x += (int)(content.icon + content.gap);
    }
    int text_w = arrow_x - arrow_size - Scale(8) - text_x;
    if(can_draw && text_w > 0) {
        int font_token = PushUIFont(current_font);
        BeginUIClip((int)(g_ui_camera.offset.x + (float)text_x * g_ui_camera.zoom),
                         (int)(g_ui_camera.offset.y + (float)y * g_ui_camera.zoom),
                         (int)((float)text_w * g_ui_camera.zoom),
                         (int)((float)h * g_ui_camera.zoom));
        DrawUIText(current_name, text_x, GetUIControlTextY(current_name, y, h, font), font, button_text);
        EndUIClip();
        PopUIFont(font_token);
    }

    if(can_draw)
        dropdown_draw_indicator(arrow_x, arrow_y, arrow_size,
                                state->open, button_text);

    if(can_draw && focused && !ui_default_style()) DrawUIFocus(btn_bounds);
    EndUIWidget(&widget);
    return changed;
}

static int
draw_dropdown_menu(int id)
{
    DropdownState *state = get_or_create_dropdown_state(id);
    int changed = 0;
    int keyboard_available;

    if(!state->open)
        return 0;
    if(state->option_count <= 0) {
        close_dropdown_state(state);
        return 0;
    }
    keyboard_available =
        !ui_popup_input_snapshot_keyboard_captures(state->input_snapshot);

    Style content_style = dropdown_style(0, 0, ButtonStateNormal);
    ContentMetrics content = Content(
        ui_pack_style_states((ControlStyle){.normal = content_style}).normal,
        (float)Scale(1000) / 1000.0f);
    int font = ui_default_style() ? (int)content.font : GetFontSize();
    int x = state->x;
    int y = state->y;
    int w = state->w;
    int h = state->h;
    int option_h = h;
    int option_count = state->option_count;
    const DropdownOption *options = state->options;
    Color panel = dropdown_panel_color(18);
    Color option_text = dropdown_text_color(panel);
    int can_draw = IsWindowReady();
    int clip_started = 0;

    int dropdown_y = 0;
    int dropdown_h = 0;
    int padding_top = Scale(4);
    int padding_bottom = Scale(4);
    int content_h = dropdown_content_height(option_count, option_h, padding_top + padding_bottom);
    int max_scroll;
    int scrollbar_w = Scale(8);
    Rectangle btn_bounds = {x, y, w, h};
    Rectangle menu_bounds = dropdown_menu_bounds(state);
    x = (int)menu_bounds.x;
    w = (int)menu_bounds.width;
    dropdown_y = (int)menu_bounds.y;
    dropdown_h = (int)menu_bounds.height;
    int option_w = w;
    max_scroll = content_h - dropdown_h;
    if(max_scroll < 0)
        max_scroll = 0;
    if(state->scroll_offset > max_scroll)
        state->scroll_offset = max_scroll;
    if(state->scroll_offset < 0)
        state->scroll_offset = 0;
    if(max_scroll > 0)
        option_w = w - scrollbar_w - Scale(2);

    Vector2 mouse = ui_mouse_world();
    int my = (int)mouse.y;
    int pointer_in_dropdown = CheckCollisionPointRec(mouse, btn_bounds) ||
                              CheckCollisionPointRec(mouse, menu_bounds);
    Rectangle scrollbar_bounds = {x + w - scrollbar_w, dropdown_y, scrollbar_w, dropdown_h};
    if(max_scroll > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(mouse, scrollbar_bounds))
        state->scrollbar_pressed = 1;

    /* Track pointer movement to distinguish click from drag */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !state->scrollbar_pressed) {
        if(!state->touch_pressed && pointer_in_dropdown) {
            /* Pointer just went down - reset drag state */
            state->touch_pressed = 1;
            state->touch_press_start_y = my;
            state->touch_press_scroll = state->scroll_offset;
            state->touch_drag_active = 0;
        } else if(state->touch_pressed && !state->touch_drag_active) {
            /* Movement beyond the threshold makes it a drag - but only
             * when the list can actually scroll, otherwise a touchpad
             * clicks natural wobble would swallow every selection. */
            int dy = my - state->touch_press_start_y;
            if(abs(dy) > Scale(8) && max_scroll > 0) {
                state->touch_drag_active = 1;
            }
        }

        /* If dragging, scroll the dropdown */
        if(state->touch_drag_active && max_scroll > 0) {
            int dy = my - state->touch_press_start_y;
            state->scroll_offset -= dy;
            /* Update start position for continuous scrolling */
            state->touch_press_start_y = my;

            /* Clamp scroll offset */
            if(state->scroll_offset < 0)
                state->scroll_offset = 0;
            if(state->scroll_offset > max_scroll)
                state->scroll_offset = max_scroll;
        }
    } else if(state->touch_pressed) {
        /* Pointer just released - only reset touch_pressed, keep touch_drag_active for selection check */
        state->touch_pressed = 0;
    }

    /* Click outside closes dropdown */
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(!state->just_opened &&
            !CheckCollisionPointRec(mouse, btn_bounds) &&
           !CheckCollisionPointRec(mouse, menu_bounds)) {
            close_dropdown_state(state);
        }
    }

    if(!state->open) return 0;
    state->highlight_index = ui_clampi(state->highlight_index, 0, option_count - 1);
    int previous_highlight = state->highlight_index;
    int navigating = 1;
    if(state->opened_frame == g_ui_frame_serial)
        navigating = 0;
    else if(keyboard_available && IsKeyPressed(KEY_UP))
        state->highlight_index = ui_clampi(state->highlight_index - 1, 0, option_count - 1);
    else if(keyboard_available && IsKeyPressed(KEY_DOWN))
        state->highlight_index = ui_clampi(state->highlight_index + 1, 0, option_count - 1);
    else if(keyboard_available && IsKeyPressed(KEY_HOME))
        state->highlight_index = 0;
    else if(keyboard_available && IsKeyPressed(KEY_END))
        state->highlight_index = option_count - 1;
    else
        navigating = 0;
    if(navigating || state->just_opened) {
        int direction = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_END) ? -1 : 1;
        int candidate = state->highlight_index;
        while(candidate >= 0 && candidate < option_count && options[candidate].disabled)
            candidate += direction;
        if(candidate >= 0 && candidate < option_count)
            state->highlight_index = candidate;
        else if(!options[previous_highlight].disabled)
            state->highlight_index = previous_highlight;
        else {
            candidate = previous_highlight;
            while(candidate >= 0 && candidate < option_count && options[candidate].disabled)
                candidate -= direction;
            if(candidate >= 0 && candidate < option_count)
                state->highlight_index = candidate;
        }
        int64_t row_top = (int64_t)state->highlight_index * option_h;
        int viewport = dropdown_h - padding_top - padding_bottom;
        int64_t scroll = state->scroll_offset;
        if(row_top < scroll) scroll = row_top;
        if(row_top + option_h > scroll + viewport)
            scroll = row_top + option_h - viewport;
        if(scroll < 0) scroll = 0;
        if(scroll > max_scroll) scroll = max_scroll;
        state->scroll_offset = (int)scroll;
    }
    if(keyboard_available && state->opened_frame != g_ui_frame_serial &&
       (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) &&
       !options[state->highlight_index].disabled) {
        state->pending_index = state->highlight_index;
        state->pending_changed = state->selected_index != state->highlight_index;
        state->selected_index = state->highlight_index;
        close_dropdown_state(state);
        return state->pending_changed;
    }

    if(state->just_opened && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        state->just_opened = 0;

    if(CheckCollisionPointRec(mouse, menu_bounds)) {
        float wheel = GetMouseWheelMove();
        if(wheel != 0.0f && max_scroll > 0) {
            state->scroll_offset -= (int)(wheel * (float)option_h);
            if(state->scroll_offset < 0)
                state->scroll_offset = 0;
            if(state->scroll_offset > max_scroll)
                state->scroll_offset = max_scroll;
        }
    }

    /* Draw dropdown background */
    if(can_draw) {
        if(ui_default_style()) {
            Style paint = dropdown_style(1, 0, ButtonStateNormal);
            paint.radius = GetThemeMetrics().radius_large + 2;
            panel = paint.background;
            option_text = paint.foreground;
            dropdown_draw_surface(menu_bounds, paint, 0);
        } else if(ui_modern_style()) {
            ThemeMetrics tokens = GetThemeMetrics();
            Color border = dropdown_panel_color(36);
            if(tokens.panel_alpha < panel.a)
                panel.a = tokens.panel_alpha;
            option_text = dropdown_text_color(panel);
            ui_draw_control_background(
                (Rectangle){x, dropdown_y, w, dropdown_h}, panel, border,
                ui_radius_px((Rectangle){x, dropdown_y, w, dropdown_h},
                             tokens.panel_radius));
        } else {
            DrawRectangle(x, dropdown_y, w, dropdown_h, panel);
            DrawUIBevel(x, dropdown_y, w, dropdown_h,
                        dropdown_panel_color(32),
                        dropdown_panel_color(8));
        }
    }

    /* Resolve thumb input before rows use the offset, so thumb and content
     * paint the same state on the drag frame. The panel is already painted. */
    if(max_scroll > 0)
        ui_scrollbar(x + w - scrollbar_w, dropdown_y + Scale(2),
                     dropdown_h - Scale(4), content_h, &state->scroll_offset, max_scroll, 1);

    if(can_draw) {
        BeginUIClip((int)(g_ui_camera.offset.x + (float)x * g_ui_camera.zoom),
                    (int)(g_ui_camera.offset.y +
                          (float)(dropdown_y + padding_top) * g_ui_camera.zoom),
                    (int)((float)option_w * g_ui_camera.zoom),
                    (int)((float)(dropdown_h - padding_top - padding_bottom) * g_ui_camera.zoom));
        clip_started = 1;
    }

    /* Draw options */
    int first = state->scroll_offset / option_h;
    int64_t last = ((int64_t)state->scroll_offset + dropdown_h - padding_top - padding_bottom + option_h - 1) / option_h;
    if(last > option_count) last = option_count;
    for(int i = first; i < last; i++) {
        int option_y = (int)((int64_t)dropdown_y + padding_top + (int64_t)i * option_h - state->scroll_offset);
        int content_top = dropdown_y + padding_top;
        int content_bottom = dropdown_y + dropdown_h - padding_bottom;
        int visible_y = option_y > content_top ? option_y : content_top;
        int option_bottom = option_y + option_h;
        int visible_bottom = option_bottom < content_bottom ? option_bottom : content_bottom;
        int visible_h = visible_bottom - visible_y;
        Rectangle visible_bounds = {x, visible_y, option_w, visible_h};

        /* Skip if outside visible area - use inclusive bounds for last item */
        if(visible_h <= 0)
            continue;

        int option_active = !options[i].disabled && CheckCollisionPointRec(mouse, visible_bounds);
        int option_hover = !options[i].disabled && (state->highlight_index == i ||
                           (option_active && UIHoverEffectsEnabled()));

        Color row_text = option_text;
        if(can_draw && ui_default_style()) {
            int selected = state->selected_index == i;
            Style paint = dropdown_style(2, selected,
                options[i].disabled ? ButtonStateDisabled :
                (option_hover ? ButtonStateHover : ButtonStateNormal));
            row_text = GetColor(Opacity(ColorToInt(paint.foreground), paint.opacity));
            if(selected || option_hover) {
                Rectangle row = {x + Scale(4), option_y + Scale(2),
                    option_w - Scale(8), option_h - Scale(4)};
                if(row.width > 0 && row.height > 0)
                    dropdown_draw_surface(row, paint, option_hover && !selected);
            }
        }

        {
            if(can_draw && option_hover && !ui_default_style()) {
                if(ui_modern_style()) {
                    ThemeMetrics tokens = GetThemeMetrics();
                    int inset = Scale(4);
                    Rectangle hover_bounds = {
                        (float)(x + inset),
                        (float)(visible_y + Scale(2)),
                        (float)(option_w - inset * 2),
                        (float)(visible_h - Scale(4))
                    };
                    if(hover_bounds.width > 0 && hover_bounds.height > 0)
                        DrawRectangleRounded(hover_bounds,
                                             ui_radius_px(hover_bounds,
                                                          tokens.control_radius),
                                             12, c_button_hover);
                } else {
                    DrawRectangle(x, visible_y, option_w, visible_h, c_button_hover);
                }
            }
            if(option_active) MarkUIClickable();

            if(option_active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !state->just_opened && !state->scrollbar_pressed &&
               (!state->touch_drag_active ||
                state->scroll_offset == state->touch_press_scroll)) {
                ClearTextInputFocus();
                UIConsumeRelease();
                state->selected_index = i;
                state->pending_index = i;
                state->pending_changed = 1;
                close_dropdown_state(state);
                state->scroll_offset = 0;
                changed = 1;
                if(clip_started)
                    EndUIClip();
                goto draw_arrow;
            }
        }

        if(can_draw) {
            int font_token = PushUIFont(options[i].font_name);
            int text_x = x + (int)content.padding;
            if(options[i].separator_before)
                DrawLine(x + Scale(16), option_y, x + option_w - Scale(16), option_y,
                    Fade(row_text, 0.18f));
            if(options[i].icon_type != UI_ICON_TYPE_NONE) {
                DrawIcon(options[i].icon_type,
                    (Rectangle){text_x, option_y + (option_h - content.icon) / 2, content.icon, content.icon}, row_text);
                text_x += (int)(content.icon + content.gap);
            }
            int text_w = x + option_w - (int)(content.padding + content.icon + content.gap) - text_x;
            BeginUIClip((int)(g_ui_camera.offset.x + text_x * g_ui_camera.zoom),
                (int)(g_ui_camera.offset.y + visible_y * g_ui_camera.zoom),
                (int)(fmaxf(0, text_w) * g_ui_camera.zoom),
                (int)(visible_h * g_ui_camera.zoom));
            DrawUIText(options[i].label, text_x,
                       GetUIControlTextY(options[i].label, option_y, option_h, font),
                       font, row_text);
            EndUIClip();
            PopUIFont(font_token);
            if(state->selected_index == i) {
                int cx = x + option_w - (int)(content.padding + content.icon / 2);
                int cy = option_y + option_h / 2;
                DrawIcon(UI_ICON_TYPE_CHECK,
                    (Rectangle){cx - content.icon / 2, cy - content.icon / 2, content.icon, content.icon}, row_text);
            }
        }
    }

    if(clip_started)
        EndUIClip();

    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) state->scrollbar_pressed = 0;

draw_arrow:
    ;

    /* Redraw arrow on top of everything */
    int arrow_pad = Scale(24);
    int arrow_size = Scale(10);
    int arrow_x = state->x + state->w - arrow_pad;
    int arrow_y = y + h / 2;

    if(can_draw && !ui_default_style())
        dropdown_draw_indicator(arrow_x, arrow_y, arrow_size,
                                state->open, option_text);
    return changed;
}

void
draw_dropdown_overlays(void)
{
    /* Escape and losing the window focus dismiss open popups, so a
     * dropdown can never trap the pointer state. */
    int focused = IsWindowFocused();
    int lost_focus = dropdown_store->previous_focused && !focused;
    int escape_pressed = IsKeyPressed(KEY_ESCAPE);

    dropdown_store->previous_focused = focused;
    if(lost_focus || escape_pressed) {
        for(DropdownState *state = dropdown_store->states;
            state != NULL; state = state->next) {
            if(escape_pressed && !lost_focus &&
               ui_popup_input_snapshot_keyboard_captures(
                   state->input_snapshot))
                continue;
            close_dropdown_state(state);
        }
    }
    DropdownState **link = &dropdown_store->states;
    while(*link != NULL) {
        DropdownState *state = *link;
        if(state->frame_seen != g_ui_frame_serial) {
            *link = state->next;
            close_dropdown_state(state);
            dropdown_resize_options(state, 0);
            free(state);
            continue;
        }
        if(state->open)
            draw_dropdown_menu(state->id);
        link = &state->next;
    }
}

/* ================================================================
 * TUTORIAL HELPERS
 * ================================================================ */
