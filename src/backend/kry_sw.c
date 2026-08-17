#include "kry_sw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * 8x8 bitmap font, ASCII 32..126. From font8x8 by Daniel Hepper
 * <https://github.com/dhepper/font8x8>, public domain.
 */
static const unsigned char k_font8x8[128][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},{0x36,0x36,0x7f,0x36,0x7f,0x36,0x36,0x00},
    {0x0c,0x3e,0x03,0x1e,0x30,0x1f,0x0c,0x00},{0x00,0x63,0x33,0x18,0x0c,0x66,0x63,0x00},
    {0x1c,0x36,0x1c,0x6e,0x3b,0x33,0x6e,0x00},{0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0c,0x06,0x06,0x06,0x0c,0x18,0x00},{0x06,0x0c,0x18,0x18,0x18,0x0c,0x06,0x00},
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},{0x00,0x0c,0x0c,0x3f,0x0c,0x0c,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x06},{0x00,0x00,0x00,0x3f,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x00},{0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00},
    {0x3e,0x63,0x73,0x7b,0x6f,0x67,0x3e,0x00},{0x0c,0x0e,0x0c,0x0c,0x0c,0x0c,0x3f,0x00},
    {0x1e,0x33,0x30,0x1c,0x06,0x33,0x3f,0x00},{0x1e,0x33,0x30,0x1c,0x30,0x33,0x1e,0x00},
    {0x38,0x3c,0x36,0x33,0x7f,0x30,0x78,0x00},{0x3f,0x03,0x1f,0x30,0x30,0x33,0x1e,0x00},
    {0x1c,0x06,0x03,0x1f,0x33,0x33,0x1e,0x00},{0x3f,0x33,0x30,0x18,0x0c,0x0c,0x0c,0x00},
    {0x1e,0x33,0x33,0x1e,0x33,0x33,0x1e,0x00},{0x1e,0x33,0x33,0x3e,0x30,0x18,0x0e,0x00},
    {0x00,0x0c,0x0c,0x00,0x00,0x0c,0x0c,0x00},{0x00,0x0c,0x0c,0x00,0x00,0x0c,0x0c,0x06},
    {0x18,0x0c,0x06,0x03,0x06,0x0c,0x18,0x00},{0x00,0x00,0x3f,0x00,0x00,0x3f,0x00,0x00},
    {0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00},{0x1e,0x33,0x30,0x18,0x0c,0x00,0x0c,0x00},
    {0x3e,0x63,0x7b,0x7b,0x7b,0x03,0x1e,0x00},{0x0c,0x1e,0x33,0x33,0x3f,0x33,0x33,0x00},
    {0x3f,0x66,0x66,0x3e,0x66,0x66,0x3f,0x00},{0x3c,0x66,0x03,0x03,0x03,0x66,0x3c,0x00},
    {0x1f,0x36,0x66,0x66,0x66,0x36,0x1f,0x00},{0x7f,0x46,0x16,0x1e,0x16,0x46,0x7f,0x00},
    {0x7f,0x46,0x16,0x1e,0x16,0x06,0x0f,0x00},{0x3c,0x66,0x03,0x03,0x73,0x66,0x7c,0x00},
    {0x33,0x33,0x33,0x3f,0x33,0x33,0x33,0x00},{0x1e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1e,0x00},{0x67,0x66,0x36,0x1e,0x36,0x66,0x67,0x00},
    {0x0f,0x06,0x06,0x06,0x46,0x66,0x7f,0x00},{0x63,0x77,0x7f,0x7f,0x6b,0x63,0x63,0x00},
    {0x63,0x67,0x6f,0x7b,0x73,0x63,0x63,0x00},{0x1c,0x36,0x63,0x63,0x63,0x36,0x1c,0x00},
    {0x3f,0x66,0x66,0x3e,0x06,0x06,0x0f,0x00},{0x1e,0x33,0x33,0x33,0x3b,0x1e,0x38,0x00},
    {0x3f,0x66,0x66,0x3e,0x36,0x66,0x67,0x00},{0x1e,0x33,0x07,0x0e,0x38,0x33,0x1e,0x00},
    {0x3f,0x2d,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},{0x33,0x33,0x33,0x33,0x33,0x33,0x3f,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1e,0x0c,0x00},{0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1c,0x1c,0x36,0x63,0x00},{0x33,0x33,0x33,0x1e,0x0c,0x0c,0x1e,0x00},
    {0x7f,0x63,0x31,0x18,0x4c,0x66,0x7f,0x00},{0x1e,0x06,0x06,0x06,0x06,0x06,0x1e,0x00},
    {0x03,0x06,0x0c,0x18,0x30,0x60,0x40,0x00},{0x1e,0x18,0x18,0x18,0x18,0x18,0x1e,0x00},
    {0x08,0x1c,0x36,0x63,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x0c,0x0c,0x18,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x1e,0x30,0x3e,0x33,0x6e,0x00},
    {0x07,0x06,0x06,0x3e,0x66,0x66,0x3b,0x00},{0x00,0x00,0x1e,0x33,0x03,0x33,0x1e,0x00},
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6e,0x00},{0x00,0x00,0x1e,0x33,0x3f,0x03,0x1e,0x00},
    {0x1c,0x36,0x06,0x0f,0x06,0x06,0x0f,0x00},{0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x1f},
    {0x07,0x06,0x36,0x6e,0x66,0x66,0x67,0x00},{0x0c,0x00,0x0e,0x0c,0x0c,0x0c,0x1e,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1e},{0x07,0x06,0x66,0x36,0x1e,0x36,0x67,0x00},
    {0x0e,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e,0x00},{0x00,0x00,0x33,0x7f,0x7f,0x6b,0x63,0x00},
    {0x00,0x00,0x1f,0x33,0x33,0x33,0x33,0x00},{0x00,0x00,0x1e,0x33,0x33,0x33,0x1e,0x00},
    {0x00,0x00,0x3b,0x66,0x66,0x3e,0x06,0x0f},{0x00,0x00,0x6e,0x33,0x33,0x3e,0x30,0x78},
    {0x00,0x00,0x3b,0x6e,0x66,0x06,0x0f,0x00},{0x00,0x00,0x3e,0x03,0x1e,0x30,0x1f,0x00},
    {0x08,0x0c,0x3e,0x0c,0x0c,0x2c,0x18,0x00},{0x00,0x00,0x33,0x33,0x33,0x33,0x6e,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1e,0x0c,0x00},{0x00,0x00,0x63,0x6b,0x7f,0x7f,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1c,0x36,0x63,0x00},{0x00,0x00,0x33,0x33,0x33,0x3e,0x30,0x1f},
    {0x00,0x00,0x3f,0x19,0x0c,0x26,0x3f,0x00},{0x38,0x0c,0x0c,0x07,0x0c,0x0c,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},{0x07,0x0c,0x0c,0x38,0x0c,0x0c,0x07,0x00},
    {0x6e,0x3b,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

#define FONT_LAST 127

int
KrySwInit(KrySw *sw, unsigned char *pixels, int w, int h)
{
    if(sw == NULL || w <= 0 || h <= 0)
        return -1;
    memset(sw, 0, sizeof(*sw));
    if(pixels == NULL) {
        sw->pixels = malloc((size_t)w * h * 4);
        if(sw->pixels == NULL)
            return -1;
        sw->owns = 1;
    } else {
        sw->pixels = pixels;
    }
    sw->w = w;
    sw->h = h;
    sw->stride = w * 4;
    sw->scale = 1000;
    sw->theme[KRY_THEME_BACKGROUND] = 0x101014ffu;
    sw->theme[KRY_THEME_TEXT] = 0xf0f0f2ffu;
    sw->theme[KRY_THEME_ICON] = 0x8a8a93ffu;
    sw->theme[KRY_THEME_SURFACE] = 0x1c1c22ffu;
    sw->theme[KRY_THEME_BUTTON] = 0x2d4d7bffu;
    memset(sw->pixels, 0, (size_t)sw->stride * h);
    return 0;
}

void
KrySwFree(KrySw *sw)
{
    if(sw == NULL)
        return;
    if(sw->owns)
        free(sw->pixels);
    sw->pixels = NULL;
}

void
KrySwMouse(KrySw *sw, int x, int y)
{
    sw->mouse_x = x;
    sw->mouse_y = y;
}

void
KrySwButtonDown(KrySw *sw, int button)
{
    if(button < 0 || button >= 8)
        return;
    sw->buttons_down |= 1 << button;
    sw->buttons_pressed |= 1 << button;
}

void
KrySwText(KrySw *sw, unsigned codepoint)
{
    if(sw->keys_n < 16)
        sw->keys[sw->keys_n++] = codepoint;
}

void
KrySwWheel(KrySw *sw, int dy)
{
    sw->wheel += dy;
}

void
KrySwButtonUp(KrySw *sw, int button)
{
    if(button < 0 || button >= 8)
        return;
    sw->buttons_down &= ~(1 << button);
}

void
KrySwAdvance(KrySw *sw, float dt)
{
    sw->buttons_pressed = 0;
    sw->keys_n = 0;
    sw->wheel = 0;
    sw->now += dt;
}

void
KrySwSetTheme(KrySw *sw, int slot, unsigned rgba)
{
    if(slot >= 0 && slot < KRY_THEME_COUNT)
        sw->theme[slot] = rgba;
}

void
KrySwDirty(KrySw *sw, int *x, int *y, int *w, int *h)
{
    if(x != NULL)
        *x = 0;
    if(y != NULL)
        *y = 0;
    if(w != NULL)
        *w = sw->w;
    if(h != NULL)
        *h = sw->h;
}

void
KrySwToRGB565(const KrySw *sw, unsigned short *dst)
{
    long i;
    long n = (long)sw->w * sw->h;

    if(dst == NULL)
        return;
    for(i = 0; i < n; i++) {
        const unsigned char *p = sw->pixels + i * 4;
        dst[i] = (unsigned short)(((unsigned)p[0] >> 3) << 11 |
                                  ((unsigned)p[1] >> 2) << 5 |
                                  ((unsigned)p[2] >> 3));
    }
}

/*
 * The vtable has no user pointer, so each function recovers the KrySw
 * through the currently selected backend identity: KrySwBackend() stashes
 * the instance in a file-static pointer when handed out. One KrySw may be
 * active per process at a time — the same constraint the null backend has.
 */
static KrySw *g_sw;

static int
clip_active(KrySw *sw, int *x, int *y, int *w, int *h)
{
    int i;
    int cx = 0;
    int cy = 0;
    int cw = sw->w;
    int ch = sw->h;

    for(i = 0; i < sw->clip_n; i++) {
        int nx = cx > sw->clip_x[i] ? cx : sw->clip_x[i];
        int ny = cy > sw->clip_y[i] ? cy : sw->clip_y[i];
        int nx2 = (cx + cw) < (sw->clip_x[i] + sw->clip_w[i])
                    ? (cx + cw) : (sw->clip_x[i] + sw->clip_w[i]);
        int ny2 = (cy + ch) < (sw->clip_y[i] + sw->clip_h[i])
                    ? (cy + ch) : (sw->clip_y[i] + sw->clip_h[i]);
        if(nx2 <= nx || ny2 <= ny)
            return 0;
        cx = nx;
        cy = ny;
        cw = nx2 - nx;
        ch = ny2 - ny;
    }
    /* intersect with the target rect */
    {
        int x2 = *x + *w;
        int y2 = *y + *h;
        int nx = cx > *x ? cx : *x;
        int ny = cy > *y ? cy : *y;
        int nx2 = (cx + cw) < x2 ? (cx + cw) : x2;
        int ny2 = (cy + ch) < y2 ? (cy + ch) : y2;
        if(nx2 <= nx || ny2 <= ny)
            return 0;
        *x = nx;
        *y = ny;
        *w = nx2 - nx;
        *h = ny2 - ny;
    }
    return 1;
}

static void
fill_rect(KrySw *sw, int x, int y, int w, int h, unsigned color)
{
    int row;
    unsigned char r = (unsigned char)(color >> 24);
    unsigned char g = (unsigned char)(color >> 16);
    unsigned char b = (unsigned char)(color >> 8);
    unsigned char a = (unsigned char)color;

    if(!clip_active(sw, &x, &y, &w, &h))
        return;
    for(row = y; row < y + h; row++) {
        unsigned char *p = sw->pixels + (size_t)row * sw->stride + x * 4;
        int col;
        for(col = 0; col < w; col++) {
            *p++ = r;
            *p++ = g;
            *p++ = b;
            *p++ = a;
        }
    }
}

static void
b_clear(unsigned color)
{
    if(g_sw == NULL)
        return;
    fill_rect(g_sw, 0, 0, g_sw->w, g_sw->h, color);
}

static void
b_rect(int x, int y, int w, int h, unsigned color)
{
    if(g_sw == NULL)
        return;
    fill_rect(g_sw, x, y, w, h, color);
}

/* Glyph cell: size-tall, size/2-wide per character, nearest-neighbor. */

static unsigned
atlas_rd_u16(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned
atlas_rd_u32(const unsigned char *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) |
           ((unsigned)p[3] << 24);
}

int
KrySwSetAtlas(KrySw *sw, const unsigned char *data, unsigned len)
{
    int i;

    if(sw == NULL || data == NULL || len < 6)
        return -1;
    if(memcmp(data, "KFA1", 4) != 0)
        return -1;
    sw->atlas = data;
    sw->atlas_len = len;
    sw->atlas_sizes = 0;
    {
        unsigned n = atlas_rd_u16(data + 4);

        if(n > 8)
            n = 8;
        for(i = 0; (unsigned)i < n; i++) {
            const unsigned char *rec = data + 6 + (unsigned)i * 16;
            KrySwAtlasSize *as = &sw->size_tab[sw->atlas_sizes++];
            unsigned toff = atlas_rd_u32(rec + 8);
            unsigned poff = atlas_rd_u32(rec + 12);

            as->px = (int)atlas_rd_u16(rec);
            as->glyphs = (int)atlas_rd_u16(rec + 2);
            as->w = (int)atlas_rd_u16(rec + 4);
            as->h = (int)atlas_rd_u16(rec + 6);
            as->table_off = toff;
            as->pixels_off = poff;
            if(toff >= len || poff >= len || as->glyphs == 0) {
                sw->atlas_sizes--;
                continue;
            }
        }
    }
    return sw->atlas_sizes > 0 ? 0 : -1;
}

/* decode one UTF-8 rune; advances *pp */
static unsigned
utf8_next(const char **pp)
{
    const unsigned char *p = (const unsigned char *)*pp;
    unsigned c = *p;

    if(c < 0x80) {
        *pp = (const char *)(p + 1);
        return c;
    }
    if((c & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
        *pp = (const char *)(p + 2);
        return ((c & 0x1f) << 6) | (p[1] & 0x3f);
    }
    if((c & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80) {
        *pp = (const char *)(p + 3);
        return ((c & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    }
    if((c & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
       (p[3] & 0xc0) == 0x80) {
        *pp = (const char *)(p + 4);
        return ((c & 0x07) << 18) | ((p[1] & 0x3f) << 12) |
               ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
    }
    *pp = (const char *)(p + 1);
    return c;
}

static const KrySwAtlasSize *
atlas_size_for(const KrySw *sw, int px)
{
    int i;
    const KrySwAtlasSize *best = NULL;
    int bestd = 1 << 30;

    for(i = 0; i < sw->atlas_sizes; i++) {
        int d = sw->size_tab[i].px - px;

        if(d < 0)
            d = -d;
        if(d < bestd) {
            bestd = d;
            best = &sw->size_tab[i];
        }
    }
    return best;
}

/* glyph record lookup; returns 16-byte record or NULL */
static const unsigned char *
atlas_glyph(const KrySw *sw, const KrySwAtlasSize *as, unsigned cp)
{
    int i;

    for(i = 0; i < as->glyphs; i++) {
        const unsigned char *g = sw->atlas + as->table_off + (unsigned)i * 18;

        if(atlas_rd_u32(g) == cp)
            return g;
    }
    return NULL;
}

static void
atlas_blit(KrySw *sw, const KrySwAtlasSize *as, const unsigned char *g,
           int px, int pen_x, int pen_y, unsigned color)
{
    unsigned gx = atlas_rd_u16(g + 4);
    unsigned gy = atlas_rd_u16(g + 6);
    unsigned gw = atlas_rd_u16(g + 8);
    unsigned gh = atlas_rd_u16(g + 10);
    int xoff = (short)(atlas_rd_u16(g + 12) | (g[13] << 8));
    int yoff = (short)(atlas_rd_u16(g + 14) | (g[15] << 8));
    int scale_num = px;
    int scale_den = as->px > 0 ? as->px : 1;
    int dy;

    for(dy = 0; dy < (int)gh * scale_num / scale_den; dy++) {
        int sy = dy * scale_den / scale_num;
        int dx;
        int rowlen = (int)gw * scale_num / scale_den;

        for(dx = 0; dx < rowlen; dx++) {
            int sx = dx * scale_den / scale_num;
            const unsigned char *sp = sw->atlas + as->pixels_off +
                ((size_t)(gy + sy) * as->w + gx + sx) * 4;

            unsigned cov = sp[3];
            unsigned dr = (color >> 24) & 0xff;
            unsigned dg = (color >> 16) & 0xff;
            unsigned db = (color >> 8) & 0xff;
            unsigned da = color & 0xff;
            int px_ = pen_x + xoff * scale_num / scale_den + dx;
            int py_ = pen_y + yoff * scale_num / scale_den + dy;
            int w1 = 1;
            int h1 = 1;
            unsigned char *dstp;

            if(cov == 0)
                continue;
            if(!clip_active(sw, &px_, &py_, &w1, &h1))
                continue;
            dstp = sw->pixels + (size_t)py_ * sw->stride + px_ * 4;
            if(cov == 255) {
                dstp[0] = (unsigned char)dr;
                dstp[1] = (unsigned char)dg;
                dstp[2] = (unsigned char)db;
                dstp[3] = (unsigned char)da;
            } else {
                /* source-over with glyph coverage as source alpha */
                unsigned inv = 255 - cov * da / 255;

                dstp[0] = (unsigned char)(dr * cov * da / 65025 +
                                         dstp[0] * inv / 255);
                dstp[1] = (unsigned char)(dg * cov * da / 65025 +
                                         dstp[1] * inv / 255);
                dstp[2] = (unsigned char)(db * cov * da / 65025 +
                                         dstp[2] * inv / 255);
                dstp[3] = (unsigned char)(da + dstp[3] * (255 - da) / 255);
            }
        }
    }
}

static void
b_text(const char *s, int x, int y, int size, unsigned color)
{
    KrySw *sw = g_sw;
    const char *c;

    if(sw == NULL || s == NULL || size <= 0)
        return;
    if(sw->atlas != NULL && sw->atlas_sizes > 0) {
        const KrySwAtlasSize *as = atlas_size_for(sw, size);
        int pen = x;

        c = s;
        while(*c != '\0') {
            unsigned cp = utf8_next(&c);
            const unsigned char *g = atlas_glyph(sw, as, cp);

            if(g != NULL) {
                float scale = (float)size / (as->px > 0 ? as->px : 1);

                atlas_blit(sw, as, g, size, pen, y, color);
                /* ui_text.c: cursor_x += (int)(advanceX*scale + 0.5f) */
                pen += (int)((float)(short)atlas_rd_u16(g + 16) * scale + 0.5f);
            }
        }
        return;
    }
    {
        int advance;
        int gw;
        int gh;

        gw = size / 2;
        gh = size;
        advance = gw;
        for(c = s; *c != '\0'; c++) {
            if((unsigned char)*c <= FONT_LAST) {
                const unsigned char *glyph = k_font8x8[(unsigned char)*c];
                int gx;
                int gy;
                for(gy = 0; gy < gh; gy++) {
                    unsigned char bits = glyph[gy * 8 / gh];
                    for(gx = 0; gx < gw; gx++) {
                        if((bits >> (gx * 8 / gw)) & 1)
                            fill_rect(sw, x + (int)(c - s) * advance + gx,
                                      y + gy, 1, 1, color);
                    }
                }
            }
        }
    }
}

static int
b_measure_text(const char *s, int size)
{
    KrySw *sw = g_sw;
    size_t n = 0;

    if(s != NULL) {
        while(s[n] != '\0')
            n++;
    }
    if(sw != NULL && sw->atlas != NULL && sw->atlas_sizes > 0 && s != NULL) {
        const KrySwAtlasSize *as = atlas_size_for(sw, size);
        const char *c = s;
        int width = 0;

        while(*c != '\0') {
            unsigned cp = utf8_next(&c);
            const unsigned char *g = atlas_glyph(sw, as, cp);

            if(g != NULL) {
                float scale = (float)size / (as->px > 0 ? as->px : 1);

                width += (int)((float)(short)atlas_rd_u16(g + 16) * scale + 0.5f);
            }
        }
        return width;
    }
    return (int)n * (size > 0 ? size / 2 : 4);
}

static void
b_clip_push(int x, int y, int w, int h)
{
    KrySw *sw = g_sw;

    if(sw == NULL || sw->clip_n >= 16)
        return;
    sw->clip_x[sw->clip_n] = x;
    sw->clip_y[sw->clip_n] = y;
    sw->clip_w[sw->clip_n] = w;
    sw->clip_h[sw->clip_n] = h;
    sw->clip_n++;
}

static void
b_clip_pop(void)
{
    if(g_sw == NULL || g_sw->clip_n == 0)
        return;
    g_sw->clip_n--;
}

static void
b_mouse(int *x, int *y)
{
    if(g_sw == NULL)
        return;
    if(x != NULL)
        *x = g_sw->mouse_x;
    if(y != NULL)
        *y = g_sw->mouse_y;
}

static int
b_mouse_down(int button)
{
    if(g_sw == NULL || button < 0 || button >= 8)
        return 0;
    return (g_sw->buttons_down >> button) & 1;
}

static int
b_mouse_pressed(int button)
{
    if(g_sw == NULL || button < 0 || button >= 8)
        return 0;
    return (g_sw->buttons_pressed >> button) & 1;
}

static unsigned
b_text_key(void)
{
    if(g_sw == NULL || g_sw->keys_n == 0)
        return 0;
    {
        unsigned cp = g_sw->keys[0];
        int i;

        for(i = 1; i < g_sw->keys_n; i++)
            g_sw->keys[i - 1] = g_sw->keys[i];
        g_sw->keys_n--;
        return cp;
    }
}

static int
b_wheel(void)
{
    if(g_sw == NULL)
        return 0;
    {
        int d = g_sw->wheel;

        g_sw->wheel = 0;
        return d;
    }
}

static int
b_width(void)
{
    return g_sw != NULL ? g_sw->w : 0;
}

static int
b_height(void)
{
    return g_sw != NULL ? g_sw->h : 0;
}

static float
b_time(void)
{
    return g_sw != NULL ? g_sw->now : 0.0f;
}

static int
b_scale_px(int px)
{
    if(g_sw == NULL)
        return px;
    return (int)((float)px * (float)g_sw->scale / 1000.0f + 0.5f);
}

static unsigned
b_theme_color(int slot)
{
    if(g_sw == NULL || slot < 0 || slot >= KRY_THEME_COUNT)
        return 0x808080ffu;
    return g_sw->theme[slot];
}

static void
b_circle(int cx, int cy, int r, unsigned color)
{
    KrySw *sw = g_sw;
    int dy;

    if(sw == NULL || r <= 0)
        return;
    for(dy = -r; dy <= r; dy++) {
        int dx = (int)(r * 1.0f);
        int row = cy + dy;
        int half = (int)sqrtf((float)(r * r - dy * dy));

        (void)dx;
        fill_rect(sw, cx - half, row, half * 2 + 1, 1, color);
    }
}

static void
b_ring(int cx, int cy, int inner, int outer, unsigned color)
{
    KrySw *sw = g_sw;
    int dy;

    if(sw == NULL || outer <= 0)
        return;
    if(inner <= 0) {
        b_circle(cx, cy, outer, color);
        return;
    }
    for(dy = -outer; dy <= outer; dy++) {
        int row = cy + dy;
        int dy2 = dy < 0 ? -dy : dy;
        int o = (int)sqrtf((float)(outer * outer - dy2 * dy2));
        int i = dy2 < inner ? (int)sqrtf((float)(inner * inner - dy2 * dy2)) : -1;

        if(i < 0)
            fill_rect(sw, cx - o, row, o * 2 + 1, 1, color);
        else {
            fill_rect(sw, cx - o, row, o - i + 1, 1, color);
            fill_rect(sw, cx + i, row, o - i + 1, 1, color);
        }
    }
}

static void
b_texture_rgba(const unsigned char *rgba, int sw, int sh, int x, int y,
               int dw, int dh, unsigned tint)
{
    KrySw *s = g_sw;
    int tr = (int)(tint >> 24);
    int tg = (int)((tint >> 16) & 0xff);
    int tb = (int)((tint >> 8) & 0xff);
    int ta = (int)(tint & 0xff);
    int dx;
    int dy;

    if(s == NULL || rgba == NULL || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;
    for(dy = 0; dy < dh; dy++) {
        int sy = dy * sh / dh;

        for(dx = 0; dx < dw; dx++) {
            int sx = dx * sw / dw;
            const unsigned char *src = rgba + ((size_t)sy * sw + sx) * 4;
            unsigned out = ((unsigned)((src[0] * tr) / 255) << 24) |
                           ((unsigned)((src[1] * tg) / 255) << 16) |
                           ((unsigned)((src[2] * tb) / 255) << 8) |
                           (unsigned)((src[3] * ta) / 255);

            fill_rect(s, x + dx, y + dy, 1, 1, out);
        }
    }
}

static void
b_texture(const char *asset_path, int x, int y, int w, int h,
          unsigned tint, int fit)
{
    /* No asset decoder in the rasterizer yet: draw a tinted placeholder
     * so layout and size are visible in headless previews. */
    (void)asset_path;
    (void)fit;
    if(g_sw == NULL)
        return;
    fill_rect(g_sw, x, y, w, h, tint);
}

const KryBackend *
KrySwBackend(KrySw *sw)
{
    if(sw == NULL)
        return &KryBackendNull;
    g_sw = sw;
    sw->backend.clear = b_clear;
    sw->backend.rect = b_rect;
    sw->backend.text = b_text;
    sw->backend.measure_text = b_measure_text;
    sw->backend.clip_push = b_clip_push;
    sw->backend.clip_pop = b_clip_pop;
    sw->backend.mouse = b_mouse;
    sw->backend.mouse_down = b_mouse_down;
    sw->backend.mouse_pressed = b_mouse_pressed;
    sw->backend.width = b_width;
    sw->backend.height = b_height;
    sw->backend.time = b_time;
    sw->backend.scale_px = b_scale_px;
    sw->backend.theme_color = b_theme_color;
    sw->backend.texture = b_texture;
    sw->backend.circle = b_circle;
    sw->backend.ring = b_ring;
    sw->backend.texture_rgba = b_texture_rgba;
    sw->backend.wheel = b_wheel;
    sw->backend.text_key = b_text_key;
    return &sw->backend;
}
