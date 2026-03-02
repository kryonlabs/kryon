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
    mp.x = w->parent_window ? w->parent_window->mouse_x : 0;
    mp.y = w->parent_window ? w->parent_window->mouse_y : 0;

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
                mouse_pos.x = w->parent_window ? w->parent_window->mouse_x : 0;
                mouse_pos.y = w->parent_window ? w->parent_window->mouse_y : 0;
                state->is_hovering = ptinrect(mouse_pos, handle_area);
                state->is_track_hovering = ptinrect(mouse_pos, track_rect);
            }

            if (w->parent_window != NULL && w->prop_enabled) {
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

            mouse_pos.x = w->parent_window ? w->parent_window->mouse_x : 0;
            mouse_pos.y = w->parent_window ? w->parent_window->mouse_y : 0;
            state->is_min_hovering = ptinrect(mouse_pos, min_handle_rect);
            state->is_max_hovering = ptinrect(mouse_pos, max_handle_rect);
            state->is_track_hovering = ptinrect(mouse_pos, track_rect);
        }

        /* Handle mouse interaction */
        if (w->parent_window != NULL && w->prop_enabled) {
            /* Click on min handle - start dragging */
            if (w->parent_window->last_mouse_buttons == 0 &&
                w->parent_window->mouse_buttons != 0) {
                if (state->is_min_hovering) {
                    state->is_dragging_min = 1;
                    float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                    if (new_val > state->max_value - min_gap) {
                        new_val = state->max_value - min_gap;
                    }
                    state->min_value = new_val;
                    util_update_range_slider_value(w);
                } else if (state->is_max_hovering) {
                    state->is_dragging_max = 1;
                    float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                    if (new_val < state->min_value + min_gap) {
                        new_val = state->min_value + min_gap;
                    }
                    state->max_value = new_val;
                    util_update_range_slider_value(w);
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
                float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val > state->max_value - min_gap) {
                    new_val = state->max_value - min_gap;
                }
                state->min_value = new_val;
                util_update_range_slider_value(w);
            }
            /* Dragging max handle */
            if (state->is_dragging_max && w->parent_window->mouse_buttons != 0) {
                float new_val = util_calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val < state->min_value + min_gap) {
                    new_val = state->min_value + min_gap;
                }
                state->max_value = new_val;
                util_update_range_slider_value(w);
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
    case WIDGET_PROGRESS_BAR:
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
            } else if (w->type == WIDGET_SWITCH) {
                placeholder_color = (w->prop_value != NULL && atoi(w->prop_value) != 0)
                                   ? 0xFF00FF00 : 0xFF888888;
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
