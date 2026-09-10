#include "ui_internal.h"
#include "ui_popup_input_internal.h"
#include "tab_bar_store.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;

typedef struct UITabBarState {
    struct UITabBarState *next;
    int id;
    int scroll;
    unsigned long frame_seen;
} UITabBarState;

struct TabBarStore {
    UITabBarState *states;
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
    UITabBarState *state;

    if(store == NULL)
        return;
    if(store == tab_bar_store || store == &fallback_store)
        abort();
    state = store->states;
    while(state != NULL) {
        UITabBarState *next = state->next;

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
    UITabBarState *state;

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
    UITabBarState **link = &tab_bar_store->states;

    ui_tab_scope_finish_frame();
    while(*link != NULL) {
        UITabBarState *state = *link;

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
    return ui_default_style() ? Scale(48) : Scale(36);
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
                     int icon_tab_w)
{
    const Tab *tab;
    int label_w;
    int w;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    if((tab->label == NULL || tab->label[0] == '\0') && tab->icon.id != 0)
        return icon_tab_w;

    if(tab->label == NULL || tab->label[0] == '\0')
        return min_tab_w;

    label_w = TextWidth(tab->label, bar.font > 0 ? bar.font : Text12);
    w = label_w + Scale(16);
    if(w < min_tab_w)
        w = min_tab_w;
    if(w > max_tab_w)
        w = max_tab_w;
    return w;
}

static int
ui_tab_bar_next_enabled(TabBarProps bar, int from, int direction)
{
    for(int step = 1; step <= bar.count; step++) {
        int index = (from + direction * step) % bar.count;

        if(index < 0)
            index += bar.count;
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
       bar.bounds.height <= 0 || bar.disabled || UIContentDisabled() ||
       bar.id <= 0 || !RegisterUIFocus(bar.id, bar.bounds) ||
       ui_popup_input_focus_captures(bar.id))
        return -1;

    SetUIFocusTextInputActive(0);
    selected = bar.selected_index;
    if(selected < 0 || selected >= bar.count)
        selected = 0;
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
                       int icon_tab_w, int tab_gap)
{
    int total = tab_gap * (bar.count - 1);

    if(total < 0)
        total = 0;
    for(int i = 0; i < bar.count; i++)
        total += ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                      icon_tab_w);
    return total;
}

static Rectangle
ui_tab_bar_rect_at(TabBarProps bar, int index, int min_tab_w, int max_tab_w,
                   int icon_tab_w, int tab_gap, int scroll, int equal_tabs)
{
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;
    int tab_x = equal_tabs ? bar_x : bar_x + tab_gap - scroll;
    int tab_w = min_tab_w;

    for(int i = 0; i <= index && i < bar.count; i++) {
        tab_w = equal_tabs ? bar_w / bar.count :
                ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                     icon_tab_w);
        if(equal_tabs && i == bar.count - 1)
            tab_w = bar_x + bar_w - tab_x;
        if(i == index)
            return (Rectangle){(float)tab_x, (float)bar_y,
                               (float)tab_w, (float)bar_h};
        tab_x += tab_w + tab_gap;
    }
    return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
}

static int
ui_tab_bar_reorder_target(TabBarProps bar, int active_index, int min_tab_w,
                          int max_tab_w, int icon_tab_w, int tab_gap,
                          int scroll, int equal_tabs, int pointer_x)
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
                                  tab_gap, scroll, equal_tabs);
        center_x = (int)(rect.x + rect.width / 2.0f);
        if(pointer_x > center_x)
            target++;
    }
    return ui_clampi(target, 0, bar.count - 1);
}

static void
ui_draw_tab_shape(int x, int y, int w, int h, int selected, Color fill,
                  Color border_light, Color border_dark)
{
    int top_y = selected ? y : y + Scale(4);
    int bottom_y = y + h - 2;

    DrawRectangle(x, top_y, w, bottom_y - top_y + 1, fill);
    DrawLine(x, top_y, x + w - 1, top_y, border_light);
    DrawLine(x, top_y, x, bottom_y, border_light);
    DrawLine(x + w - 1, top_y, x + w - 1, bottom_y, border_dark);
    if(!selected)
        DrawLine(x, bottom_y, x + w - 1, bottom_y, border_dark);
}

static int
ui_pane_tab_bar_tab_width(PaneTabBar bar, int index, int min_tab_w,
                          int max_tab_w, int icon_tab_w)
{
    const Tab *tab;
    int label_w;
    int w;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    if((tab->label == NULL || tab->label[0] == '\0') && tab->icon.id != 0)
        return icon_tab_w;

    if(tab->label == NULL || tab->label[0] == '\0')
        return min_tab_w;

    label_w = TextWidth(tab->label, bar.font > 0 ? bar.font : Text12);
    w = label_w + Scale(16);
    if(w < min_tab_w)
        w = min_tab_w;
    if(w > max_tab_w)
        w = max_tab_w;
    return w;
}

int
RenderTabBar(TabBarProps bar)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int clicked_tab = -1;
    int font = bar.font > 0 ? bar.font : Text12;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;
    int tab_gap = ui_default_style() ? Scale(6) : 0;
    int default_min_tab_w = ui_default_style() ? Scale(72) : Scale(120);
    int default_max_tab_w = ui_default_style() ? Scale(168) : default_min_tab_w;
    int min_tab_w = bar.min_tab_width > 0 ? bar.min_tab_width : default_min_tab_w;
    int max_tab_w = bar.max_tab_width > 0 ? bar.max_tab_width : default_max_tab_w;
    int icon_tab_w = bar_h + tab_gap * 2;
    int cues = UITransitionCuesEnabled();
    int disabled = bar.disabled || UIContentDisabled();
    int focused = 0;
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
    focused = !disabled && bar.id > 0 && GetUIFocus() == bar.id &&
              !ui_popup_input_focus_captures(bar.id);

    if(ui_default_style())
        DrawRectangle(bar_x, bar_y, bar_w, bar_h,
                      ui_default_scheme().surface_container);
    else {
        DrawRectangle(bar_x, bar_y, bar_w, bar_h, DarkenUIColor(c_bg, 12));
        DrawLine(bar_x, bar_y, bar_x + bar_w, bar_y, DarkenUIColor(c_bg, 38));
        DrawLine(bar_x, bar_y + bar_h - 1, bar_x + bar_w,
                 bar_y + bar_h - 1, c_link);
    }

    if(max_tab_w < min_tab_w)
        max_tab_w = min_tab_w;
    if(icon_tab_w > max_tab_w)
        icon_tab_w = max_tab_w;

    // Calculate if scrolling is needed
    int total_tabs_w = ui_tab_bar_total_width(bar, min_tab_w, max_tab_w,
                                              icon_tab_w, tab_gap);
    int needs_scroll = total_tabs_w > bar_w;
    int equal_tabs = !needs_scroll;

    // Set scroll offset
    if(*scroll_offset < 0)
        *scroll_offset = 0;
    int max_scroll = total_tabs_w - bar_w;
    if(max_scroll < 0)
        max_scroll = 0;
    if(*scroll_offset > max_scroll)
        *scroll_offset = max_scroll;

    if(equal_tabs)
        *scroll_offset = 0;

    if(needs_scroll && bar.focus_selected &&
       bar.selected_index >= 0 && bar.selected_index < bar.count) {
        int selected_tab_w = ui_tab_bar_tab_width(bar, bar.selected_index,
                                                  min_tab_w, max_tab_w, icon_tab_w);
        int selected_tab_x = bar_x + tab_gap - *scroll_offset;
        for(int i = 0; i < bar.selected_index; i++)
            selected_tab_x += ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w,
                                                   icon_tab_w) + tab_gap;
        int selected_tab_end = selected_tab_x + selected_tab_w;

        if(selected_tab_x < bar_x)
            *scroll_offset -= (bar_x - selected_tab_x) + tab_gap;
        else if(selected_tab_end > bar_x + bar_w)
            *scroll_offset += (selected_tab_end - (bar_x + bar_w)) + tab_gap;

        if(*scroll_offset < 0)
            *scroll_offset = 0;
        if(*scroll_offset > max_scroll)
            *scroll_offset = max_scroll;
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
        int threshold = Scale(6);

        if(!tab_bar_store->reorder_drag_active &&
           abs_dx >= threshold && abs_dx >= abs_dy) {
            tab_bar_store->reorder_drag_active = 1;
            g_ui_pointer_owner = UI_POINTER_OWNER_REORDER;
        }
        if(tab_bar_store->reorder_drag_active) {
            drag_target = ui_tab_bar_reorder_target(
                bar, tab_bar_store->press_index, min_tab_w, max_tab_w,
                icon_tab_w, tab_gap,
                *scroll_offset, equal_tabs, (int)mouse_world.x);
            PushUIInputCapture((Rectangle){0.0f, 0.0f,
                                           (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
    }

    ui_begin_world_clip(bar.bounds);
    PushUIInputClip(bar.bounds);
    for(int i = 0; i < bar.count; i++) {
        const Tab *tab = &bar.tabs[i];
        int tab_w = equal_tabs ? bar_w / bar.count :
                    ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w, icon_tab_w);
        if(equal_tabs && i == bar.count - 1)
            tab_w = bar_x + bar_w - tab_x;
        Rectangle tab_rect = {(float)tab_x, (float)bar_y, (float)tab_w, (float)bar_h};
        int input_captured = UIInputCapturesClick(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && UIHoverEffectsEnabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = disabled || tab->disabled;

        if(is_selected && bar.selected_tab_bounds != NULL)
            *bar.selected_tab_bounds = tab_rect;

        Color tab_fill;
        if(ui_default_style()) {
            ThemeScheme scheme = ui_default_scheme();
            int indicator_w = Scale(56);
            int indicator_h = Scale(28);
            int indicator_x;
            int indicator_y = bar_y + (bar_h - indicator_h) / 2;

            if(!is_disabled)
                ui_default_state_layer(tab_rect,
                                        is_selected ? scheme.on_secondary :
                                                      scheme.on_surface_variant,
                                        is_hovered, 0,
                                        is_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
            if(is_selected) {
                if(indicator_w > tab_w - Scale(24))
                    indicator_w = tab_w - Scale(24);
                if(indicator_w > 0) {
                    indicator_x = tab_x + (tab_w - indicator_w) / 2;
                    DrawRectangleRounded((Rectangle){(float)indicator_x,
                                                     (float)indicator_y,
                                                     (float)indicator_w,
                                                     (float)indicator_h},
                                         0.50f, 12, scheme.secondary);
                }
            }
        } else {
            if(is_disabled) {
                tab_fill = DarkenUIColor(c_bg, 10);
            } else if(is_selected) {
                tab_fill = LightenUIColor(c_bg, 4);
            } else if(is_hovered) {
                tab_fill = LightenUIColor(c_bg, cues ? 6 : 4);
            } else {
                tab_fill = DarkenUIColor(c_bg, 4);
            }
            ui_draw_tab_shape(tab_x, bar_y, tab_w, bar_h, is_selected,
                              tab_fill, LightenUIColor(tab_fill, 18),
                              DarkenUIColor(tab_fill, 18));
        }

        if(!ui_default_style() && is_selected) {
            DrawLine(tab_x, bar_y + bar_h - 1, tab_x + tab_w - 1,
                     bar_y + bar_h - 1, c_link);
        } else if(!ui_default_style() && is_hovered && !is_disabled) {
            (void)cues;
            DrawLine(tab_x + Scale(4), bar_y + Scale(5),
                     tab_x + tab_w - Scale(5), bar_y + Scale(5),
                     LightenUIColor(tab_fill, 10));
        } else if(!ui_default_style() && !is_disabled) {
            DrawLine(tab_x + tab_w - 1, bar_y + Scale(8),
                     tab_x + tab_w - 1, bar_y + bar_h - Scale(4),
                     DarkenUIColor(c_bg, 14));
        }

        if(!ui_default_style() && owns_drag && drag_target == i) {
            int marker_x = tab_x;

            if(drag_target > tab_bar_store->press_index)
                marker_x = tab_x + tab_w;
            DrawRectangle(marker_x - Scale(1), bar_y + Scale(4),
                          Scale(2), bar_h - Scale(8), c_link);
        }

        // Draw tab text and icon
        int text_pad = ui_default_style() ? Scale(8) : Scale(12);
        int icon_size = tab->icon_size > 0 ? tab->icon_size : Scale(16);
        int has_label = tab->label != NULL && tab->label[0] != '\0';
        int icon_x = tab_x + text_pad;
        int text_x = icon_x + icon_size + Scale(4);
        int content_h = bar_h - Scale(8);
        int content_y = bar_y + (bar_h - content_h) / 2;
        int close_size = Scale(18);
        int close_pad = Scale(6);
        Rectangle close_rect = {
            (float)(tab_x + tab_w - text_pad - close_size),
            (float)(bar_y + (bar_h - close_size) / 2),
            (float)close_size,
            (float)close_size
        };
        int close_active = tab->closeable && !is_disabled &&
                           CheckCollisionPointRec(mouse_world, close_rect) &&
                           !input_captured;
        int close_hovered = close_active && UIHoverEffectsEnabled();

        Color text_color = ui_default_style()
                               ? ui_default_scheme().on_surface_variant
                               : c_text;
        Color icon_tint = WHITE;

        if(is_disabled) {
            text_color = DarkenUIColor(c_text, 70);
            text_color.a = text_color.a > 150 ? 150 : text_color.a;
            icon_tint.a = 150;
        } else if(is_selected) {
            if(ui_default_style()) {
                text_color = ui_default_scheme().primary;
            } else {
                text_color = LightenUIColor(c_text, 10);
            }
        }

        // Draw icon if present
        if(tab->icon.id != 0) {
            if(!has_label)
                icon_x = tab_x + (tab_w - icon_size) / 2;
            else if(ui_default_style()) {
                int gap = Scale(4);
                int label_w = TextWidth(tab->label, font);
                int content_w = icon_size + gap + label_w;
                if(content_w > tab_w - text_pad * 2)
                    content_w = tab_w - text_pad * 2;
                icon_x = tab_x + (tab_w - content_w) / 2;
                text_x = icon_x + icon_size + gap;
            }
            Rectangle icon_rect = {
                (float)icon_x,
                (float)(bar_y + (bar_h - icon_size) / 2),
                (float)icon_size,
                (float)icon_size
            };
            Rectangle icon_src = {0, 0, (float)tab->icon.width, (float)tab->icon.height};
            DrawTexturePro(tab->icon, icon_src, icon_rect, kryon_zero_vector2, 0, icon_tint);
            text_x = icon_x + icon_size + Scale(4);
        } else {
            text_x = ui_default_style() && has_label
                         ? tab_x + (tab_w - TextWidth(tab->label, font)) / 2
                         : tab_x + text_pad;
        }

        // Draw tab label
        Rectangle text_rect = {
            (float)text_x,
            (float)content_y,
            (float)(tab_x + tab_w - text_pad - text_x -
                    (tab->closeable ? close_size + close_pad : 0)),
            (float)content_h
        };

        if(ui_default_style() && tab->icon.id == 0) {
            text_rect.x = (float)(tab_x + text_pad);
            text_rect.width = (float)(tab_w - text_pad * 2 -
                (tab->closeable ? close_size + close_pad : 0));
        }

        if(text_rect.width > 0 && has_label) {
            if(tab->italic) {
                int y = TextBaselineY(tab->label, (int)text_rect.y,
                                   (int)text_rect.height, font);
                ui_begin_world_clip(text_rect);
                RenderTextStyled(tab->label, (int)text_rect.x, y,
                                   (TextStyle){font, text_color, 1, 0});
                EndUIClip();
            } else if(ui_default_style()) {
                int label_x = (int)text_rect.x +
                    ((int)text_rect.width - TextWidth(tab->label, font)) / 2;
                int label_y = TextBaselineY(tab->label, (int)text_rect.y,
                                            (int)text_rect.height, font);
                ui_begin_world_clip(text_rect);
                RenderText(tab->label, label_x, label_y, font, text_color);
                EndUIClip();
            } else
                DrawLeftUIControlTextInRect(tab->label, text_rect, font, text_color);
        }

        if(tab->closeable) {
            Color close_color = close_hovered ? c_link : icon_tint;
            if(close_hovered)
                DrawRectangleRounded(close_rect, 0.40f, 6,
                                     ui_default_style() ? ui_default_scheme().surface_variant
                                                         : DarkenUIColor(c_button_hover, 8));
            RenderText("x",
                         (int)(close_rect.x + (close_rect.width -
                                               (float)TextWidth("x", font)) * 0.5f),
                         TextBaselineY("x", (int)close_rect.y, (int)close_rect.height, font),
                         font, close_color);
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
                UIConsumeRelease();
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
                    SetUIFocus(bar.id);
                    focused = 1;
                }
            }
        }

        if(focused && i == (clicked_tab >= 0 ? clicked_tab : bar.selected_index))
            RenderFocus(tab_rect);

        tab_x += tab_w + tab_gap;
    }

    PopUIInputClip();
    EndUIClip();

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
                *scroll_offset, equal_tabs, (int)mouse_world.x);
        if(target >= 0 && target < bar.count &&
           target != tab_bar_store->press_index) {
            *bar.reordered_from_index = tab_bar_store->press_index;
            *bar.reordered_to_index = target;
        }
        clicked_tab = -1;
        UIConsumeRelease();
    }
    if((released || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) && owns_press) {
        tab_bar_store->press_index = -1;
        tab_bar_store->press_bar_id = 0;
        tab_bar_store->press_bar_bounds =
            (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
        tab_bar_store->reorder_drag_active = 0;
        if(g_ui_pointer_owner == UI_POINTER_OWNER_REORDER)
            g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
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
        UIConsumeRelease();
    return clicked_tab;
}

PaneTabBarResult
RenderPaneTabBar(PaneTabBar bar)
{
    PaneTabBarResult result = {-1, -1};
    TabBarProps tabs = {0};
    Vector2 mouse = ui_mouse_world();
    int font = bar.font > 0 ? bar.font : Text12;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_h = (int)bar.bounds.height;
    int tab_gap = ui_default_style() ? 0 : Scale(4);
    int min_tab_w = bar.min_tab_width > 0 ? bar.min_tab_width : Scale(92);
    int max_tab_w = bar.max_tab_width > 0 ? bar.max_tab_width : min_tab_w;
    int icon_tab_w = bar_h + tab_gap * 2;
    int scroll = bar.scroll_offset != NULL ? *bar.scroll_offset : 0;
    int total_gap_w;
    int total_tabs_w;
    int needs_scroll;
    int equal_tabs;
    int tab_x;
    int drag_threshold = Scale(6);

    if(bar.dragged_index != NULL)
        *bar.dragged_index = -1;

    tabs.bounds = bar.bounds;
    tabs.tabs = bar.tabs;
    tabs.count = bar.count;
    tabs.selected_index = bar.selected_index;
    tabs.font = font;
    tabs.min_tab_width = min_tab_w;
    tabs.max_tab_width = max_tab_w;
    tabs.scroll_offset = bar.scroll_offset;
    tabs.focus_selected = 0;
    tabs.closed_index = NULL;
    result.clicked_index = RenderTabBar(tabs);

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 ||
       bar.bounds.height <= 0)
        return result;

    if(max_tab_w < min_tab_w)
        max_tab_w = min_tab_w;
    if(icon_tab_w > max_tab_w)
        icon_tab_w = max_tab_w;

    total_gap_w = tab_gap * (bar.count - 1);
    total_tabs_w = total_gap_w;
    for(int i = 0; i < bar.count; i++)
        total_tabs_w += ui_pane_tab_bar_tab_width(bar, i, min_tab_w,
                                                  max_tab_w, icon_tab_w);
    needs_scroll = total_tabs_w > (int)bar.bounds.width;
    equal_tabs = ui_default_style() && !needs_scroll;

    if(bar.scroll_offset != NULL)
        scroll = *bar.scroll_offset;
    if(!needs_scroll)
        scroll = 0;

    tab_x = equal_tabs ? bar_x : bar_x + tab_gap - scroll;
    for(int i = 0; i < bar.count; i++) {
        int tab_w = equal_tabs ? (int)bar.bounds.width / bar.count :
                    ui_pane_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w, icon_tab_w);
        if(equal_tabs && i == bar.count - 1)
            tab_w = bar_x + (int)bar.bounds.width - tab_x;
        Rectangle tab_rect = {(float)tab_x, (float)bar_y,
                              (float)tab_w, (float)bar_h};
        if(CheckCollisionPointRec(mouse, tab_rect) &&
           !UIInputCapturesClick(mouse)) {
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                tab_bar_store->pane_press_index = i;
                tab_bar_store->pane_press_position = mouse;
                tab_bar_store->pane_drag_reported = 0;
            } else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
                      tab_bar_store->pane_press_index == i &&
                      !tab_bar_store->pane_drag_reported) {
                int dx = (int)(mouse.x -
                               tab_bar_store->pane_press_position.x);
                int dy = (int)(mouse.y -
                               tab_bar_store->pane_press_position.y);
                if(dx < 0)
                    dx = -dx;
                if(dy < 0)
                    dy = -dy;
                if(dx >= drag_threshold || dy >= drag_threshold) {
                    result.dragged_index = i;
                    if(bar.dragged_index != NULL)
                        *bar.dragged_index = i;
                    tab_bar_store->pane_drag_reported = 1;
                }
            }
            MarkClickable();
        }
        tab_x += tab_w + tab_gap;
    }
    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        tab_bar_store->pane_press_index = -1;
        tab_bar_store->pane_drag_reported = 0;
    }

    return result;
}

PaneDropZone
GetPaneDropZone(Rectangle bounds, Vector2 mouse)
{
    int edge;

    if(!CheckCollisionPointRec(mouse, bounds))
        return PaneDropNone;

    edge = Scale(46);
    if(mouse.x < bounds.x + (float)edge)
        return PaneDropLeft;
    if(mouse.x > bounds.x + bounds.width - (float)edge)
        return PaneDropRight;
    if(mouse.y < bounds.y + (float)edge)
        return PaneDropTop;
    if(mouse.y > bounds.y + bounds.height - (float)edge)
        return PaneDropBottom;

    return PaneDropCenter;
}

void
RenderPaneDropPreview(Rectangle bounds, PaneDropZone zone)
{
    Rectangle preview = bounds;

    if(zone == PaneDropNone)
        return;

    if(zone == PaneDropLeft) {
        preview.width = bounds.width * 0.35f;
    } else if(zone == PaneDropRight) {
        preview.x = bounds.x + bounds.width * 0.65f;
        preview.width = bounds.width * 0.35f;
    } else if(zone == PaneDropTop) {
        preview.height = bounds.height * 0.35f;
    } else if(zone == PaneDropBottom) {
        preview.y = bounds.y + bounds.height * 0.65f;
        preview.height = bounds.height * 0.35f;
    }

    DrawRectangleRec(preview, Fade(c_link, 0.18f));
    DrawRectangleLinesEx(preview, 2.0f, c_link);
}
