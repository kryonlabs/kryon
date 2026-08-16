/*
 * Minimal PNG writer for krb-run: 8-bit RGBA, stored (uncompressed)
 * deflate blocks. No external dependencies, deterministic output.
 */

#include "png_write.h"

#include <stdlib.h>
#include <string.h>

static unsigned
crc32_buf(const unsigned char *p, size_t n)
{
    static unsigned table[256];
    static int built = 0;
    unsigned crc = 0xffffffffu;
    size_t i;

    if(!built) {
        int i;
        for(i = 0; i < 256; i++) {
            unsigned c = (unsigned)i;
            int k;
            for(k = 0; k < 8; k++)
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        built = 1;
    }
    for(i = 0; i < n; i++)
        crc = table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

static void
be32(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void
png_chunk(FILE *f, const char *type, const unsigned char *data, size_t len)
{
    unsigned char hdr[8];
    unsigned char *buf;
    unsigned crc;

    buf = malloc(4 + len);
    if(buf == NULL)
        return;
    memcpy(buf, type, 4);
    if(len > 0 && data != NULL)
        memcpy(buf + 4, data, len);
    crc = crc32_buf(buf, 4 + len);
    be32(hdr, (unsigned)len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if(len > 0)
        fwrite(data, 1, len, f);
    be32(hdr, crc);
    fwrite(hdr, 1, 4, f);
    free(buf);
}

static unsigned
adler32_buf(const unsigned char *p, size_t n)
{
    unsigned a = 1;
    unsigned b = 0;
    size_t i;

    for(i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

int
krb_png_write(const char *path, const unsigned char *rgba, int w, int h)
{
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    unsigned char ihdr[13];
    unsigned char tail[4];
    size_t rawlen = (size_t)h * (1 + (size_t)w * 4);
    unsigned char *raw;
    unsigned char *z;
    size_t zn = 0;
    size_t off = 0;
    size_t left;
    const unsigned char *src;
    FILE *f;
    int row;
    int ok = 0;

    f = fopen(path, "wb");
    if(f == NULL)
        return -1;
    raw = malloc(rawlen);
    z = malloc(rawlen + (rawlen / 65535 + 1) * 5 + 16);
    if(raw == NULL || z == NULL)
        goto out;

    for(row = 0; row < h; row++) {
        raw[off++] = 0; /* filter: none */
        memcpy(raw + off, rgba + (size_t)row * w * 4, (size_t)w * 4);
        off += (size_t)w * 4;
    }

    z[zn++] = 0x78; /* zlib header, fastest */
    z[zn++] = 0x01;
    src = raw;
    left = rawlen;
    while(left > 0) {
        size_t chunk = left > 65535 ? 65535 : left;
        unsigned char bhdr[5];
        bhdr[0] = (left - chunk == 0) ? 1 : 0; /* BFINAL on last block */
        bhdr[1] = (unsigned char)(chunk & 0xff);
        bhdr[2] = (unsigned char)(chunk >> 8);
        bhdr[3] = (unsigned char)(~chunk & 0xff);
        bhdr[4] = (unsigned char)((~chunk >> 8) & 0xff);
        memcpy(z + zn, bhdr, 5);
        zn += 5;
        memcpy(z + zn, src, chunk);
        zn += chunk;
        src += chunk;
        left -= chunk;
    }
    be32(tail, adler32_buf(raw, rawlen));
    memcpy(z + zn, tail, 4);
    zn += 4;

    fwrite(sig, 1, 8, f);
    be32(ihdr, (unsigned)w);
    be32(ihdr + 4, (unsigned)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 6;  /* RGBA */
    ihdr[10] = 0; /* deflate */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, zn);
    png_chunk(f, "IEND", NULL, 0);
    ok = 1;
out:
    free(raw);
    free(z);
    if(fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}
