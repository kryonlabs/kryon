/*
 * Kryon Graphics Engine - Drawing Operations
 * C89/C90 compliant
 *
 * Reference: Plan 9 libmemdraw/draw.c
 */

#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Rectangle intersection test
 */
int rectXrect(Rectangle r, Rectangle s)
{
    return r.min.x < s.max.x && s.min.x < r.max.x &&
           r.min.y < s.max.y && s.min.y < r.max.y;
}

/*
 * Clip rectangle to clipping rectangle
 * Returns 0 if resulting rectangle is empty
 */
int rect_clip(Rectangle *rp, Rectangle clipr)
{
    if (rp->min.x < clipr.min.x) {
        rp->min.x = clipr.min.x;
    }
    if (rp->min.y < clipr.min.y) {
        rp->min.y = clipr.min.y;
    }
    if (rp->max.x > clipr.max.x) {
        rp->max.x = clipr.max.x;
    }
    if (rp->max.y > clipr.max.y) {
        rp->max.y = clipr.max.y;
    }

    if (rp->min.x >= rp->max.x || rp->min.y >= rp->max.y) {
        return 0;
    }

    return 1;
}

/*
 * Intersect two rectangles
 * Returns 0 if result is empty
 */
int rect_intersect(Rectangle *rp, Rectangle s)
{
    return rect_clip(rp, s);
}

/*
 * Get pixel address for RGBA32 image
 */
static unsigned char *addr_rgba32(Memimage *img, Point p)
{
    int offset;
    int width;

    if (p.x < img->r.min.x || p.x >= img->r.max.x ||
        p.y < img->r.min.y || p.y >= img->r.max.y) {
        return NULL;
    }

    width = Dx(img->r) * 4;
    offset = (p.y - img->r.min.y) * width + (p.x - img->r.min.x) * 4;

    return img->data->bdata + offset;
}

/*
 * Fill entire image with color
 */
void memfillcolor(Memimage *dst, unsigned long color)
{
    memfillcolor_rect(dst, dst->r, color);
}

/*
 * Fill rectangle with solid color (RGBA32 only)
 */
void memfillcolor_rect(Memimage *dst, Rectangle r, unsigned long color)
{
    unsigned char *pixel;
    unsigned char r_val, g_val, b_val, a_val;
    int x, y;
    Rectangle draw_rect;

    if (dst == NULL) {
        return;
    }

    if (dst->chan != RGBA32) {
        fprintf(stderr, "memfillcolor_rect: only RGBA32 supported\n");
        return;
    }

    /* Clip to destination bounds */
    draw_rect = r;
    if (!rect_clip(&draw_rect, dst->clipr)) {
        return;  /* Empty after clipping */
    }

    /* Extract color components (little-endian RGBA32: A B G R) */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    /* Fill pixels */
    for (y = draw_rect.min.y; y < draw_rect.max.y; y++) {
        for (x = draw_rect.min.x; x < draw_rect.max.x; x++) {
            pixel = addr_rgba32(dst, Pt(x, y));
            if (pixel != NULL) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
    }
}

/*
 * Simple bit-blit (copy source to destination)
 * For now, only supports RGBA32 to RGBA32 copy with no clipping of source
 */
void memdraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
             Memimage *mask, Point mp, int op)
{
    unsigned char *src_pixel;
    unsigned char *dst_pixel;
    int dx, dy;
    Point dp;
    int src_x, src_y;
    unsigned char alpha;

    (void)mask;   /* Unused for now */
    (void)mp;     /* Unused for now */
    (void)op;     /* Always use SoverD for now */

    if (dst == NULL || src == NULL) {
        return;
    }

    if (dst->chan != RGBA32 || src->chan != RGBA32) {
        fprintf(stderr, "memdraw: only RGBA32 supported\n");
        return;
    }

    /* Clip destination rectangle */
    if (!rect_clip(&r, dst->clipr)) {
        return;  /* Empty after clipping */
    }

    /* Copy pixels */
    for (dy = 0; dy < Dy(r); dy++) {
        for (dx = 0; dx < Dx(r); dx++) {
            dp = Pt(r.min.x + dx, r.min.y + dy);
            src_x = sp.x + dx;
            src_y = sp.y + dy;

            /* Check source bounds */
            if (src_x < src->r.min.x || src_x >= src->r.max.x ||
                src_y < src->r.min.y || src_y >= src->r.max.y) {
                continue;
            }

            src_pixel = addr_rgba32(src, Pt(src_x, src_y));
            dst_pixel = addr_rgba32(dst, dp);

            if (src_pixel != NULL && dst_pixel != NULL) {
                alpha = src_pixel[3];

                if (alpha == 255) {
                    /* Opaque source - copy directly */
                    dst_pixel[0] = src_pixel[0];
                    dst_pixel[1] = src_pixel[1];
                    dst_pixel[2] = src_pixel[2];
                    dst_pixel[3] = src_pixel[3];
                } else if (alpha > 0) {
                    /* Alpha blend */
                    dst_pixel[0] = (src_pixel[0] * alpha + dst_pixel[0] * (255 - alpha)) / 255;
                    dst_pixel[1] = (src_pixel[1] * alpha + dst_pixel[1] * (255 - alpha)) / 255;
                    dst_pixel[2] = (src_pixel[2] * alpha + dst_pixel[2] * (255 - alpha)) / 255;
                    dst_pixel[3] = alpha + dst_pixel[3] * (255 - alpha) / 255;
                }
                /* If alpha == 0, destination unchanged */
            }
        }
    }
}

/*
 * Parse color string to unsigned long
 * Formats supported:
 *   "#RRGGBB" - hex RGB
 *   "#RRGGBBAA" - hex RGBA
 *   "0xRRGGBB" - hex RGB
 *   "0xRRGGBBAA" - hex RGBA
 *   Decimal value
 */
unsigned long strtocolor(const char *str)
{
    unsigned long color = 0xFF000000;  /* Default opaque */

    if (str == NULL) {
        return color;
    }

    if (str[0] == '#') {
        /* Hex color */
        sscanf(str + 1, "%lx", &color);
        if (strlen(str + 1) == 6) {
            color |= 0xFF000000;  /* Add alpha if not specified */
        }
    } else if (strncmp(str, "0x", 2) == 0) {
        /* Hex color with 0x prefix */
        sscanf(str + 2, "%lx", &color);
        if (strlen(str + 2) == 6) {
            color |= 0xFF000000;
        }
    } else {
        /* Decimal */
        color = atol(str);
    }

    return color;
}

/*
 * Parse rectangle string "x y w h"
 */
int parse_rect(const char *str, Rectangle *r)
{
    int x, y, w, h;

    if (str == NULL || r == NULL) {
        return -1;
    }

    if (sscanf(str, "%d %d %d %d", &x, &y, &w, &h) != 4) {
        return -1;
    }

    r->min.x = x;
    r->min.y = y;
    r->max.x = x + w;
    r->max.y = y + h;

    return 0;
}
