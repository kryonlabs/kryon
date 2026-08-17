#include "k2b_atlas.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../vendor/raylib/src/external/stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_W 512

/* serialized glyph record (18 bytes, field order fixed):
 * cp u32 | x u16 | y u16 | w u16 | h u16 | xoff i16 | yoff i16 | adv u16 */

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

    /* header: magic u32, size_count u16, then size_count 16-byte records:
     * px u16 | glyphs u16 | w u16 | h u16 | table_off u32 | pixels_off u32 */
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
        unsigned char *rec;
        int px = sizes[s];
        stbtt_pack_context pc;
        stbtt_packedchar *chars;
        unsigned rec_bytes = (unsigned)cp_count * 18;
        unsigned table_off = 0;
        unsigned pixels_off = 0;
        int height = 128 + px * 4;
        unsigned char *pixbuf = malloc((size_t)ATLAS_W * height);
        int i;

        if(pixbuf == NULL)
            continue;
        memset(pixbuf, 0, (size_t)ATLAS_W * height);

        if(stbtt_PackBegin(&pc, pixbuf, ATLAS_W, height, 0, 1, NULL) == 0) {
            free(pixbuf);
            continue;
        }
        stbtt_PackSetSkipMissingCodepoints(&pc, 1);
        chars = calloc((size_t)cp_count, sizeof(*chars));
        if(chars != NULL) {
            for(i = 0; i < cp_count; i++)
                stbtt_PackFontRange(&pc, ttf, 0, (float)px,
                                    (int)codepoints[i], 1, &chars[i]);
        }
        /* grow the output buffer for this size's table + pixels */
        while(buf_len + rec_bytes + (unsigned)ATLAS_W * pc.height * 4 >
              buf_cap) {
            unsigned char *nb = realloc(buf, buf_cap * 2);

            if(nb == NULL)
                break;
            buf = nb;
            buf_cap *= 2;
        }
        if(chars != NULL &&
           buf_len + rec_bytes + (unsigned)ATLAS_W * pc.height * 4 <=
           buf_cap) {
            unsigned char *tab;

            table_off = buf_len;
            tab = buf + table_off;
            for(i = 0; i < cp_count; i++) {
                stbtt_packedchar *c = &chars[i];
                unsigned char *g = tab + (unsigned)i * 18;

                put_u32(g, codepoints[i]);
                put_u16(g + 4, (unsigned short)c->x0);
                put_u16(g + 6, (unsigned short)c->y0);
                put_u16(g + 8, (unsigned short)(c->x1 - c->x0));
                put_u16(g + 10, (unsigned short)(c->y1 - c->y0));
                put_u16(g + 12, (unsigned short)(short)c->xoff);
                put_u16(g + 14, (unsigned short)(short)c->yoff);
                put_u16(g + 16, (unsigned short)
                       (c->xadvance > 0 ? c->xadvance : px / 2));
            }
            buf_len += rec_bytes;
            pixels_off = buf_len;
            {
                unsigned char *rgba = buf + pixels_off;
                long n = (long)ATLAS_W * pc.height;
                long k;

                for(k = 0; k < n; k++) {
                    rgba[k * 4 + 0] = 0xff;
                    rgba[k * 4 + 1] = 0xff;
                    rgba[k * 4 + 2] = 0xff;
                    rgba[k * 4 + 3] = pc.pixels[k];
                }
                buf_len += (unsigned)n * 4;
            }
            rec = buf + 6 + (unsigned)s * 16; /* buf may have been realloc'd */
            put_u16(rec, (unsigned)px);
            put_u16(rec + 2, (unsigned)cp_count);
            put_u16(rec + 4, ATLAS_W);
            put_u16(rec + 6, (unsigned)pc.height);
            put_u32(rec + 8, table_off);
            put_u32(rec + 12, pixels_off);
        }
        free(chars);
        stbtt_PackEnd(&pc);
        free(pixbuf);
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
