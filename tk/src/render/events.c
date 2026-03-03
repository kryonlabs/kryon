/*
 * Kryon Rendering Event Processing
 * C89/C90 compliant
 *
 * Event handling and hit testing for widgets
 */

#include "render.h"
#include "util.h"
#include "graphics.h"
#include "widget.h"
#include "window.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * External functions
 */
extern char *strdup(const char *s);
extern int g_render_dirty;

/*
 * Forward declarations for dropdown state management
 * (defined in render.c, will be refactored later)
 */
typedef struct {
    char **options;       /* Array of option strings */
    int num_options;      /* Number of options */
    int selected_index;   /* Currently selected option (-1 if none) */
    int is_open;          /* Whether dropdown popup is visible */
    int hovered_item;     /* Which option item is being hovered (-1 if none) */
} DropdownState;

/*
 * Check if widget was clicked (mouse released over widget)
 */
int render_is_widget_clicked(KryonWidget *w)
{
    Point mp;
    Rectangle widget_rect;

    if (w->parent_window == NULL || w->prop_enabled == 0) {
        return 0;
    }

    /* Check if parent window is still valid (prevents use-after-free on WM reload) */
    if (!window_is_valid(w->parent_window)) {
        return 0;
    }

    /* Get mouse position */
    mp.x = w->parent_window->mouse_x;
    mp.y = w->parent_window->mouse_y;

    /* Block clicks if point is inside any open dropdown popup */
    if (render_is_point_in_open_popup(w->parent_window, mp.x, mp.y)) {
        /* Special case: allow clicks on the dropdown that owns the popup */
        DropdownState *state = (DropdownState *)w->internal_data;
        if (w->type != WIDGET_DROPDOWN || state == NULL || !state->is_open) {
            return 0;
        }
    }

    if (parse_rect(w->prop_rect, &widget_rect) < 0) {
        return 0;
    }

    /* Detect click: button was pressed, now released */
    if (w->parent_window->last_mouse_buttons != 0 &&
        w->parent_window->mouse_buttons == 0) {
        mp.x = w->parent_window->mouse_x;
        mp.y = w->parent_window->mouse_y;
        if (ptinrect(mp, widget_rect)) {
            return 1;
        }
    }

    return 0;
}

/*
 * Check if point (x,y) is inside any open dropdown popup
 * Returns 1 if point should be blocked (inside popup), 0 otherwise
 */
int render_is_point_in_open_popup(KryonWindow *win, int x, int y)
{
    int i;
    Point pt;
    Rectangle popup_rect;
    int item_height = 24;

    if (win == NULL) return 0;

    pt.x = x;
    pt.y = y;

    /* Check all widgets in window */
    for (i = 0; i < win->nwidgets; i++) {
        KryonWidget *w = win->widgets[i];
        DropdownState *state;

        if (w->type != WIDGET_DROPDOWN) continue;

        state = (DropdownState *)w->internal_data;
        if (state == NULL || !state->is_open) continue;
        if (state->num_options <= 0 || state->options == NULL) continue;

        /* Parse widget rectangle to get popup position */
        if (parse_rect(w->prop_rect, &popup_rect) < 0) continue;

        /* Calculate popup rect (extends below button) */
        popup_rect.min.y = popup_rect.max.y + 1;  /* Below button */
        popup_rect.max.y = popup_rect.min.y + state->num_options * item_height;

        /* Check if point is inside this popup */
        if (ptinrect(pt, popup_rect)) {
            return 1;  /* Point is inside an open popup - block click */
        }
    }

    /* Recursively check child windows */
    for (i = 0; i < win->nchildren; i++) {
        if (render_is_point_in_open_popup(win->children[i], x, y)) {
            return 1;
        }
    }

    return 0;
}

/*
 * Update slider value and trigger re-render
 */
void render_update_slider_value(KryonWidget *w, float new_value)
{
    char buf[32];

    if (w == NULL) return;

    if (new_value < 0.0f) new_value = 0.0f;
    if (new_value > 100.0f) new_value = 100.0f;

    sprintf(buf, "%.1f", new_value);
    free(w->prop_value);
    w->prop_value = strdup(buf);

    g_render_dirty = 1;
}
