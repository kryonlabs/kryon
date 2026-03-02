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
 * strdup prototype for C89 compatibility (not in C89 standard)
 */
extern char *strdup(const char *s);

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
 * Slider widget state
 */
typedef struct {
    float value;           /* Current value (0-100) */
    int is_dragging;       /* Currently dragging handle */
    int is_hovering;       /* Mouse over handle */
    int is_track_hovering; /* Mouse over track area */
} SliderState;

/*
 * Range slider widget state
 */
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
 * Forward declarations for slider state management
 */
static SliderState *slider_state_create(void);
static void slider_state_destroy(SliderState *state);
static RangeSliderState *range_slider_state_create(void);
static void range_slider_state_destroy(RangeSliderState *state);

/*
 * Public cleanup function for widget.c
 */
void slider_cleanup_widget_internal_data(void *data);

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
 * Draw Tk-style circular handle for slider
 * Draws a filled circle using polygon approximation with bevel effect
 */
static void draw_square_handle(Memimage *screen, Point center, int radius,
                                unsigned long color, unsigned long border,
                                unsigned long highlight, unsigned long shadow,
                                int is_dragging)
{
    Rectangle handle_rect;
    int thickness = 2;  /* Tk default borderwidth is 2 */

    handle_rect.min.x = center.x - radius;
    handle_rect.min.y = center.y - radius;
    handle_rect.max.x = center.x + radius;
    handle_rect.max.y = center.y + radius;

    /* Fill handle */
    memfillcolor_rect(screen, handle_rect, color);

    /* 3D bevel effect - draw BEFORE border so border is on top */
    if (!is_dragging) {
        /* Normal bevel: highlight on top/left, shadow on bottom/right */
        /* Top edge (highlight) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                     Pt(handle_rect.max.x - 1, handle_rect.min.y), highlight, thickness);
        /* Left edge (highlight) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                     Pt(handle_rect.min.x, handle_rect.max.y - 1), highlight, thickness);
        /* Bottom edge (shadow) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.max.y - thickness),
                     Pt(handle_rect.max.x - 1, handle_rect.max.y - thickness), shadow, thickness);
        /* Right edge (shadow) */
        memdraw_line(screen, Pt(handle_rect.max.x - thickness, handle_rect.min.y),
                     Pt(handle_rect.max.x - thickness, handle_rect.max.y - 1), shadow, thickness);
    } else {
        /* Inverted bevel when pressed: shadow on top/left, highlight on bottom/right */
        /* Top edge (shadow) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                     Pt(handle_rect.max.x - 1, handle_rect.min.y), shadow, thickness);
        /* Left edge (shadow) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                     Pt(handle_rect.min.x, handle_rect.max.y - 1), shadow, thickness);
        /* Bottom edge (highlight) */
        memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.max.y - thickness),
                     Pt(handle_rect.max.x - 1, handle_rect.max.y - thickness), highlight, thickness);
        /* Right edge (highlight) */
        memdraw_line(screen, Pt(handle_rect.max.x - thickness, handle_rect.min.y),
                     Pt(handle_rect.max.x - thickness, handle_rect.max.y - 1), highlight, thickness);
    }

    /* Black border on the very edge */
    memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                 Pt(handle_rect.max.x - 1, handle_rect.min.y), border, 1);
    memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.min.y),
                 Pt(handle_rect.min.x, handle_rect.max.y - 1), border, 1);
    memdraw_line(screen, Pt(handle_rect.min.x, handle_rect.max.y - 1),
                 Pt(handle_rect.max.x - 1, handle_rect.max.y - 1), border, 1);
    memdraw_line(screen, Pt(handle_rect.max.x - 1, handle_rect.min.y),
                 Pt(handle_rect.max.x - 1, handle_rect.max.y - 1), border, 1);
}

/*
 * Draw Tk-style two-square handle for horizontal sliders
 * Two squares positioned horizontally (side by side) with 3D bevel effect
 */
static void draw_two_square_handle(Memimage *screen, Point center,
                                    unsigned long color, unsigned long border,
                                    unsigned long highlight, unsigned long shadow,
                                    int is_dragging)
{
    int square_size = 10;  /* Each square is 10x10 pixels */
    int square_gap = 2;    /* 2 pixel gap between squares */
    Rectangle sq1, sq2;
    int total_width = square_size * 2 + square_gap;
    int left_x = center.x - total_width / 2;

    /* Left square */
    sq1.min.x = left_x;
    sq1.min.y = center.y - square_size / 2;
    sq1.max.x = sq1.min.x + square_size;
    sq1.max.y = sq1.min.y + square_size;

    /* Right square */
    sq2.min.x = left_x + square_size + square_gap;
    sq2.min.y = center.y - square_size / 2;
    sq2.max.x = sq2.min.x + square_size;
    sq2.max.y = sq2.min.y + square_size;

    /* Draw first square with 3D bevel */
    memfillcolor_rect(screen, sq1, color);
    /* Top edge highlight */
    memdraw_line(screen, Pt(sq1.min.x, sq1.min.y),
                Pt(sq1.max.x - 1, sq1.min.y), highlight, 2);
    /* Left edge highlight */
    memdraw_line(screen, Pt(sq1.min.x, sq1.min.y),
                Pt(sq1.min.x, sq1.max.y - 1), highlight, 2);
    /* Bottom edge shadow */
    memdraw_line(screen, Pt(sq1.min.x, sq1.max.y - 2),
                Pt(sq1.max.x - 1, sq1.max.y - 2), shadow, 2);
    /* Right edge shadow */
    memdraw_line(screen, Pt(sq1.max.x - 2, sq1.min.y),
                Pt(sq1.max.x - 2, sq1.max.y - 1), shadow, 2);
    /* Border */
    memdraw_line(screen, Pt(sq1.min.x, sq1.min.y),
                Pt(sq1.max.x - 1, sq1.min.y), border, 1);
    memdraw_line(screen, Pt(sq1.min.x, sq1.min.y),
                Pt(sq1.min.x, sq1.max.y - 1), border, 1);
    memdraw_line(screen, Pt(sq1.min.x, sq1.max.y - 1),
                Pt(sq1.max.x - 1, sq1.max.y - 1), border, 1);
    memdraw_line(screen, Pt(sq1.max.x - 1, sq1.min.y),
                Pt(sq1.max.x - 1, sq1.max.y - 1), border, 1);

    /* Draw second square with 3D bevel */
    memfillcolor_rect(screen, sq2, color);
    /* Top edge highlight */
    memdraw_line(screen, Pt(sq2.min.x, sq2.min.y),
                Pt(sq2.max.x - 1, sq2.min.y), highlight, 2);
    /* Left edge highlight */
    memdraw_line(screen, Pt(sq2.min.x, sq2.min.y),
                Pt(sq2.min.x, sq2.max.y - 1), highlight, 2);
    /* Bottom edge shadow */
    memdraw_line(screen, Pt(sq2.min.x, sq2.max.y - 2),
                Pt(sq2.max.x - 1, sq2.max.y - 2), shadow, 2);
    /* Right edge shadow */
    memdraw_line(screen, Pt(sq2.max.x - 2, sq2.min.y),
                Pt(sq2.max.x - 2, sq2.max.y - 1), shadow, 2);
    /* Border */
    memdraw_line(screen, Pt(sq2.min.x, sq2.min.y),
                Pt(sq2.max.x - 1, sq2.min.y), border, 1);
    memdraw_line(screen, Pt(sq2.min.x, sq2.min.y),
                Pt(sq2.min.x, sq2.max.y - 1), border, 1);
    memdraw_line(screen, Pt(sq2.min.x, sq2.max.y - 1),
                Pt(sq2.max.x - 1, sq2.max.y - 1), border, 1);
    memdraw_line(screen, Pt(sq2.max.x - 1, sq2.min.y),
                Pt(sq2.max.x - 1, sq2.max.y - 1), border, 1);
}

/*
 * Get the bounding rectangle for a two-square handle
 */
static Rectangle get_two_square_handle_rect(Point center)
{
    int square_size = 10;
    int square_gap = 2;
    int total_width = square_size * 2 + square_gap;
    Rectangle rect;
    int left_x = center.x - total_width / 2;

    rect.min.x = left_x;
    rect.min.y = center.y - square_size / 2;
    rect.max.x = left_x + total_width;
    rect.max.y = center.y + square_size / 2;

    return rect;
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
 * Calculate handle center position for slider
 */
static Point calculate_handle_position(Rectangle track_rect, float value, int handle_radius)
{
    Point center;
    int track_width;

    track_width = Dx(track_rect) - 6;  /* 3px inset on each side */
    center.x = track_rect.min.x + 3 + (int)((value / 100.0f) * track_width);
    center.y = track_rect.min.y + Dy(track_rect) / 2;

    return center;
}

/*
 * Check if point is inside handle (square region for detection)
 * Note: Handle is drawn as square centered at center point
 */
static int is_point_in_handle(Point p, Point center, int radius)
{
    Rectangle handle_rect;
    handle_rect.min.x = center.x - radius;
    handle_rect.min.y = center.y - radius;
    handle_rect.max.x = center.x + radius;
    handle_rect.max.y = center.y + radius;
    return ptinrect(p, handle_rect);
}

/*
 * Calculate slider value from mouse position
 */
static float calculate_value_from_position(int mouse_x, Rectangle track_rect)
{
    int track_start, track_width, offset;

    track_start = track_rect.min.x + 3;  /* 3px inset */
    track_width = Dx(track_rect) - 6;
    offset = mouse_x - track_start;

    if (offset < 0) offset = 0;
    if (offset > track_width) offset = track_width;

    return ((float)offset / (float)track_width) * 100.0f;
}

/*
 * Update slider value and trigger re-render
 */
static void update_slider_value(struct KryonWidget *w, float new_value)
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

/*
 * Get the group identifier for a radio button
 * Returns: group name if prop_group is set, otherwise parent container's ID
 */
static const char *get_radio_group_id(KryonWidget *w)
{
    /* Static buffer for default group name */
    static char default_group[64];

    /* If explicit group name, use it */
    if (w->prop_group != NULL && w->prop_group[0] != '\0') {
        return w->prop_group;
    }

    /* Otherwise, use parent container as group */
    if (w->parent != NULL && w->parent->name != NULL) {
        return w->parent->name;
    }

    /* Last resort: use parent window ID */
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

    /* Iterate through all widgets */
    for (i = 0; i < count; i++) {
        KryonWidget *other;

        other = widget_get_by_index(i);
        if (other == NULL) {
            continue;
        }

        /* Skip non-radio buttons and different groups */
        if (other->type != WIDGET_RADIO_BUTTON) {
            continue;
        }
        if (strcmp(get_radio_group_id(other), group_id) != 0) {
            continue;
        }

        /* Skip the clicked widget */
        if (other->id == clicked_widget->id) {
            continue;
        }

        /* Deselect this radio button */
        if (other->prop_value != NULL) {
            free(other->prop_value);
        }
        other->prop_value = strdup("0");
    }
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

    case WIDGET_RADIO_BUTTON:
        {
            /* Tk-style radio button with circular indicator */
            Rectangle radio_rect;      /* The circular indicator */
            Point text_pos;
            Subfont *font;
            int is_hovered = 0;
            int is_selected = 0;
            int radio_diameter = 14;   /* Tk standard: 12-14px */
            int dot_diameter = 6;      /* Selected dot: 6-8px */
            uint32_t bg_color, border_color, dot_color;

            /* Check selection state */
            if (w->prop_value != NULL && atoi(w->prop_value) != 0) {
                is_selected = 1;
            }

            /* Radio button indicator (circle) */
            radio_rect.min.x = widget_rect.min.x;
            radio_rect.min.y = widget_rect.min.y + (widget_rect.max.y - widget_rect.min.y - radio_diameter) / 2;
            radio_rect.max.x = radio_rect.min.x + radio_diameter;
            radio_rect.max.y = radio_rect.min.y + radio_diameter;

            /* Check hover state */
            if (w->parent_window != NULL) {
                Point mp;
                mp.x = w->parent_window->mouse_x;
                mp.y = w->parent_window->mouse_y;
                is_hovered = ptinrect(mp, widget_rect);
            }

            /* Tk-style colors (RRGGBBAA format) */
            bg_color = is_hovered ? 0xE0E0E0FF : 0xFFFFFFFF;  /* Light gray on hover, white otherwise */
            border_color = 0x000000FF;      /* Black border */
            dot_color = 0x000000FF;         /* Black dot */

            /* Draw circular background (filled rectangle for circle approximation) */
            memfillcolor_rect(screen, radio_rect, bg_color);

            /* Draw circular border - 4 sides to approximate circle */
            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.min.y),
                         Pt(radio_rect.max.x - 1, radio_rect.min.y), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.min.y),
                         Pt(radio_rect.min.x, radio_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.min.x, radio_rect.max.y - 1),
                         Pt(radio_rect.max.x - 1, radio_rect.max.y - 1), border_color, 1);
            memdraw_line(screen, Pt(radio_rect.max.x - 1, radio_rect.min.y),
                         Pt(radio_rect.max.x - 1, radio_rect.max.y - 1), border_color, 1);

            /* Draw selected dot (filled circle in center) */
            if (is_selected) {
                Rectangle dot_rect;
                int dot_offset = (radio_diameter - dot_diameter) / 2;

                dot_rect.min.x = radio_rect.min.x + dot_offset;
                dot_rect.min.y = radio_rect.min.y + dot_offset;
                dot_rect.max.x = dot_rect.min.x + dot_diameter;
                dot_rect.max.y = dot_rect.min.y + dot_diameter;

                memfillcolor_rect(screen, dot_rect, dot_color);
            }

            /* Label text (4px spacing from radio indicator) */
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

            /* Handle click - select this radio, deselect group members */
            if (is_widget_clicked(w)) {
                /* Only act if not already selected */
                if (!is_selected) {
                    /* Deselect all other radio buttons in the group */
                    deselect_radio_group(w);

                    /* Select this radio button */
                    free(w->prop_value);
                    w->prop_value = strdup("1");
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
    {
        /* Tk-style slider with track, fill, and interactive handle */
        SliderState *state;
        float value;
        Rectangle track_rect, fill_rect;
        Point center, mouse_pos;
        unsigned long trough_color, trough_highlight, trough_shadow;
        unsigned long handle_color, handle_highlight, handle_shadow;
        unsigned long border_color, fill_color;

        /* Get or create slider state */
        state = (SliderState *)w->internal_data;
        if (state == NULL) {
            state = slider_state_create();
            if (state == NULL) break;
            w->internal_data = state;
            /* Initialize value from prop_value */
            if (w->prop_value != NULL) {
                state->value = (float)atof(w->prop_value);
                if (state->value < 0.0f) state->value = 0.0f;
                if (state->value > 100.0f) state->value = 100.0f;
            }
        }

        value = state->value;

        /* Color constants - Tk-style */
        trough_color = 0xC0C0C0FF;      /* Classic Tk trough color */
        trough_highlight = 0xE0E0E0FF;  /* Lighter highlight */
        trough_shadow = 0xA0A0A0FF;     /* Darker shadow */
        border_color = 0x000000FF;      /* Black border */
        fill_color = 0x00008BFF;        /* Tk-style blue */

        /* Calculate track rectangle (simple 12px height, centered) */
        track_rect.min.x = widget_rect.min.x;
        track_rect.min.y = widget_rect.min.y + Dy(widget_rect) / 2 - 6;
        track_rect.max.x = widget_rect.max.x;
        track_rect.max.y = track_rect.min.y + 12;

        /* Draw trough background */
        memfillcolor_rect(screen, track_rect, trough_color);

        /* Trough beveled edges - highlight (top) */
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                     Pt(track_rect.max.x - 1, track_rect.min.y), trough_highlight, 2);
        /* Trough beveled edges - shadow (bottom) */
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

        /* Calculate and draw blue fill from left edge to handle position */
        if (value > 0.0f) {
            fill_rect.min.x = track_rect.min.x + 3;
            fill_rect.min.y = track_rect.min.y + 3;
            fill_rect.max.x = track_rect.min.x + 3 + (int)((value / 100.0f) * (Dx(track_rect) - 6));
            fill_rect.max.y = track_rect.max.y - 3;

            if (fill_rect.max.x > fill_rect.min.x) {
                memfillcolor_rect(screen, fill_rect, fill_color);
            }
        }

        /* Calculate handle center position */
        {
            int handle_radius = 10;
            center = calculate_handle_position(track_rect, value, handle_radius);
        }

        /* Check mouse hover state */
        {
            Rectangle handle_area = get_two_square_handle_rect(center);

            mouse_pos.x = w->parent_window ? w->parent_window->mouse_x : 0;
            mouse_pos.y = w->parent_window ? w->parent_window->mouse_y : 0;
            state->is_hovering = ptinrect(mouse_pos, handle_area);
            state->is_track_hovering = ptinrect(mouse_pos, track_rect);
        }

        /* Handle mouse interaction */
        if (w->parent_window != NULL && w->prop_enabled) {
            /* Click on track or handle - start dragging */
            if (w->parent_window->last_mouse_buttons == 0 &&
                w->parent_window->mouse_buttons != 0) {
                if (state->is_hovering || state->is_track_hovering) {
                    state->is_dragging = 1;
                    update_slider_value(w, calculate_value_from_position(mouse_pos.x, track_rect));
                    state->value = (float)atof(w->prop_value);
                }
            }
            /* Release mouse - stop dragging */
            if (w->parent_window->last_mouse_buttons != 0 &&
                w->parent_window->mouse_buttons == 0) {
                state->is_dragging = 0;
            }
            /* Dragging - update value (independent check, not else-if!) */
            if (state->is_dragging && w->parent_window->mouse_buttons != 0) {
                float new_val = calculate_value_from_position(mouse_pos.x, track_rect);
                update_slider_value(w, new_val);
                state->value = (float)atof(w->prop_value);
            }
        }

        /* Update handle colors based on current state */
        if (state->is_dragging) {
            handle_color = 0xD0D0D0FF;  /* Darker when dragging */
        } else if (state->is_hovering) {
            handle_color = 0xE8E8E8FF;  /* Lighter on hover */
        } else {
            handle_color = 0xE0E0E0FF;  /* Light gray normally (Tk style) */
        }
        handle_highlight = lighten_color(handle_color);
        handle_shadow = darken_color(handle_color);

        /* Draw TWO-SQUARE handle (Tk style, horizontal orientation) */
        draw_two_square_handle(screen, center, handle_color,
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
        int handle_radius = 10;
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
        trough_color = 0xE0E0E0FF;      /* Light gray trough */
        trough_highlight = 0xFFFFFFFF;  /* White highlight */
        trough_shadow = 0x808080FF;     /* Dark gray shadow */
        border_color = 0x000000FF;      /* Black border */
        fill_color = 0x00008BFF;        /* Tk-style blue */

        /* Calculate track rectangle (8px height, centered) */
        track_rect.min.x = widget_rect.min.x;
        track_rect.min.y = widget_rect.min.y + Dy(widget_rect) / 2 - 4;
        track_rect.max.x = widget_rect.max.x;
        track_rect.max.y = track_rect.min.y + 8;

        /* Draw trough background */
        memfillcolor_rect(screen, track_rect, trough_color);

        /* Trough beveled edges - highlight (top) */
        memdraw_line(screen, Pt(track_rect.min.x, track_rect.min.y),
                     Pt(track_rect.max.x - 1, track_rect.min.y), trough_highlight, 2);
        /* Trough beveled edges - shadow (bottom) */
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

        /* Calculate and draw blue fill between handles */
        fill_rect.min.x = track_rect.min.x + 3 + (int)((min_val / 100.0f) * (Dx(track_rect) - 6));
        fill_rect.min.y = track_rect.min.y + 3;
        fill_rect.max.x = track_rect.min.x + 3 + (int)((max_val / 100.0f) * (Dx(track_rect) - 6));
        fill_rect.max.y = track_rect.max.y - 3;

        if (fill_rect.max.x > fill_rect.min.x) {
            memfillcolor_rect(screen, fill_rect, fill_color);
        }

        /* Calculate handle positions */
        min_center = calculate_handle_position(track_rect, min_val, handle_radius);
        max_center = calculate_handle_position(track_rect, max_val, handle_radius);

        /* Check mouse hover state */
        {
            Rectangle min_handle_rect = get_two_square_handle_rect(min_center);
            Rectangle max_handle_rect = get_two_square_handle_rect(max_center);

            mouse_pos.x = w->parent_window ? w->parent_window->mouse_x : 0;
            mouse_pos.y = w->parent_window ? w->parent_window->mouse_y : 0;
            state->is_min_hovering = ptinrect(mouse_pos, min_handle_rect);
            state->is_max_hovering = ptinrect(mouse_pos, max_handle_rect);
            state->is_track_hovering = ptinrect(mouse_pos, track_rect);
        }

        /* Handle mouse interaction */
        if (w->parent_window != NULL && w->prop_enabled) {
            /* Click on track or handle - start dragging */
            if (w->parent_window->last_mouse_buttons == 0 &&
                w->parent_window->mouse_buttons != 0) {
                if (state->is_min_hovering) {
                    state->is_dragging_min = 1;
                } else if (state->is_max_hovering) {
                    state->is_dragging_max = 1;
                } else if (state->is_track_hovering) {
                    /* Move nearest handle */
                    float new_val = calculate_value_from_position(mouse_pos.x, track_rect);
                    float dist_to_min = new_val - min_val;
                    float dist_to_max = max_val - new_val;
                    if (dist_to_min < dist_to_max) {
                        state->is_dragging_min = 1;
                    } else {
                        state->is_dragging_max = 1;
                    }
                }
            }
            /* Release mouse - stop dragging */
            if (w->parent_window->last_mouse_buttons != 0 &&
                w->parent_window->mouse_buttons == 0) {
                state->is_dragging_min = 0;
                state->is_dragging_max = 0;
            }

            /* Update values while dragging */
            if (state->is_dragging_min && w->parent_window->mouse_buttons != 0) {
                float new_val = calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val > max_val - min_gap) new_val = max_val - min_gap;
                if (new_val < 0.0f) new_val = 0.0f;
                state->min_value = new_val;
            } else if (state->is_dragging_max && w->parent_window->mouse_buttons != 0) {
                float new_val = calculate_value_from_position(mouse_pos.x, track_rect);
                if (new_val < min_val + min_gap) new_val = min_val + min_gap;
                if (new_val > 100.0f) new_val = 100.0f;
                state->max_value = new_val;
            }

            /* Update prop_value if values changed */
            if (state->is_dragging_min || state->is_dragging_max) {
                char buf[64];
                sprintf(buf, "%.1f,%.1f", state->min_value, state->max_value);
                free(w->prop_value);
                w->prop_value = strdup(buf);
                g_render_dirty = 1;
            }
        }

        /* Draw min handle */
        if (state->is_dragging_min) {
            handle_color = 0xD0D0D0FF;  /* Darker when dragging */
        } else if (state->is_min_hovering) {
            handle_color = 0xE8E8E8FF;  /* Lighter on hover */
        } else {
            handle_color = 0xE0E0E0FF;  /* Light gray normally (Tk style) */
        }
        handle_highlight = lighten_color(handle_color);
        handle_shadow = darken_color(handle_color);

        draw_two_square_handle(screen, min_center, handle_color,
                              border_color, handle_highlight, handle_shadow,
                              state->is_dragging_min);

        /* Draw max handle */
        if (state->is_dragging_max) {
            handle_color = 0xD0D0D0FF;  /* Darker when dragging */
        } else if (state->is_max_hovering) {
            handle_color = 0xE8E8E8FF;  /* Lighter on hover */
        } else {
            handle_color = 0xE0E0E0FF;  /* Light gray normally (Tk style) */
        }
        handle_highlight = lighten_color(handle_color);
        handle_shadow = darken_color(handle_color);

        draw_two_square_handle(screen, max_center, handle_color,
                              border_color, handle_highlight, handle_shadow,
                              state->is_dragging_max);
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
