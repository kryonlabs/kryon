/*
 * Kryon Rendering Subsystem
 * C89/C90 compliant
 *
 * Public interface for all rendering modules
 */

#ifndef KRYON_RENDER_RENDER_H
#define KRYON_RENDER_RENDER_H

#include "graphics.h"
#include "widget.h"
#include "window.h"

/*
 * Screen management (screen.c)
 */

/*
 * Set global screen for rendering
 */
void render_set_screen(Memimage *screen);

/*
 * Mark screen as dirty (needs refresh)
 */
void render_mark_dirty(KryonWindow *win);

/*
 * Render all windows
 */
void render_all(void);

/*
 * Internal: Get rendering context flag (for widgets.c)
 */
int *render_get_context(void);

/*
 * Legacy widget rendering (defined in render.c)
 * Handles complex widgets that haven't been fully migrated yet
 */
void render_widget_legacy(KryonWidget *w, Memimage *screen);

/*
 * Widget rendering (widgets.c, primitives.c)
 */

/*
 * Render a single widget to screen
 */
void render_widget(KryonWidget *w, Memimage *screen);

/*
 * Render a window and all its widgets (recursive)
 */
void render_window(KryonWindow *win, Memimage *screen);

/*
 * Drawing primitives (primitives.c)
 */

/*
 * Draw Tk-style square/circular handle for slider
 * Draws a filled square with bevel effect
 */
void render_draw_square_handle(Memimage *screen, Point center, int radius,
                               unsigned long color, unsigned long border,
                               unsigned long highlight, unsigned long shadow,
                               int is_dragging);

/*
 * Draw Tk-style two-square handle for horizontal sliders
 * Two squares positioned horizontally (side by side) with 3D bevel effect
 */
void render_draw_two_square_handle(Memimage *screen, Point center,
                                   unsigned long color, unsigned long border,
                                   unsigned long highlight, unsigned long shadow,
                                   int is_dragging);

/*
 * Event processing (events.c)
 */

/*
 * Check if widget was clicked (mouse released over widget)
 */
int render_is_widget_clicked(KryonWidget *w);

/*
 * Check if point (x,y) is inside any open dropdown popup
 * Returns 1 if point should be blocked (inside popup), 0 otherwise
 */
int render_is_point_in_open_popup(KryonWindow *win, int x, int y);

/*
 * Update slider value and trigger re-render
 */
void render_update_slider_value(struct KryonWidget *w, float new_value);

/*
 * State management (for widget.c integration)
 */

/*
 * Public cleanup function for dropdown widgets
 */
void dropdown_cleanup_widget_internal_data(void *data);

/*
 * Public cleanup function for slider widgets
 */
void slider_cleanup_widget_internal_data(void *data);

/*
 * Parse comma-separated options string for dropdown widgets
 * Format: "option1,option2,option3"
 */
int dropdown_parse_options(struct KryonWidget *w, const char *options_str);

#endif /* KRYON_RENDER_RENDER_H */
