/*
 * Kryon Widget Rendering
 * C89/C90 compliant
 *
 * Widget-specific renderers for all widget types
 */

#include "render.h"
#include "util.h"
#include "graphics.h"
#include "widget.h"
#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * External functions
 */
extern char *strdup(const char *s);
extern int g_render_dirty;
extern int *render_get_context(void);

/*
 * Widget state structures
 */
typedef struct {
    char **options;       /* Array of option strings */
    int num_options;      /* Number of options */
    int selected_index;   /* Currently selected option (-1 if none) */
    int is_open;          /* Whether dropdown popup is visible */
    int hovered_item;     /* Which option item is being hovered (-1 if none) */
} DropdownState;

typedef struct {
    float value;           /* Current value (0-100) */
    int is_dragging;       /* Currently dragging handle */
    int is_hovering;       /* Mouse over handle */
    int is_track_hovering; /* Mouse over track area */
} SliderState;

typedef struct {
    float min_value;       /* Current minimum value (0-100) */
    float max_value;       /* Current maximum value (0-100) */
    int is_dragging_min;   /* Currently dragging min handle */
    int is_dragging_max;   /* Currently dragging max handle */
    int is_min_hovering;   /* Mouse over min handle */
    int is_max_hovering;   /* Mouse over max handle */
    int is_track_hovering; /* Mouse over track area */
} RangeSliderState;

/*
 * Forward declarations
 */
static SliderState *slider_state_create(void);
static void slider_state_destroy(SliderState *state);
static RangeSliderState *range_slider_state_create(void);
static void range_slider_state_destroy(RangeSliderState *state);

/*
 * Get the group identifier for a radio button
 */
static const char *get_radio_group_id(KryonWidget *w)
{
    static char default_group[64];

    if (w->prop_group != NULL && w->prop_group[0] != '\0') {
        return w->prop_group;
    }

    if (w->parent != NULL && w->parent->name != NULL) {
        return w->parent->name;
    }

    snprintf(default_group, sizeof(default_group), "window_%u",
             w->parent_window ? w->parent_window->id : 0);
    return default_group;
}

/*
 * Deselect all radio buttons in the same group except the clicked one
 */
static void deselect_radio_group(KryonWidget *clicked_widget)
{
    const char *group_id;
    int i;
    int count;

    if (clicked_widget == NULL) {
        return;
    }

    group_id = get_radio_group_id(clicked_widget);
    count = widget_get_count();

    for (i = 0; i < count; i++) {
        KryonWidget *other;

        other = widget_get_by_index(i);
        if (other == NULL) {
            continue;
        }

        if (other->type != WIDGET_RADIO_BUTTON) {
            continue;
        }
        if (strcmp(get_radio_group_id(other), group_id) != 0) {
            continue;
        }

        if (other->id == clicked_widget->id) {
            continue;
        }

        if (other->prop_value != NULL) {
            free(other->prop_value);
        }
        other->prop_value = strdup("0");
    }
}

/*
 * External event processing functions (from events.c)
 */
extern int render_is_widget_clicked(KryonWidget *w);
extern int render_is_point_in_open_popup(KryonWindow *win, int x, int y);
extern void render_update_slider_value(KryonWidget *w, float new_value);

/*
 * Render a single widget to screen
 */
void render_widget(KryonWidget *w, Memimage *screen)
{
    Rectangle widget_rect;
    unsigned long color;
    static int render_count = 0;
    int *render_only_popups;
    int is_popup_pass;

    if (w == NULL || screen == NULL) {
        return;
    }

    /* Check visibility */
    if (!w->prop_visible) {
        return;
    }

    /* Get render context */
    render_only_popups = render_get_context();
    is_popup_pass = *render_only_popups;

    /* During popup-only pass, skip non-dropdown widgets */
    if (is_popup_pass && w->type != WIDGET_DROPDOWN) {
        return;
    }

    render_count++;

    /* Parse widget rectangle */
    if (parse_rect(w->prop_rect, &widget_rect) < 0) {
        return;
    }

    switch (w->type) {
    case WIDGET_BUTTON:
    case WIDGET_SUBMIT_BUTTON:
    case WIDGET_ICON_BUTTON:
    case WIDGET_FAB_BUTTON:
        /* Button background: use prop_color if set, else neutral Tk-style gray */
        if (w->prop_color != NULL && w->prop_color[0] != '\0') {
            color = util_parse_color(w->prop_color);
            if (color == 0) {
                color = 0xD9D9D9FF;
            }
        } else {
            color = 0xC0C0C0FF;
        }

        /* Hover highlight */
        if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
            Point mp;
            mp.x = w->parent_window->mouse_x;
            mp.y = w->parent_window->mouse_y;
            if (ptinrect(mp, widget_rect)) {
                color = util_lighten_color(color);
            }
        }

        /* Check for pressed state */
        int is_pressed = 0;
        if (w->parent_window != NULL && window_is_valid(w->parent_window) && w->parent_window->mouse_buttons != 0) {
            Point mp;
            mp.x = w->parent_window->mouse_x;
            mp.y = w->parent_window->mouse_y;
            if (ptinrect(mp, widget_rect)) {
                is_pressed = 1;
                color = util_darken_color(color);
            }
        }

        memfillcolor_rect(screen, widget_rect, color);

        /* Draw Tk-style beveled edges */
        {
            unsigned long highlight = util_lighten_color(color);
            unsigned long shadow = util_darken_color(color);
            int thickness = 2;

            if (is_pressed) {
                unsigned long temp = highlight;
                highlight = shadow;
                shadow = temp;
            }

            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                         Pt(widget_rect.max.x - 1, widget_rect.min.y), highlight, thickness);
            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                         Pt(widget_rect.min.x, widget_rect.max.y - 1), highlight, thickness);
            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                         Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
            memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                         Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
        }

        /* Render text label */
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;
            int text_width;
            int button_width, button_height;

            {
                unsigned int cr = (color >> 24) & 0xFF;
                unsigned int cg = (color >> 16) & 0xFF;
                unsigned int cb = (color >> 8) & 0xFF;
                text_color = ((cr + cg + cb) / 3 > 128) ? DBlack : DWhite;
            }

            text_width = (int)strlen(w->prop_text) * 8;
            button_width = widget_rect.max.x - widget_rect.min.x;
            button_height = widget_rect.max.y - widget_rect.min.y;

            text_pos.x = widget_rect.min.x + (button_width - text_width) / 2;
            text_pos.y = widget_rect.min.y + (button_height - 16) / 2;

            font = memdraw_get_default_font();
            if (font != NULL) {
                memdraw_text_font(screen, text_pos, w->prop_text, font, text_color);
            } else {
                memdraw_text(screen, text_pos, w->prop_text, text_color);
            }
        }
        break;

    case WIDGET_LABEL:
    case WIDGET_HEADING:
    case WIDGET_PARAGRAPH:
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;
            int widget_height;

            if (w->prop_color != NULL && w->prop_color[0] != '\0') {
                text_color = util_parse_color(w->prop_color);
                if (text_color == 0) {
                    text_color = DBlack;
                }
            } else {
                text_color = DBlack;
            }

            widget_height = widget_rect.max.y - widget_rect.min.y;
            text_pos.x = widget_rect.min.x + 4;
            text_pos.y = widget_rect.min.y + (widget_height - 16) / 2;

            font = memdraw_get_default_font();
            if (font != NULL) {
                memdraw_text_font(screen, text_pos, w->prop_text, font, text_color);
            } else {
                memdraw_text(screen, text_pos, w->prop_text, text_color);
            }
        }
        break;

    case WIDGET_CHECKBOX:
        {
            Rectangle check_rect;
            int check_size = 16;
            Point text_pos;
            Subfont *font;
            int is_hovered = 0;
            uint32_t bg_color, highlight_color, shadow_color;

            check_rect.min.x = widget_rect.min.x;
            check_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - check_size) / 2;
            check_rect.max.x = check_rect.min.x + check_size;
            check_rect.max.y = check_rect.min.y + check_size;

            if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                Point mp;
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
            } else {
                is_hovered = 0;
            }

            bg_color = is_hovered ? 0xE0E0E0FF : 0xFFFFFFFF;
            highlight_color = 0xFFFFFFFF;
            shadow_color = 0x808080FF;

            memfillcolor_rect(screen, check_rect, bg_color);

            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                         Pt(check_rect.max.x - 1, check_rect.min.y), highlight_color, 2);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                         Pt(check_rect.min.x, check_rect.max.y - 1), highlight_color, 2);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.max.y - 1),
                         Pt(check_rect.max.x - 1, check_rect.max.y - 1), shadow_color, 2);
            memdraw_line(screen, Pt(check_rect.max.x - 1, check_rect.min.y),
                         Pt(check_rect.max.x - 1, check_rect.max.y - 1), shadow_color, 2);

            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                        Pt(check_rect.max.x - 1, check_rect.min.y), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                        Pt(check_rect.min.x, check_rect.max.y - 1), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.max.y - 1),
                        Pt(check_rect.max.x - 1, check_rect.max.y - 1), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.max.x - 1, check_rect.min.y),
                        Pt(check_rect.max.x - 1, check_rect.max.y - 1), DBlack, 1);

            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                int cx = check_rect.min.x;
                int cy = check_rect.min.y;
                Point check_start = Pt(cx + 4, cy + 8);
                Point check_mid = Pt(cx + 7, cy + 11);
                Point check_end = Pt(cx + 12, cy + 5);

                memdraw_line(screen, check_start, check_mid, DBlack, 3);
                memdraw_line(screen, check_mid, check_end, DBlack, 3);
                memdraw_line(screen, Pt(check_start.x + 1, check_start.y),
                            Pt(check_mid.x + 1, check_mid.y), DBlack, 3);
                memdraw_line(screen, Pt(check_mid.x + 1, check_mid.y),
                            Pt(check_end.x, check_end.y - 1), DBlack, 3);
            }

            if (w->prop_text != NULL && w->prop_text[0] != '\0') {
                text_pos.x = check_rect.max.x + 4;
                text_pos.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - 16) / 2;
                font = memdraw_get_default_font();
                if (font != NULL) {
                    memdraw_text_font(screen, text_pos, w->prop_text, font, DBlack);
                } else {
                    memdraw_text(screen, text_pos, w->prop_text, DBlack);
                }
            }

            if (render_is_widget_clicked(w)) {
                int new_val = (w->prop_value == NULL || atoi(w->prop_value) == 0) ? 1 : 0;
                free(w->prop_value);
                w->prop_value = (char *)malloc(2);
                if (w->prop_value != NULL) {
                    sprintf(w->prop_value, "%d", new_val);
                }
            }
        }
        break;

    case WIDGET_RADIO_BUTTON:
        {
            Rectangle radio_rect;
            Point text_pos;
            Subfont *font;
            int is_hovered = 0;
            int is_selected = 0;
            int radio_diameter = 14;
            int dot_diameter = 6;
            uint32_t bg_color, border_color, dot_color;

            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                is_selected = 1;
            }

            radio_rect.min.x = widget_rect.min.x;
            radio_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - radio_diameter) / 2;
            radio_rect.max.x = radio_rect.min.x + radio_diameter;
            radio_rect.max.y = radio_rect.min.y + radio_diameter;

            if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                Point mp;
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
            } else {
                is_hovered = 0;
            }

            bg_color = is_hovered ? 0xE0E0E0FF : 0xFFFFFFFF;
            border_color = 0x000000FF;
            dot_color = 0x000000FF;

            memfillcolor_rect(screen, radio_rect, bg_color);

            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.min.y),
                         Pt(radio_rect.max.x - 1, radio_rect.min.y), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.min.y),
                         Pt(radio_rect.min.x, radio_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.max.y - 1),
                         Pt(radio_rect.max.x - 1, radio_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.max.x - 1, radio_rect.min.y),
                         Pt(radio_rect.max.x - 1, radio_rect.max.y - 1), border_color, 1);

            if (is_selected) {
                Rectangle dot_rect;
                int dot_offset = (radio_diameter - dot_diameter) / 2;

                dot_rect.min.x = radio_rect.min.x + dot_offset;
                dot_rect.min.y = radio_rect.min.y + dot_offset;
                dot_rect.max.x = dot_rect.min.x + dot_diameter;
                dot_rect.max.y = dot_rect.min.y + dot_diameter;

                memfillcolor_rect(screen, dot_rect, dot_color);
            }

            if (w->prop_text != NULL && w->prop_text[0] != '\0') {
                text_pos.x = radio_rect.max.x + 4;
                text_pos.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - 16) / 2;
                font = memdraw_get_default_font();
                if (font != NULL) {
                    memdraw_text_font(screen, text_pos, w->prop_text, font, DBlack);
                } else {
                    memdraw_text(screen, text_pos, w->prop_text, DBlack);
                }
            }

            if (render_is_widget_clicked(w)) {
                if (!is_selected) {
                    deselect_radio_group(w);
                    free(w->prop_value);
                    w->prop_value = strdup("1");
                }
            }
        }
        break;

    case WIDGET_CONTAINER:
    case WIDGET_BOX:
    case WIDGET_FRAME:
    case WIDGET_CARD:
        color = 0xFFF0F0F0;
        memfillcolor_rect(screen, widget_rect, color);
        break;

    case WIDGET_SWITCH:
        {
            Rectangle switch_rect;
            Point text_pos;
            Subfont *font;
            int switch_width = 40;
            int switch_height = 20;
            int is_on = 0;
            int is_hovered = 0;
            uint32_t track_color, handle_color;
            Point handle_center;
            int handle_radius = 8;

            /* Parse value (0 = off, 1 = on) */
            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                is_on = 1;
            }

            /* Position switch on left side with space for text */
            switch_rect.min.x = widget_rect.min.x;
            switch_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - switch_height) / 2;
            switch_rect.max.x = switch_rect.min.x + switch_width;
            switch_rect.max.y = switch_rect.min.y + switch_height;

            /* Check hover state */
            if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                Point mp;
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, switch_rect);
            }

            /* Tk-style colors */
            if (is_on) {
                track_color = is_hovered ? 0x4CAF50FF : 0x45A049FF;  /* Green when on */
            } else {
                track_color = is_hovered ? 0xBBBBBBFF : 0xAAAAAAFF;  /* Gray when off */
            }
            handle_color = 0xFFFFFFFF;  /* White handle */

            /* Draw track (pill-shaped) */
            memfillcolor_rect(screen, switch_rect, track_color);

            /* Calculate handle position (slides left/right) */
            if (is_on) {
                handle_center.x = switch_rect.max.x - handle_radius - 2;
            } else {
                handle_center.x = switch_rect.min.x + handle_radius + 2;
            }
            handle_center.y = switch_rect.min.y + (switch_height / 2);

            /* Draw handle (circle) */
            {
                Rectangle handle_rect;
                handle_rect.min.x = handle_center.x - handle_radius;
                handle_rect.min.y = handle_center.y - handle_radius;
                handle_rect.max.x = handle_center.x + handle_radius;
                handle_rect.max.y = handle_center.y + handle_radius;
                memfillcolor_rect(screen, handle_rect, handle_color);

                /* Handle border (subtle) */
                memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                            Pt(handle_rect.max.x - 1, handle_rect.min.y), 0xDDDDDDFF, 1);
                memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                            Pt(handle_rect.min.x, handle_rect.max.y - 1), 0xDDDDDDFF, 1);
                memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.max.y - 1),
                            Pt(handle_rect.max.x - 1, handle_rect.max.y - 1), 0x999999FF, 1);
                memdraw_line(screen, Pt(handle_rect.max.x - 1, handle_rect.min.y),
                            Pt(handle_rect.max.x - 1, handle_rect.max.y - 1), 0x999999FF, 1);
            }

            /* Draw label text to the right of the switch */
            if (w->prop_text != NULL && w->prop_text[0] != '\0') {
                text_pos.x = switch_rect.max.x + 8;  /* 8px spacing between switch and text */
                text_pos.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - 16) / 2;
                font = memdraw_get_default_font();
                if (font != NULL) {
                    memdraw_text_font(screen, text_pos, w->prop_text, font, DBlack);
                } else {
                    memdraw_text(screen, text_pos, w->prop_text, DBlack);
                }
            }

            /* Handle click to toggle */
            if (render_is_widget_clicked(w)) {
                int new_val = is_on ? 0 : 1;
                free(w->prop_value);
                w->prop_value = (char *)malloc(2);
                if (w->prop_value != NULL) {
                    sprintf(w->prop_value, "%d", new_val);
                }
            }
        }
        break;

    case WIDGET_SLIDER:
    case WIDGET_RANGE_SLIDER:
    case WIDGET_DROPDOWN:
    case WIDGET_TEXT_INPUT:
    case WIDGET_PASSWORD_INPUT:
    case WIDGET_SEARCH_INPUT:
    case WIDGET_NUMBER_INPUT:
    case WIDGET_EMAIL_INPUT:
    case WIDGET_URL_INPUT:
    case WIDGET_TEL_INPUT:
    case WIDGET_IMAGE:
    case WIDGET_CANVAS:
    case WIDGET_DIVIDER:
    default:
        /* For now, these widgets are handled in the original render.c */
        /* They will be migrated in a follow-up */
        {
            extern void render_widget_legacy(KryonWidget *w, Memimage *screen);
            render_widget_legacy(w, screen);
        }
        break;
    }
}

/*
 * Render a window and all its widgets (recursive)
 */
void render_window(KryonWindow *win, Memimage *screen)
{
    Rectangle win_rect;
    int i;
    Memimage *target;

    if (win == NULL || screen == NULL) {
        return;
    }

    if (!win->visible) {
        return;
    }

    if (parse_rect(win->rect, &win_rect) < 0) {
        return;
    }

    if (win->vdev != NULL && win->vdev->draw_buffer != NULL) {
        target = win->vdev->draw_buffer;
    } else {
        target = screen;
    }

    for (i = 0; i < win->nwidgets; i++) {
        render_widget(win->widgets[i], target);
    }

    for (i = 0; i < win->nchildren; i++) {
        render_window(win->children[i], target);
    }

    if (win->vdev != NULL && win->vdev->draw_buffer != NULL) {
        if (target != screen) {
            Point sp, zero;
            sp.x = 0;
            sp.y = 0;
            zero.x = 0;
            zero.y = 0;
            memdraw(screen, win_rect, win->vdev->draw_buffer, sp, NULL, zero, SoverD);
        }
    }
}
