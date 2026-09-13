#include "ui_internal.h"
#include "dropdown_store.h"
#include "ui_popup_input_internal.h"
#include "ui_style_internal.h"
#include "theme.h"
#include "runtime/dropdown.h"
#include "ui_paint_internal.h"
#include "ui_style_sheet.h"

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
    int class_name;
    int highlight_index;
    int pending_changed;
    int pending_index;
    PopupGesture gesture;
    int scrollbar_pressed;
    int clip_top;
    int clip_bottom;
    PopupInputToken input_snapshot;
    unsigned long frame_seen;
    unsigned long opened_frame;
} DropdownState;

enum {
    DROPDOWN_ROLE_PANEL = 2,
    DROPDOWN_ROLE_OPTION = 26,
    DROPDOWN_ROLE_SCROLLBAR = 27
};

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
    }
    free(state->options);
    state->options = options;
    state->option_count = count;
}

/* Dropdowns expose semantic roles; KSS owns the visual result. */
static StyleFrame
dropdown_style_frame(int role, int selected, ButtonState state, int class_name)
{
    ButtonProps props = {0};
    StyleFrame base;
    StyleFrame accent;

    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.class_name = class_name;
    if(role == 1)
        props.emphasis = ButtonEmphasisFilled;
    if(role == 2 && selected && state != ButtonStateDisabled) {
        props.tone = ButtonToneAccent;
        props.emphasis = ButtonEmphasisFilled;
        props.selected = 1;
        state = ButtonStateSelected;
    }
    base = ui_control_style_frame_role_kind(props, state, 0, 0.0f, 0.0f,
        0.0f, StyleKindDropdown(), role);
    if(role != DROPDOWN_ROLE_OPTION)
        return base;

    props.tone = ButtonToneAccent;
    props.emphasis = ButtonEmphasisFilled;
    props.selected = selected;
    accent = ui_control_style_frame_role_kind(props,
        selected && state != ButtonStateDisabled ? ButtonStateSelected : state,
        0, 0.0f, 0.0f, 0.0f, StyleKindDropdown(), role);
    base.value = Appearance(base.value, accent.value, role, state, selected);
    if(selected && state != ButtonStateDisabled)
        base.fill = accent.fill;
    return base;
}

static Style
dropdown_style(int role, int selected, ButtonState state, int class_name)
{
    return ui_unpack_style(dropdown_style_frame(role, selected, state,
                                               class_name).value);
}

static ControlStyle
dropdown_trigger_style(int class_name)
{
    return (ControlStyle){
        .normal = dropdown_style(0, 0, ButtonStateNormal, class_name),
        .hover = dropdown_style(0, 0, ButtonStateHover, class_name),
        .pressed = dropdown_style(0, 0, ButtonStatePressed, class_name),
        .focused = dropdown_style(0, 0, ButtonStateFocus, class_name),
        .disabled = dropdown_style(0, 0, ButtonStateDisabled, class_name)
    };
}

static Color
dropdown_paint_trigger(int id, Rectangle bounds, int hovered, int pressed,
                       int focused, int class_name)
{
    ButtonProps props = {.id = id, .bounds = bounds, .tone = ButtonToneNeutral,
        .emphasis = ButtonEmphasisSoft, .disabled = ContentDisabled(),
        .class_name = class_name};
    ButtonSpec spec = {.props = props,
        .style = dropdown_trigger_style(class_name),
        .style_kind = StyleKindDropdown(),
        .style_resolved = 1};
    Activation sample = {.hovered = hovered, .pressed = pressed, .focused = focused};
    if(focused && IsFocusActivatePressed(id))
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
        TransitionCuesEnabled(), GetFrameTime() * 1000.0f,
        metrics.transition_normal_ms, metrics.transition_fast_ms);
    StyleFrame appearance = ui_resolve_button_spec_frame(spec,
        input.interaction.state, 1, motion.hover.value, motion.press.value,
        motion.focus.value, StyleKindDropdown());
    ButtonFrame frame = BuildFrame(props, input, appearance, motion, (Rectangle){0},
        ColorToInt(ui_app_style().background), (float)Scale(1000) / 1000.0f,
        appearance.value.font_size > 0.0f
            ? (int)(appearance.value.font_size + 0.5f)
            : GetFontSize(),
        GetFontSize());
    if(frame.repaint)
        InvalidateTree(INVALIDATE_PAINT);
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
    DropdownIndicator indicator =
        DropdownIndicatorFor(center_x, center_y, size, open);

    if(size <= 0)
        return;
    DrawLine(indicator.x1, indicator.y1, indicator.x2, indicator.y2, color);
    DrawLine(indicator.x3, indicator.y3, indicator.x4, indicator.y4, color);
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
    StyleFrame panel_frame = dropdown_style_frame(DROPDOWN_ROLE_PANEL, 0,
                                                  ButtonStateNormal,
                                                  state->class_name);
    return PopupBounds((Rectangle){state->x, state->y, state->w, state->h},
        view, state->option_count, (float)Scale(1000) / 1000.0f,
        panel_frame);
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
    DisabledScope(props.disabled);
    if(props.disabled)
        MarkDisabled();
    char editor_id[96];
    DropdownState *state = get_or_create_dropdown_state(id);
    Widget widget;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    StyleFrame trigger_frame = dropdown_style_frame(0, 0, ButtonStateNormal,
                                                    props.class_name);
    Style content_style = ui_unpack_style(trigger_frame.value);
    ContentMetrics content = Content(
        ui_pack_style_states((ControlStyle){.normal = content_style}).normal,
        runtime_scale);
    DropdownTriggerMetrics trigger_metrics =
        DropdownTriggerMetricsFor(runtime_scale, trigger_frame);
    int font = (int)content.font;
    int arrow_size = trigger_metrics.indicator_size;
    int changed = 0;
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = ui_mouse_world();
    Color button_text;
    int button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    int active = button_inside &&
                 (state->open
                      ? !ui_base_input_captures_click(mouse, 1)
                      : !InputCapturesClick(mouse));
    int hover = active && HoverEffectsEnabled();
    int can_draw = IsWindowReady();

    state->frame_seen = g_ui_frame_serial;
    state->input_snapshot = ui_popup_input_snapshot();
    if(ContentDisabled()) {
        close_dropdown_state(state);
        state->pending_changed = 0;
    }

    snprintf(editor_id, sizeof(editor_id), "dropdown:%d", id);
    widget = BeginWidget("dropdown", editor_id, btn_bounds,
                           WIDGET_MOVABLE |
                           WIDGET_RESIZABLE);
    btn_bounds = widget.bounds;
    x = (int)btn_bounds.x;
    y = (int)btn_bounds.y;
    w = (int)btn_bounds.width;
    h = (int)btn_bounds.height;
    if(w < trigger_metrics.min_width)
        w = trigger_metrics.min_width;
    if(h < trigger_metrics.min_height)
        h = trigger_metrics.min_height;
    btn_bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    WidgetSetBounds(&widget, btn_bounds);
    button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    active = !ContentDisabled() && button_inside &&
             (state->open
                  ? !ui_base_input_captures_click(mouse, 1)
                  : !InputCapturesClick(mouse));
    hover = active && HoverEffectsEnabled();
    int focused = !ContentDisabled() && id > 0 && RegisterFocus(id, btn_bounds);
    if(focused) SetFocusTextInputActive(0);

    if(state->pending_changed) {
        if(selected_index != NULL)
            *selected_index = state->pending_index;
        state->selected_index = state->pending_index;
        state->pending_changed = 0;
        changed = 1;
    }

    /* Store state for overlay drawing */
    state->x = x;
    state->y = y;
    state->w = w;
    state->h = h;
    state->clip_top = dropdown_store->clip_top;
    state->clip_bottom = dropdown_store->clip_bottom;
    state->selected_index = selected_index != NULL ? *selected_index : state->selected_index;
    state->class_name = props.class_name;
    if(option_count < 0)
        option_count = 0;
    dropdown_resize_options(state, option_count);
    for(int i = 0; i < option_count; i++) {
        state->options[i].icon_type = options != NULL ? options[i].icon_type : ICON_NONE;
        state->options[i].disabled = options != NULL && options[i].disabled;
        state->options[i].separator_before = options != NULL && options[i].separator_before;
        dropdown_copy_text(&state->options[i].label, options != NULL ? options[i].label :
            (props.options != NULL ? props.options[i] : NULL));
    }

    if(active)
        MarkClickable();

    int pointer_activate = active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int next_open = Trigger(state->open, ContentDisabled(), option_count, focused,
        !ui_popup_input_keyboard_captures(), pointer_activate,
        IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER),
        IsKeyPressed(KEY_SPACE), IsKeyPressed(KEY_DOWN));
    if(next_open != state->open) {
        ClearTextInputFocus();
        if(pointer_activate)
            ConsumeRelease();
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
            hover || state->open, active && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            focused, props.class_name);

    /* Draw current selection text, clipped before the chevron. */
    int current_index = state->selected_index;
    if(current_index < 0 || current_index >= option_count)
        current_index = 0;
    const char *current_name = option_count > 0 ? state->options[current_index].label : "";
    int has_icon = option_count > 0 &&
                   state->options[current_index].icon_type != ICON_NONE;
    DropdownTriggerContent trigger_content =
        DropdownTriggerContentFor(btn_bounds, content, trigger_metrics,
                                  has_icon != 0);
    if(option_count > 0 && state->options[current_index].icon_type != ICON_NONE) {
        if(can_draw)
            DrawIcon(state->options[current_index].icon_type,
                     trigger_content.icon_bounds, button_text);
    }
    int text_w = (int)trigger_content.clip_bounds.width;
    if(can_draw && text_w > 0) {
        BeginClip((int)(g_ui_camera.offset.x +
                        trigger_content.clip_bounds.x * g_ui_camera.zoom),
                  (int)(g_ui_camera.offset.y +
                        trigger_content.clip_bounds.y * g_ui_camera.zoom),
                  (int)(trigger_content.clip_bounds.width * g_ui_camera.zoom),
                  (int)(trigger_content.clip_bounds.height * g_ui_camera.zoom));
        RenderText(current_name, (int)trigger_content.text_bounds.x,
                   (int)trigger_content.text_bounds.y, font, button_text);
        EndClip();
    }

    if(can_draw)
        dropdown_draw_indicator(trigger_content.indicator_center_x,
                                trigger_content.indicator_center_y,
                                arrow_size,
                                state->open, button_text);

    EndWidget(&widget);
    DisabledEndScope();
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

    Style content_style = dropdown_style(0, 0, ButtonStateNormal, state->class_name);
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

    int content_h;
    int max_scroll;
    Rectangle btn_bounds = {x, y, w, h};
    Rectangle menu_bounds = dropdown_menu_bounds(state);
    StyleFrame panel_frame = dropdown_style_frame(DROPDOWN_ROLE_PANEL, 0,
                                                  ButtonStateNormal,
                                                  state->class_name);
    StyleFrame option_frame = dropdown_style_frame(DROPDOWN_ROLE_OPTION, 0,
                                                   ButtonStateNormal,
                                                   state->class_name);
    StyleFrame scrollbar_frame = dropdown_style_frame(DROPDOWN_ROLE_SCROLLBAR,
        0, ButtonStateNormal, state->class_name);
    DropdownMenuMetrics metrics = DropdownMenuMetricsFor(
        (float)Scale(1000) / 1000.0f, panel_frame, option_frame,
        scrollbar_frame);
    MenuLayout menu_layout;
    x = (int)menu_bounds.x;
    w = (int)menu_bounds.width;
    menu_layout = MenuLayoutFor(menu_bounds, option_count, option_h,
                                metrics.padding_top, metrics.padding_bottom,
                                metrics.scrollbar_width,
                                metrics.scrollbar_gap);
    content_h = menu_layout.content_height;
    max_scroll = menu_layout.max_scroll;
    int option_w = menu_layout.option_width;
    state->scroll_offset = ScrollOffset(state->scroll_offset, max_scroll);

    Vector2 mouse = ui_mouse_world();
    int my = (int)mouse.y;
    int pointer_in_dropdown = CheckCollisionPointRec(mouse, btn_bounds) ||
                              CheckCollisionPointRec(mouse, menu_bounds);
    Rectangle scrollbar_bounds = menu_layout.scrollbar_bounds;
    if(max_scroll > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(mouse, scrollbar_bounds))
        state->scrollbar_pressed = 1;

    state->gesture = PopupDragGesture(state->gesture, state->scroll_offset,
        IsMouseButtonDown(MOUSE_BUTTON_LEFT), pointer_in_dropdown,
        state->scrollbar_pressed, my, max_scroll, metrics.drag_threshold);
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
            menu_layout.content_bounds.height, max_scroll);
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
        Style paint = ui_unpack_style(panel_frame.value);
        dropdown_draw_surface(menu_bounds, paint, 0);
    }

    /* Resolve thumb input before rows use the offset, so thumb and content
     * paint the same state on the drag frame. The panel is already painted. */
    if(max_scroll > 0) {
        Rectangle track = ScrollbarTrackBounds(scrollbar_bounds,
                                               metrics.scrollbar_track_inset);
        ui_scrollbar((int)track.x, (int)track.y, (int)track.height,
                     content_h, &state->scroll_offset, max_scroll, 1);
    }

    if(can_draw) {
        BeginClip((int)(g_ui_camera.offset.x +
                        menu_layout.content_bounds.x * g_ui_camera.zoom),
                    (int)(g_ui_camera.offset.y +
                          menu_layout.content_bounds.y * g_ui_camera.zoom),
                    (int)(menu_layout.content_bounds.width * g_ui_camera.zoom),
                    (int)(menu_layout.content_bounds.height * g_ui_camera.zoom));
        clip_started = 1;
    }

    /* Draw options */
    VisibleRows rows = Rows(option_count, state->scroll_offset,
        menu_layout.content_bounds.height, option_h);
    for(int i = rows.first; i < rows.end; i++) {
        OptionPaint option_paint = OptionPaintFor(menu_bounds, option_w, i,
            option_h, state->scroll_offset, metrics.padding_top,
            metrics.padding_bottom, metrics.highlight_inset_x,
            metrics.highlight_inset_y);
        int option_y = option_paint.option_y;
        int visible_h = (int)option_paint.visible_bounds.height;
        Rectangle visible_bounds = option_paint.visible_bounds;

        /* Skip if outside visible area - use inclusive bounds for last item */
        if(visible_h <= 0)
            continue;

        int option_active = !options[i].disabled && CheckCollisionPointRec(mouse, visible_bounds);
        int option_hover = !options[i].disabled && (state->highlight_index == i ||
                           (option_active && HoverEffectsEnabled()));

        Color row_text = content_style.foreground;
        if(can_draw) {
            int selected = state->selected_index == i;
            Style paint = dropdown_style(DROPDOWN_ROLE_OPTION, selected,
                options[i].disabled ? ButtonStateDisabled :
                (option_hover ? ButtonStateHover : ButtonStateNormal),
                state->class_name);
            row_text = GetColor(Opacity(ColorToInt(paint.foreground), paint.opacity));
            if(selected || option_hover) {
                Rectangle row = option_paint.highlight_bounds;
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
                ConsumeRelease();
                state->pending_changed = state->selected_index != i;
                state->selected_index = i;
                state->pending_index = i;
                close_dropdown_state(state);
                state->scroll_offset = 0;
                changed = state->pending_changed;
                if(clip_started)
                    EndClip();
                return changed;
            }
        }

        if(can_draw) {
            int has_icon = options[i].icon_type != ICON_NONE;
            Rectangle row_bounds = {(float)x, (float)option_y,
                                    (float)option_w, (float)option_h};
            DropdownOptionContent option_content =
                DropdownOptionContentFor(row_bounds, visible_bounds, content,
                                         metrics, has_icon != 0);
            if(options[i].separator_before)
                DrawLine((int)option_content.separator_bounds.x,
                    (int)option_content.separator_bounds.y,
                    (int)(option_content.separator_bounds.x +
                          option_content.separator_bounds.width),
                    (int)option_content.separator_bounds.y,
                    Fade(row_text, 0.18f));
            if(has_icon)
                DrawIcon(options[i].icon_type, option_content.icon_bounds,
                         row_text);
            BeginClip((int)(g_ui_camera.offset.x +
                            option_content.clip_bounds.x * g_ui_camera.zoom),
                (int)(g_ui_camera.offset.y +
                      option_content.clip_bounds.y * g_ui_camera.zoom),
                (int)(option_content.clip_bounds.width * g_ui_camera.zoom),
                (int)(option_content.clip_bounds.height * g_ui_camera.zoom));
            RenderText(options[i].label, (int)option_content.text_bounds.x,
                       (int)option_content.text_bounds.y,
                       font, row_text);
            EndClip();
            if(state->selected_index == i) {
                DrawIcon(ICON_CHECK, option_content.check_bounds, row_text);
            }
        }
    }

    if(clip_started)
        EndClip();

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
