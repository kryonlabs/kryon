#include "ui_internal.h"
#include "dropdown_store.h"
#include "ui_popup_input_internal.h"
#include "ui_style_internal.h"
#include "runtime/dropdown.h"
#include "ui_paint_internal.h"

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
    PopupGesture gesture;
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

/* Dropdowns use the same neutral face and accent selection as buttons. */
static Style
dropdown_style(int role, int selected, ButtonState state)
{
    ButtonProps props = {0};
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    Style base = ResolveButtonStyle(props, state);
    if(role == 0)
        return ui_style_apply_effects(base);
    props.tone = ButtonToneAccent;
    props.emphasis = role == 2
        ? SelectionEmphasis(ColorToInt(GetThemeSurface())) : ButtonEmphasisFilled;
    Style accent = ResolveButtonStyle(props, ButtonStateNormal);
    return ui_style_apply_effects(ui_unpack_style(Appearance(
        ui_pack_style_states((ControlStyle){.normal = base}).normal,
        ui_pack_style_states((ControlStyle){.normal = accent}).normal,
        ColorToInt(GetThemeSurface()), role, state, selected)));
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

static Color
dropdown_paint_trigger(int id, Rectangle bounds, int hovered, int pressed, int focused)
{
    ButtonProps props = {.id = id, .bounds = bounds, .tone = ButtonToneNeutral,
        .emphasis = ButtonEmphasisSoft, .disabled = UIContentDisabled(),
        .style = dropdown_trigger_style()};
    Activation sample = {.hovered = hovered, .pressed = pressed, .focused = focused};
    if(focused && IsUIFocusActivatePressed(id))
        sample.pressed = true;
    ButtonInput input = ResolveButtonInput((int)props.state, props.disabled,
        props.loading, props.selected, sample);
    ThemeMetrics metrics = GetThemeMetrics();
    unsigned int key = (2166136261u ^ (unsigned int)id) * 16777619u;
    if(id == 0) {
        key = (key ^ (unsigned int)(int)bounds.x) * 16777619u;
        key = (key ^ (unsigned int)(int)bounds.y) * 16777619u;
    }
    InteractionMotion motion = AdvanceButtonMotion(key, (int)props.state, input,
        UITransitionCuesEnabled(), GetFrameTime() * 1000.0f,
        metrics.transition_normal_ms, metrics.transition_fast_ms);
    StyleFrame appearance = ui_button_style_frame(props, input.interaction.state,
        1, motion.hover.value, motion.press.value, motion.focus.value);
    ButtonFrame frame = BuildFrame(props, input, appearance, motion, (Rectangle){0},
        ColorToInt(GetThemeSurface()), (float)Scale(1000) / 1000.0f,
        Scale(appearance.value.font_size), GetFontSize());
    if(frame.repaint)
        InvalidateTree(UI_INVALIDATE_PAINT);
    frame.appearance = ui_style_apply_effects_frame(frame.appearance);
    frame.material.value = frame.appearance.value;
    frame.material.fill = ui_style_apply_effects_fill(frame.material.fill);
    for(int i = 0; i < MaterialLayerCount(frame.material.value.material); i++)
        ui_draw_surface(PaintMaterialLayer(frame.material, i));
    return GetColor(frame.foreground);
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

static Rectangle
dropdown_menu_bounds(const DropdownState *state)
{
    int bottom = state->clip_bottom > 0 ? state->clip_bottom : ui_view_height;
    Rectangle view = {0, state->clip_top, ui_view_width, bottom - state->clip_top};
    return PopupBounds((Rectangle){state->x, state->y, state->w, state->h},
        view, state->option_count, (float)Scale(1000) / 1000.0f);
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
    state->gesture = (PopupGesture){0};
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
ui_dropdown(DropdownProps props)
{
    int id = props.id;
    int x = (int)props.bounds.x;
    int y = (int)props.bounds.y;
    int w = (int)props.bounds.width;
    int h = (int)props.bounds.height;
    const DropdownOption *options = props.items;
    int option_count = props.option_count;
    int *selected_index = props.selected_index;
    BeginDisabled(props.disabled);
    if(props.disabled)
        MarkDisabled();
    char editor_id[96];
    DropdownState *state = get_or_create_dropdown_state(id);
    UIWidget widget;
    Style content_style = dropdown_style(0, 0, ButtonStateNormal);
    ContentMetrics content = Content(
        ui_pack_style_states((ControlStyle){.normal = content_style}).normal,
        (float)Scale(1000) / 1000.0f);
    int font = (int)content.font;
    int arrow_pad = Scale(24);
    int arrow_size = Scale(10);
    int changed = 0;
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = ui_mouse_world();
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
        dropdown_copy_text(&state->options[i].label, options != NULL ? options[i].label :
            (props.options != NULL ? props.options[i] : NULL));
        const char *font_name = options != NULL ? options[i].font_name : NULL;
        if(font_name != NULL && font_name[0] != '\0')
            dropdown_copy_text(&state->options[i].font_name, font_name);
        else {
            free((void *)state->options[i].font_name);
            state->options[i].font_name = NULL;
        }
    }

    if(active)
        MarkClickable();

    int pointer_activate = active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int next_open = Trigger(state->open, UIContentDisabled(), option_count, focused,
        !ui_popup_input_keyboard_captures(), pointer_activate,
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER),
        IsKeyPressed(KEY_SPACE), IsKeyPressed(KEY_DOWN));
    if(next_open != state->open) {
        ClearTextInputFocus();
        if(pointer_activate)
            UIConsumeRelease();
        state->open = next_open;
        if(state->open) {
            close_other_dropdowns(id);
            state->just_opened = 1;
            state->opened_frame = g_ui_frame_serial;
            state->scroll_offset = 0;
            state->highlight_index = ClampIndex(state->selected_index, option_count);
            state->gesture = (PopupGesture){0};
        }
    }

    button_text = content_style.foreground;
    if(can_draw)
        button_text = dropdown_paint_trigger(id, btn_bounds,
            hover || state->open, active && IsMouseButtonDown(MOUSE_BUTTON_LEFT), focused);

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
        RenderText(current_name, text_x, GetUIControlTextY(current_name, y, h, font), font, button_text);
        EndUIClip();
        PopUIFont(font_token);
    }

    if(can_draw)
        dropdown_draw_indicator(arrow_x, arrow_y, arrow_size,
                                state->open, button_text);

    EndUIWidget(&widget);
    EndDisabled();
    return changed;
}

static int
dropdown_paint_menu(int id)
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
    int font = (int)content.font;
    int x = state->x;
    int y = state->y;
    int w = state->w;
    int h = state->h;
    int option_h = h;
    int option_count = state->option_count;
    const DropdownOption *options = state->options;
    int can_draw = IsWindowReady();
    int clip_started = 0;

    int dropdown_y = 0;
    int dropdown_h = 0;
    int padding_top = Scale(4);
    int padding_bottom = Scale(4);
    int content_h = ContentHeight(option_count, option_h, padding_top + padding_bottom);
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
    state->scroll_offset = ScrollOffset(state->scroll_offset, max_scroll);
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

    state->gesture = PopupDragGesture(state->gesture, state->scroll_offset,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT), pointer_in_dropdown,
        state->scrollbar_pressed, my, max_scroll, Scale(8));
    state->scroll_offset = state->gesture.offset;
    if(Dismiss(state->open, state->just_opened, option_count, h, false, false,
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !pointer_in_dropdown))
        close_dropdown_state(state);

    if(!state->open) return 0;
    int opening = state->opened_frame == g_ui_frame_serial;
    int navigating = !opening && keyboard_available &&
        (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
         IsKeyPressed(KEY_HOME) || IsKeyPressed(KEY_END));
    if(navigating || state->just_opened) {
        Navigation nav = StartNavigation(state->highlight_index, option_count,
            navigating && IsKeyPressed(KEY_UP), navigating && IsKeyPressed(KEY_DOWN),
            navigating && IsKeyPressed(KEY_HOME), navigating && IsKeyPressed(KEY_END));
        while(nav.searching)
            nav = ScanNavigation(nav, !options[nav.index].disabled);
        state->highlight_index = nav.result;
        state->scroll_offset = RevealRow(state->scroll_offset, nav.result, option_h,
            dropdown_h - padding_top - padding_bottom, max_scroll);
    }
    int highlighted_enabled = state->highlight_index >= 0 &&
        state->highlight_index < option_count && !options[state->highlight_index].disabled;
    if(CanCommit(highlighted_enabled, opening, keyboard_available,
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER), false, false, false, false)) {
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
            state->scroll_offset = WheelOffset(state->scroll_offset, wheel, option_h, max_scroll);
        }
    }

    if(can_draw) {
        Style paint = dropdown_style(1, 0, ButtonStateNormal);
        paint.radius = GetThemeMetrics().radius_large + 2;
        dropdown_draw_surface(menu_bounds, paint, 0);
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
    VisibleRows rows = Rows(option_count, state->scroll_offset,
        dropdown_h - padding_top - padding_bottom, option_h);
    for(int i = rows.first; i < rows.end; i++) {
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

        Color row_text = content_style.foreground;
        if(can_draw) {
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
            if(option_active) MarkClickable();

            if(CanCommit(!options[i].disabled, state->just_opened, false, false,
                option_active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
                state->scrollbar_pressed, state->gesture.dragging,
                state->scroll_offset == state->gesture.origin_offset)) {
                ClearTextInputFocus();
                UIConsumeRelease();
                state->pending_changed = state->selected_index != i;
                state->selected_index = i;
                state->pending_index = i;
                close_dropdown_state(state);
                state->scroll_offset = 0;
                changed = state->pending_changed;
                if(clip_started)
                    EndUIClip();
                return changed;
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
            RenderText(options[i].label, text_x,
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


    return changed;
}

void
ui_dropdown_overlays(void)
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
            if(Dismiss(state->open, false, state->option_count, state->h,
                escape_pressed, lost_focus, false))
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
            dropdown_paint_menu(state->id);
        link = &state->next;
    }
}
