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
 * Check if widget was clicked (mouse released over widget)
 */
static int is_widget_clicked(KryonWidget *w)
{
    Point mp;
    Rectangle widget_rect;

    if (w->parent_window == NULL || w->prop_enabled == 0) {
        return 0;
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
        /* Button background: use prop_color if set, else neutral Tk-style gray */
        if (w->prop_color != NULL && w->prop_color[0] != '\0') {
            color = parse_color(w->prop_color);
            if (color == 0) {
                color = 0xD9D9D9FF;  /* Fallback to gray on parse error */
            }
        } else {
            color = 0xC0C0C0FF;  /* Button gray - darker than window bg for contrast */
        }
        /* Hover highlight: lighten button when cursor is over it */
        if (w->parent_window != NULL) {
            Point mp;
            mp.x = w->parent_window->mouse_x;
            mp.y = w->parent_window->mouse_y;
            if (ptinrect(mp, widget_rect)) {
                color = lighten_color(color);
            }
        }

        /* Check for pressed state - button appears "sunken" when clicked */
        int is_pressed = 0;
        if (w->parent_window != NULL && w->parent_window->mouse_buttons != 0) {
            Point mp;
            mp.x = w->parent_window->mouse_x;
            mp.y = w->parent_window->mouse_y;
            if (ptinrect(mp, widget_rect)) {
                is_pressed = 1;
                /* Darken color to simulate being pressed down */
                color = darken_color(color);
            }
        }

        memfillcolor_rect(screen, widget_rect, color);

        /* Draw Tk-style beveled edges */
        {
            unsigned long highlight = lighten_color(color);
            unsigned long shadow = darken_color(color);
            int thickness = 2;

            /* When pressed, invert the bevel to look "sunken" */
            if (is_pressed) {
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

        /* Render text label - centered */
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;
            int text_width;
            int button_width, button_height;

            /* Choose text color: black on light backgrounds, white on dark */
            {
                unsigned int cr = (color >> 24) & 0xFF;
                unsigned int cg = (color >> 16) & 0xFF;
                unsigned int cb = (color >> 8) & 0xFF;
                text_color = ((cr + cg + cb) / 3 > 128) ? DBlack : DWhite;
            }

            /* Calculate text width (8px per character for built-in font) */
            text_width = (int)strlen(w->prop_text) * 8;

            /* Calculate button dimensions */
            button_width = widget_rect.max.x - widget_rect.min.x;
            button_height = widget_rect.max.y - widget_rect.min.y;

            /* Horizontally center; vertically center 16px-tall font */
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
        /* No background fill - label text is rendered directly onto the window background.
         * prop_color sets the text color; default is black. */
        if (w->prop_text != NULL && w->prop_text[0] != '\0') {
            Point text_pos;
            unsigned long text_color;
            Subfont *font;
            int widget_height;

            /* Text color from prop_color, default black */
            if (w->prop_color != NULL && w->prop_color[0] != '\0') {
                text_color = parse_color(w->prop_color);
                if (text_color == 0) {
                    text_color = DBlack;
                }
            } else {
                text_color = DBlack;
            }

            /* Small left padding; vertically center 16px font */
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

            /* Checkbox indicator */
            check_rect.min.x = widget_rect.min.x;
            check_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - check_size) / 2;
            check_rect.max.x = check_rect.min.x + check_size;
            check_rect.max.y = check_rect.min.y + check_size;

            /* Check hover state */
            if (w->parent_window != NULL) {
                Point mp;
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
            }

            /* Tk-style colors (format: 0xRRGGBBAA) */
            bg_color = is_hovered ? 0xFFE0E0E0 : 0xFFFFFFFF;  /* Light gray on hover, white otherwise */
            highlight_color = 0xFFFFFFFF;  /* White highlight */
            shadow_color = 0xFF888888;    /* Dark gray shadow (Tk standard) */

            /* Background with hover effect */
            memfillcolor_rect(screen, check_rect, bg_color);

            /* Beveled 3D edges - highlight (top/left) */
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                         Pt(check_rect.max.x - 1, check_rect.min.y), highlight_color, 2);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                         Pt(check_rect.min.x, check_rect.max.y - 1), highlight_color, 2);

            /* Beveled 3D edges - shadow (bottom/right) */
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.max.y - 1),
                         Pt(check_rect.max.x - 1, check_rect.max.y - 1), shadow_color, 2);
            memdraw_line(screen, Pt(check_rect.max.x - 1, check_rect.min.y),
                         Pt(check_rect.max.x - 1, check_rect.max.y - 1), shadow_color, 2);

            /* Draw dark border for better visibility (Tk style) */
            /* Use memdraw_line to draw rectangle border since memdraw_rect doesn't exist */
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                        Pt(check_rect.max.x - 1, check_rect.min.y), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.min.y),
                        Pt(check_rect.min.x, check_rect.max.y - 1), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.min.x, check_rect.max.y - 1),
                        Pt(check_rect.max.x - 1, check_rect.max.y - 1), DBlack, 1);
            memdraw_line(screen, Pt(check_rect.max.x - 1, check_rect.min.y),
                        Pt(check_rect.max.x - 1, check_rect.max.y - 1), DBlack, 1);

            /* Thick checkmark if checked - Tk style checkmark */
            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                int cx = check_rect.min.x;
                int cy = check_rect.min.y;
                /* Draw a prominent checkmark with proper coordinates for 16x16 box */
                /* Checkmark: from lower-left to middle to upper-right */
                Point check_start = Pt(cx + 4, cy + 8);   /* Lower-left */
                Point check_mid = Pt(cx + 7, cy + 11);    /* Middle */
                Point check_end = Pt(cx + 12, cy + 5);    /* Upper-right */

                /* Draw thick checkmark using multiple lines for bold appearance */
                memdraw_line(screen, check_start, check_mid, DBlack, 3);
                memdraw_line(screen, check_mid, check_end, DBlack, 3);
                /* Draw again slightly offset for extra thickness */
                memdraw_line(screen, Pt(check_start.x + 1, check_start.y),
                            Pt(check_mid.x + 1, check_mid.y), DBlack, 3);
                memdraw_line(screen, Pt(check_mid.x + 1, check_mid.y),
                            Pt(check_end.x, check_end.y - 1), DBlack, 3);
            }

            /* Label text */
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

            /* Handle click */
            if (is_widget_clicked(w)) {
                int new_val = (w->prop_value == NULL || atoi(w->prop_value) == 0) ? 1 : 0;
                free(w->prop_value);
                w->prop_value = (char *)malloc(2);
                if (w->prop_value != NULL) {
                    sprintf(w->prop_value, "%d", new_val);
                }
            }
        }
        break;

    case WIDGET_SWITCH:
        /* Switch/toggle rendering */
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

        /* Draw Tk-style beveled border (thinner than buttons) */
        {
            unsigned long border_color = DGray;
            unsigned long highlight = lighten_color(border_color);
            unsigned long shadow = darken_color(border_color);
            int thickness = 1;  /* Thinner than button borders */

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
        break;

    case WIDGET_SLIDER:
    case WIDGET_RANGE_SLIDER:
        /* Slider rendering */
        color = 0xFFE0E0E0;  /* Track color */
        memfillcolor_rect(screen, widget_rect, color);
        /* Render slider handle */
        {
            float value;
            int handle_radius = 8;
            Point center;
            unsigned long handle_color = DBlue;
            unsigned long highlight, shadow;

            /* Parse slider value (0-100) */
            value = 0.0f;
            if (w->prop_value != NULL) {
                value = (float)atof(w->prop_value);
            }
            if (value < 0.0f) value = 0.0f;
            if (value > 100.0f) value = 100.0f;

            /* Calculate handle center position */
            center.x = widget_rect.min.x + (int)((value / 100.0f) * Dx(widget_rect));
            center.y = widget_rect.min.y + Dy(widget_rect) / 2;

            /* Get highlight/shadow colors */
            highlight = lighten_color(handle_color);
            shadow = darken_color(handle_color);

            /* Draw filled circle for handle */
            memdraw_ellipse(screen, center, handle_radius, handle_radius, handle_color, 1);

            /* Add bevel effect - highlight on top half, shadow on bottom */
            memdraw_line(screen, Pt(center.x - handle_radius + 1, center.y - handle_radius + 2),
                         Pt(center.x + handle_radius - 1, center.y - handle_radius + 2), highlight, 1);
            memdraw_line(screen, Pt(center.x - handle_radius + 1, center.y + handle_radius - 2),
                         Pt(center.x + handle_radius - 1, center.y + handle_radius - 2), shadow, 1);
        }
        break;

    case WIDGET_PROGRESS_BAR:
        /* Progress bar rendering */
        color = 0xFFE0E0E0;  /* Background */
        memfillcolor_rect(screen, widget_rect, color);
        /* Render progress fill based on value */
        {
            float progress;
            Rectangle fill_rect;
            unsigned long progress_color = DGreen;
            unsigned long highlight, shadow;

            /* Parse progress value (0-100) */
            progress = 0.0f;
            if (w->prop_value != NULL) {
                progress = (float)atof(w->prop_value);
            }
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 100.0f) progress = 100.0f;

            /* Calculate fill rectangle with 1px padding */
            fill_rect.min.x = widget_rect.min.x + 1;
            fill_rect.min.y = widget_rect.min.y + 1;
            fill_rect.max.x = widget_rect.min.x + 1 + (int)((progress / 100.0f) * (Dx(widget_rect) - 2));
            fill_rect.max.y = widget_rect.max.y - 1;

            /* Get highlight/shadow colors */
            highlight = lighten_color(progress_color);
            shadow = darken_color(progress_color);

            /* Fill the progress area */
            memfillcolor_rect(screen, fill_rect, progress_color);

            /* Add bevel edges to progress fill */
            memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.min.y),
                         Pt(fill_rect.max.x - 1, fill_rect.min.y), highlight, 1);
            memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.min.y),
                         Pt(fill_rect.min.x, fill_rect.max.y - 1), highlight, 1);
            memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.max.y - 1),
                         Pt(fill_rect.max.x - 1, fill_rect.max.y - 1), shadow, 1);
            memdraw_line(screen, Pt(fill_rect.max.x - 1, fill_rect.min.y),
                         Pt(fill_rect.max.x - 1, fill_rect.max.y - 1), shadow, 1);
        }
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

    /* Clear screen to Tk-style window background gray */
    memfillcolor(g_screen, 0xD9D9D9FF);

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
