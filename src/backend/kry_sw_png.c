/* kry_sw_png — dependency-free PNG decoder for the software rasterizer.
 *
 * Decodes 8-bit non-interlaced PNGs (grayscale, grayscale+alpha, RGB,
 * palette, RGBA) to tightly packed RGBA8. Chunk CRCs and the zlib adler32
 * are not verified; anything malformed or unsupported yields NULL instead
 * of an error, matching the rasterizer's best-effort drawing contract. The
 * inflate implementation follows the classic puff structure (zlib's
 * reference decoder outline) over stored, fixed, and dynamic blocks. */

#include "kry_sw_png.h"

#include <stdlib.h>
#include <string.h>

/* ---- inflate ---- */

typedef struct {
    const unsigned char *in;
    size_t in_len;
    size_t in_pos;
    unsigned bitbuf;
    int bitcnt;
} SwBits;

typedef struct {
    short count[16];
    short symbol[288];
} SwHuff;

static int
sw_bits(SwBits *z, int need)
{
    unsigned val = z->bitbuf;

    while(z->bitcnt < need) {
        if(z->in_pos >= z->in_len)
            return -1;
        val |= (unsigned)z->in[z->in_pos++] << z->bitcnt;
        z->bitcnt += 8;
    }
    z->bitbuf = val >> need;
    z->bitcnt -= need;
    return (int)(val & ((1u << need) - 1));
}

/* Canonical Huffman decode: walk lengths from short to long, consuming one
 * bit per level. Returns the symbol or -1 on a malformed code. */
static int
sw_decode(SwBits *z, const SwHuff *h)
{
    int code = 0, first = 0, index = 0, len;

    for(len = 1; len <= 15; len++) {
        int bit = sw_bits(z, 1);

        if(bit < 0)
            return -1;
        code |= bit;
        if(code - first < h->count[len])
            return h->symbol[index + (code - first)];
        index += h->count[len];
        first += h->count[len];
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* Build a decode table from code lengths. Returns negative when the set is
 * over-subscribed; a positive leftover (incomplete set) is tolerated — a
 * stream that uses a missing code simply fails in sw_decode. */
static int
sw_build(SwHuff *h, const short *length, int n)
{
    short offs[16];
    int symbol, len, left;

    for(len = 0; len < 16; len++)
        h->count[len] = 0;
    for(symbol = 0; symbol < n; symbol++)
        h->count[length[symbol]]++;
    if(h->count[0] == n)
        return 0;
    left = 1;
    for(len = 1; len <= 15; len++) {
        left <<= 1;
        left -= h->count[len];
        if(left < 0)
            return left;
    }
    offs[1] = 0;
    for(len = 1; len < 15; len++)
        offs[len + 1] = offs[len] + h->count[len];
    for(symbol = 0; symbol < n; symbol++)
        if(length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = (short)symbol;
    return left;
}

static const short sw_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,
    59,67,83,99,115,131,163,195,227,258
};
static const short sw_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const short sw_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,
    513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const short sw_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

static int
sw_inflate_codes(SwBits *z, const SwHuff *lencode, const SwHuff *distcode,
                 unsigned char *out, size_t out_len, size_t *out_pos)
{
    for(;;) {
        int sym = sw_decode(z, lencode);

        if(sym < 0)
            return -1;
        if(sym < 256) {
            if(*out_pos >= out_len)
                return -1;
            out[(*out_pos)++] = (unsigned char)sym;
        } else if(sym == 256) {
            return 0;
        } else {
            int len, dist, extra, i;

            sym -= 257;
            if(sym >= 29)
                return -1;
            extra = sw_bits(z, sw_len_extra[sym]);
            if(extra < 0)
                return -1;
            len = sw_len_base[sym] + extra;
            sym = sw_decode(z, distcode);
            if(sym < 0 || sym >= 30)
                return -1;
            extra = sw_bits(z, sw_dist_extra[sym]);
            if(extra < 0)
                return -1;
            dist = sw_dist_base[sym] + extra;
            if((size_t)dist > *out_pos)
                return -1;
            for(i = 0; i < len; i++) {
                if(*out_pos >= out_len)
                    return -1;
                out[*out_pos] = out[*out_pos - dist];
                (*out_pos)++;
            }
        }
    }
}

static void
sw_fixed_tables(SwHuff *lencode, SwHuff *distcode)
{
    short lengths[288];
    int i;

    for(i = 0; i < 144; i++)
        lengths[i] = 8;
    for(; i < 256; i++)
        lengths[i] = 9;
    for(; i < 280; i++)
        lengths[i] = 7;
    for(; i < 288; i++)
        lengths[i] = 8;
    (void)sw_build(lencode, lengths, 288);
    for(i = 0; i < 30; i++)
        lengths[i] = 5;
    (void)sw_build(distcode, lengths, 30);
}

static int
sw_dynamic_tables(SwBits *z, SwHuff *lencode, SwHuff *distcode)
{
    static const short order[19] = {
        16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
    };
    short lengths[288 + 30];
    SwHuff clencode;
    int nlen, ndist, ncode, index;

    nlen = sw_bits(z, 5);
    ndist = sw_bits(z, 5);
    ncode = sw_bits(z, 4);
    if(nlen < 0 || ndist < 0 || ncode < 0)
        return -1;
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if(nlen > 286 || ndist > 30)
        return -1;
    for(index = 0; index < ncode; index++) {
        int bits = sw_bits(z, 3);

        if(bits < 0)
            return -1;
        lengths[order[index]] = (short)bits;
    }
    for(; index < 19; index++)
        lengths[order[index]] = 0;
    if(sw_build(&clencode, lengths, 19) != 0)
        return -1;
    index = 0;
    while(index < nlen + ndist) {
        int sym = sw_decode(z, &clencode);

        if(sym < 0)
            return -1;
        if(sym < 16) {
            lengths[index++] = (short)sym;
        } else {
            int rep, val = 0;

            if(sym == 16) {
                int extra;

                if(index == 0)
                    return -1;
                val = lengths[index - 1];
                extra = sw_bits(z, 2);
                if(extra < 0)
                    return -1;
                rep = 3 + extra;
            } else if(sym == 17) {
                int extra = sw_bits(z, 3);

                if(extra < 0)
                    return -1;
                rep = 3 + extra;
            } else {
                int extra = sw_bits(z, 7);

                if(extra < 0)
                    return -1;
                rep = 11 + extra;
            }
            if(index + rep > nlen + ndist)
                return -1;
            while(rep-- > 0)
                lengths[index++] = (short)val;
        }
    }
    if(lengths[256] == 0)
        return -1;
    if(sw_build(lencode, lengths, nlen) < 0)
        return -1;
    if(sw_build(distcode, lengths + nlen, ndist) < 0)
        return -1;
    return 0;
}

/* Inflate a zlib stream (2-byte header, deflate data, 4-byte adler). The
 * output size is known from the PNG header; writes beyond it are rejected. */
static unsigned char *
sw_inflate(const unsigned char *in, size_t in_len, size_t out_len)
{
    SwBits z;
    unsigned char *out;
    size_t out_pos = 0;
    int last, type;

    if(in_len < 6 || (in[0] & 0x0f) != 8 || ((in[1] >> 5) & 1) != 0)
        return NULL;
    out = (unsigned char *)malloc(out_len);
    if(out == NULL)
        return NULL;
    z.in = in + 2;
    z.in_len = in_len - 2;
    z.in_pos = 0;
    z.bitbuf = 0;
    z.bitcnt = 0;
    do {
        SwHuff lencode, distcode;

        last = sw_bits(&z, 1);
        type = sw_bits(&z, 2);
        if(last < 0 || type < 0)
            goto fail;
        if(type == 0) {
            unsigned len, nlen;
            size_t copy;

            z.bitbuf = 0;
            z.bitcnt = 0;
            if(z.in_pos + 4 > z.in_len)
                goto fail;
            len = (unsigned)z.in[z.in_pos] | ((unsigned)z.in[z.in_pos + 1] << 8);
            nlen = (unsigned)z.in[z.in_pos + 2] | ((unsigned)z.in[z.in_pos + 3] << 8);
            z.in_pos += 4;
            if(len != (~nlen & 0xffff))
                goto fail;
            copy = len;
            if(copy > z.in_len - z.in_pos || copy > out_len - out_pos)
                goto fail;
            memcpy(out + out_pos, z.in + z.in_pos, copy);
            z.in_pos += copy;
            out_pos += copy;
        } else if(type == 1) {
            sw_fixed_tables(&lencode, &distcode);
            if(sw_inflate_codes(&z, &lencode, &distcode, out, out_len,
                                &out_pos) != 0)
                goto fail;
        } else if(type == 2) {
            if(sw_dynamic_tables(&z, &lencode, &distcode) != 0 ||
               sw_inflate_codes(&z, &lencode, &distcode, out, out_len,
                                &out_pos) != 0)
                goto fail;
        } else {
            goto fail;
        }
    } while(last == 0);
    if(out_pos != out_len)
        goto fail;
    return out;
fail:
    free(out);
    return NULL;
}

/* ---- PNG ---- */

static const unsigned char sw_png_sig[8] = {
    0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'
};

static int
sw_paeth(int a, int b, int c)
{
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;

    if(pa <= pb && pa <= pc)
        return a;
    if(pb <= pc)
        return b;
    return c;
}

static unsigned
sw_be32(const unsigned char *p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | (unsigned)p[3];
}

static int
sw_png_packed_index(const unsigned char *row, int x, int depth)
{
    int bit = x * depth;
    int shift = 8 - depth - (bit & 7);
    int mask = (1 << depth) - 1;

    return (row[bit >> 3] >> shift) & mask;
}

unsigned char *
kry_sw_png_rgba(const unsigned char *data, size_t len, int *w_out, int *h_out)
{
    static const signed char channels_of[8] = {-1, -1, 3, 1, 2, -1, 4, -1};
    unsigned char palette[256 * 3];
    unsigned char trns[256];
    unsigned char *idat, *raw, *rgba;
    size_t idat_len, idat_cap, pos, expect, row_bytes;
    int palette_len, trns_len, w, h, depth, color, channels, bpp, x, y;

    if(data == NULL || len < 8 || w_out == NULL || h_out == NULL)
        return NULL;
    if(memcmp(data, sw_png_sig, 8) != 0)
        return NULL;

    palette_len = 0;
    trns_len = 0;
    idat = NULL;
    idat_len = 0;
    idat_cap = 0;
    w = h = depth = color = 0;
    pos = 8;
    while(pos + 8 <= len) {
        unsigned clen = sw_be32(data + pos);
        const unsigned char *type = data + pos + 4;
        const unsigned char *body = data + pos + 8;

        if(clen > len - pos - 12)
            return NULL; /* chunk (plus CRC) runs past the file */
        if(memcmp(type, "IHDR", 4) == 0 && clen == 13) {
            w = (int)sw_be32(body);
            h = (int)sw_be32(body + 4);
            depth = body[8];
            color = body[9];
            if(body[12] != 0)
                return NULL; /* Adam7 interlacing unsupported */
        } else if(memcmp(type, "PLTE", 4) == 0) {
            palette_len = (int)clen / 3;
            if(palette_len > 256 || clen % 3 != 0 ||
               clen > (unsigned)palette_len * 3)
                return NULL;
            memcpy(palette, body, (size_t)palette_len * 3);
        } else if(memcmp(type, "tRNS", 4) == 0) {
            trns_len = clen > 256 ? 256 : (int)clen;
            memcpy(trns, body, (size_t)trns_len);
        } else if(memcmp(type, "IDAT", 4) == 0) {
            if(idat_len + clen > idat_cap) {
                unsigned char *grown;

                idat_cap = (idat_len + clen) * 2 + 1024;
                grown = (unsigned char *)realloc(idat, idat_cap);
                if(grown == NULL) {
                    free(idat);
                    return NULL;
                }
                idat = grown;
            }
            memcpy(idat + idat_len, body, clen);
            idat_len += clen;
        } else if(memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + clen;
    }
    if(w <= 0 || h <= 0 || w > 8192 || h > 8192)
        return NULL;
    if(color < 0 || color > 7 || channels_of[color] <= 0)
        return NULL;
    if(color == 3) {
        if(depth != 1 && depth != 2 && depth != 4 && depth != 8)
            return NULL;
    } else if(depth != 8) {
        return NULL;
    }
    if(color == 3 && palette_len == 0)
        return NULL;
    if(idat == NULL)
        return NULL;
    channels = channels_of[color];
    row_bytes = color == 3 ? (((size_t)w * (size_t)depth + 7u) >> 3)
                           : (size_t)w * (size_t)channels;
    bpp = color == 3 && depth < 8 ? 1 : channels;
    expect = (size_t)h * (row_bytes + 1);
    if(expect > (size_t)1 << 27)
        return NULL;
    raw = sw_inflate(idat, idat_len, expect);
    free(idat);
    if(raw == NULL)
        return NULL;

    /* Reverse per-scanline filters in place, then expand to RGBA8. */
    for(y = 0; y < h; y++) {
        unsigned char *row = raw + (size_t)y * (row_bytes + 1);
        unsigned char *prior = y > 0
            ? raw + (size_t)(y - 1) * (row_bytes + 1) + 1
            : NULL;
        int filter = row[0];

        row++;
        for(x = 0; x < (int)row_bytes; x++) {
            int left = x >= bpp ? row[x - bpp] : 0;
            int up = prior != NULL ? prior[x] : 0;
            int ul = prior != NULL && x >= bpp ? prior[x - bpp] : 0;

            switch(filter) {
            case 0:
                break;
            case 1:
                row[x] = (unsigned char)(row[x] + left);
                break;
            case 2:
                row[x] = (unsigned char)(row[x] + up);
                break;
            case 3:
                row[x] = (unsigned char)(row[x] + ((left + up) >> 1));
                break;
            case 4:
                row[x] = (unsigned char)(row[x] + sw_paeth(left, up, ul));
                break;
            default:
                free(raw);
                return NULL;
            }
        }
    }

    rgba = (unsigned char *)malloc((size_t)w * h * 4);
    if(rgba == NULL) {
        free(raw);
        return NULL;
    }
    for(y = 0; y < h; y++) {
        const unsigned char *row = raw + (size_t)y * (row_bytes + 1) + 1;

        for(x = 0; x < w; x++) {
            unsigned char *dst = rgba + ((size_t)y * w + x) * 4;

            switch(color) {
            case 0:
                dst[0] = dst[1] = dst[2] = row[x];
                dst[3] = 0xff;
                break;
            case 4:
                dst[0] = dst[1] = dst[2] = row[x * 2];
                dst[3] = row[x * 2 + 1];
                break;
            case 2:
                dst[0] = row[x * 3];
                dst[1] = row[x * 3 + 1];
                dst[2] = row[x * 3 + 2];
                dst[3] = 0xff;
                break;
            case 6:
                dst[0] = row[x * 4];
                dst[1] = row[x * 4 + 1];
                dst[2] = row[x * 4 + 2];
                dst[3] = row[x * 4 + 3];
                break;
            default: /* palette */
                {
                    int idx = depth == 8 ? row[x]
                                         : sw_png_packed_index(row, x, depth);

                    if(idx >= palette_len) {
                        free(raw);
                        free(rgba);
                        return NULL;
                    }
                    dst[0] = palette[idx * 3];
                    dst[1] = palette[idx * 3 + 1];
                    dst[2] = palette[idx * 3 + 2];
                    dst[3] = idx < trns_len ? trns[idx] : 0xff;
                }
                break;
            }
        }
    }
    free(raw);
    *w_out = w;
    *h_out = h;
    return rgba;
}
