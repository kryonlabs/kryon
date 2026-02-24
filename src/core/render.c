/*
 * Kryon Graphics Engine - Widget Rendering
 * C89/C90 compliant
 *
 * Bridges widget properties to pixel rendering
 */

#include "graphics.h"
#include "window.h"
#include "widget.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * External function for marking dirty state
 */
extern void devdraw_mark_dirty(void);

/*
 * Global screen reference
 */
static Memimage *g_screen = NULL;

/*
 * Set global screen for rendering
 */
void render_set_screen(Memimage *screen)
{
    g_screen = screen;
}

/*
 * Render a single widget to screen
 */
void render_widget(KryonWidget *w, Memimage *screen)
{
    Rectangle widget_rect;
    unsigned long color;
    static int render_count = 0;

    if (w == NULL || screen == NULL) {
        return;
    }

    /* Check visibility */
    if (!w->prop_visible) {
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
        /* Button rendering */
        if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
            /* Green when active: 0x00FF00FF in 9front format */
            color = DGreen;
        } else {
            /* Red when inactive: 0xFF0000FF in 9front format */
            color = DRed;
        }
        memfillcolor_rect(screen, widget_rect, color);

        /* TODO: Render text label */
        break;

    case WIDGET_LABEL:
    case WIDGET_HEADING:
    case WIDGET_PARAGRAPH:
        /* Label rendering - yellow background */
        color = DYellow;
        memfillcolor_rect(screen, widget_rect, color);

        /* Render text label */
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;

            /* Position text with padding */
            text_pos.x = widget_rect.min.x + 8;
            text_pos.y = widget_rect.min.y + 12;

            /* White text in 9front format */
            text_color = DWhite;

            /* Check if a 9front font is available */
            font = memdraw_get_default_font();
            if (font != NULL) {
                memdraw_text_font(screen, text_pos, w->prop_text, font, text_color);
            } else {
                memdraw_text(screen, text_pos, w->prop_text, text_color);
            }
        }
        break;

    case WIDGET_CHECKBOX:
    case WIDGET_SWITCH:
        /* Checkbox/toggle rendering */
        if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
            color = 0xFF00FF00;  /* Green when checked */
        } else {
            color = 0xFF888888;  /* Gray when unchecked */
        }
        memfillcolor_rect(screen, widget_rect, color);
        break;

    case WIDGET_CONTAINER:
    case WIDGET_BOX:
    case WIDGET_FRAME:
    case WIDGET_CARD:
        /* Container rendering - light gray background */
        color = 0xFFF0F0F0;
        memfillcolor_rect(screen, widget_rect, color);
        break;

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

        /* Draw border */
        /* TODO: Draw border rectangle */
        break;

    case WIDGET_SLIDER:
    case WIDGET_RANGE_SLIDER:
        /* Slider rendering */
        color = 0xFFE0E0E0;  /* Track color */
        memfillcolor_rect(screen, widget_rect, color);
        /* TODO: Render slider handle */
        break;

    case WIDGET_PROGRESS_BAR:
        /* Progress bar rendering */
        color = 0xFFE0E0E0;  /* Background */
        memfillcolor_rect(screen, widget_rect, color);
        /* TODO: Render progress fill */
        break;

    case WIDGET_IMAGE:
    case WIDGET_CANVAS:
        /* Placeholder for image/canvas */
        color = 0xFF808080;  /* Gray placeholder */
        memfillcolor_rect(screen, widget_rect, color);
        break;

    case WIDGET_DIVIDER:
        /* Divider line */
        color = 0xFF000000;  /* Black line */
        memfillcolor_rect(screen, widget_rect, color);
        break;

    default:
        /* Default widget rendering - light gray */
        color = 0xFFD0D0D0;
        memfillcolor_rect(screen, widget_rect, color);
        break;
    }
}

/*
 * Render a window and all its widgets
 */
void render_window(KryonWindow *win, Memimage *screen)
{
    Rectangle win_rect;
    int i;

    if (win == NULL || screen == NULL) {
        return;
    }

    /* Parse window rectangle */
    if (parse_rect(win->rect, &win_rect) < 0) {
        return;
    }

    /* Clear window background - use transparent instead of white */
    /* Don't clear - let the screen background show through */
    /* memfillcolor_rect(screen, win_rect, 0xFFFFFFFF); */

    /* Render all widgets */
    for (i = 0; i < win->nwidgets; i++) {
        render_widget(win->widgets[i], screen);
    }
}

/*
 * Render all windows
 */
void render_all(void)
{
    KryonWindow *win;
    uint32_t i;
    static int render_count = 0;
    static int first_done = 0;

    if (g_screen == NULL) {
        return;
    }

    render_count++;

    /* Clear screen to dark blue */
    memfillcolor(g_screen, DBlue);

    /* Render all windows */
    for (i = 1; ; i++) {
        win = window_get(i);
        if (win == NULL) {
            break;
        }
        if (win->visible) {
            render_window(win, g_screen);
        }
    }

    /* Notify all drawterm clients that screen updated */
    devdraw_mark_dirty();
}

/*
 * Mark screen as dirty (needs refresh)
 */
void mark_dirty(KryonWindow *win)
{
    (void)win;  /* Unused for now */

    /* Mark draw device as dirty */
    devdraw_mark_dirty();
}
