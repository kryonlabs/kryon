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
 * Global dirty flag
 */
extern int g_render_dirty;

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
 * Dropdown widget state
 */
typedef struct {
    char **options;       /* Array of option strings */
    int num_options;      /* Number of options */
    int selected_index;   /* Currently selected option (-1 if none) */
    int is_open;          /* Whether dropdown popup is visible */
    int hovered_item;     /* Which option item is being hovered (-1 if none) */
} DropdownState;

/*
 * Forward declarations for dropdown state management
 */
static DropdownState *dropdown_state_create(void);
static void dropdown_state_destroy(DropdownState *state);
int dropdown_parse_options(struct KryonWidget *w, const char *options_str);

/*
 * Forward declarations for click handling
 */
static int is_point_in_open_popup(KryonWindow *win, int x, int y);

/*
 * Public cleanup function for widget.c
 */
void dropdown_cleanup_widget_internal_data(void *data);

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

    /* Get mouse position */
    mp.x = w->parent_window->mouse_x;
    mp.y = w->parent_window->mouse_y;

    /* Block clicks if point is inside any open dropdown popup */
    if (is_point_in_open_popup(w->parent_window, mp.x, mp.y)) {
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
static int is_point_in_open_popup(KryonWindow *win, int x, int y)
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
        if (is_point_in_open_popup(win->children[i], x, y)) {
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

    /* During popup-only pass, skip non-dropdown widgets */
    if (g_render_ctx.render_only_popups && w->type != WIDGET_DROPDOWN) {
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

            /* Tk-style colors (format: 0xRRGGBBAA) - all RGB equal for neutral gray */
            bg_color = is_hovered ? 0xE0E0E0FF : 0xFFFFFFFF;  /* Light gray on hover, white otherwise */
            highlight_color = 0xFFFFFFFF;  /* White highlight */
            shadow_color = 0x808080FF;    /* Dark gray shadow (Tk standard) */

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

    case WIDGET_DROPDOWN:
        {
            DropdownState *state = (DropdownState *)w->internal_data;
            int is_popup_pass = (g_render_ctx.render_only_popups != 0);
            int has_open_popup = (state != NULL && state->is_open);
            int is_hovered = 0;
            Point mp;
            Point text_pos;
            Subfont *font;
            Rectangle popup_rect;
            uint32_t bg_color, border_color, text_color;
            int button_height;
            int arrow_size = 8;
            int item_height = 24;
            int i;
            int has_state = (state != NULL);

            /* Check hover state */
            if (w->parent_window != NULL) {
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
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
                    uint32_t highlight = 0xFFFFFFFF;  /* White highlight */
                    uint32_t shadow = 0x808080FF;    /* Dark gray shadow */
                    int thickness = 2;

                    /* When open, invert bevel to look "sunken" */
                    if (has_state && state->is_open) {
                        uint32_t temp = highlight;
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
                    Point arrow_pts[3];

                    arrow_pts[0].x = arrow_x;
                    arrow_pts[0].y = arrow_y;
                    arrow_pts[1].x = arrow_x + arrow_size;
                    arrow_pts[1].y = arrow_y;
                    arrow_pts[2].x = arrow_x + arrow_size / 2;
                    arrow_pts[2].y = arrow_y + arrow_size;

                    memdraw_poly(screen, arrow_pts, 3, 0x404040FF, 1);  /* Dark gray arrow (R=G=B=64) */
                }

                /* Handle click on button */
                if (is_widget_clicked(w)) {
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
                    uint32_t item_bg = 0xFFFFFFFF;  /* White */
                    Point item_text_pos;

                    item_rect.min.x = popup_rect.min.x + 1;
                    item_rect.min.y = popup_rect.min.y + i * item_height + 1;
                    item_rect.max.x = popup_rect.max.x - 1;
                    item_rect.max.y = item_rect.min.y + item_height - 1;

                    /* Check if mouse is over this item */
                    if (w->parent_window != NULL) {
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
                if (w->parent_window != NULL) {
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
    {
        /* Tk-style progress bar with proper trough and bar bevels */
        float progress;
        Rectangle fill_rect;
        unsigned long trough_color, trough_highlight, trough_shadow;
        unsigned long bar_color, bar_highlight, bar_shadow;
        unsigned long border_color;

        /* Parse progress value (0-100) */
        progress = 0.0f;
        if (w->prop_value != NULL) {
            progress = (float)atof(w->prop_value);
        }
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 100.0f) progress = 100.0f;

        /* Trough colors */
        trough_color = 0xE0E0E0FF;      /* Light gray trough (R=G=B=224, A=255) */
        trough_highlight = 0xFFFFFFFF;  /* White highlight */
        trough_shadow = 0x808080FF;     /* Dark gray shadow */
        border_color = 0x000000FF;      /* Black border */

        /* Bar colors - Tk-style blue */
        bar_color = 0x00008BFF;         /* Dark blue */
        bar_highlight = lighten_color(bar_color);
        bar_shadow = darken_color(bar_color);

        /* Draw trough background */
        memfillcolor_rect(screen, widget_rect, trough_color);

        /* Trough beveled edges - highlight (top/left) */
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                     Pt(widget_rect.max.x - 1, widget_rect.min.y), trough_highlight, 2);
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                     Pt(widget_rect.min.x, widget_rect.max.y - 1), trough_highlight, 2);

        /* Trough beveled edges - shadow (bottom/right) */
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                     Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), trough_shadow, 2);
        memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                     Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), trough_shadow, 2);

        /* Trough border */
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                     Pt(widget_rect.max.x - 1, widget_rect.min.y), border_color, 1);
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.min.y),
                     Pt(widget_rect.min.x, widget_rect.max.y - 1), border_color, 1);
        memdraw_line(screen, Pt(widget_rect.min.x, widget_rect.max.y - 1),
                     Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), border_color, 1);
        memdraw_line(screen, Pt(widget_rect.max.x - 1, widget_rect.min.y),
                     Pt(widget_rect.max.x - 1, widget_rect.max.y - 1), border_color, 1);

        /* Calculate fill rectangle (inset 3px for border + bevel) */
        if (progress > 0.0f) {
            fill_rect.min.x = widget_rect.min.x + 3;
            fill_rect.min.y = widget_rect.min.y + 3;
            fill_rect.max.x = widget_rect.min.x + 3 + (int)((progress / 100.0f) * (Dx(widget_rect) - 6));
            fill_rect.max.y = widget_rect.max.y - 3;

            /* Don't draw if fill area is too small */
            if (fill_rect.max.x > fill_rect.min.x) {
                /* Draw progress bar fill */
                memfillcolor_rect(screen, fill_rect, bar_color);

                /* Bar beveled edges - highlight (top/left) */
                memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.min.y),
                             Pt(fill_rect.max.x - 1, fill_rect.min.y), bar_highlight, 1);
                memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.min.y),
                             Pt(fill_rect.min.x, fill_rect.max.y - 1), bar_highlight, 1);

                /* Bar beveled edges - shadow (bottom/right) */
                memdraw_line(screen, Pt(fill_rect.min.x, fill_rect.max.y - 1),
                             Pt(fill_rect.max.x - 1, fill_rect.max.y - 1), bar_shadow, 1);
                memdraw_line(screen, Pt(fill_rect.max.x - 1, fill_rect.min.y),
                             Pt(fill_rect.max.x - 1, fill_rect.max.y - 1), bar_shadow, 1);
            }
        }
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

/*
 * Mark screen as dirty (needs refresh)
 */
void mark_dirty(KryonWindow *win)
{
    (void)win;  /* Unused for now */

    /* Mark draw device as dirty */
    devdraw_mark_dirty();
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
 * Cleans up dropdown internal data
 */
void dropdown_cleanup_widget_internal_data(void *data)
{
    DropdownState *state = (DropdownState *)data;
    dropdown_state_destroy(state);
}
