/*
 * kry_sha256.c - SHA-256 per FIPS 180-4. Compact, dependency-free; the
 * ksync crypto stack keeps its own copy because it predates this module.
 */
#include "kry_sha256.h"

#include <stdio.h>
#include <string.h>

static const unsigned int K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static unsigned int
rotr(unsigned int x, int n)
{
    return (x >> n) | (x << (32 - n));
}

static void
sha256_block(KrySha256 *ctx, const unsigned char block[64])
{
    unsigned int w[64];
    unsigned int a, b, c, d, e, f, g, h;
    int i;

    for(i = 0; i < 16; i++)
        w[i] = ((unsigned int)block[i * 4] << 24) |
               ((unsigned int)block[i * 4 + 1] << 16) |
               ((unsigned int)block[i * 4 + 2] << 8) |
               (unsigned int)block[i * 4 + 3];
    for(i = 16; i < 64; i++) {
        unsigned int s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for(i = 0; i < 64; i++) {
        unsigned int S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned int ch = (e & f) ^ (~e & g);
        unsigned int t1 = h + S1 + ch + K[i] + w[i];
        unsigned int S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void
kry_sha256_init(KrySha256 *ctx)
{
    ctx->state[0] = 0x6a09e667u; ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u; ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu; ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu; ctx->state[7] = 0x5be0cd19u;
    ctx->bit_len = 0;
    ctx->buf_len = 0;
}

void
kry_sha256_update(KrySha256 *ctx, const void *data, size_t len)
{
    const unsigned char *p = data;

    if(ctx == NULL || (len > 0 && data == NULL))
        return;
    while(len > 0) {
        size_t take = 64 - ctx->buf_len;

        if(take > len)
            take = len;
        memcpy(ctx->buf + ctx->buf_len, p, take);
        ctx->buf_len += take;
        p += take;
        len -= take;
        if(ctx->buf_len == 64) {
            sha256_block(ctx, ctx->buf);
            ctx->bit_len += 512;
            ctx->buf_len = 0;
        }
    }
}

void
kry_sha256_final(KrySha256 *ctx, unsigned char out[32])
{
    unsigned long long bits;
    int i;

    if(ctx == NULL || out == NULL)
        return;
    bits = ctx->bit_len + (unsigned long long)ctx->buf_len * 8;

    /* append 0x80 then zeros to 56 mod 64, then the 64-bit big-endian
     * bit length */
    unsigned char pad = 0x80;

    kry_sha256_update(ctx, &pad, 1);
    pad = 0;
    while(ctx->buf_len != 56)
        kry_sha256_update(ctx, &pad, 1);
    for(i = 7; i >= 0; i--)
        kry_sha256_update(ctx,
                          &(unsigned char){(unsigned char)((bits >> (i * 8)) & 0xff)},
                          1);

    for(i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (unsigned char)ctx->state[i];
    }
}

char *
kry_sha256_hex(const unsigned char digest[32], char out[65])
{
    static const char hex[] = "0123456789abcdef";
    int i;

    if(out == NULL)
        return NULL;
    for(i = 0; i < 32; i++) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    out[64] = '\0';
    return out;
}

int
kry_sha256_file(const char *path, char hex_out[65])
{
    KrySha256 ctx;
    unsigned char buf[8192];
    unsigned char digest[32];
    FILE *f;
    size_t got;

    if(path == NULL || hex_out == NULL)
        return 0;
    f = fopen(path, "rb");
    if(f == NULL)
        return 0;
    kry_sha256_init(&ctx);
    while((got = fread(buf, 1, sizeof(buf), f)) > 0)
        kry_sha256_update(&ctx, buf, got);
    if(ferror(f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    kry_sha256_final(&ctx, digest);
    kry_sha256_hex(digest, hex_out);
    return 1;
}

int
kry_sha256_hex_equal(const char *a, const char *b)
{
    if(a == NULL || b == NULL)
        return 0;
    while(*a != '\0' && *b != '\0') {
        char ca = *a >= 'A' && *a <= 'F' ? (char)(*a + 32) : *a;
        char cb = *b >= 'A' && *b <= 'F' ? (char)(*b + 32) : *b;

        if(ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}
