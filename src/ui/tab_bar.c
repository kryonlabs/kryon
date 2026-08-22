#include "ui_internal.h"

int
ui_tab_bar_height(void)
{
    return ui_material_style() ? ScaleUIPx(48) : ScaleUIPx(36);
}

int
GetUITabBarHeight(void)
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
    const UITab *tab;
    int label_w;
    int w;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    if((tab->label == NULL || tab->label[0] == '\0') && tab->icon.id != 0)
        return icon_tab_w;

    if(tab->label == NULL || tab->label[0] == '\0')
        return min_tab_w;

    label_w = TextWidth(tab->label, bar.font > 0 ? bar.font : UI_TEXT_12);
    w = label_w + ScaleUIPx(16);
    if(w < min_tab_w)
        w = min_tab_w;
    if(w > max_tab_w)
        w = max_tab_w;
    return w;
}

static int
ui_pane_tab_bar_tab_width(UIPaneTabBar bar, int index, int min_tab_w,
                          int max_tab_w, int icon_tab_w)
{
    const UITab *tab;
    int label_w;
    int w;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    if((tab->label == NULL || tab->label[0] == '\0') && tab->icon.id != 0)
        return icon_tab_w;

    if(tab->label == NULL || tab->label[0] == '\0')
        return min_tab_w;

    label_w = TextWidth(tab->label, bar.font > 0 ? bar.font : UI_TEXT_12);
    w = label_w + ScaleUIPx(16);
    if(w < min_tab_w)
        w = min_tab_w;
    if(w > max_tab_w)
        w = max_tab_w;
    return w;
}

int
DrawUITabBar(TabBarProps bar)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int clicked_tab = -1;
    int font = bar.font > 0 ? bar.font : UI_TEXT_12;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;
    int tab_gap = ScaleUIPx(4);
    int default_min_tab_w = ui_material_style() ? ScaleUIPx(72) : ScaleUIPx(120);
    int default_max_tab_w = ui_material_style() ? ScaleUIPx(168) : default_min_tab_w;
    int min_tab_w = bar.min_tab_width > 0 ? bar.min_tab_width : default_min_tab_w;
    int max_tab_w = bar.max_tab_width > 0 ? bar.max_tab_width : default_max_tab_w;
    int icon_tab_w = bar_h + tab_gap * 2;
    int cues = UITransitionCuesEnabled();
    static int default_scroll_offset = 0;
    static Vector2 last_drag_pos = {0};
    static int is_dragging = 0;
    static int last_clicked_tab = -1;
    static double last_click_time = 0.0;
    int *scroll_offset = bar.scroll_offset != NULL ? bar.scroll_offset
                                                   : &default_scroll_offset;

    if(bar.closed_index != NULL)
        *bar.closed_index = -1;
    if(bar.double_clicked_index != NULL)
        *bar.double_clicked_index = -1;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 || bar.bounds.height <= 0)
        return -1;

    if(ui_material_style())
        DrawRectangle(bar_x, bar_y, bar_w, bar_h,
                      ui_material_scheme().surface_container);
    else {
        DrawRectangle(bar_x, bar_y, bar_w, bar_h, DarkenUIColor(c_bg, 12));
        DrawLine(bar_x, bar_y, bar_x + bar_w, bar_y, DarkenUIColor(c_bg, 38));
    }

    if(max_tab_w < min_tab_w)
        max_tab_w = min_tab_w;
    if(icon_tab_w > max_tab_w)
        icon_tab_w = max_tab_w;

    if(ui_material_style())
        tab_gap = ScaleUIPx(6);

    // Calculate if scrolling is needed
    int total_gap_w = tab_gap * (bar.count - 1);
    int total_tabs_w = total_gap_w;
    for(int i = 0; i < bar.count; i++)
        total_tabs_w += ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w, icon_tab_w);
    int needs_scroll = total_tabs_w > bar_w;
    int equal_tabs = 0;

    // Set scroll offset
    if(*scroll_offset < 0)
        *scroll_offset = 0;
    int max_scroll = total_tabs_w - bar_w;
    if(max_scroll < 0)
        max_scroll = 0;
    if(*scroll_offset > max_scroll)
        *scroll_offset = max_scroll;

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

    // Material top tabs are distributed equally across the full app bar.
    int tab_x = equal_tabs ? bar_x : bar_x + tab_gap - *scroll_offset;

    for(int i = 0; i < bar.count; i++) {
        const UITab *tab = &bar.tabs[i];
        int tab_w = equal_tabs ? bar_w / bar.count :
                    ui_tab_bar_tab_width(bar, i, min_tab_w, max_tab_w, icon_tab_w);
        if(equal_tabs && i == bar.count - 1)
            tab_w = bar_x + bar_w - tab_x;
        Rectangle tab_rect = {(float)tab_x, (float)bar_y, (float)tab_w, (float)bar_h};
        int input_captured = UIInputCapturesClick(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && UIHoverEffectsEnabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = tab->disabled;

        Color tab_fill;
        if(ui_material_style()) {
            UIMaterialScheme scheme = ui_material_scheme();
            int indicator_w = ScaleUIPx(56);
            int indicator_h = ScaleUIPx(28);
            int indicator_x;
            int indicator_y = bar_y + (bar_h - indicator_h) / 2;

            if(!is_disabled)
                ui_material_state_layer(tab_rect,
                                        is_selected ? scheme.on_secondary :
                                                      scheme.on_surface_variant,
                                        is_hovered, 0,
                                        is_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
            if(is_selected) {
                if(indicator_w > tab_w - ScaleUIPx(24))
                    indicator_w = tab_w - ScaleUIPx(24);
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
                tab_fill = DarkenUIColor(c_bg, 18);
            } else if(is_selected) {
                tab_fill = c_button;
            } else if(is_hovered) {
                tab_fill = DarkenUIColor(c_button_hover, cues ? 2 : 8);
            } else {
                tab_fill = DarkenUIColor(c_bg, 10);
            }
            DrawRectangleRounded(tab_rect, 0.15f, 4, tab_fill);
        }

        if(!ui_material_style() && is_selected) {
            // Strong bevel for selected tab (appears raised)
            DrawUIBevel(tab_x, bar_y, tab_w, bar_h,
                         LightenUIColor(tab_fill, 50),
                         DarkenUIColor(tab_fill, 30));
            if(cues && tab_w > ScaleUIPx(18)) {
                int cue_h = ScaleUIPx(2);
                if(cue_h < 1)
                    cue_h = 1;
                DrawRectangle(tab_x + ScaleUIPx(9), bar_y + bar_h - cue_h,
                              tab_w - ScaleUIPx(18), cue_h,
                              LightenUIColor(c_button_hover, 18));
            }
        } else if(!ui_material_style() && is_hovered && !is_disabled) {
            // Enhanced bevel for hovered tab
            DrawUIBevel(tab_x, bar_y, tab_w, bar_h,
                         LightenUIColor(tab_fill, cues ? 42 : 30),
                         DarkenUIColor(tab_fill, 20));
            if(cues && tab_w > ScaleUIPx(8)) {
                Color cue = LightenUIColor(tab_fill, 40);
                cue.a = cue.a > 150 ? 150 : cue.a;
                DrawRectangle(tab_x + ScaleUIPx(4), bar_y + ScaleUIPx(1),
                              tab_w - ScaleUIPx(8), ScaleUIPx(1), cue);
            }
        } else if(!ui_material_style() && !is_disabled) {
            // Subtle bevel for normal tab
            DrawUIBevel(tab_x, bar_y, tab_w, bar_h,
                         LightenUIColor(tab_fill, 20),
                         DarkenUIColor(tab_fill, 15));
        }

        // Draw tab text and icon
        int text_pad = ScaleUIPx(8);
        int icon_size = tab->icon_size > 0 ? tab->icon_size : ScaleUIPx(16);
        int has_label = tab->label != NULL && tab->label[0] != '\0';
        int icon_x = tab_x + text_pad;
        int text_x = icon_x + icon_size + ScaleUIPx(4);
        int content_h = bar_h - ScaleUIPx(8);
        int content_y = bar_y + (bar_h - content_h) / 2;
        int close_size = ScaleUIPx(18);
        int close_pad = ScaleUIPx(6);
        Rectangle close_rect = {
            (float)(tab_x + tab_w - text_pad - close_size),
            (float)(bar_y + (bar_h - close_size) / 2),
            (float)close_size,
            (float)close_size
        };
        int close_active = tab->closeable &&
                           CheckCollisionPointRec(mouse_world, close_rect) &&
                           !input_captured;
        int close_hovered = close_active && UIHoverEffectsEnabled();

        Color text_color = ui_material_style()
                               ? ui_material_scheme().on_surface_variant
                               : c_text;
        Color icon_tint = WHITE;

        if(is_disabled) {
            text_color = DarkenUIColor(c_text, 70);
            text_color.a = text_color.a > 150 ? 150 : text_color.a;
            icon_tint.a = 150;
        } else if(is_selected) {
            if(ui_material_style()) {
                text_color = ui_material_scheme().primary;
            } else {
                text_color = LightenUIColor(c_text, 10);
            }
        }

        // Draw icon if present
        if(tab->icon.id != 0) {
            if(!has_label)
                icon_x = tab_x + (tab_w - icon_size) / 2;
            else if(ui_material_style()) {
                int gap = ScaleUIPx(4);
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
            DrawTexturePro(tab->icon, icon_src, icon_rect, (Vector2){0}, 0, icon_tint);
            text_x = icon_x + icon_size + ScaleUIPx(4);
        } else {
            text_x = ui_material_style() && has_label
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

        if(text_rect.width > 0 && has_label) {
            if(tab->italic) {
                int y = TextBaselineY(tab->label, (int)text_rect.y,
                                   (int)text_rect.height, font);
                BeginUIClip((int)text_rect.x, (int)text_rect.y,
                            (int)text_rect.width, (int)text_rect.height);
                DrawUITextStyled(tab->label, (int)text_rect.x, y,
                                   (TextStyle){font, text_color, 1, 0});
                EndUIClip();
            } else if(ui_material_style())
                DrawCenteredUIControlText(tab->label,
                                          (int)(text_rect.x + text_rect.width / 2),
                                          (int)(text_rect.y + text_rect.height / 2),
                                          font, text_color);
            else
                DrawLeftUIControlTextInRect(tab->label, text_rect, font, text_color);
        }

        if(tab->closeable) {
            Color close_color = close_hovered ? c_link : icon_tint;
            if(close_hovered)
                DrawRectangleRounded(close_rect, 0.40f, 6,
                                     ui_material_style() ? ui_material_scheme().surface_variant
                                                         : DarkenUIColor(c_button_hover, 8));
            DrawUIText("x",
                         (int)(close_rect.x + (close_rect.width -
                                               (float)TextWidth("x", font)) * 0.5f),
                         TextBaselineY("x", (int)close_rect.y, (int)close_rect.height, font),
                         font, close_color);
        }

        // Handle click detection
        if(is_active) {
            if(is_disabled)
                MarkUIDisabled();
            else
                MarkUIClickable();

            if(close_active && released) {
                clicked_tab = -1;
                if(bar.closed_index != NULL)
                    *bar.closed_index = i;
                last_clicked_tab = -1;
                last_click_time = 0.0;
                UIConsumeRelease();
            } else if(!close_active && released) {
                double now = GetTime();

                if(bar.double_clicked_index != NULL && last_clicked_tab == i &&
                   now - last_click_time <= 0.45)
                    *bar.double_clicked_index = i;
                last_clicked_tab = i;
                last_click_time = now;
                clicked_tab = i;
            }
        }

        tab_x += tab_w + tab_gap;
    }

    if(needs_scroll) {
        // Handle manual drag scrolling
        Vector2 current_pos = mouse_world;
        int is_mouse_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        // Check if mouse is over tab bar area
        Rectangle scroll_area = {(float)bar_x, (float)bar_y, (float)bar_w, (float)bar_h};
        int is_over_bar = CheckCollisionPointRec(current_pos, scroll_area);

        if(is_mouse_down && is_over_bar && !is_dragging) {
            is_dragging = 1;
            last_drag_pos = current_pos;
        }

        if(is_dragging) {
            if(is_mouse_down) {
                float dx = current_pos.x - last_drag_pos.x;
                *scroll_offset -= (int)dx;

                // Clamp scroll offset
                if(*scroll_offset < 0)
                    *scroll_offset = 0;
                if(*scroll_offset > max_scroll)
                    *scroll_offset = max_scroll;

                last_drag_pos = current_pos;
            } else {
                is_dragging = 0;
            }
        }
    }

    if(clicked_tab >= 0)
        UIConsumeRelease();
    return clicked_tab;
}

UIPaneTabBarResult
DrawUIPaneTabBar(UIPaneTabBar bar)
{
    UIPaneTabBarResult result = {-1, -1};
    TabBarProps tabs = {0};
    Vector2 mouse = ui_mouse_world();
    int font = bar.font > 0 ? bar.font : UI_TEXT_12;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_h = (int)bar.bounds.height;
    int tab_gap = ui_material_style() ? 0 : ScaleUIPx(4);
    int min_tab_w = bar.min_tab_width > 0 ? bar.min_tab_width : ScaleUIPx(92);
    int max_tab_w = bar.max_tab_width > 0 ? bar.max_tab_width : min_tab_w;
    int icon_tab_w = bar_h + tab_gap * 2;
    int scroll = bar.scroll_offset != NULL ? *bar.scroll_offset : 0;
    int total_gap_w;
    int total_tabs_w;
    int needs_scroll;
    int equal_tabs;
    int tab_x;
    int drag_threshold = ScaleUIPx(6);
    static Vector2 press_pos = {0};
    static int press_index = -1;
    static int drag_reported = 0;

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
    result.clicked_index = DrawUITabBar(tabs);

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
    equal_tabs = ui_material_style() && !needs_scroll;

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
                press_index = i;
                press_pos = mouse;
                drag_reported = 0;
            } else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
                      press_index == i &&
                      !drag_reported) {
                int dx = (int)(mouse.x - press_pos.x);
                int dy = (int)(mouse.y - press_pos.y);
                if(dx < 0)
                    dx = -dx;
                if(dy < 0)
                    dy = -dy;
                if(dx >= drag_threshold || dy >= drag_threshold) {
                    result.dragged_index = i;
                    if(bar.dragged_index != NULL)
                        *bar.dragged_index = i;
                    drag_reported = 1;
                }
            }
            MarkUIClickable();
        }
        tab_x += tab_w + tab_gap;
    }
    if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        press_index = -1;
        drag_reported = 0;
    }

    return result;
}

UIPaneDropZone
GetUIPaneDropZone(Rectangle bounds, Vector2 mouse)
{
    int edge;

    if(!CheckCollisionPointRec(mouse, bounds))
        return UI_PANE_DROP_NONE;

    edge = ScaleUIPx(46);
    if(mouse.x < bounds.x + (float)edge)
        return UI_PANE_DROP_LEFT;
    if(mouse.x > bounds.x + bounds.width - (float)edge)
        return UI_PANE_DROP_RIGHT;
    if(mouse.y < bounds.y + (float)edge)
        return UI_PANE_DROP_TOP;
    if(mouse.y > bounds.y + bounds.height - (float)edge)
        return UI_PANE_DROP_BOTTOM;

    return UI_PANE_DROP_CENTER;
}

void
DrawUIPaneDropPreview(Rectangle bounds, UIPaneDropZone zone)
{
    Rectangle preview = bounds;

    if(zone == UI_PANE_DROP_NONE)
        return;

    if(zone == UI_PANE_DROP_LEFT) {
        preview.width = bounds.width * 0.35f;
    } else if(zone == UI_PANE_DROP_RIGHT) {
        preview.x = bounds.x + bounds.width * 0.65f;
        preview.width = bounds.width * 0.35f;
    } else if(zone == UI_PANE_DROP_TOP) {
        preview.height = bounds.height * 0.35f;
    } else if(zone == UI_PANE_DROP_BOTTOM) {
        preview.y = bounds.y + bounds.height * 0.65f;
        preview.height = bounds.height * 0.35f;
    }

    DrawRectangleRec(preview, Fade(c_link, 0.18f));
    DrawRectangleLinesEx(preview, 2.0f, c_link);
}
