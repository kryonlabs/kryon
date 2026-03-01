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
 * Parse hex color string "#RRGGBB" to uint32_t 0xRRGGBBAA
 * Returns color value, or 0 on error
 */
static unsigned long parse_color(const char *hex_color)
{
    unsigned long r, g, b;

    if (hex_color == NULL || hex_color[0] != '#') {
        return 0;
    }

    /* Skip '#' */
    hex_color++;

    /* Parse RGB */
    if (sscanf(hex_color, "%2lx%2lx%2lx", &r, &g, &b) != 3) {
        return 0;
    }

    /* Return in 0xRRGGBBAA format (AA = FF for opaque) */
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

/*
 * Lighten a color by ~30% for highlight edges (Tk-style bevel)
 */
static unsigned long lighten_color(unsigned long color)
{
    int r, g, b, a;

    r = ((color >> 24) & 0xFF);
    g = ((color >> 16) & 0xFF);
    b = ((color >> 8) & 0xFF);
    a = color & 0xFF;

    /* Lighten by blending with white (7/8 original + 1/8 white) */
    r = (r * 7 + 255) / 8;
    g = (g * 7 + 255) / 8;
    b = (b * 7 + 255) / 8;

    return ((unsigned long)r << 24) | ((unsigned long)g << 16) |
           ((unsigned long)b << 8) | a;
}

/*
 * Darken a color by ~30% for shadow edges (Tk-style bevel)
 */
static unsigned long darken_color(unsigned long color)
{
    int r, g, b, a;

    r = ((color >> 24) & 0xFF);
    g = ((color >> 16) & 0xFF);
    b = ((color >> 8) & 0xFF);
    a = color & 0xFF;

    /* Darken to 2/3 of original value */
    r = r * 2 / 3;
    g = g * 2 / 3;
    b = b * 2 / 3;

    return ((unsigned long)r << 24) | ((unsigned long)g << 16) |
           ((unsigned long)b << 8) | a;
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
        /* Button rendering - use custom color if specified */
        if (w->prop_color != NULL && w->prop_color[0] != '\0') {
            color = parse_color(w->prop_color);
            if (color == 0) {
                /* Fallback to default colors on parse error */
                if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                    color = DGreen;
                } else {
                    color = DRed;
                }
            }
        } else {
            /* Default colors when no custom color specified */
            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                /* Green when active: 0x00FF00FF in 9front format */
                color = DGreen;
            } else {
                /* Red when inactive: 0xFF0000FF in 9front format */
                color = DRed;
            }
        }
        memfillcolor_rect(screen, widget_rect, color);

        /* Draw Tk-style beveled edges */
        {
            unsigned long highlight = lighten_color(color);
            unsigned long shadow = darken_color(color);
            int thickness = 2;

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

        /* Render text label - centered */
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;
            int text_width;
            int button_width, button_height;

            /* White text */
            text_color = DWhite;

            /* Calculate text width (assuming 8px per character for font) */
            text_width = (int)strlen(w->prop_text) * 8;

            /* Calculate button dimensions */
            button_width = widget_rect.max.x - widget_rect.min.x;
            button_height = widget_rect.max.y - widget_rect.min.y;

            /* Center text: button_center - text_half_width */
            text_pos.x = widget_rect.min.x + (button_width - text_width) / 2;
            /* Vertically center 16px tall font */
            text_pos.y = widget_rect.min.y + (button_height - 16) / 2 + 12; /* +12 for baseline */

            /* Check if a 9front font is available */
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
        /* Label rendering - use custom color if specified, else yellow */
        if (w->prop_color != NULL && w->prop_color[0] != '\0') {
            color = parse_color(w->prop_color);
            if (color == 0) {
                color = DYellow;  /* Fallback to yellow on parse error */
            }
        } else {
            color = DYellow;  /* Default yellow */
        }
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

    /* Check visibility */
    if (!win->visible) {
        return;
    }

    /* Parse window rectangle */
    if (parse_rect(win->rect, &win_rect) < 0) {
        return;
    }

    /* Determine rendering target */
    /* If window has virtual framebuffer (nested WM), render to it */
    /* Otherwise render directly to screen */
    if (win->vdev != NULL && win->vdev->draw_buffer != NULL) {
        target = win->vdev->draw_buffer;
    } else {
        target = screen;
    }

    /* Render all widgets */
    for (i = 0; i < win->nwidgets; i++) {
        render_widget(win->widgets[i], target);
    }

    /* Recursively render child windows */
    for (i = 0; i < win->nchildren; i++) {
        render_window(win->children[i], target);
    }

    /* Compose virtual framebuffer to screen (if used) */
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

    /* Clear screen to dark blue */
    memfillcolor(g_screen, DBlue);

    /* Render only top-level windows */
    /* Nested windows will be rendered recursively by their parents */
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
