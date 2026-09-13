#include "ui_internal.h"
#include "ui_popup_input_internal.h"
#include "ui_style_internal.h"
#include "tab_bar_store.h"
#include "runtime/paned_view.h"
#include "runtime/tab_bar.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;

static StyleFrame
ui_tab_bar_style_frame(int style_kind, ButtonState state, int disabled,
                       int selected, int class_name)
{
    ButtonProps props = {0};
    props.class_name = class_name;
    props.tone = selected ? ButtonToneAccent : ButtonToneNeutral;
    props.emphasis = selected ? ButtonEmphasisFilled : ButtonEmphasisSoft;
    props.size = ControlSizeMedium;
    props.disabled = disabled;
    props.selected = selected;
    return ui_control_style_frame_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                       style_kind);
}

static int
ui_tab_bar_font(TabBarProps bar, int disabled)
{
    StyleFrame tab_frame;

    tab_frame = ui_tab_bar_style_frame(StyleKindTab(),
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        bar.class_name);
    return tab_frame.value.font_size > 0.0f
        ? (int)(tab_frame.value.font_size + 0.5f)
        : GetSmallFontSize();
}

static float
ui_tab_roundness(Rectangle bounds, float radius)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;
    if(min_side <= 0.0f || radius <= 0.0f)
        return 0.0f;
    radius = radius / min_side;
    if(radius < 0.0f)
        return 0.0f;
    if(radius > 0.5f)
        return 0.5f;
    return radius;
}

typedef struct TabBarState {
    struct TabBarState *next;
    int id;
    int scroll;
    unsigned long frame_seen;
} TabBarState;

struct TabBarStore {
    TabBarState *states;
    Vector2 last_drag_position;
    int dragging_scroll;
    int scroll_drag_bar_id;
    Rectangle scroll_drag_bar_bounds;
    int last_clicked_tab;
    int last_clicked_bar_id;
    Rectangle last_clicked_bar_bounds;
    double last_click_time;
    Vector2 press_position;
    int press_index;
    int press_bar_id;
    Rectangle press_bar_bounds;
    int reorder_drag_active;
    Vector2 pane_press_position;
    int pane_press_index;
    int pane_drag_reported;
};

static TabBarStore fallback_store = {
    .last_clicked_tab = -1,
    .press_index = -1,
    .pane_press_index = -1
};
static TabBarStore *tab_bar_store = &fallback_store;

TabBarStore *
tab_bar_store_new(void)
{
    TabBarStore *store = calloc(1, sizeof(*store));

    if(store == NULL)
        abort();
    store->last_clicked_tab = -1;
    store->press_index = -1;
    store->pane_press_index = -1;
    return store;
}

void
tab_bar_store_free(TabBarStore *store)
{
    TabBarState *state;

    if(store == NULL)
        return;
    if(store == tab_bar_store || store == &fallback_store)
        abort();
    state = store->states;
    while(state != NULL) {
        TabBarState *next = state->next;

        free(state);
        state = next;
    }
    free(store);
}

TabBarStore *
tab_bar_store_swap(TabBarStore *store)
{
    TabBarStore *previous = tab_bar_store;

    tab_bar_store = store != NULL ? store : &fallback_store;
    return previous;
}

TabBarStore *
tab_bar_store_current(void)
{
    return tab_bar_store;
}

static int
ui_tab_bar_same_identity(int id, Rectangle bounds, int other_id,
                         Rectangle other_bounds)
{
    if(id > 0 || other_id > 0)
        return id > 0 && id == other_id;
    return bounds.x == other_bounds.x && bounds.y == other_bounds.y &&
           bounds.width == other_bounds.width &&
           bounds.height == other_bounds.height;
}

int *
ui_tab_bar_owned_scroll(int id, int *fallback)
{
    TabBarState *state;

    if(id <= 0)
        return fallback;
    for(state = tab_bar_store->states; state != NULL; state = state->next) {
        if(state->id == id) {
            state->frame_seen = g_ui_frame_serial;
            return &state->scroll;
        }
    }
    state = calloc(1, sizeof(*state));
    if(state == NULL)
        abort();
    state->id = id;
    state->frame_seen = g_ui_frame_serial;
    state->next = tab_bar_store->states;
    tab_bar_store->states = state;
    return &state->scroll;
}

void
ui_tab_bar_finish_frame(void)
{
    TabBarState **link = &tab_bar_store->states;

    ui_tab_scope_finish_frame();
    while(*link != NULL) {
        TabBarState *state = *link;

        if(state->frame_seen != g_ui_frame_serial) {
            *link = state->next;
            free(state);
        } else {
            link = &state->next;
        }
    }
}


int
ui_tab_bar_height(void)
{
    StyleFrame bar_frame = ui_tab_bar_style_frame(StyleKindTabBar(),
        ButtonStateNormal, 0, 0, 0);
    return TabBarPolicyHeight((float)Scale(1000) / 1000.0f, bar_frame);
}

int
GetTabBarHeight(void)
{
    return ui_tab_bar_height();
}

int
TabBarHeight(void)
{
    return ui_tab_bar_height();
}

static int
ui_tab_bar_tab_width(TabBarProps bar, int index, int min_tab_w, int max_tab_w,
                     int icon_tab_w, int font)
{
    const Tab *tab;
    int label_w;
    int has_label;
    StyleFrame bar_frame;
    StyleFrame tab_frame;
    StyleFrame close_frame;
    TabBarMetrics metrics;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    has_label = tab->label != NULL && tab->label[0] != '\0';
    label_w = has_label ? TextWidth(tab->label, font) : 0;
    bar_frame = ui_tab_bar_style_frame(StyleKindTabBar(),
        bar.disabled ? ButtonStateDisabled : ButtonStateNormal,
        bar.disabled, 0, bar.class_name);
    tab_frame = ui_tab_bar_style_frame(StyleKindTab(),
        tab->disabled ? ButtonStateDisabled : ButtonStateNormal,
        tab->disabled, 0, bar.class_name);
    close_frame = ui_tab_bar_style_frame(StyleKindTabClose(),
        tab->disabled ? ButtonStateDisabled : ButtonStateNormal,
        tab->disabled, 0, bar.class_name);
    metrics = TabBarDefaultMetrics(min_tab_w, max_tab_w,
                                   (float)Scale(1000) / 1000.0f,
                                   bar_frame, tab_frame, close_frame);
    if(icon_tab_w > 0)
        metrics.icon_width = icon_tab_w;
    return TabBarTabWidth(label_w, has_label, tab->icon.id != 0,
                          tab->closeable, metrics);
}

static int
ui_tab_bar_next_enabled(TabBarProps bar, int from, int direction)
{
    for(int step = 1; step <= bar.count; step++) {
        int index = TabBarWrappedIndex(from, direction, step, bar.count);

        if(!bar.tabs[index].disabled)
            return index;
    }
    return from;
}

int
ui_tab_bar_keyboard_input(TabBarProps bar)
{
    int selected;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 ||
       bar.bounds.height <= 0 || bar.disabled || ContentDisabled() ||
       bar.id <= 0 || !RegisterFocus(bar.id, bar.bounds) ||
       ui_popup_input_focus_captures(bar.id))
        return -1;

    SetFocusTextInputActive(0);
    selected = TabBarSelectedIndexFor(bar.selected_index, bar.count);
    if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP))
        return ui_tab_bar_next_enabled(bar, selected, -1);
    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN))
        return ui_tab_bar_next_enabled(bar, selected, 1);
    if(IsKeyPressed(KEY_HOME))
        return ui_tab_bar_next_enabled(bar, -1, 1);
    if(IsKeyPressed(KEY_END))
        return ui_tab_bar_next_enabled(bar, 0, -1);
    if((IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) &&
       bar.tabs[selected].closeable && bar.closed_index != NULL)
        *bar.closed_index = selected;
    return -1;
}

static int
ui_tab_bar_total_width(TabBarProps bar, int min_tab_w, int max_tab_w,
                       int icon_tab_w, int tab_gap, int font)
{
    int total = 0;

    for(int i = 0; i < bar.count; i++)
        total += ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                      icon_tab_w, font);
    return TabBarTotalWidth(total, bar.count, tab_gap);
}

static Rectangle
ui_tab_bar_rect_at(TabBarProps bar, int index, int min_tab_w, int max_tab_w,
                   int icon_tab_w, int tab_gap, int scroll, int equal_tabs,
                   int font)
{
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int tab_x = equal_tabs ? bar_x : bar_x + tab_gap - scroll;
    int tab_w = min_tab_w;

    if(equal_tabs)
        return TabBarEqualTabBounds(bar.bounds, bar.count, index);
    for(int i = 0; i <= index && i < bar.count; i++) {
        tab_w = ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                     icon_tab_w, font);
        if(i == index)
            return (Rectangle){(float)tab_x, (float)bar_y,
                               (float)tab_w, bar.bounds.height};
        tab_x += tab_w + tab_gap;
    }
    return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
}

static int
ui_tab_bar_reorder_target(TabBarProps bar, int active_index, int min_tab_w,
                          int max_tab_w, int icon_tab_w, int tab_gap,
                          int scroll, int equal_tabs, int pointer_x, int font)
{
    int target = 0;

    if(active_index < 0 || active_index >= bar.count)
        return -1;
    for(int i = 0; i < bar.count; i++) {
        Rectangle rect;
        int center_x;

        if(i == active_index)
            continue;
        rect = ui_tab_bar_rect_at(bar, i, min_tab_w, max_tab_w, icon_tab_w,
                                  tab_gap, scroll, equal_tabs, font);
        center_x = (int)(rect.x + rect.width / 2.0f);
        if(pointer_x > center_x)
            target++;
    }
    return ui_clampi(target, 0, bar.count - 1);
}

int
RenderTabBar(TabBarProps bar)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int clicked_tab = -1;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;
    int disabled = bar.disabled || ContentDisabled();
    StyleFrame bar_frame = ui_tab_bar_style_frame(StyleKindTabBar(),
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        bar.class_name);
    StyleFrame metric_tab_frame = ui_tab_bar_style_frame(StyleKindTab(),
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        bar.class_name);
    StyleFrame metric_close_frame = ui_tab_bar_style_frame(StyleKindTabClose(),
        disabled ? ButtonStateDisabled : ButtonStateNormal, disabled, 0,
        bar.class_name);
    TabBarMetrics default_metrics = TabBarDefaultMetrics(
        bar.min_tab_width, bar.max_tab_width,
        (float)Scale(1000) / 1000.0f, bar_frame, metric_tab_frame,
        metric_close_frame);
    int font = ui_tab_bar_font(bar, disabled);
    int tab_gap = default_metrics.gap;
    int min_tab_w = default_metrics.min_width;
    int max_tab_w = default_metrics.max_width;
    int icon_tab_w = default_metrics.icon_width;
    int focused = 0;
    int can_draw = IsWindowReady();
    int default_scroll_offset = 0;
    int *scroll_offset = bar.scroll_offset != NULL
                             ? bar.scroll_offset
                             : ui_tab_bar_owned_scroll(bar.id,
                                                       &default_scroll_offset);

    if(bar.closed_index != NULL)
        *bar.closed_index = -1;
    if(bar.double_clicked_index != NULL)
        *bar.double_clicked_index = -1;
    if(bar.reordered_from_index != NULL)
        *bar.reordered_from_index = -1;
    if(bar.reordered_to_index != NULL)
        *bar.reordered_to_index = -1;
    if(bar.selected_tab_bounds != NULL)
        *bar.selected_tab_bounds = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
    if(bar.middle_clicked_index != NULL)
        *bar.middle_clicked_index = -1;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 || bar.bounds.height <= 0)
        return -1;

    clicked_tab = ui_tab_bar_keyboard_input(bar);
    focused = !disabled && bar.id > 0 && GetFocus() == bar.id &&
              !ui_popup_input_focus_captures(bar.id);

    if(can_draw) {
        Style bar_style = ui_unpack_style(
            ui_style_apply_effects_frame(bar_frame).value);
        ui_draw_material(bar.bounds, bar.bounds, bar_style.background,
                         bar_style.border, bar_style.border,
                         bar_style.radius, bar_style.border_width,
                         0.0f, 0.0f, disabled, bar_style.focus, 0.0f,
                         bar_style.opacity,
                         ui_style_apply_effects_fill(bar_frame.fill),
                         bar_style.material);
    }

    if(max_tab_w < min_tab_w)
        max_tab_w = min_tab_w;
    if(icon_tab_w > max_tab_w)
        icon_tab_w = max_tab_w;

    int total_tabs_w = ui_tab_bar_total_width(bar, min_tab_w, max_tab_w,
                                              icon_tab_w, tab_gap, font);
    TabBarScroll scroll_policy = TabBarScrollFor(bar.bounds.width,
                                                 total_tabs_w,
                                                 *scroll_offset);
    int needs_scroll = !scroll_policy.equal_tabs;
    int equal_tabs = scroll_policy.equal_tabs;
    int max_scroll = scroll_policy.max_scroll;

    *scroll_offset = scroll_policy.scroll;

    if(needs_scroll && bar.focus_selected &&
       bar.selected_index >= 0 && bar.selected_index < bar.count) {
        int selected_tab_w = ui_tab_bar_tab_width(bar, bar.selected_index,
                                                  min_tab_w, max_tab_w,
                                                  icon_tab_w, font);
        int selected_tab_x = bar_x + tab_gap - *scroll_offset;
        for(int i = 0; i < bar.selected_index; i++)
            selected_tab_x += ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                                   icon_tab_w, font) + tab_gap;
        *scroll_offset = TabBarRevealScroll((float)(selected_tab_x - tab_gap),
                                            (float)(selected_tab_w + tab_gap * 2),
                                            bar.bounds, *scroll_offset,
                                            max_scroll);
    }

    // Default top tabs are distributed equally across the full app bar.
    int tab_x = equal_tabs ? bar_x : bar_x + tab_gap - *scroll_offset;
    int reorder_enabled = bar.reordered_from_index != NULL &&
                          bar.reordered_to_index != NULL;
    int drag_target = -1;

    int owns_press = ui_tab_bar_same_identity(bar.id,bar.bounds,
                                               tab_bar_store->press_bar_id,
                                               tab_bar_store->press_bar_bounds);
    int owns_drag = tab_bar_store->reorder_drag_active && owns_press;
    if(!disabled && reorder_enabled && owns_press &&
       tab_bar_store->press_index >= 0 &&
       tab_bar_store->press_index < bar.count &&
       IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int dx = (int)(mouse_world.x - tab_bar_store->press_position.x);
        int dy = (int)(mouse_world.y - tab_bar_store->press_position.y);
        int abs_dx = dx < 0 ? -dx : dx;
        int abs_dy = dy < 0 ? -dy : dy;
        TabBarPaint drag_paint = TabBarPaintFor(bar_frame, metric_tab_frame,
                                                metric_close_frame,
                                                (float)Scale(1000) / 1000.0f);
        int threshold = drag_paint.reorder_drag_threshold;

        if(!tab_bar_store->reorder_drag_active &&
           abs_dx >= threshold && abs_dx >= abs_dy) {
            tab_bar_store->reorder_drag_active = 1;
            g_ui_pointer_owner = POINTER_OWNER_REORDER;
        }
        if(tab_bar_store->reorder_drag_active) {
            drag_target = ui_tab_bar_reorder_target(
                bar, tab_bar_store->press_index, min_tab_w, max_tab_w,
                icon_tab_w, tab_gap,
                *scroll_offset, equal_tabs, (int)mouse_world.x, font);
            PushInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
    }

    if(can_draw)
        ui_begin_world_clip(bar.bounds);
    PushInputClip(bar.bounds);
    for(int i = 0; i < bar.count; i++) {
        const Tab *tab = &bar.tabs[i];
        int tab_w = equal_tabs ? bar_w / bar.count :
                    ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                         icon_tab_w, font);
        if(equal_tabs && i == bar.count - 1)
            tab_w = bar_x + bar_w - tab_x;
        Rectangle tab_rect = {(float)tab_x, (float)bar_y, (float)tab_w, (float)bar_h};
        int input_captured = InputCapturesClick(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && HoverEffectsEnabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = disabled || tab->disabled;
        ButtonState tab_state = is_disabled ? ButtonStateDisabled :
            ((is_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ?
                 ButtonStatePressed :
             (is_hovered ? ButtonStateHover :
              (is_selected ? ButtonStateSelected : ButtonStateNormal)));
        StyleFrame tab_frame = ui_tab_bar_style_frame(StyleKindTab(),
            tab_state, is_disabled, is_selected, bar.class_name);
        StyleFrame close_frame = ui_tab_bar_style_frame(StyleKindTabClose(),
            ButtonStateNormal, is_disabled, 0, bar.class_name);
        TabBarPaint paint;
        StyleFrame styled_tab_frame;
        Style tab_style;
        Style close_style;

        if(is_selected && bar.selected_tab_bounds != NULL)
            *bar.selected_tab_bounds = tab_rect;

        paint = TabBarPaintFor(bar_frame, tab_frame, close_frame,
                               (float)Scale(1000) / 1000.0f);
        styled_tab_frame = ui_style_apply_effects_frame(tab_frame);
        tab_style = ui_unpack_style(styled_tab_frame.value);
        close_style = ui_unpack_style(
            ui_style_apply_effects_frame(close_frame).value);
        if(can_draw) {
            ui_draw_material(tab_rect, bar.bounds, tab_style.background,
                             tab_style.border, tab_style.border,
                             tab_style.radius, tab_style.border_width,
                             is_hovered ? 1.0f : 0.0f,
                             (is_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ? 1.0f : 0.0f,
                             is_disabled, tab_style.focus,
                             focused && is_selected ? 1.0f : 0.0f,
                             tab_style.opacity,
                             ui_style_apply_effects_fill(styled_tab_frame.fill),
                             tab_style.material);
        }

        if(can_draw && is_selected && paint.tab_border_color != 0) {
            DrawLine(tab_x, bar_y + bar_h - 1, tab_x + tab_w - 1,
                     bar_y + bar_h - 1, GetColor(paint.tab_border_color));
        }

        if(can_draw && !ui_default_style() && owns_drag && drag_target == i) {
            Rectangle marker = TabBarDragMarkerBounds(
                tab_x, tab_w, bar_y, bar_h,
                drag_target > tab_bar_store->press_index,
                (float)Scale(1000) / 1000.0f);
            DrawRectangleRec(marker,
                          GetColor(paint.focus_color));
        }

        // Draw tab text and icon
        int icon_size = paint.icon_size;
        int has_label = tab->label != NULL && tab->label[0] != '\0';
        int close_size = paint.close_size;
        int label_w = has_label ? TextWidth(tab->label, font) : 0;
        TabBarContentLayout content_layout =
            TabBarContentLayoutFor(tab_rect, label_w, has_label,
                                   tab->icon.id != 0, tab->closeable,
                                   paint);
        Rectangle close_rect = content_layout.close_bounds;
        int close_active = tab->closeable && !is_disabled &&
                           CheckCollisionPointRec(mouse_world, close_rect) &&
                           !input_captured;
        int close_hovered = close_active && HoverEffectsEnabled();

        Color text_color = Fade(GetColor(paint.text_color), tab_style.opacity);
        Color icon_tint = Fade(GetColor(paint.icon_color), tab_style.opacity);

        // Draw icon if present
        if(tab->icon.id != 0 && icon_size > 0) {
            Rectangle icon_rect = content_layout.icon_bounds;
            Rectangle icon_src = {0, 0, (float)tab->icon.width, (float)tab->icon.height};
            if(can_draw)
                DrawTexturePro(tab->icon, icon_src, icon_rect,
                               kryon_zero_vector2, 0, icon_tint);
        }

        // Draw tab label
        Rectangle text_rect = content_layout.text_bounds;

        if(can_draw && text_rect.width > 0 && has_label)
            DrawLeftControlTextInRect(tab->label, text_rect, font, text_color);

        if(can_draw && tab->closeable && close_size > 0) {
            if(close_hovered) {
                close_frame = ui_tab_bar_style_frame(StyleKindTabClose(),
                    ButtonStateHover, is_disabled, 0, bar.class_name);
                paint = TabBarPaintFor(bar_frame, tab_frame, close_frame,
                                       (float)Scale(1000) / 1000.0f);
                close_style = ui_unpack_style(
                    ui_style_apply_effects_frame(close_frame).value);
                DrawRectangleRounded(close_rect,
                    ui_tab_roundness(close_rect, close_frame.value.radius),
                    6, GetColor(close_frame.value.background));
            }
            RenderText("x",
                         (int)(close_rect.x + (close_rect.width -
                                               (float)TextWidth("x", font)) * 0.5f),
                         TextBaselineY("x", (int)close_rect.y, (int)close_rect.height, font),
                         font, Fade(GetColor(paint.close_color),
                                    close_style.opacity));
        }

        // Handle click detection
        if(is_active) {
            if(is_disabled)
                MarkDisabled();
            else
                MarkClickable();

            if(!is_disabled && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
                clicked_tab = -1;
                if(bar.middle_clicked_index != NULL)
                    *bar.middle_clicked_index = i;
            }

            if(!is_disabled && !close_active &&
               IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tab_bar_store->press_index = i;
                tab_bar_store->press_bar_id = bar.id;
                tab_bar_store->press_bar_bounds = bar.bounds;
                tab_bar_store->press_position = mouse_world;
                tab_bar_store->reorder_drag_active = 0;
            }

            if(close_active && released && !owns_drag) {
                clicked_tab = -1;
                if(bar.closed_index != NULL)
                    *bar.closed_index = i;
                tab_bar_store->last_clicked_tab = -1;
                tab_bar_store->last_clicked_bar_id = 0;
                tab_bar_store->last_clicked_bar_bounds =
                    (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
                tab_bar_store->last_click_time = 0.0;
                ConsumeRelease();
            } else if(!close_active && released && !owns_drag &&
                      (tab_bar_store->press_index < 0 ||
                       (ui_tab_bar_same_identity(bar.id,bar.bounds,
                                                 tab_bar_store->press_bar_id,
                                                 tab_bar_store->press_bar_bounds) &&
                        tab_bar_store->press_index == i))) {
                double now = GetTime();

                if(bar.double_clicked_index != NULL &&
                   ui_tab_bar_same_identity(bar.id,bar.bounds,
                                            tab_bar_store->last_clicked_bar_id,
                                            tab_bar_store->last_clicked_bar_bounds) &&
                   tab_bar_store->last_clicked_tab == i &&
                   now - tab_bar_store->last_click_time <= 0.45)
                    *bar.double_clicked_index = i;
                tab_bar_store->last_clicked_tab = i;
                tab_bar_store->last_clicked_bar_id = bar.id;
                tab_bar_store->last_clicked_bar_bounds = bar.bounds;
                tab_bar_store->last_click_time = now;
                clicked_tab = i;
                if(bar.id > 0) {
                    SetFocus(bar.id);
                    focused = 1;
                }
            }
        }

        if(can_draw && focused &&
           i == (clicked_tab >= 0 ? clicked_tab : bar.selected_index))
            RenderFocus(tab_rect);

        tab_x += tab_w + tab_gap;
    }

    PopInputClip();
    if(can_draw)
        EndClip();

    owns_press = ui_tab_bar_same_identity(bar.id,bar.bounds,
                                           tab_bar_store->press_bar_id,
                                           tab_bar_store->press_bar_bounds);
    owns_drag = tab_bar_store->reorder_drag_active && owns_press;
    if(!disabled && reorder_enabled && owns_drag && released &&
       tab_bar_store->press_index >= 0 &&
       tab_bar_store->press_index < bar.count) {
        int target = drag_target;

        if(target < 0)
            target = ui_tab_bar_reorder_target(
                bar, tab_bar_store->press_index, min_tab_w, max_tab_w,
                icon_tab_w, tab_gap,
                *scroll_offset, equal_tabs, (int)mouse_world.x, font);
        if(target >= 0 && target < bar.count &&
           target != tab_bar_store->press_index) {
            *bar.reordered_from_index = tab_bar_store->press_index;
            *bar.reordered_to_index = target;
        }
        clicked_tab = -1;
        ConsumeRelease();
    }
    if((released || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) && owns_press) {
        tab_bar_store->press_index = -1;
        tab_bar_store->press_bar_id = 0;
        tab_bar_store->press_bar_bounds =
            (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
        tab_bar_store->reorder_drag_active = 0;
        if(g_ui_pointer_owner == POINTER_OWNER_REORDER)
            g_ui_pointer_owner = POINTER_OWNER_NONE;
    }

    if(!disabled && needs_scroll && !(reorder_enabled && owns_drag)) {
        // Handle manual drag scrolling
        Vector2 current_pos = mouse_world;
        int is_mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        // Check if mouse is over tab bar area
        Rectangle scroll_area = {(float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h};
        int is_over_bar = CheckCollisionPointRec(current_pos, scroll_area);

        int owns_scroll_drag = ui_tab_bar_same_identity(
            bar.id,bar.bounds,tab_bar_store->scroll_drag_bar_id,
            tab_bar_store->scroll_drag_bar_bounds);

        if(is_mouse_down && is_over_bar && !tab_bar_store->dragging_scroll) {
            tab_bar_store->dragging_scroll = 1;
            tab_bar_store->scroll_drag_bar_id = bar.id;
            tab_bar_store->scroll_drag_bar_bounds = bar.bounds;
            tab_bar_store->last_drag_position = current_pos;
            owns_scroll_drag = 1;
        }

        if(tab_bar_store->dragging_scroll && owns_scroll_drag) {
            if(is_mouse_down) {
                float dx = current_pos.x -
                           tab_bar_store->last_drag_position.x;
                *scroll_offset -= (int)dx;

                // Clamp scroll offset
                if(*scroll_offset < 0)
                    *scroll_offset = 0;
                if(*scroll_offset > max_scroll)
                    *scroll_offset = max_scroll;

                tab_bar_store->last_drag_position = current_pos;
            } else {
                tab_bar_store->dragging_scroll = 0;
                tab_bar_store->scroll_drag_bar_id = 0;
                tab_bar_store->scroll_drag_bar_bounds =
                    (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
            }
        }
    }

    if(clicked_tab >= 0)
        ConsumeRelease();
    return clicked_tab;
}

PaneDropZone
GetPaneDropZone(Rectangle bounds, Vector2 mouse)
{
    StyleFrame frame = {0};
    frame.value = ResolveActiveStyle((StyleData){0},
        StyleControlRoleFacts(StyleKindPanedView(), 0, 0, 12,
            ButtonToneNeutral, ButtonEmphasisSoft, ControlSizeMedium,
            ButtonStateNormal),
        ButtonStateNormal);
    PanedViewMetrics metrics = PanedViewMetricsFor((float)GetScale(),
                                                   frame);
    return (PaneDropZone)PanedViewDropZoneFor(bounds, mouse, metrics);
}
