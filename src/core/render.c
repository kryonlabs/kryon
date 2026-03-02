/*
 * Kryon Graphics Engine - Widget Rendering (Legacy)
 * C89/C90 compliant
 *
 * This file now contains:
 * - Legacy widget rendering for complex widgets (sliders, dropdowns, etc.)
 * - Widget state management (to be migrated later)
 *
 * Simple widget rendering has been moved to:
 * - src/core/render/primitives.c - Drawing primitives
 * - src/core/render/widgets.c - Button, label, checkbox, radio button
 * - src/core/render/events.c - Event handling
 * - src/core/render/screen.c - Screen management
 * - src/core/util/color.c - Color utilities
 * - src/core/util/geom.c - Geometry utilities
 */

#include "graphics.h"
#include "window.h"
#include "widget.h"
#include "render/render.h"
#include "util.h"
#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * External function for marking dirty state
 */
extern void devdraw_mark_dirty(void);

/*
 * strdup prototype for C89 compatibility (not in C89 standard)
 */
extern char *strdup(const char *s);

/*
 * Global dirty flag
 */
extern int g_render_dirty;

/*
 * Widget state structures (legacy - to be migrated)
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
 * Forward declarations for state management
 */
static DropdownState *dropdown_state_create(void);
static void dropdown_state_destroy(DropdownState *state);
static SliderState *slider_state_create(void);
static void slider_state_destroy(SliderState *state);
static RangeSliderState *range_slider_state_create(void);
static void range_slider_state_destroy(RangeSliderState *state);
void mark_dirty(KryonWindow *win);  /* Public for use in other files */

/*
 * ========== Legacy Widget Rendering ==========
 * These functions handle complex widgets that haven't been migrated yet
 */

/*
 * Legacy widget rendering for sliders, dropdowns, and progress bars
 */
void render_widget_legacy(KryonWidget *w, Memimage *screen)
{
    Rectangle widget_rect;
    unsigned long color;
    int *render_only_popups;
    int is_popup_pass;
    Point mp;

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

    /* Parse widget rectangle */
    if (parse_rect(w->prop_rect, &widget_rect) < 0) {
        return;
    }

    /* Get mouse position */
    if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
        mp.x = w->parent_window->mouse_x;
        mp.y = w->parent_window->mouse_y;
    } else {
        mp.x = 0;
        mp.y = 0;
    }

    switch (w->type) {
    case WIDGET_TEXT_INPUT:
    case WIDGET_PASSWORD_INPUT:
    case WIDGET_SEARCH_INPUT:
    case WIDGET_NUMBER_INPUT:
    case WIDGET_EMAIL_INPUT:
    case WIDGET_URL_INPUT:
    case WIDGET_TEL_INPUT:
        /* Text input rendering - white background */
        color = 0xFFFFFFFF;
        memfillcolor_rect(screen, widget_rect, color);

        /* Draw Tk-style beveled border (thinner than buttons) */
        {
            unsigned long border_color = DGray;
            unsigned long highlight = util_lighten_color(border_color);
            unsigned long shadow = util_darken_color(border_color);
            int thickness = 1;

            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                         Pt(widget_rect.max.x - 1, widget_rect.min.y), highlight, thickness);
            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                         Pt(widget_rect.min.x, widget_rect.max.y - 1), highlight, thickness);
            memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                         Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
            memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                         Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
        }
        break;

    case WIDGET_SLIDER:
        {
            SliderState *state;
            float value;
            Rectangle track_rect, fill_rect;
            Point center;
            unsigned long trough_color, trough_highlight, trough_shadow;
            unsigned long handle_color, handle_highlight, handle_shadow;
            unsigned long border_color, fill_color;

            state = (SliderState *)w->internal_data;
            if (state == NULL) {
                state = slider_state_create();
                if (state == NULL) break;
                w->internal_data = state;
                if (w->prop_value != NULL) {
                    state->value = (float)atof(w->prop_value);
                    if (state->value < 0.0f) state->value = 0.0f;
                    if (state->value > 100.0f) state->value = 100.0f;
                }
            }

            value = state->value;

            trough_color = 0xC0C0C0FF;
            trough_highlight = 0xE0E0E0FF;
            trough_shadow = 0xA0A0A0FF;
            border_color = 0x000000FF;
            fill_color = 0x00008BFF;

            track_rect.min.x = widget_rect.min.x;
            track_rect.min.y = widget_rect.min.y + Dy(widget_rect) / 2 - 6;
            track_rect.max.x = widget_rect.max.x;
            track_rect.max.y = track_rect.min.y + 12;

            memfillcolor_rect(screen, track_rect, trough_color);

            memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                         Pt(track_rect.max.x - 1, track_rect.min.y), trough_highlight, 2);
            memdraw_line(screen, Pt(track_rect.min.x, track_rect.max.y - 1),
                         Pt(track_rect.max.x - 1, track_rect.max.y - 1), trough_shadow, 2);

            memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                         Pt(track_rect.max.x - 1, track_rect.min.y), border_color, 1);
            memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                         Pt(track_rect.min.x, track_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(track_rect.min.x, track_rect.max.y - 1),
                         Pt(track_rect.max.x - 1, track_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(track_rect.max.x - 1, track_rect.min.y),
                         Pt(track_rect.max.x - 1, track_rect.max.y - 1), border_color, 1);

            if (value > 0.0f) {
                fill_rect.min.x = track_rect.min.x + 3;
                fill_rect.min.y = track_rect.min.y + 3;
                fill_rect.max.x = track_rect.min.x + 3 + (int)((value / 100.0f) * (Dx(track_rect) - 6));
                fill_rect.max.y = track_rect.max.y - 3;

                if (fill_rect.max.x > fill_rect.min.x) {
                    memfillcolor_rect(screen, fill_rect, fill_color);
                }
            }

            center = util_calculate_handle_position(track_rect, value, 10);

            {
                Rectangle handle_area = util_get_two_square_handle_rect(center);
                Point mouse_pos;
                if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                    mouse_pos.x = w->parent_window->mouse_x;
                    mouse_pos.y = w->parent_window->mouse_y;
                } else {
                    mouse_pos.x = 0;
                    mouse_pos.y = 0;
                }
                state->is_hovering = ptinrect(mouse_pos, handle_area);
                state->is_track_hovering = ptinrect(mouse_pos, track_rect);
            }

            if (w->parent_window != NULL && window_is_valid(w->parent_window) && w->prop_enabled) {
                if (w->parent_window->last_mouse_buttons == 0 &&
                    w->parent_window->mouse_buttons != 0) {
                    if (state->is_hovering || state->is_track_hovering) {
                        state->is_dragging = 1;
                        float new_val = util_calculate_value_from_position(mp.x, track_rect);
                        render_update_slider_value(w, new_val);
                        state->value = (float)atof(w->prop_value);
                    }
                }
                if (w->parent_window->last_mouse_buttons != 0 &&
                    w->parent_window->mouse_buttons == 0) {
                    state->is_dragging = 0;
                }
                if (state->is_dragging && w->parent_window->mouse_buttons != 0) {
                    float new_val = util_calculate_value_from_position(mp.x, track_rect);
                    render_update_slider_value(w, new_val);
                    state->value = (float)atof(w->prop_value);
                }
            }

            if (state->is_dragging) {
                handle_color = 0xD0D0D0FF;
            } else if (state->is_hovering) {
                handle_color = 0xE8E8E8FF;
            } else {
                handle_color = 0xE0E0E0FF;
            }
            handle_highlight = util_lighten_color(handle_color);
            handle_shadow = util_darken_color(handle_color);

            render_draw_two_square_handle(screen, center, handle_color,
                                          border_color, handle_highlight, handle_shadow,
                                          state->is_dragging);
        }
        break;

    case WIDGET_RANGE_SLIDER:
    {
        /* Tk-style range slider with dual handles */
        RangeSliderState *state;
        float min_val, max_val;
        Rectangle track_rect, fill_rect;
        Point min_center, max_center, mouse_pos;
        unsigned long trough_color, trough_highlight, trough_shadow;
        unsigned long handle_color, handle_highlight, handle_shadow;
        unsigned long border_color, fill_color;
        const float min_gap = 5.0f;  /* Minimum 5% gap between handles */

        /* Get or create range slider state */
        state = (RangeSliderState *)w->internal_data;
        if (state == NULL) {
            state = range_slider_state_create();
            if (state == NULL) break;
            w->internal_data = state;
            /* Initialize values from prop_value (format: "min,max") */
            if (w->prop_value != NULL && strchr(w->prop_value, ',') != NULL) {
                sscanf(w->prop_value, "%f,%f", &state->min_value, &state->max_value);
                if (state->min_value < 0.0f) state->min_value = 0.0f;
                if (state->min_value > 100.0f) state->min_value = 100.0f;
                if (state->max_value < 0.0f) state->max_value = 0.0f;
                if (state->max_value > 100.0f) state->max_value = 100.0f;
                /* Ensure min < max with minimum gap */
                if (state->max_value - state->min_value < min_gap) {
                    state->max_value = state->min_value + min_gap;
                    if (state->max_value > 100.0f) {
                        state->max_value = 100.0f;
                        state->min_value = 100.0f - min_gap;
                    }
                }
            }
        }

        min_val = state->min_value;
        max_val = state->max_value;

        /* Color constants - Tk-style */
        trough_color = 0xC0C0C0FF;
        trough_highlight = 0xE0E0E0FF;
        trough_shadow = 0xA0A0A0FF;
        border_color = 0x000000FF;
        fill_color = 0x00008BFF;

        /* Calculate track rectangle (12px height, centered) */
        track_rect.min.x = widget_rect.min.x;
        track_rect.min.y = widget_rect.min.y + Dy(widget_rect) / 2 - 6;
        track_rect.max.x = widget_rect.max.x;
        track_rect.max.y = track_rect.min.y + 12;

        /* Draw simple trough background */
        memfillcolor_rect(screen, track_rect, trough_color);

        /* Trough beveled edges */
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                     Pt(track_rect.max.x - 1, track_rect.min.y), trough_highlight, 2);
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.max.y - 1),
                     Pt(track_rect.max.x - 1, track_rect.max.y - 1), trough_shadow, 2);

        /* Trough border */
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                     Pt(track_rect.max.x - 1, track_rect.min.y), border_color, 1);
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                     Pt(track_rect.min.x, track_rect.max.y - 1), border_color, 1);
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.max.y - 1),
                     Pt(track_rect.max.x - 1, track_rect.max.y - 1), border_color, 1);
        memdraw_line(screen, Pt(track_rect.max.x - 1, track_rect.min.y),
                     Pt(track_rect.max.x - 1, track_rect.max.y - 1), border_color, 1);

        /* Calculate and draw blue fill between min and max handles */
        fill_rect.min.x = track_rect.min.x + 2 + (int)((min_val / 100.0f) * (Dx(track_rect) - 4));
        fill_rect.min.y = track_rect.min.y + 2;
        fill_rect.max.x = track_rect.min.x + 2 + (int)((max_val / 100.0f) * (Dx(track_rect) - 4));
        fill_rect.max.y = track_rect.max.y - 2;

        if (fill_rect.max.x > fill_rect.min.x) {
            memfillcolor_rect(screen, fill_rect, fill_color);
        }

        /* Calculate handle positions */
        min_center.x = track_rect.min.x + 2 + (int)((min_val / 100.0f) * (Dx(track_rect) - 4));
        min_center.y = track_rect.min.y + Dy(track_rect) / 2;
        max_center.x = track_rect.min.x + 2 + (int)((max_val / 100.0f) * (Dx(track_rect) - 4));
        max_center.y = track_rect.min.y + Dy(track_rect) / 2;

        /* Check mouse hover state */
        {
            Rectangle min_handle_rect = util_get_two_square_handle_rect(min_center);
            Rectangle max_handle_rect = util_get_two_square_handle_rect(max_center);

            if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                mouse_pos.x = w->parent_window->mouse_x;
                mouse_pos.y = w->parent_window->mouse_y;
            } else {
                mouse_pos.x = 0;
                mouse_pos.y = 0;
            }
            state->is_min_hovering = ptinrect(mouse_pos, min_handle_rect);
            state->is_max_hovering = ptinrect(mouse_pos, max_handle_rect);
            state->is_track_hovering = ptinrect(mouse_pos, track_rect);
        }

        /* Handle mouse interaction */
        if (w->parent_window != NULL && window_is_valid(w->parent_window) && w->prop_enabled) {
            /* Click on min handle - start dragging */
            if (w->parent_window->last_mouse_buttons == 0 &&
                w->parent_window->mouse_buttons != 0) {
                if (state->is_min_hovering) {
                    char buf[64];
                    state->is_dragging_min = 1;
                    float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                    if (new_val > state->max_value - min_gap) {
                        new_val = state->max_value - min_gap;
                    }
                    state->min_value = new_val;
                    sprintf(buf, "%.1f,%.1f", state->min_value, state->max_value);
                    free(w->prop_value);
                    w->prop_value = strdup(buf);
                    mark_dirty(w->parent_window);
                } else if (state->is_max_hovering) {
                    char buf[64];
                    state->is_dragging_max = 1;
                    float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                    if (new_val < state->min_value + min_gap) {
                        new_val = state->min_value + min_gap;
                    }
                    state->max_value = new_val;
                    sprintf(buf, "%.1f,%.1f", state->min_value, state->max_value);
                    free(w->prop_value);
                    w->prop_value = strdup(buf);
                    mark_dirty(w->parent_window);
                }
            }
            /* Release mouse - stop dragging */
            if (w->parent_window->last_mouse_buttons != 0 &&
                w->parent_window->mouse_buttons == 0) {
                state->is_dragging_min = 0;
                state->is_dragging_max = 0;
            }
            /* Dragging min handle */
            if (state->is_dragging_min && w->parent_window->mouse_buttons != 0) {
                char buf[64];
                float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val > state->max_value - min_gap) {
                    new_val = state->max_value - min_gap;
                }
                state->min_value = new_val;
                sprintf(buf, "%.1f,%.1f", state->min_value, state->max_value);
                free(w->prop_value);
                w->prop_value = strdup(buf);
                mark_dirty(w->parent_window);
            }
            /* Dragging max handle */
            if (state->is_dragging_max && w->parent_window->mouse_buttons != 0) {
                char buf[64];
                float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val < state->min_value + min_gap) {
                    new_val = state->min_value + min_gap;
                }
                state->max_value = new_val;
                sprintf(buf, "%.1f,%.1f", state->min_value, state->max_value);
                free(w->prop_value);
                w->prop_value = strdup(buf);
                mark_dirty(w->parent_window);
            }
        }

        /* Draw min handle */
        if (state->is_dragging_min) {
            handle_color = 0xD0D0D0FF;
        } else if (state->is_min_hovering) {
            handle_color = 0xE8E8E8FF;
        } else {
            handle_color = 0xE0E0E0FF;
        }
        handle_highlight = util_lighten_color(handle_color);
        handle_shadow = util_darken_color(handle_color);

        render_draw_two_square_handle(screen, min_center, handle_color,
                                     border_color, handle_highlight, handle_shadow,
                                     state->is_dragging_min);

        /* Draw max handle */
        if (state->is_dragging_max) {
            handle_color = 0xD0D0D0FF;
        } else if (state->is_max_hovering) {
            handle_color = 0xE8E8E8FF;
        } else {
            handle_color = 0xE0E0E0FF;
        }
        handle_highlight = util_lighten_color(handle_color);
        handle_shadow = util_darken_color(handle_color);

        render_draw_two_square_handle(screen, max_center, handle_color,
                                     border_color, handle_highlight, handle_shadow,
                                     state->is_dragging_max);
    }
    break;

    case WIDGET_DROPDOWN:
        {
            DropdownState *state;
            int is_popup_pass;
            int has_open_popup;
            int is_hovered;
            Point mp;
            Point text_pos;
            Subfont *font;
            Rectangle popup_rect;
            unsigned long bg_color, border_color, text_color;
            int button_height;
            int arrow_size = 8;
            int item_height = 24;
            int i;
            int has_state;
            Point arrow_pts[3];

            state = (DropdownState *)w->internal_data;
            has_state = (state != NULL);
            is_popup_pass = *render_only_popups;
            has_open_popup = (has_state && state->is_open);
            is_hovered = 0;

            /* Check hover state */
            if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
            } else {
                mp.x = 0;
                mp.y = 0;
                is_hovered = 0;
            }

            /* Pass 1: Draw button only (skip popup) */
            if (!is_popup_pass) {
                /* Determine colors based on state (R=G=B for neutral grays) */
                if (has_state && state->is_open) {
                    bg_color = 0xD0D0D0FF;  /* Medium gray when open (R=G=B=208) */
                } else if (is_hovered) {
                    bg_color = 0xE0E0E0FF;  /* Light gray on hover (R=G=B=224) */
                } else {
                    bg_color = 0xFFFFFFFF;  /* White when closed */
                }
                border_color = 0x808080FF;  /* Dark gray border (R=G=B=128) */
                text_color = 0x000000FF;    /* Black text */

                /* Draw button background */
                memfillcolor_rect(screen, widget_rect, bg_color);

                /* Draw beveled edges */
                {
                    unsigned long highlight = 0xFFFFFFFF;  /* White highlight */
                    unsigned long shadow = 0x808080FF;    /* Dark gray shadow */
                    int thickness = 2;

                    /* When open, invert bevel to look "sunken" */
                    if (has_state && state->is_open) {
                        unsigned long temp = highlight;
                        highlight = shadow;
                        shadow = temp;
                    }

                    /* Top edge (highlight) */
                    memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                                 Pt(widget_rect.max.x - 1, widget_rect.min.y), highlight, thickness);
                    /* Left edge (highlight) */
                    memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                                 Pt(widget_rect.min.x, widget_rect.max.y - 1), highlight, thickness);
                    /* Bottom edge (shadow) */
                    memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                                 Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
                    /* Right edge (shadow) */
                    memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                                 Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), shadow, thickness);
                }

                /* Draw border */
                memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                            Pt(widget_rect.max.x - 1, widget_rect.min.y), border_color, 1);
                memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                            Pt(widget_rect.min.x, widget_rect.max.y - 1), border_color, 1);
                memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                            Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), border_color, 1);
                memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                            Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), border_color, 1);

                /* Calculate text or selected option */
                button_height = widget_rect.max.y - widget_rect.min.y;
                text_pos.x = widget_rect.min.x + 4;
                text_pos.y = widget_rect.min.y + (button_height - 16) / 2;

                if (has_state && state->selected_index >= 0 &&
                    state->selected_index < state->num_options && state->options != NULL) {
                    /* Draw selected option text */
                    font = memdraw_get_default_font();
                    if (font != NULL) {
                        memdraw_text_font(screen, text_pos, state->options[state->selected_index], font, text_color);
                    } else {
                        memdraw_text(screen, text_pos, state->options[state->selected_index], text_color);
                    }
                } else if (w->prop_text != NULL && w->prop_text[0] != '\0') {
                    /* Draw placeholder text */
                    font = memdraw_get_default_font();
                    if (font != NULL) {
                        memdraw_text_font(screen, text_pos, w->prop_text, font, text_color);
                    } else {
                        memdraw_text(screen, text_pos, w->prop_text, text_color);
                    }
                }

                /* Draw down arrow on right side */
                {
                    int arrow_x = widget_rect.max.x - arrow_size - 4;
                    int arrow_y = widget_rect.min.y + (button_height - arrow_size) / 2;

                    arrow_pts[0].x = arrow_x;
                    arrow_pts[0].y = arrow_y;
                    arrow_pts[1].x = arrow_x + arrow_size;
                    arrow_pts[1].y = arrow_y;
                    arrow_pts[2].x = arrow_x + arrow_size / 2;
                    arrow_pts[2].y = arrow_y + arrow_size;

                    memdraw_poly(screen, arrow_pts, 3, 0x404040FF, 1);  /* Dark gray arrow (R=G=B=64) */
                }

                /* Handle click on button */
                if (render_is_widget_clicked(w)) {
                    if (!has_state) {
                        state = dropdown_state_create();
                        w->internal_data = state;
                        has_state = (state != NULL);
                    }
                    if (has_state) {
                        state->is_open = !state->is_open;
                        g_render_dirty = 1;
                    }
                }
            }

            /* Pass 2: Draw popup only (if open) */
            if (is_popup_pass && has_open_popup && state->num_options > 0 && state->options != NULL) {
                border_color = 0x808080FF;  /* Dark gray border (R=G=B=128) */
                text_color = 0x000000FF;    /* Black text */

                /* Calculate popup rectangle below button */
                popup_rect.min.x = widget_rect.min.x;
                popup_rect.min.y = widget_rect.max.y + 1;
                popup_rect.max.x = widget_rect.max.x;
                popup_rect.max.y = popup_rect.min.y + state->num_options * item_height;

                /* Draw popup background */
                memfillcolor_rect(screen, popup_rect, 0xFFFFFFFF);  /* White */

                /* Draw popup border */
                memdraw_line(screen, Pt(popup_rect.min.x, popup_rect.min.y),
                            Pt(popup_rect.max.x - 1, popup_rect.min.y), border_color, 1);
                memdraw_line(screen, Pt(popup_rect.min.x, popup_rect.min.y),
                            Pt(popup_rect.min.x, popup_rect.max.y - 1), border_color, 1);
                memdraw_line(screen, Pt(popup_rect.min.x, popup_rect.max.y - 1),
                            Pt(popup_rect.max.x - 1, popup_rect.max.y - 1), border_color, 1);
                memdraw_line(screen, Pt(popup_rect.max.x - 1, popup_rect.min.y),
                            Pt(popup_rect.max.x - 1, popup_rect.max.y - 1), border_color, 1);

                /* Reset hovered item */
                state->hovered_item = -1;

                /* Draw menu items */
                for (i = 0; i < state->num_options; i++) {
                    Rectangle item_rect;
                    unsigned long item_bg = 0xFFFFFFFF;  /* White */
                    Point item_text_pos;

                    item_rect.min.x = popup_rect.min.x + 1;
                    item_rect.min.y = popup_rect.min.y + i * item_height + 1;
                    item_rect.max.x = popup_rect.max.x - 1;
                    item_rect.max.y = item_rect.min.y + item_height - 1;

                    /* Check if mouse is over this item */
                    if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                        if (ptinrect(mp, item_rect)) {
                            state->hovered_item = i;
                            item_bg = 0xE0E0E0FF;  /* Light gray hover (R=G=B=224) */
                        }
                    }

                    /* Highlight selected item */
                    if (i == state->selected_index) {
                        item_bg = 0xD0D0D0FF;  /* Medium gray (R=G=B=208) */
                    }

                    /* Draw item background */
                    memfillcolor_rect(screen, item_rect, item_bg);

                    /* Draw item text */
                    item_text_pos.x = item_rect.min.x + 4;
                    item_text_pos.y = item_rect.min.y + (item_height - 16) / 2;
                    font = memdraw_get_default_font();
                    if (font != NULL) {
                        memdraw_text_font(screen, item_text_pos, state->options[i], font, text_color);
                    } else {
                        memdraw_text(screen, item_text_pos, state->options[i], text_color);
                    }
                }

                /* Handle click on menu item */
                if (w->parent_window != NULL && window_is_valid(w->parent_window)) {
                    if (w->parent_window->last_mouse_buttons != 0 &&
                        w->parent_window->mouse_buttons == 0) {
                        /* Mouse released - check if clicking on popup */
                        if (state->hovered_item >= 0 && state->hovered_item < state->num_options) {
                            /* Select the item */
                            state->selected_index = state->hovered_item;

                            /* Update prop_value */
                            free(w->prop_value);
                            w->prop_value = (char *)malloc(16);
                            if (w->prop_value != NULL) {
                                sprintf(w->prop_value, "%d", state->selected_index);
                            }

                            /* Close popup */
                            state->is_open = 0;
                            g_render_dirty = 1;
                        } else if (!ptinrect(mp, widget_rect) && !ptinrect(mp, popup_rect)) {
                            /* Clicked outside - close popup */
                            state->is_open = 0;
                            g_render_dirty = 1;
                        }
                    }
                }
            }
        }
        break;

    case WIDGET_PROGRESS_BAR:
        {
            float value = 0.0f;
            float max_value = 100.0f;
            Rectangle trough_rect, bar_rect;
            unsigned long trough_color, trough_highlight, trough_shadow;
            unsigned long bar_color, bar_highlight, bar_shadow;
            unsigned long border_color;
            int bar_height;
            Point text_pos;
            char text_buf[32];
            Subfont *font;

            /* Parse value (0-100) */
            if (w->prop_value != NULL) {
                value = (float)atof(w->prop_value);
                if (value < 0.0f) value = 0.0f;
                if (value > max_value) value = max_value;
            }

            /* Calculate bar height (20px like sliders) */
            bar_height = 20;
            trough_rect.min.x = widget_rect.min.x;
            trough_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - bar_height) / 2;
            trough_rect.max.x = widget_rect.max.x;
            trough_rect.max.y = trough_rect.min.y + bar_height;

            /* Tk-style colors */
            trough_color = 0xC0C0C0FF;       /* Neutral gray trough */
            trough_highlight = 0xE0E0E0FF;   /* Light highlight */
            trough_shadow = 0xA0A0A0FF;      /* Dark shadow */
            border_color = 0x000000FF;       /* Black border */
            bar_color = 0x00008BFF;          /* Dark blue fill (Tk default) */
            bar_highlight = 0x4169E1FF;      /* Lighter blue highlight */
            bar_shadow = 0x000060FF;         /* Darker blue shadow */

            /* Draw trough background */
            memfillcolor_rect(screen, trough_rect, trough_color);

            /* Trough beveled edges */
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.min.y),
                         Pt(trough_rect.max.x - 1, trough_rect.min.y), trough_highlight, 2);
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.min.y),
                         Pt(trough_rect.min.x, trough_rect.max.y - 1), trough_highlight, 2);
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.max.y - 1),
                         Pt(trough_rect.max.x - 1, trough_rect.max.y - 1), trough_shadow, 2);
            memdraw_line(screen, Pt(trough_rect.max.x - 1, trough_rect.min.y),
                         Pt(trough_rect.max.x - 1, trough_rect.max.y - 1), trough_shadow, 2);

            /* Trough border */
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.min.y),
                        Pt(trough_rect.max.x - 1, trough_rect.min.y), border_color, 1);
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.min.y),
                        Pt(trough_rect.min.x, trough_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(trough_rect.min.x, trough_rect.max.y - 1),
                        Pt(trough_rect.max.x - 1, trough_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(trough_rect.max.x - 1, trough_rect.min.y),
                        Pt(trough_rect.max.x - 1, trough_rect.max.y - 1), border_color, 1);

            /* Calculate and draw filled bar */
            if (value > 0.0f) {
                int fill_width = (int)((value / max_value) * (Dx(trough_rect) - 6));

                /* Clip fill width to trough width */
                if (fill_width < 0) fill_width = 0;
                if (fill_width > Dx(trough_rect) - 6) fill_width = Dx(trough_rect) - 6;

                if (fill_width > 0) {
                    bar_rect.min.x = trough_rect.min.x + 3;
                    bar_rect.min.y = trough_rect.min.y + 3;
                    bar_rect.max.x = bar_rect.min.x + fill_width;
                    bar_rect.max.y = trough_rect.max.y - 3;

                    memfillcolor_rect(screen, bar_rect, bar_color);

                    /* Bar beveled edges (subtle) */
                    memdraw_line(screen, Pt(bar_rect.min.x, bar_rect.min.y),
                                 Pt(bar_rect.max.x - 1, bar_rect.min.y), bar_highlight, 1);
                    memdraw_line(screen, Pt(bar_rect.min.x, bar_rect.min.y),
                                 Pt(bar_rect.min.x, bar_rect.max.y - 1), bar_highlight, 1);
                    memdraw_line(screen, Pt(bar_rect.min.x, bar_rect.max.y - 1),
                                 Pt(bar_rect.max.x - 1, bar_rect.max.y - 1), bar_shadow, 1);
                    memdraw_line(screen, Pt(bar_rect.max.x - 1, bar_rect.min.y),
                                 Pt(bar_rect.max.x - 1, bar_rect.max.y - 1), bar_shadow, 1);
                }
            }

            /* Draw percentage text if prop_text is set or if there's room */
            if (w->prop_text != NULL && w->prop_text[0] != '\0') {
                /* Use custom text */
                text_pos.x = widget_rect.min.x + 4;
                text_pos.y = widget_rect.min.y + 2;
                font = memdraw_get_default_font();
                if (font != NULL) {
                    memdraw_text_font(screen, text_pos, w->prop_text, font, DBlack);
                } else {
                    memdraw_text(screen, text_pos, w->prop_text, DBlack);
                }
            } else if (Dx(widget_rect) > 100) {
                /* Show percentage if wide enough */
                int percentage = (int)((value / max_value) * 100.0f);
                sprintf(text_buf, "%d%%", percentage);
                text_pos.x = widget_rect.min.x + 4;
                text_pos.y = widget_rect.min.y + 2;
                font = memdraw_get_default_font();
                if (font != NULL) {
                    memdraw_text_font(screen, text_pos, text_buf, font, DBlack);
                } else {
                    memdraw_text(screen, text_pos, text_buf, DBlack);
                }
            }
        }
        break;

    case WIDGET_SWITCH:
    case WIDGET_IMAGE:
    case WIDGET_CANVAS:
    case WIDGET_DIVIDER:
        /* For now, use simple placeholder rendering */
        /* These will be fully migrated in a follow-up */
        {
            unsigned long placeholder_color = 0xFFD0D0D0;
            if (w->type == WIDGET_DIVIDER) {
                placeholder_color = 0xFF000000;
            }
            memfillcolor_rect(screen, widget_rect, placeholder_color);
        }
        break;

    default:
        /* Unknown widget type */
        color = 0xFFD0D0D0;
        memfillcolor_rect(screen, widget_rect, color);
        break;
    }
}

/*
 * Backwards compatibility wrappers for old function names
 */
void mark_dirty(KryonWindow *win)
{
    render_mark_dirty(win);
}

/*
 * ========== Dropdown Widget State Management ==========
 */

/*
 * Create dropdown state
 */
static DropdownState *dropdown_state_create(void)
{
    DropdownState *state = (DropdownState *)calloc(1, sizeof(DropdownState));
    if (state == NULL) return NULL;
    state->options = NULL;
    state->num_options = 0;
    state->selected_index = -1;
    state->is_open = 0;
    state->hovered_item = -1;
    return state;
}

/*
 * Destroy dropdown state and free options array
 */
static void dropdown_state_destroy(DropdownState *state)
{
    int i;
    if (state == NULL) return;
    if (state->options != NULL) {
        for (i = 0; i < state->num_options; i++) {
            free(state->options[i]);
        }
        free(state->options);
    }
    free(state);
}

/*
 * Parse comma-separated options string
 * Format: "option1,option2,option3"
 */
int dropdown_parse_options(struct KryonWidget *w, const char *options_str)
{
    DropdownState *state;
    char *str_copy, *token;
    int count = 0, i = 0;
    size_t len;

    if (w == NULL || options_str == NULL) return -1;

    state = (DropdownState *)w->internal_data;
    if (state == NULL) {
        state = dropdown_state_create();
        if (state == NULL) return -1;
        w->internal_data = state;
    }

    /* Free existing options */
    for (i = 0; i < state->num_options; i++) {
        free(state->options[i]);
    }
    free(state->options);
    state->options = NULL;
    state->num_options = 0;

    /* Count options */
    len = strlen(options_str);
    str_copy = (char *)malloc(len + 1);
    if (str_copy == NULL) return -1;
    strcpy(str_copy, options_str);

    token = strtok(str_copy, ",");
    while (token != NULL) {
        count++;
        token = strtok(NULL, ",");
    }
    free(str_copy);

    if (count == 0) return 0;

    /* Allocate and parse options */
    state->options = (char **)malloc(count * sizeof(char *));
    if (state->options == NULL) {
        dropdown_state_destroy(state);
        w->internal_data = NULL;
        return -1;
    }

    /* Copy string again for parsing */
    str_copy = (char *)malloc(len + 1);
    if (str_copy == NULL) {
        dropdown_state_destroy(state);
        w->internal_data = NULL;
        return -1;
    }
    strcpy(str_copy, options_str);

    token = strtok(str_copy, ",");
    while (token != NULL && i < count) {
        len = strlen(token);
        state->options[i] = (char *)malloc(len + 1);
        if (state->options[i] == NULL) {
            free(str_copy);
            dropdown_state_destroy(state);
            w->internal_data = NULL;
            return -1;
        }
        strcpy(state->options[i], token);
        i++;
        token = strtok(NULL, ",");
    }
    free(str_copy);

    state->num_options = count;

    /* Set selected index from prop_value */
    if (w->prop_value != NULL) {
        state->selected_index = atoi(w->prop_value);
        if (state->selected_index < 0 || state->selected_index >= state->num_options) {
            state->selected_index = -1;
        }
    }

    return 0;
}

/*
 * Public cleanup function for widget.c
 */
void dropdown_cleanup_widget_internal_data(void *data)
{
    DropdownState *state = (DropdownState *)data;
    dropdown_state_destroy(state);
}

/*
 * ========== Slider Widget State Management ==========
 */

/*
 * Create slider state
 */
static SliderState *slider_state_create(void)
{
    SliderState *state = (SliderState *)calloc(1, sizeof(SliderState));
    if (state == NULL) return NULL;
    state->value = 0.0f;
    state->is_dragging = 0;
    state->is_hovering = 0;
    state->is_track_hovering = 0;
    return state;
}

/*
 * Destroy slider state
 */
static void slider_state_destroy(SliderState *state)
{
    if (state == NULL) return;
    free(state);
}

/*
 * Create range slider state
 */
static RangeSliderState *range_slider_state_create(void)
{
    RangeSliderState *state = (RangeSliderState *)calloc(1, sizeof(RangeSliderState));
    if (state == NULL) return NULL;
    state->min_value = 25.0f;
    state->max_value = 75.0f;
    state->is_dragging_min = 0;
    state->is_dragging_max = 0;
    state->is_min_hovering = 0;
    state->is_max_hovering = 0;
    state->is_track_hovering = 0;
    return state;
}

/*
 * Destroy range slider state
 */
static void range_slider_state_destroy(RangeSliderState *state)
{
    if (state == NULL) return;
    free(state);
}

/*
 * Public cleanup function for slider widgets
 */
void slider_cleanup_widget_internal_data(void *data)
{
    if (data == NULL) return;
    free(data);
}
