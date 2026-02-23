/*
 * Kryon Graphics Engine - Drawing Operations
 * C89/C90 compliant
 *
 * Reference: Plan 9 libmemdraw/draw.c
 */

#include "graphics.h"
#include "pixconv.h"
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
 * Point-in-rectangle test
 */
int ptinrect(Point p, Rectangle r)
{
    return p.x >= r.min.x && p.x < r.max.x &&
           p.y >= r.min.y && p.y < r.max.y;
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
 * Get pixel address for RGB24 image
 */
static unsigned char *addr_rgb24(Memimage *img, Point p)
{
    int offset;
    int width;

    if (p.x < img->r.min.x || p.x >= img->r.max.x ||
        p.y < img->r.min.y || p.y >= img->r.max.y) {
        return NULL;
    }

    width = Dx(img->r) * 3;
    offset = (p.y - img->r.min.y) * width + (p.x - img->r.min.x) * 3;

    return img->data->bdata + offset;
}

/*
 * Get pixel address for GREY8 image
 */
static unsigned char *addr_grey8(Memimage *img, Point p)
{
    int offset;
    int width;

    if (p.x < img->r.min.x || p.x >= img->r.max.x ||
        p.y < img->r.min.y || p.y >= img->r.max.y) {
        return NULL;
    }

    width = Dx(img->r);
    offset = (p.y - img->r.min.y) * width + (p.x - img->r.min.x);

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
 * Fill rectangle with solid color (supports RGBA32, RGB24, GREY8)
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

    /* Clip to destination bounds */
    draw_rect = r;
    if (!rect_clip(&draw_rect, dst->clipr)) {
        return;  /* Empty after clipping */
    }

    /* Extract color components (32-bit RGBA: R G B A byte order) */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    /* Dispatch based on pixel format */
    switch (dst->chan) {
    case RGBA32:
        /* Fill pixels in RGBA32 format */
        for (y = draw_rect.min.y; y < draw_rect.max.y; y++) {
            for (x = draw_rect.min.x; x < draw_rect.max.x; x++) {
                pixel = addr_rgba32(dst, Pt(x, y));
                if (pixel != NULL) {
                    /* Store in byte order: R G B A */
                    pixel[0] = r_val;
                    pixel[1] = g_val;
                    pixel[2] = b_val;
                    pixel[3] = a_val;
                }
            }
        }
        break;

    case RGB24:
        /* Fill pixels in RGB24 format */
        for (y = draw_rect.min.y; y < draw_rect.max.y; y++) {
            for (x = draw_rect.min.x; x < draw_rect.max.x; x++) {
                pixel = addr_rgb24(dst, Pt(x, y));
                if (pixel != NULL) {
                    /* RGB24 byte order: R G B */
                    pixel[0] = r_val;
                    pixel[1] = g_val;
                    pixel[2] = b_val;
                }
            }
        }
        break;

    case GREY8:
        {
            /* Calculate luminance using ITU-R BT.601 formula */
            int grey = (299 * (int)r_val + 587 * (int)g_val + 114 * (int)b_val) / 1000;
            if (grey < 0) grey = 0;
            if (grey > 255) grey = 255;

            for (y = draw_rect.min.y; y < draw_rect.max.y; y++) {
                for (x = draw_rect.min.x; x < draw_rect.max.x; x++) {
                    pixel = addr_grey8(dst, Pt(x, y));
                    if (pixel != NULL) {
                        pixel[0] = (unsigned char)grey;
                    }
                }
            }
        }
        break;

    default:
        fprintf(stderr, "memfillcolor_rect: unsupported channel 0x%08lX\n", dst->chan);
        break;
    }
}

/*
 * Porter-Duff compositing operators
 * Reference: "Compositing Digital Images" by Porter and Duff (1984)
 */

/*
 * Clear operator - destination cleared regardless of source
 */
static void composite_clear(unsigned char *dst_pixel,
                           const unsigned char *src_pixel)
{
    (void)src_pixel;  /* Source ignored */
    dst_pixel[0] = 0;
    dst_pixel[1] = 0;
    dst_pixel[2] = 0;
    dst_pixel[3] = 0;
}

/*
 * Source operator - replace destination with source
 */
static void composite_source(unsigned char *dst_pixel,
                            const unsigned char *src_pixel)
{
    dst_pixel[0] = src_pixel[0];
    dst_pixel[1] = src_pixel[1];
    dst_pixel[2] = src_pixel[2];
    dst_pixel[3] = src_pixel[3];
}

/*
 * Source over Destination - alpha blend
 */
static void composite_soverd(unsigned char *dst_pixel,
                            const unsigned char *src_pixel)
{
    int src_alpha = src_pixel[3];
    int dst_alpha = dst_pixel[3];
    int i;

    if (src_alpha == 255) {
        /* Opaque source - copy directly */
        dst_pixel[0] = src_pixel[0];
        dst_pixel[1] = src_pixel[1];
        dst_pixel[2] = src_pixel[2];
        dst_pixel[3] = 255;
    } else if (src_alpha > 0) {
        /* Alpha blend */
        for (i = 0; i < 3; i++) {
            dst_pixel[i] = (src_pixel[i] * src_alpha + dst_pixel[i] * dst_alpha * (255 - src_alpha) / 255) / 255;
        }
        dst_pixel[3] = src_alpha + dst_alpha * (255 - src_alpha) / 255;
    }
    /* If src_alpha == 0, destination unchanged */
}

/*
 * Source in Destination - source visible only where destination is opaque
 */
static void composite_sind(unsigned char *dst_pixel,
                          const unsigned char *src_pixel)
{
    int dst_alpha = dst_pixel[3];
    int src_alpha = src_pixel[3];
    int out_alpha;
    int i;

    /* Result alpha = source alpha * destination alpha */
    out_alpha = src_alpha * dst_alpha / 255;

    if (out_alpha > 0) {
        for (i = 0; i < 3; i++) {
            dst_pixel[i] = src_pixel[i] * dst_alpha / 255;
        }
        dst_pixel[3] = out_alpha;
    } else {
        dst_pixel[0] = 0;
        dst_pixel[1] = 0;
        dst_pixel[2] = 0;
        dst_pixel[3] = 0;
    }
}

/*
 * Source out Destination - source visible only where destination is transparent
 */
static void composite_soutd(unsigned char *dst_pixel,
                           const unsigned char *src_pixel)
{
    int dst_alpha = dst_pixel[3];
    int src_alpha = src_pixel[3];
    int out_alpha;
    int i;

    /* Result alpha = source alpha * (1 - destination alpha) */
    out_alpha = src_alpha * (255 - dst_alpha) / 255;

    if (out_alpha > 0) {
        for (i = 0; i < 3; i++) {
            dst_pixel[i] = src_pixel[i];
        }
        dst_pixel[3] = out_alpha;
    } else {
        dst_pixel[0] = 0;
        dst_pixel[1] = 0;
        dst_pixel[2] = 0;
        dst_pixel[3] = 0;
    }
}

/*
 * Destination over Source - destination visible where source is transparent
 */
static void composite_dovers(unsigned char *dst_pixel,
                            const unsigned char *src_pixel)
{
    int dst_alpha = dst_pixel[3];
    int src_alpha = src_pixel[3];
    int out_alpha;
    int i;

    /* Result alpha = dest alpha * (1 - source alpha) + source alpha */
    out_alpha = dst_alpha * (255 - src_alpha) / 255 + src_alpha;

    if (out_alpha > 0) {
        for (i = 0; i < 3; i++) {
            dst_pixel[i] = (dst_pixel[i] * dst_alpha * (255 - src_alpha) / 255 +
                           src_pixel[i] * src_alpha) / out_alpha;
        }
        dst_pixel[3] = out_alpha;
    } else {
        dst_pixel[0] = 0;
        dst_pixel[1] = 0;
        dst_pixel[2] = 0;
        dst_pixel[3] = 0;
    }
}

/*
 * Bit-blit with Porter-Duff compositing
 * Supports RGBA32 to RGBA32 copy with various compositing operators
 */
void memdraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
             Memimage *mask, Point mp, int op)
{
    unsigned char *src_pixel;
    unsigned char *dst_pixel;
    int dx, dy;
    Point dp;
    int src_x, src_y;

    (void)mask;   /* TODO: Implement mask support */
    (void)mp;     /* TODO: Implement mask support */

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
                /* Apply compositing operator */
                switch (op) {
                case Clear:  /* Clear */
                    composite_clear(dst_pixel, src_pixel);
                    break;
                case SinD:  /* Source in Destination */
                    composite_sind(dst_pixel, src_pixel);
                    break;
                case SoutD:  /* Source out Destination */
                    composite_soutd(dst_pixel, src_pixel);
                    break;
                case DoverS:  /* Destination over Source */
                    composite_dovers(dst_pixel, src_pixel);
                    break;
                case SoverD:  /* Source over Destination (default) */
                default:
                    composite_soverd(dst_pixel, src_pixel);
                    break;
                }
            }
        }
    }
}

/*
 * Draw polygon (filled or outlined)
 * Uses scanline fill algorithm for filled polygons
 */
void memdraw_poly(Memimage *dst, Point *points, int npoints, unsigned long color, int fill)
{
    unsigned char *pixel;
    unsigned char r_val, g_val, b_val, a_val;
    int i, y, x;
    int min_y, max_y;
    int *scan_x;
    int n_intersections;

    if (dst == NULL || points == NULL || npoints < 3) {
        return;
    }

    if (dst->chan != RGBA32) {
        fprintf(stderr, "memdraw_poly: only RGBA32 supported\n");
        return;
    }

    /* Extract color components (32-bit RGBA: R G B A byte order) */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    /* For outline, just draw lines between points */
    if (!fill) {
        for (i = 0; i < npoints; i++) {
            Point p0, p1;
            p0 = points[i];
            p1 = points[(i + 1) % npoints];
            memdraw_line(dst, p0, p1, color, 1);
        }
        return;
    }

    /* Find Y bounds */
    min_y = dst->r.max.y;
    max_y = dst->r.min.y;
    for (i = 0; i < npoints; i++) {
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }
        if (points[i].y > max_y) {
            max_y = points[i].y;
        }
    }

    /* Clip to image bounds */
    if (min_y < dst->r.min.y) {
        min_y = dst->r.min.y;
    }
    if (max_y >= dst->r.max.y) {
        max_y = dst->r.max.y - 1;
    }

    /* Allocate scanline intersection array */
    scan_x = (int *)malloc(sizeof(int) * npoints * 2);
    if (scan_x == NULL) {
        fprintf(stderr, "memdraw_poly: cannot allocate scanline buffer\n");
        return;
    }

    /* Scanline fill algorithm */
    for (y = min_y; y <= max_y; y++) {
        n_intersections = 0;

        /* Find intersections with polygon edges */
        for (i = 0; i < npoints; i++) {
            Point p0, p1;
            int x0, y0, x1, y1;
            float x_intersect;

            p0 = points[i];
            p1 = points[(i + 1) % npoints];

            x0 = p0.x;
            y0 = p0.y;
            x1 = p1.x;
            y1 = p1.y;

            /* Check if edge crosses this scanline */
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                /* Calculate intersection X coordinate */
                x_intersect = x0 + (float)(y - y0) * (x1 - x0) / (float)(y1 - y0);
                scan_x[n_intersections++] = (int)x_intersect;
            }
        }

        /* Sort intersections */
        for (i = 0; i < n_intersections - 1; i++) {
            int j;
            for (j = i + 1; j < n_intersections; j++) {
                if (scan_x[i] > scan_x[j]) {
                    int temp = scan_x[i];
                    scan_x[i] = scan_x[j];
                    scan_x[j] = temp;
                }
            }
        }

        /* Fill between pairs of intersections */
        for (i = 0; i < n_intersections - 1; i += 2) {
            for (x = scan_x[i]; x < scan_x[i + 1]; x++) {
                if (x >= dst->r.min.x && x < dst->r.max.x) {
                    pixel = addr_rgba32(dst, Pt(x, y));
                    if (pixel != NULL) {
                        if (a_val == 255) {
                            /* Store in byte order: R G B A */
                            pixel[0] = r_val;
                            pixel[1] = g_val;
                            pixel[2] = b_val;
                            pixel[3] = a_val;
                        } else if (a_val > 0) {
                            /* Alpha blend in byte order */
                            pixel[0] = (r_val * a_val + pixel[0] * (255 - a_val)) / 255;
                            pixel[1] = (g_val * a_val + pixel[1] * (255 - a_val)) / 255;
                            pixel[2] = (b_val * a_val + pixel[2] * (255 - a_val)) / 255;
                            pixel[3] = a_val + pixel[3] * (255 - a_val) / 255;
                        }
                    }
                }
            }
        }
    }

    free(scan_x);
}

/*
 * 8x16 bitmap font for ASCII 32-126
 * Each character is 16 bytes (2 bytes per row, 8 pixels per row)
 */
static const unsigned char default_font_8x16[95][16] = {
    /* 0x20 (space) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x21 (!) */
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,
     0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 0x22 (") */
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x23 (#) */
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,
     0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00},
    /* 0x24 ($) */
    {0x00,0x10,0x10,0x7C,0xD6,0xD0,0x7C,0x10,
     0x7C,0xD6,0xD0,0x7C,0x10,0x10,0x00,0x00},
    /* 0x25 (%) */
    {0x00,0x00,0x00,0x00,0xC6,0xC6,0x0C,0x18,
     0x30,0x60,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x26 (&) */
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,
     0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 0x27 (') */
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x28 (() */
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,
     0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00},
    /* 0x29 ()) */
    {0x00,0x00,0x60,0x30,0x18,0x18,0x18,0x18,
     0x18,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    /* 0x2A (*) */
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,
     0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2B (+) */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,
     0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2C (,) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00},
    /* 0x2D (-) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x2E (.) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 0x2F (/) */
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,
     0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00},
    /* 0x30 (0) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xCE,0xDE,0xF6,
     0xE6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x31 (1) */
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,
     0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},
    /* 0x32 (2) */
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,
     0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 0x33 (3) */
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,
     0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x34 (4) */
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,
     0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00},
    /* 0x35 (5) */
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,
     0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x36 (6) */
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,
     0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x37 (7) */
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,
     0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},
    /* 0x38 (8) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,
     0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x39 (9) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,
     0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},
    /* 0x3A (:) */
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,
     0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
    /* 0x3B (;) */
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,
     0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00},
    /* 0x3C (<) */
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,
     0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},
    /* 0x3D (=) */
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,
     0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x3E (>) */
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,
     0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},
    /* 0x3F (?) */
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,
     0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 0x40 (@) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xDE,
     0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00},
    /* 0x41 (A) */
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,
     0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x42 (B) */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,
     0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00},
    /* 0x43 (C) */
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,
     0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00},
    /* 0x44 (D) */
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,
     0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00},
    /* 0x45 (E) */
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,
     0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    /* 0x46 (F) */
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,
     0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 0x47 (G) */
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,
     0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00},
    /* 0x48 (H) */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,
     0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x49 (I) */
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,
     0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 0x4A (J) */
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,
     0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00},
    /* 0x4B (K) */
    {0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,
     0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 0x4C (L) */
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,
     0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00},
    /* 0x4D (M) */
    {0x00,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,
     0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x4E (N) */
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,
     0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x4F (O) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,
     0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x50 (P) */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,
     0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 0x51 (Q) */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,
     0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00},
    /* 0x52 (R) */
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,
     0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 0x53 (S) */
    {0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,
     0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x54 (T) */
    {0x00,0x00,0x7E,0x7E,0x5A,0x18,0x18,0x18,
     0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 0x55 (U) */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,
     0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x56 (V) */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,
     0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00},
    /* 0x57 (W) */
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,
     0xD6,0xFE,0xEE,0x6C,0x00,0x00,0x00,0x00},
    /* 0x58 (X) */
    {0x00,0x00,0xC6,0xC6,0x6C,0x7C,0x38,0x38,
     0x7C,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x59 (Y) */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,
     0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 0x5A (Z) */
    {0x00,0x00,0xFE,0xC6,0x86,0x0C,0x18,0x30,
     0x60,0xC2,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 0x5B ([) */
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,
     0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00},
    /* 0x5C (\) */
    {0x00,0x00,0x00,0x80,0xC0,0x60,0x30,0x18,
     0x0C,0x06,0x02,0x00,0x00,0x00,0x00,0x00},
    /* 0x5D (]) */
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,
     0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00},
    /* 0x5E (^) */
    {0x00,0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x5F (_) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00},
    /* 0x60 (`) */
    {0x00,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 0x61 (a) */
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,
     0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 0x62 (b) */
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,
     0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},
    /* 0x63 (c) */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,
     0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x64 (d) */
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,
     0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 0x65 (e) */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,
     0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x66 (f) */
    {0x00,0x00,0x38,0x6C,0x64,0x60,0xF0,0x60,
     0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 0x67 (g) */
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,
     0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00},
    /* 0x68 (h) */
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,
     0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 0x69 (i) */
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,
     0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 0x6A (j) */
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,
     0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    /* 0x6B (k) */
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,
     0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00},
    /* 0x6C (l) */
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,
     0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},
    /* 0x6D (m) */
    {0x00,0x00,0x00,0x00,0x00,0xEC,0xFE,0xD6,
     0xD6,0xD6,0xD6,0xC6,0x00,0x00,0x00,0x00},
    /* 0x6E (n) */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,
     0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},
    /* 0x6F (o) */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,
     0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x70 (p) */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,
     0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},
    /* 0x71 (q) */
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,
     0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00},
    /* 0x72 (r) */
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,
     0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00},
    /* 0x73 (s) */
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,
     0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00},
    /* 0x74 (t) */
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,
     0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00},
    /* 0x75 (u) */
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,
     0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00},
    /* 0x76 (v) */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,
     0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00},
    /* 0x77 (w) */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,
     0xD6,0xD6,0xFE,0x6C,0x00,0x00,0x00,0x00},
    /* 0x78 (x) */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,
     0x38,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    /* 0x79 (y) */
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,
     0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00},
    /* 0x7A (z) */
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,
     0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00},
    /* 0x7B ({) */
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,
     0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00},
    /* 0x7C (|) */
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,
     0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    /* 0x7D (}) */
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,
     0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00},
    /* 0x7E (~) */
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

/*
 * Draw a single character using the embedded font
 */
static void memdraw_char(Memimage *dst, Point p, unsigned char ch, unsigned long color)
{
    unsigned char *pixel;
    unsigned char r_val, g_val, b_val, a_val;
    int row, col;
    int font_idx;

    if (dst == NULL) {
        return;
    }

    if (dst->chan != RGBA32) {
        fprintf(stderr, "memdraw_char: only RGBA32 supported\n");
        return;
    }

    if (ch < 32 || ch > 126) {
        /* Unsupported character, draw space */
        ch = 32;
    }

    font_idx = ch - 32;

    /* Extract color components (32-bit RGBA: R G B A byte order) */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    /* Draw character */
    for (row = 0; row < 16; row++) {
        unsigned char font_row;
        font_row = default_font_8x16[font_idx][row];

        for (col = 0; col < 8; col++) {
            int x, y;
            unsigned char mask;

            x = p.x + col;
            y = p.y + row;

            /* Check bounds */
            if (x < dst->r.min.x || x >= dst->r.max.x ||
                y < dst->r.min.y || y >= dst->r.max.y) {
                continue;
            }

            mask = 0x80 >> col;
            if (font_row & mask) {
                /* Pixel is set - draw it */
                pixel = addr_rgba32(dst, Pt(x, y));
                if (pixel != NULL) {
                    if (a_val == 255) {
                        /* Store in little-endian byte order: B G R A */
                        pixel[0] = b_val;
                        pixel[1] = g_val;
                        pixel[2] = r_val;
                        pixel[3] = a_val;
                    } else if (a_val > 0) {
                        /* Alpha blend in little-endian byte order */
                        pixel[0] = (b_val * a_val + pixel[0] * (255 - a_val)) / 255;
                        pixel[1] = (g_val * a_val + pixel[1] * (255 - a_val)) / 255;
                        pixel[2] = (r_val * a_val + pixel[2] * (255 - a_val)) / 255;
                        pixel[3] = a_val + pixel[3] * (255 - a_val) / 255;
                    }
                }
            }
        }
    }
}

/*
 * Draw text string
 */
void memdraw_text(Memimage *dst, Point p, const char *str, unsigned long color)
{
    int x, y;
    int i;

    if (dst == NULL || str == NULL) {
        return;
    }

    x = p.x;
    y = p.y;

    for (i = 0; str[i] != '\0'; i++) {
        memdraw_char(dst, Pt(x, y), (unsigned char)str[i], color);
        x += 8;  /* Character width */

        /* Handle newline */
        if (str[i] == '\n') {
            x = p.x;
            y += 16;  /* Character height */
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

/*
 * Draw line using Bresenham's algorithm
 * Supports thickness (number of parallel lines)
 */
void memdraw_line(Memimage *dst, Point p0, Point p1, unsigned long color, int thickness)
{
    unsigned char *pixel;
    unsigned char r_val, g_val, b_val, a_val;
    int dx, dy, sx, sy, err, err2;
    int x, y;
    int x0, y0, x1, y1;

    if (dst == NULL) {
        return;
    }

    if (dst->chan != RGBA32) {
        fprintf(stderr, "memdraw_line: only RGBA32 supported\n");
        return;
    }

    /* Extract color components (32-bit RGBA: R G B A byte order) */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    /* Draw line (thickness not yet implemented, keeping it simple) */
    x0 = p0.x;
    y0 = p0.y;
    x1 = p1.x;
    y1 = p1.y;

    /* Bresenham's algorithm */
    dx = abs(x1 - x0);
    dy = abs(y1 - y0);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    x = x0;
    y = y0;

    while (1) {
        /* Draw pixel */
        if (x >= dst->r.min.x && x < dst->r.max.x &&
            y >= dst->r.min.y && y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x, y));
            if (pixel != NULL) {
                /* Simple alpha blending in little-endian byte order */
                if (a_val == 255) {
                    pixel[0] = b_val;
                    pixel[1] = g_val;
                    pixel[2] = r_val;
                    pixel[3] = a_val;
                } else if (a_val > 0) {
                    pixel[0] = (b_val * a_val + pixel[0] * (255 - a_val)) / 255;
                    pixel[1] = (g_val * a_val + pixel[1] * (255 - a_val)) / 255;
                    pixel[2] = (r_val * a_val + pixel[2] * (255 - a_val)) / 255;
                    pixel[3] = a_val + pixel[3] * (255 - a_val) / 255;
                }
            }
        }

        if (x == x1 && y == y1) {
            break;
        }

        err2 = 2 * err;
        if (err2 > -dy) {
            err = err - dy;
            x = x + sx;
        }
        if (err2 < dx) {
            err = err + dx;
            y = y + sy;
        }
    }
}

/*
 * Draw ellipse using Bresenham-based algorithm
 * center: ellipse center point
 * rx: horizontal radius
 * ry: vertical radius
 * color: fill color
 * fill: non-zero to fill, zero to draw outline only
 */
void memdraw_ellipse(Memimage *dst, Point center, int rx, int ry,
                     unsigned long color, int fill)
{
    unsigned char *pixel;
    unsigned char r_val, g_val, b_val, a_val;
    int x, y;
    int x_change, y_change;
    int ellipse_error;
    int two_a_sq, two_b_sq;
    int stopping_x, stopping_y;
    int x0, y0;

    if (dst == NULL) {
        return;
    }

    if (dst->chan != RGBA32) {
        fprintf(stderr, "memdraw_ellipse: only RGBA32 supported\n");
        return;
    }

    if (rx <= 0 || ry <= 0) {
        return;
    }

    /* Extract color components */
    r_val = (color >> 0) & 0xFF;
    g_val = (color >> 8) & 0xFF;
    b_val = (color >> 16) & 0xFF;
    a_val = (color >> 24) & 0xFF;

    two_a_sq = 2 * rx * rx;
    two_b_sq = 2 * ry * ry;

    x = 0;
    y = ry;
    x_change = ry * ry * (1 - 2 * rx);
    y_change = rx * rx;
    ellipse_error = 0;
    stopping_x = two_b_sq * rx;
    stopping_y = 0;

    x0 = center.x;
    y0 = center.y;

    /* Region 1 */
    while (stopping_x >= stopping_y) {
        /* Draw 4-way symmetry */
        /* Point 1: (x0+x, y0+y) */
        if (x0 + x >= dst->r.min.x && x0 + x < dst->r.max.x &&
            y0 + y >= dst->r.min.y && y0 + y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 + x, y0 + y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 2: (x0-x, y0+y) */
        if (x0 - x >= dst->r.min.x && x0 - x < dst->r.max.x &&
            y0 + y >= dst->r.min.y && y0 + y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 - x, y0 + y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 3: (x0+x, y0-y) */
        if (x0 + x >= dst->r.min.x && x0 + x < dst->r.max.x &&
            y0 - y >= dst->r.min.y && y0 - y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 + x, y0 - y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 4: (x0-x, y0-y) */
        if (x0 - x >= dst->r.min.x && x0 - x < dst->r.max.x &&
            y0 - y >= dst->r.min.y && y0 - y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 - x, y0 - y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }

        y++;
        stopping_y += two_a_sq;
        ellipse_error += y_change;
        y_change += two_a_sq;
        if (2 * ellipse_error + x_change > 0) {
            x--;
            stopping_x -= two_b_sq;
            ellipse_error += x_change;
            x_change += two_b_sq;
        }
    }

    /* Region 2 */
    x = rx;
    y = 0;
    x_change = ry * ry;
    y_change = rx * rx * (1 - 2 * ry);
    ellipse_error = 0;
    stopping_x = 0;
    stopping_y = two_a_sq * ry;

    while (stopping_x <= stopping_y) {
        /* Draw 4-way symmetry */
        /* Point 1: (x0+x, y0+y) */
        if (x0 + x >= dst->r.min.x && x0 + x < dst->r.max.x &&
            y0 + y >= dst->r.min.y && y0 + y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 + x, y0 + y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 2: (x0-x, y0+y) */
        if (x0 - x >= dst->r.min.x && x0 - x < dst->r.max.x &&
            y0 + y >= dst->r.min.y && y0 + y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 - x, y0 + y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 3: (x0+x, y0-y) */
        if (x0 + x >= dst->r.min.x && x0 + x < dst->r.max.x &&
            y0 - y >= dst->r.min.y && y0 - y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 + x, y0 - y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }
        /* Point 4: (x0-x, y0-y) */
        if (x0 - x >= dst->r.min.x && x0 - x < dst->r.max.x &&
            y0 - y >= dst->r.min.y && y0 - y < dst->r.max.y) {
            pixel = addr_rgba32(dst, Pt(x0 - x, y0 - y));
            if (pixel != NULL && a_val == 255) {
                pixel[0] = r_val;
                pixel[1] = g_val;
                pixel[2] = b_val;
                pixel[3] = a_val;
            }
        }

        x++;
        stopping_x += two_b_sq;
        ellipse_error += x_change;
        x_change += two_b_sq;
        if (2 * ellipse_error + y_change > 0) {
            y--;
            stopping_y -= two_a_sq;
            ellipse_error += y_change;
            y_change += two_a_sq;
        }
    }

    /* Note: fill parameter not yet implemented - would require scanline fill algorithm */
    (void)fill;
}
