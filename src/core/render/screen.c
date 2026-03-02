/*
 * Kryon Rendering Screen Management
 * C89/C90 compliant
 *
 * Screen buffer, dirty state, and multi-pass rendering coordination
 */

#include "render.h"
#include "graphics.h"
#include "window.h"
#include "widget.h"

/*
 * External function for marking dirty state
 */
extern void devdraw_mark_dirty(void);

/*
 * Global screen reference
 */
static Memimage *g_screen = NULL;

/*
 * Rendering context for multi-pass rendering
 */
static struct {
    int render_only_popups;  /* 1 = only render open dropdown popups */
} g_render_ctx = {0};

/*
 * Set global screen for rendering
 */
void render_set_screen(Memimage *screen)
{
    g_screen = screen;
}

/*
 * Mark screen as dirty (needs refresh)
 */
void render_mark_dirty(KryonWindow *win)
{
    (void)win;  /* Unused for now */

    /* Mark draw device as dirty */
    devdraw_mark_dirty();
}

/*
 * Helper: Get the rendering context
 * Returns pointer to render_only_popups flag
 */
int *render_get_context(void)
{
    return &g_render_ctx.render_only_popups;
}

/*
 * Render all windows
 */
void render_all(void)
{
    KryonWindow *win;
    uint32_t i;
    static int render_count = 0;

    if (g_screen == NULL) {
        return;
    }

    render_count++;

    /* Clear screen to Tk-style window background gray */
    memfillcolor(g_screen, 0xD9D9D9FF);

    /* Pass 1: Render all widgets (no dropdown popups) */
    g_render_ctx.render_only_popups = 0;
    for (i = 1; ; i++) {
        win = window_get(i);
        if (win == NULL) {
            break;
        }
        /* Only render top-level windows (parent == NULL) */
        if (win->visible && win->parent == NULL) {
            render_window(win, g_screen);
        }
    }

    /* Pass 2: Render all open dropdown popups on top */
    g_render_ctx.render_only_popups = 1;
    for (i = 1; ; i++) {
        win = window_get(i);
        if (win == NULL) {
            break;
        }
        /* Only render top-level windows (parent == NULL) */
        if (win->visible && win->parent == NULL) {
            render_window(win, g_screen);
        }
    }

    /* Reset flag */
    g_render_ctx.render_only_popups = 0;

    /* Notify all drawterm clients that screen updated */
    devdraw_mark_dirty();
}
