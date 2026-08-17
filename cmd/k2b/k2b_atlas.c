#include "k2b_atlas.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../vendor/raylib/src/external/stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_W 512

static void
put_u16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void
put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* Layout: "KFA1" u32 | size_count u16 | per-size 16-byte records
 * (px u16 | glyphs u16 | w u16 | h u16 | table_off u32 | pixels_off u32),
 * per-glyph 18-byte records (cp u32 | x,y,w,h u16 | xoff,yoff i16 |
 * adv u16), then per-size RGBA8 white-on-alpha bitmaps.
 *
 * Rasterization is raylib-identical: per-glyph
 * stbtt_GetCodepointBitmap at stbtt_ScaleForPixelHeight(size) with
 * advances from stbtt_GetCodepointHMetrics * scale — the same calls
 * rtext.c LoadFontData(FONT_DEFAULT) makes, so glyph coverage matches
 * the native renderer bit-for-bit. Glyphs row-pack first-fit. */
unsigned char *
k2b_bake_atlas(const char *ttf_path, const unsigned int *codepoints,
               int cp_count, const int *sizes, int size_count,
               unsigned *out_len)
{
    unsigned char *ttf = NULL;
    long ttf_len;
    FILE *f;
    stbtt_fontinfo font;
    unsigned char *buf = NULL;
    unsigned buf_cap;
    unsigned buf_len;
    int s;

    if(out_len != NULL)
        *out_len = 0;
    if(ttf_path == NULL || codepoints == NULL || cp_count <= 0 ||
       sizes == NULL || size_count <= 0)
        return NULL;
    f = fopen(ttf_path, "rb");
    if(f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    ttf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    ttf = malloc((size_t)ttf_len);
    if(ttf == NULL || fread(ttf, 1, (size_t)ttf_len, f) != (size_t)ttf_len) {
        fclose(f);
        free(ttf);
        return NULL;
    }
    fclose(f);
    if(!stbtt_InitFont(&font, ttf, 0)) {
        free(ttf);
        return NULL;
    }

    buf_len = 6 + (unsigned)size_count * 16;
    buf_cap = buf_len + 4096;
    buf = malloc(buf_cap);
    if(buf == NULL) {
        free(ttf);
        return NULL;
    }
    put_u32(buf, 0x3141464Bu); /* "KFA1" */
    put_u16(buf + 4, (unsigned)size_count);

    for(s = 0; s < size_count; s++) {
        int px = sizes[s];
        float scale = stbtt_ScaleForPixelHeight(&font, (float)px);
        int ascent = 0;
        int descent = 0;
        int line_gap = 0;

        /* rtext.c line 730: offsetY += (int)(ascent*scale) — raylib's
         * glyph offset is line-top-relative, not baseline-relative */
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
        unsigned char *raw_px;
        unsigned rec_bytes = (unsigned)cp_count * 18;
        unsigned table_off;
        unsigned pixels_off;
        int height;
        int i;

        /* rec is recomputed after every realloc of buf */
        {
            int total_h = 1;
            int row_h = 0;
            int cur_x = 0;

            for(i = 0; i < cp_count; i++) {
                int x0 = 0;
                int y0 = 0;
                int x1 = 0;
                int y1 = 0;
                int w;
                int h;

                stbtt_GetCodepointBitmapBox(&font, (int)codepoints[i], scale,
                                            scale, &x0, &y0, &x1, &y1);
                w = x1 - x0;
                h = y1 - y0;
                if(w < 0)
                    w = 0;
                if(h < 0)
                    h = 0;
                if(cur_x + w > ATLAS_W) {
                    total_h += row_h > 0 ? row_h : 1;
                    row_h = 0;
                    cur_x = 0;
                }
                cur_x += w + 1;
                if(h > row_h)
                    row_h = h;
            }
            height = total_h + row_h + 1;
        }
        while(buf_len + rec_bytes + (unsigned)ATLAS_W * height * 4 >
              buf_cap) {
            unsigned char *nb = realloc(buf, buf_cap * 2);

            if(nb == NULL)
                break;
            buf = nb;
            buf_cap *= 2;
        }
        if(buf_len + rec_bytes + (unsigned)ATLAS_W * height * 4 > buf_cap)
            continue;
        raw_px = calloc((size_t)ATLAS_W * height, 1);
        if(raw_px == NULL)
            continue;
        table_off = buf_len;
        {
            int row_y = 0;
            int row_h = 0;
            int cur_x = 0;
            unsigned char *tab = buf + table_off;

            for(i = 0; i < cp_count; i++) {
                int w = 0;
                int h = 0;
                int xoff = 0;
                int yoff = 0;
                int advance = 0;
                int lsb = 0;
                unsigned char *bm;
                unsigned char *g = tab + (unsigned)i * 18;

                bm = stbtt_GetCodepointBitmap(&font, scale, scale,
                                              (int)codepoints[i], &w, &h,
                                              &xoff, &yoff);
                if(cur_x + w > ATLAS_W) {
                    row_y += row_h > 0 ? row_h : 1;
                    row_h = 0;
                    cur_x = 0;
                }
                if(w > 0 && h > 0 && bm != NULL && w <= ATLAS_W &&
                   row_y + h <= height && cur_x + w <= ATLAS_W) {
                    int gy;

                    for(gy = 0; gy < h; gy++)
                        memcpy(raw_px + (size_t)(row_y + gy) * ATLAS_W +
                               cur_x, bm + (size_t)gy * w, (size_t)w);
                }
                stbtt_FreeBitmap(bm, NULL);
                stbtt_GetCodepointHMetrics(&font, (int)codepoints[i],
                                           &advance, &lsb);
                put_u32(g, codepoints[i]);
                put_u16(g + 4, (unsigned short)cur_x);
                put_u16(g + 6, (unsigned short)row_y);
                put_u16(g + 8, (unsigned short)(w > 0 ? w : 0));
                put_u16(g + 10, (unsigned short)(h > 0 ? h : 0));
                put_u16(g + 12, (unsigned short)(short)xoff);
                put_u16(g + 14,
                        (unsigned short)(short)(yoff +
                                                (int)((float)ascent * scale)));
                put_u16(g + 16, (unsigned short)(int)(advance * scale));
                cur_x += w + 1;
                if(h > row_h)
                    row_h = h;
            }
            buf_len += rec_bytes;
        }
        pixels_off = buf_len;
        {
            unsigned char *rgba = buf + pixels_off;
            long n = (long)ATLAS_W * height;
            long k;

            for(k = 0; k < n; k++) {
                rgba[k * 4 + 0] = 0xff;
                rgba[k * 4 + 1] = 0xff;
                rgba[k * 4 + 2] = 0xff;
                rgba[k * 4 + 3] = raw_px[k];
            }
            buf_len += (unsigned)n * 4;
        }
        free(raw_px);
        {
            unsigned char *rec = buf + 6 + (unsigned)s * 16;

            put_u16(rec, (unsigned)px);
        put_u16(rec + 2, (unsigned)cp_count);
        put_u16(rec + 4, ATLAS_W);
        put_u16(rec + 6, (unsigned)height);
            put_u16(rec + 2, (unsigned)cp_count);
            put_u16(rec + 4, ATLAS_W);
            put_u16(rec + 6, (unsigned)height);
            put_u32(rec + 8, table_off);
            put_u32(rec + 12, pixels_off);
        }
    }
    free(ttf);
    if(buf_len <= 6 + (unsigned)size_count * 16) {
        free(buf);
        return NULL;
    }
    if(out_len != NULL)
        *out_len = buf_len;
    return buf;
}
