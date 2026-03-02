/*
 * Kryon Rendering Primitives
 * C89/C90 compliant
 *
 * Basic drawing functions: rectangles, borders, handles
 */

#include "render.h"
#include "util.h"
#include "graphics.h"

/*
 * Draw Tk-style square/circular handle for slider
 * Draws a filled square with bevel effect
 */
void render_draw_square_handle(Memimage *screen, Point center, int radius,
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
void render_draw_two_square_handle(Memimage *screen, Point center,
                                   unsigned long color, unsigned long border,
                                   unsigned long highlight, unsigned long shadow,
                                   int is_dragging)
{
    int square_size = 10;  /* Each square is 10x10 pixels */
    int square_gap = 0;    /* No gap between squares */
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
