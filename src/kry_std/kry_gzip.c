/*
 * kry_gzip.c - gzip (RFC 1952) and zlib (RFC 1950) container support for
 * the Kry standard library.
 *
 * Inflate is the classic bit-reader + Huffman walker (zlib-style, rewritten
 * from RFC 1951): stored, fixed and dynamic blocks, no length-delimited
 * recursion. Deflate output uses stored blocks only, which keeps the writer
 * tiny and lossless. gzip verifies CRC-32 and ISIZE; zlib verifies Adler-32
 * so corrupt input is rejected instead of silently truncated.
 */
#include "kry_gzip.h"
#include "kry_zlib.h"

#include <stdlib.h>
#include <string.h>

#define WINDOW_SIZE 32768u
#define GZIP_STORED_MAX 65535u

/* --- CRC-32 -------------------------------------------------------------- */

static unsigned long
crc32_table(unsigned int index)
{
    static unsigned long table[256];
    static int ready;
    unsigned long c;
    unsigned int i;

    if(!ready) {
        for(i = 0; i < 256; i++) {
            unsigned int k;

            c = (unsigned long)i;
            for(k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320ul ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = 1;
    }
    return table[index];
}

unsigned long
kry_gzip_crc32(const unsigned char *data, unsigned long len)
{
    unsigned long crc = 0xFFFFFFFFul;
    unsigned long i;

    for(i = 0; i < len; i++)
        crc = crc32_table((unsigned int)(crc ^ data[i]) & 0xFFu) ^
              (crc >> 8);
    return crc ^ 0xFFFFFFFFul;
}

/* --- bit reader ---------------------------------------------------------- */

typedef struct {
    const unsigned char *data;
    unsigned long len;
    unsigned long pos;  /* byte cursor */
    unsigned long bit;  /* bits consumed from current byte */
    int bad;
} BitReader;

static unsigned
read_bits(BitReader *r, int count)
{
    unsigned value = 0;
    int i;

    for(i = 0; i < count; i++) {
        if(r->pos >= r->len) {
            r->bad = 1;
            return value;
        }
        value |= (unsigned)((r->data[r->pos] >> r->bit) & 1u) << i;
        if(++r->bit == 8) {
            r->bit = 0;
            r->pos++;
        }
    }
    return value;
}

static void
align_byte(BitReader *r)
{
    if(r->bit != 0) {
        r->bit = 0;
        r->pos++;
    }
}

/* --- output window -------------------------------------------------------- */

typedef struct {
    unsigned char *buf;
    unsigned long len;
    unsigned long cap;
    int bad;
} Window;

static int
window_push(Window *w, unsigned char byte)
{
    if(w->len + 1 > w->cap) {
        unsigned long ncap = w->cap != 0 ? w->cap * 2 : 4096;
        unsigned char *nbuf;

        while(ncap < w->len + 1)
            ncap *= 2;
        nbuf = realloc(w->buf, ncap);
        if(nbuf == NULL) {
            w->bad = 1;
            return 0;
        }
        w->buf = nbuf;
        w->cap = ncap;
    }
    w->buf[w->len++] = byte;
    return 1;
}

static unsigned char
window_at(Window *w, unsigned long back)
{
    return w->buf[w->len - back];
}

/* --- Huffman -------------------------------------------------------------- */

typedef struct {
    unsigned short counts[16];
    unsigned short symbols[288];
} Huff;

/* Build a decode table. allow_incomplete permits empty and incomplete code
 * sets (legal for distance trees whose symbols never appear); complete sets
 * are required otherwise. Returns 0 only when the lengths are unusable. */
static int
huff_build(Huff *h, const unsigned char *lengths, int n,
           int allow_incomplete)
{
    int i;
    int left;
    unsigned short offs[16];
    unsigned symbol;

    for(i = 0; i < 16; i++)
        h->counts[i] = 0;
    for(i = 0; i < n; i++)
        h->counts[lengths[i]]++;
    if(h->counts[0] == n)
        return allow_incomplete; /* no codes at all */
    left = 1;
    for(i = 1; i < 16; i++) {
        left <<= 1;
        left -= h->counts[i];
        if(left < 0)
            return 0; /* over-subscribed */
    }
    offs[1] = 0;
    for(i = 1; i < 15; i++)
        offs[i + 1] = offs[i] + h->counts[i];
    for(symbol = 0; symbol < (unsigned)n; symbol++)
        if(lengths[symbol] != 0)
            h->symbols[offs[lengths[symbol]]++] = (unsigned short)symbol;
    return allow_incomplete || left == 0;
}

static int
huff_decode(BitReader *r, Huff *h)
{
    int code = 0;
    int first = 0;
    int index = 0;
    int len;

    for(len = 1; len < 16; len++) {
        code |= (int)read_bits(r, 1);
        int count = h->counts[len];

        if(code - count < first)
            return h->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if(r->bad)
            return -1;
    }
    return -1;
}

static const unsigned short LENGTH_BASE[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned short LENGTH_EXTRA[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short DIST_BASE[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193,
    12289, 16385, 24577};
static const unsigned short DIST_EXTRA[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const unsigned char CODE_LENGTH_ORDER[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* Inflate a raw DEFLATE stream from the reader into the window. Returns 0
 * on malformed input. */
static int
inflate_raw(BitReader *r, Window *w)
{
    int final;

    do {
        int type;

        final = (int)read_bits(r, 1);
        type = (int)read_bits(r, 2);
        if(r->bad)
            return 0;
        if(type == 0) {
            unsigned stored_len;
            unsigned i;

            align_byte(r);
            if(r->pos + 4 > r->len)
                return 0;
            stored_len = (unsigned)r->data[r->pos] |
                         ((unsigned)r->data[r->pos + 1] << 8);
            {
                unsigned nlen = (unsigned)r->data[r->pos + 2] |
                                ((unsigned)r->data[r->pos + 3] << 8);

                if((stored_len ^ 0xFFFFu) != nlen)
                    return 0;
            }
            r->pos += 4;
            if(r->pos + stored_len > r->len)
                return 0;
            for(i = 0; i < stored_len; i++)
                if(!window_push(w, r->data[r->pos + i]))
                    return 0;
            r->pos += stored_len;
        } else if(type == 1 || type == 2) {
            Huff lit;
            Huff dist;
            unsigned char lengths[288 + 32];
            unsigned nlen_codes;
            unsigned ndist_codes;
            unsigned code_bits;
            unsigned index;

            if(type == 1) {
                unsigned i;

                for(i = 0; i < 144; i++)
                    lengths[i] = 8;
                for(; i < 256; i++)
                    lengths[i] = 9;
                for(; i < 280; i++)
                    lengths[i] = 7;
                for(; i < 288; i++)
                    lengths[i] = 8;
                if(!huff_build(&lit, lengths, 288, 0))
                    return 0;
                for(i = 0; i < 30; i++)
                    lengths[i] = 5;
                if(!huff_build(&dist, lengths, 30, 1))
                    return 0;
            } else {
                unsigned char clen[19];
                unsigned i;
                unsigned total;

                nlen_codes = read_bits(r, 5) + 257;
                ndist_codes = read_bits(r, 5) + 1;
                code_bits = read_bits(r, 4) + 4;
                if(r->bad || nlen_codes > 286 || ndist_codes > 30)
                    return 0;
                memset(clen, 0, sizeof(clen));
                for(i = 0; i < code_bits; i++)
                    clen[CODE_LENGTH_ORDER[i]] = (unsigned char)read_bits(r, 3);
                if(r->bad || !huff_build(&lit, clen, 19, 0))
                    return 0;
                total = nlen_codes + ndist_codes;
                index = 0;
                while(index < total) {
                    int sym = huff_decode(r, &lit);

                    if(sym < 0)
                        return 0;
                    if(sym < 16) {
                        lengths[index++] = (unsigned char)sym;
                        continue;
                    }
                    {
                        unsigned char repeat_value = 0;
                        unsigned repeat_count;

                        if(sym == 16) {
                            if(index == 0)
                                return 0;
                            repeat_value = lengths[index - 1];
                            repeat_count = read_bits(r, 2) + 3;
                        } else if(sym == 17) {
                            repeat_count = read_bits(r, 3) + 3;
                        } else {
                            repeat_count = read_bits(r, 7) + 11;
                        }
                        if(r->bad || index + repeat_count > total)
                            return 0;
                        while(repeat_count-- != 0)
                            lengths[index++] = repeat_value;
                    }
                }
                if(lengths[256] == 0)
                    return 0; /* missing end-of-block code */
                if(!huff_build(&lit, lengths, (int)nlen_codes, 0))
                    return 0;
                if(!huff_build(&dist, lengths + nlen_codes,
                               (int)ndist_codes, 1))
                    return 0;
            }
            for(;;) {
                int sym = huff_decode(r, &lit);

                if(sym < 0)
                    return 0;
                if(sym == 256)
                    break;
                if(sym < 256) {
                    if(!window_push(w, (unsigned char)sym))
                        return 0;
                    continue;
                }
                {
                    int ls = sym - 257;
                    unsigned length;
                    unsigned dsym;
                    unsigned back;
                    unsigned i;

                    if(ls >= 29)
                        return 0;
                    length = LENGTH_BASE[ls] + read_bits(r, LENGTH_EXTRA[ls]);
                    dsym = (unsigned)huff_decode(r, &dist);
                    if(r->bad || (int)dsym < 0 || dsym >= 30)
                        return 0;
                    back = DIST_BASE[dsym] + read_bits(r, DIST_EXTRA[dsym]);
                    if(back > w->len || back == 0 || back > WINDOW_SIZE)
                        return 0;
                    for(i = 0; i < length; i++)
                        if(!window_push(w, window_at(w, back)))
                            return 0;
                }
                if(r->bad)
                    return 0;
            }
        } else
            return 0;
    } while(!final && !r->bad);
    return !r->bad;
}

int
kry_gzip_is(const unsigned char *data, unsigned long len)
{
    return data != NULL && len >= 2 && data[0] == 0x1F && data[1] == 0x8B;
}

unsigned char *
kry_gzip_decompress(const unsigned char *data, unsigned long len,
                    unsigned long *out_len)
{
    BitReader r;
    Window w;
    unsigned char flags;
    unsigned long trailer;
    unsigned long expected_crc;
    unsigned long expected_size;
    unsigned char *result;

    if(out_len != NULL)
        *out_len = 0;
    if(!kry_gzip_is(data, len) || len < 18)
        return NULL;
    flags = data[3];
    if(data[2] != 8 || (flags & 0xE0) != 0)
        return NULL; /* method must be deflate, reserved bits clear */

    r.data = data;
    r.len = len;
    r.pos = 10;
    r.bit = 0;
    r.bad = 0;
    if(flags & 0x04) { /* FEXTRA */
        unsigned xlen;

        if(r.pos + 2 > len)
            return NULL;
        xlen = (unsigned)data[r.pos] | ((unsigned)data[r.pos + 1] << 8);
        r.pos += 2 + xlen;
        if(r.pos > len)
            return NULL;
    }
    if(flags & 0x08) { /* FNAME */
        while(r.pos < len && data[r.pos] != 0)
            r.pos++;
        r.pos++;
        if(r.pos > len)
            return NULL;
    }
    if(flags & 0x10) { /* FCOMMENT */
        while(r.pos < len && data[r.pos] != 0)
            r.pos++;
        r.pos++;
        if(r.pos > len)
            return NULL;
    }
    if(flags & 0x02) { /* FHCRC */
        if(r.pos + 2 > len)
            return NULL;
        r.pos += 2;
    }

    w.buf = NULL;
    w.len = 0;
    w.cap = 0;
    w.bad = 0;
    if(!inflate_raw(&r, &w)) {
        free(w.buf);
        return NULL;
    }
    align_byte(&r);
    trailer = r.pos;
    if(trailer + 8 > len) {
        free(w.buf);
        return NULL;
    }
    expected_crc = (unsigned long)data[trailer] |
                   ((unsigned long)data[trailer + 1] << 8) |
                   ((unsigned long)data[trailer + 2] << 16) |
                   ((unsigned long)data[trailer + 3] << 24);
    expected_size = (unsigned long)data[trailer + 4] |
                    ((unsigned long)data[trailer + 5] << 8) |
                    ((unsigned long)data[trailer + 6] << 16) |
                    ((unsigned long)data[trailer + 7] << 24);
    if(expected_size != (w.len & 0xFFFFFFFFul) ||
       kry_gzip_crc32(w.buf, w.len) != expected_crc) {
        free(w.buf);
        return NULL;
    }
    if(window_push(&w, 0)) { /* NUL convenience for text callers */
        result = w.buf;
        if(out_len != NULL)
            *out_len = w.len - 1;
        return result;
    }
    free(w.buf);
    return NULL;
}

unsigned char *
kry_gzip_compress(const unsigned char *data, unsigned long len,
                  unsigned long *out_len)
{
    unsigned long total;
    unsigned long blocks;
    unsigned long header = 10;
    unsigned long i;
    unsigned long done = 0;
    unsigned long crc = kry_gzip_crc32(data, len);
    unsigned char *out;

    if(out_len != NULL)
        *out_len = 0;
    if(data == NULL && len != 0)
        return NULL;
    blocks = len / GZIP_STORED_MAX + (len % GZIP_STORED_MAX != 0 ? 1 : 0);
    if(blocks == 0)
        blocks = 1;
    total = header + blocks * 5 + len + 8;
    out = malloc(total);
    if(out == NULL)
        return NULL;

    /* fixed header: magic, deflate, no flags, no mtime/OS */
    out[0] = 0x1F;
    out[1] = 0x8B;
    out[2] = 0x08;
    out[3] = 0x00;
    out[4] = out[5] = out[6] = out[7] = 0;
    out[8] = 0x00;
    out[9] = 0xFF;

    i = header;
    for(;;) {
        unsigned long chunk = len - done;

        if(chunk > GZIP_STORED_MAX)
            chunk = GZIP_STORED_MAX;
        out[i++] = (done + chunk >= len) ? 0x01 : 0x00; /* BFINAL, BTYPE=00 */
        out[i++] = (unsigned char)(chunk & 0xFF);
        out[i++] = (unsigned char)((chunk >> 8) & 0xFF);
        out[i++] = (unsigned char)(~chunk & 0xFF);
        out[i++] = (unsigned char)((~chunk >> 8) & 0xFF);
        if(chunk != 0)
            memcpy(out + i, data + done, chunk);
        i += chunk;
        done += chunk;
        if(done >= len)
            break;
    }
    out[i++] = (unsigned char)(crc & 0xFF);
    out[i++] = (unsigned char)((crc >> 8) & 0xFF);
    out[i++] = (unsigned char)((crc >> 16) & 0xFF);
    out[i++] = (unsigned char)((crc >> 24) & 0xFF);
    out[i++] = (unsigned char)(len & 0xFF);
    out[i++] = (unsigned char)((len >> 8) & 0xFF);
    out[i++] = (unsigned char)((len >> 16) & 0xFF);
    out[i++] = (unsigned char)((len >> 24) & 0xFF);
    if(out_len != NULL)
        *out_len = i;
    return out;
}

/* --- zlib container (RFC 1950) -------------------------------------------- */

unsigned long
kry_zlib_adler32(const unsigned char *data, unsigned long len)
{
    unsigned long a = 1;
    unsigned long b = 0;
    unsigned long i = 0;

    if(data == NULL)
        return 1;
    while(i < len) {
        /* NMAX: the most bytes processable before 32-bit overflow */
        unsigned long chunk = len - i;
        unsigned long n;

        if(chunk > 5552)
            chunk = 5552;
        for(n = 0; n < chunk; n++) {
            a += data[i + n];
            b += a;
        }
        a %= 65521;
        b %= 65521;
        i += chunk;
    }
    return (b << 16) | a;
}

int
kry_zlib_is(const unsigned char *data, unsigned long len)
{
    unsigned cmf;
    unsigned flg;

    if(data == NULL || len < 2)
        return 0;
    cmf = data[0];
    flg = data[1];
    if((cmf & 0x0Fu) != 8)
        return 0; /* method must be deflate */
    if((cmf >> 4) > 7)
        return 0; /* window larger than the 32 KiB inflater support */
    if(((cmf << 8) | flg) % 31 != 0)
        return 0; /* FCHECK */
    if(flg & 0x20)
        return 0; /* FDICT: preset dictionaries unsupported */
    return 1;
}

unsigned char *
kry_zlib_decompress(const unsigned char *data, unsigned long len,
                    unsigned long *out_len)
{
    BitReader r;
    Window w;
    unsigned long trailer;
    unsigned long expected;
    unsigned char *result;

    if(out_len != NULL)
        *out_len = 0;
    if(!kry_zlib_is(data, len))
        return NULL;

    r.data = data;
    r.len = len;
    r.pos = 2;
    r.bit = 0;
    r.bad = 0;
    w.buf = NULL;
    w.len = 0;
    w.cap = 0;
    w.bad = 0;
    if(!inflate_raw(&r, &w)) {
        free(w.buf);
        return NULL;
    }
    align_byte(&r);
    trailer = r.pos;
    if(trailer + 4 > len) {
        free(w.buf);
        return NULL;
    }
    expected = ((unsigned long)data[trailer] << 24) |
               ((unsigned long)data[trailer + 1] << 16) |
               ((unsigned long)data[trailer + 2] << 8) |
               (unsigned long)data[trailer + 3];
    if(kry_zlib_adler32(w.buf, w.len) != expected) {
        free(w.buf);
        return NULL;
    }
    if(window_push(&w, 0)) { /* NUL convenience for text callers */
        result = w.buf;
        if(out_len != NULL)
            *out_len = w.len - 1;
        return result;
    }
    free(w.buf);
    return NULL;
}

unsigned char *
kry_zlib_compress(const unsigned char *data, unsigned long len,
                  unsigned long *out_len)
{
    unsigned long total;
    unsigned long blocks;
    unsigned long i;
    unsigned long done = 0;
    unsigned long adler = kry_zlib_adler32(data, len);
    unsigned char *out;

    if(out_len != NULL)
        *out_len = 0;
    if(data == NULL && len != 0)
        return NULL;
    blocks = len / GZIP_STORED_MAX + (len % GZIP_STORED_MAX != 0 ? 1 : 0);
    if(blocks == 0)
        blocks = 1;
    total = 2 + blocks * 5 + len + 4;
    out = malloc(total);
    if(out == NULL)
        return NULL;

    /* fixed header: CMF 0x78 (deflate, 32 KiB window), FLG 0x01 (FCHECK) */
    out[0] = 0x78;
    out[1] = 0x01;

    i = 2;
    for(;;) {
        unsigned long chunk = len - done;

        if(chunk > GZIP_STORED_MAX)
            chunk = GZIP_STORED_MAX;
        out[i++] = (done + chunk >= len) ? 0x01 : 0x00; /* BFINAL, BTYPE=00 */
        out[i++] = (unsigned char)(chunk & 0xFF);
        out[i++] = (unsigned char)((chunk >> 8) & 0xFF);
        out[i++] = (unsigned char)(~chunk & 0xFF);
        out[i++] = (unsigned char)((~chunk >> 8) & 0xFF);
        if(chunk != 0)
            memcpy(out + i, data + done, chunk);
        i += chunk;
        done += chunk;
        if(done >= len)
            break;
    }
    out[i++] = (unsigned char)((adler >> 24) & 0xFF);
    out[i++] = (unsigned char)((adler >> 16) & 0xFF);
    out[i++] = (unsigned char)((adler >> 8) & 0xFF);
    out[i++] = (unsigned char)(adler & 0xFF);
    if(out_len != NULL)
        *out_len = i;
    return out;
}
