#include "ui_internal.h"

int
GetUITabBarHeight(void)
{
    return ScaleUIPx(36);
}

static int
ui_tab_bar_tab_width(UITabBar bar, int index, int min_tab_w, int icon_tab_w)
{
    const UITab *tab;

    if(index < 0 || index >= bar.count || bar.tabs == NULL)
        return min_tab_w;

    tab = &bar.tabs[index];
    if((tab->label == NULL || tab->label[0] == '\0') && tab->icon.id != 0)
        return icon_tab_w;

    return min_tab_w;
}

int
DrawUITabBar(UITabBar bar)
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
    int min_tab_w = bar.min_tab_width > 0 ? bar.min_tab_width : ScaleUIPx(120);
    int max_tab_w = bar.max_tab_width > 0 ? bar.max_tab_width : min_tab_w;
    int icon_tab_w = bar_h + tab_gap * 2;
    int cues = UITransitionCuesEnabled();
    static int default_scroll_offset = 0;
    static Vector2 last_drag_pos = {0};
    static int is_dragging = 0;
    int *scroll_offset = bar.scroll_offset != NULL ? bar.scroll_offset
                                                   : &default_scroll_offset;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 || bar.bounds.height <= 0)
        return -1;

    // Draw tab bar background (recessed appearance)
    DrawRectangle(bar_x, bar_y, bar_w, bar_h, DarkenUIColor(c_bg, 12));
    DrawLine(bar_x, bar_y, bar_x + bar_w, bar_y, DarkenUIColor(c_bg, 38));

    if(max_tab_w < min_tab_w)
        max_tab_w = min_tab_w;
    if(icon_tab_w > max_tab_w)
        icon_tab_w = max_tab_w;

    // Calculate if scrolling is needed
    int total_gap_w = tab_gap * (bar.count - 1);
    int total_tabs_w = total_gap_w;
    for(int i = 0; i < bar.count; i++)
        total_tabs_w += ui_tab_bar_tab_width(bar, i, min_tab_w, icon_tab_w);
    int needs_scroll = total_tabs_w > bar_w;

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
                                                  min_tab_w, icon_tab_w);
        int selected_tab_x = bar_x + tab_gap - *scroll_offset;
        for(int i = 0; i < bar.selected_index; i++)
            selected_tab_x += ui_tab_bar_tab_width(bar, i, min_tab_w,
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

    // Start from left (not centered)
    int tab_x = bar_x + tab_gap - *scroll_offset;

    for(int i = 0; i < bar.count; i++) {
        const UITab *tab = &bar.tabs[i];
        int tab_w = ui_tab_bar_tab_width(bar, i, min_tab_w, icon_tab_w);
        Rectangle tab_rect = {(float)tab_x, (float)bar_y, (float)tab_w, (float)bar_h};
        int input_captured = UIInputCapturesClick(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && UIHoverEffectsEnabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = tab->disabled;

        // Determine tab background color
        Color tab_fill;
        if(is_disabled) {
            tab_fill = DarkenUIColor(c_bg, 18);
        } else if(is_selected) {
            tab_fill = c_button;
        } else if(is_hovered) {
            tab_fill = DarkenUIColor(c_button_hover, cues ? 2 : 8);
        } else {
            tab_fill = DarkenUIColor(c_bg, 10);
        }

        // Draw tab shape with rounded top corners (Chromium-style)
        float corner_radius = 0.15f;
        DrawRectangleRounded(tab_rect, corner_radius, 4, tab_fill);

        // Apply 3D bevel effect for depth
        if(is_selected) {
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
        } else if(is_hovered && !is_disabled) {
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
        } else if(!is_disabled) {
            // Subtle bevel for normal tab
            DrawUIBevel(tab_x, bar_y, tab_w, bar_h,
                         LightenUIColor(tab_fill, 20),
                         DarkenUIColor(tab_fill, 15));
        }

        // Draw tab text and icon
        int text_pad = ScaleUIPx(8);
        int icon_size = tab->icon_size > 0 ? tab->icon_size : ScaleUIPx(16);
        int icon_x = tab_x + text_pad;
        int text_x = icon_x + icon_size + ScaleUIPx(4);
        int content_h = bar_h - ScaleUIPx(8);
        int content_y = bar_y + (bar_h - content_h) / 2;

        Color text_color = c_text;
        Color icon_color = c_icon;

        if(is_disabled) {
            text_color = DarkenUIColor(c_text, 70);
            text_color.a = text_color.a > 150 ? 150 : text_color.a;
            icon_color = DarkenUIColor(c_icon, 40);
        } else if(is_selected) {
            text_color = LightenUIColor(c_text, 10);
            icon_color = LightenUIColor(c_icon, cues ? 18 : 10);
        }

        // Draw icon if present
        if(tab->icon.id != 0) {
            Rectangle icon_rect = {
                (float)icon_x,
                (float)(bar_y + (bar_h - icon_size) / 2),
                (float)icon_size,
                (float)icon_size
            };
            Rectangle icon_src = {0, 0, (float)tab->icon.width, (float)tab->icon.height};
            DrawTexturePro(tab->icon, icon_src, icon_rect, (Vector2){0}, 0, icon_color);
            text_x = icon_x + icon_size + ScaleUIPx(4);
        } else {
            text_x = tab_x + text_pad;
        }

        // Draw tab label
        Rectangle text_rect = {
            (float)text_x,
            (float)content_y,
            (float)(tab_x + tab_w - text_pad - text_x),
            (float)content_h
        };

        if(text_rect.width > 0 && tab->label != NULL) {
            DrawLeftUIControlTextInRect(tab->label, text_rect, font, text_color);
        }

        // Handle click detection
        if(is_active) {
            if(is_disabled)
                MarkUIDisabled();
            else
                MarkUIClickable();

            if(released)
                clicked_tab = i;
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
