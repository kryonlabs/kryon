#include "ksync_crypto.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* SHA-256                                                             */
/* ------------------------------------------------------------------ */

typedef struct KsyncSha256Ctx {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t data[64];
    size_t data_len;
} KsyncSha256Ctx;

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t
rotr32(uint32_t value, uint32_t bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void
sha256_transform(KsyncSha256Ctx *ctx, const uint8_t data[64])
{
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h;

    for(int i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               (uint32_t)data[i * 4 + 3];
    }
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(m[i - 15], 7) ^ rotr32(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr32(m[i - 2], 17) ^ rotr32(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for(int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + m[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void
sha256_init(KsyncSha256Ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void
sha256_update(KsyncSha256Ctx *ctx, const uint8_t *data, size_t len)
{
    for(size_t i = 0; i < len; i++) {
        ctx->data[ctx->data_len++] = data[i];
        if(ctx->data_len == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void
sha256_final(KsyncSha256Ctx *ctx, uint8_t hash[32])
{
    size_t i = ctx->data_len;

    ctx->data[i++] = 0x80;
    if(i > 56) {
        while(i < 64)
            ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while(i < 56)
        ctx->data[i++] = 0;

    ctx->bit_len += ctx->data_len * 8;
    for(int j = 0; j < 8; j++)
        ctx->data[63 - j] = (uint8_t)(ctx->bit_len >> (j * 8));
    sha256_transform(ctx, ctx->data);

    for(i = 0; i < 4; i++) {
        for(int j = 0; j < 8; j++)
            hash[j * 4 + i] = (uint8_t)(ctx->state[j] >> (24 - i * 8));
    }
}

void
KsyncCryptoSha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    KsyncSha256Ctx sha;

    if(out == NULL)
        return;
    if(data == NULL)
        len = 0;
    sha256_init(&sha);
    sha256_update(&sha, data, len);
    sha256_final(&sha, out);
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA256 (RFC 2104) and PBKDF2 (RFC 2898)                        */
/* ------------------------------------------------------------------ */

static void
hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data,
            size_t data_len, uint8_t out[32])
{
    KsyncSha256Ctx sha;
    uint8_t block[64];
    uint8_t pad[64];
    uint8_t inner[32];

    memset(block, 0, sizeof(block));
    if(key != NULL && key_len > 64) {
        KsyncCryptoSha256(key, key_len, block);
    } else if(key != NULL) {
        memcpy(block, key, key_len);
    }
    for(int i = 0; i < 64; i++)
        pad[i] = block[i] ^ 0x36U;
    sha256_init(&sha);
    sha256_update(&sha, pad, sizeof(pad));
    if(data != NULL && data_len > 0)
        sha256_update(&sha, data, data_len);
    sha256_final(&sha, inner);
    for(int i = 0; i < 64; i++)
        pad[i] = block[i] ^ 0x5cU;
    sha256_init(&sha);
    sha256_update(&sha, pad, sizeof(pad));
    sha256_update(&sha, inner, sizeof(inner));
    sha256_final(&sha, out);
}

void
KsyncCryptoHmacSha256(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len, uint8_t out[32])
{
    if(out == NULL)
        return;
    hmac_sha256(key, key_len, data, data_len, out);
}

void
KsyncCryptoPbkdf2Sha256(const uint8_t *password, size_t password_len,
                        const uint8_t *salt, size_t salt_len,
                        unsigned long iterations, uint8_t out[32])
{
    uint8_t salt_block[36];
    uint8_t u[32];
    uint8_t t[32];
    unsigned long i;
    size_t salt_used;

    if(out == NULL)
        return;
    if(iterations == 0)
        iterations = 1;
    memset(salt_block, 0, sizeof(salt_block));
    salt_used = salt != NULL && salt_len > 0 ? (salt_len < 32 ? salt_len : 32) : 0;
    memcpy(salt_block, salt, salt_used);
    /* salt (max 32 bytes) || block index 1 as big endian uint32 */
    salt_block[salt_used + 3] = 1;
    hmac_sha256(password, password_len, salt_block, salt_used + 4, u);
    memcpy(t, u, sizeof(t));
    for(i = 1; i < iterations; i++) {
        hmac_sha256(password, password_len, u, sizeof(u), u);
        for(size_t j = 0; j < 32; j++)
            t[j] ^= u[j];
    }
    memcpy(out, t, 32);
}

/* ------------------------------------------------------------------ */
/* ChaCha20-Poly1305 AEAD (RFC 8439)                                   */
/* ------------------------------------------------------------------ */

static uint32_t
load_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void
store_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t
rotl32(uint32_t v, int c)
{
    return (v << c) | (v >> (32 - c));
}

static void
chacha20_quarter_round(uint32_t *s, int a, int b, int c, int d)
{
    s[a] += s[b];
    s[d] ^= s[a];
    s[d] = rotl32(s[d], 16);
    s[c] += s[d];
    s[b] ^= s[c];
    s[b] = rotl32(s[b], 12);
    s[a] += s[b];
    s[d] ^= s[a];
    s[d] = rotl32(s[d], 8);
    s[c] += s[d];
    s[b] ^= s[c];
    s[b] = rotl32(s[b], 7);
}

static void
chacha20_block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
               uint8_t out[64])
{
    static const uint8_t constants[16] = {
        'e', 'x', 'p', 'a', 'n', 'd', ' ', '3',
        '2', '-', 'b', 'y', 't', 'e', ' ', 'k'
    };
    uint32_t s[16];
    uint32_t x[16];

    s[0] = load_le32(constants);
    s[1] = load_le32(constants + 4);
    s[2] = load_le32(constants + 8);
    s[3] = load_le32(constants + 12);
    for(int i = 0; i < 8; i++)
        s[4 + i] = load_le32(key + i * 4);
    s[12] = counter;
    s[13] = load_le32(nonce);
    s[14] = load_le32(nonce + 4);
    s[15] = load_le32(nonce + 8);
    memcpy(x, s, sizeof(s));
    for(int i = 0; i < 10; i++) {
        chacha20_quarter_round(x, 0, 4, 8, 12);
        chacha20_quarter_round(x, 1, 5, 9, 13);
        chacha20_quarter_round(x, 2, 6, 10, 14);
        chacha20_quarter_round(x, 3, 7, 11, 15);
        chacha20_quarter_round(x, 0, 5, 10, 15);
        chacha20_quarter_round(x, 1, 6, 11, 12);
        chacha20_quarter_round(x, 2, 7, 8, 13);
        chacha20_quarter_round(x, 3, 4, 9, 14);
    }
    for(int i = 0; i < 16; i++)
        store_le32(out + i * 4, x[i] + s[i]);
}

static void
chacha20_xor_stream(const uint8_t key[32], const uint8_t nonce[12],
                    const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t block[64];
    uint32_t counter = 1;

    while(len > 0) {
        size_t n = len < sizeof(block) ? len : sizeof(block);
        chacha20_block(key, nonce, counter++, block);
        for(size_t i = 0; i < n; i++)
            out[i] = in[i] ^ block[i];
        in += n;
        out += n;
        len -= n;
    }
}

/* 26-bit limb implementation following the reference pseudocode in RFC 8439. */
static void
poly1305_mac(const uint8_t key[32], const uint8_t *msg, size_t len,
             uint8_t tag[16])
{
    unsigned long long r[5];
    unsigned long long h[5];
    unsigned long long s[5];
    unsigned long long c;

    r[0] = (load_le32(key + 0) >> 0) & 0x3ffffffULL;
    r[1] = (load_le32(key + 3) >> 2) & 0x3ffff03ULL;
    r[2] = (load_le32(key + 6) >> 4) & 0x3ffc0ffULL;
    r[3] = (load_le32(key + 9) >> 6) & 0x3f03fffULL;
    r[4] = (load_le32(key + 12) >> 8) & 0x00fffffULL;

    s[0] = r[1] * 5;
    s[1] = r[2] * 5;
    s[2] = r[3] * 5;
    s[3] = r[4] * 5;

    memset(h, 0, sizeof(h));

    while(len > 0) {
        uint64_t in[5];
        size_t n = len < 16 ? len : 16;
        uint8_t block[17];

        memset(block, 0, sizeof(block));
        memcpy(block, msg, n);
        if(n < 16)
            block[n] = 1;
        in[0] = (load_le32(block + 0) >> 0) & 0x3ffffffULL;
        in[1] = (load_le32(block + 3) >> 2) & 0x3ffffffULL;
        in[2] = (load_le32(block + 6) >> 4) & 0x3ffffffULL;
        in[3] = (load_le32(block + 9) >> 6) & 0x3ffffffULL;
        in[4] = (load_le32(block + 12) >> 8) | ((uint64_t)(n == 16) << 24);

        h[0] += in[0];
        h[1] += in[1];
        h[2] += in[2];
        h[3] += in[3];
        h[4] += in[4];

        {
            unsigned long long d0 = h[0] * r[0] + h[1] * s[3] + h[2] * s[2] + h[3] * s[1] + h[4] * s[0];
            unsigned long long d1 = h[1] * r[0] + h[2] * s[3] + h[3] * s[2] + h[4] * s[1] + h[0] * r[1];
            unsigned long long d2 = h[2] * r[0] + h[3] * s[3] + h[4] * s[2] + h[0] * r[2] + h[1] * r[1];
            unsigned long long d3 = h[3] * r[0] + h[4] * s[3] + h[0] * r[3] + h[1] * r[2] + h[2] * r[1];
            unsigned long long d4 = h[4] * r[0] + h[0] * r[4] + h[1] * r[3] + h[2] * r[2] + h[3] * r[1];

            c = d0 >> 26;
            h[0] = d0 & 0x3ffffffULL;
            d1 += c;
            c = d1 >> 26;
            h[1] = d1 & 0x3ffffffULL;
            d2 += c;
            c = d2 >> 26;
            h[2] = d2 & 0x3ffffffULL;
            d3 += c;
            c = d3 >> 26;
            h[3] = d3 & 0x3ffffffULL;
            d4 += c;
            c = d4 >> 26;
            h[4] = d4 & 0x3ffffffULL;
            h[0] += c * 5;
            c = h[0] >> 26;
            h[0] &= 0x3ffffffULL;
            h[1] += c;
        }

        msg += n;
        len -= n;
    }

    /* final carry propagation */
    c = h[1] >> 26;
    h[1] &= 0x3ffffffULL;
    h[2] += c;
    c = h[2] >> 26;
    h[2] &= 0x3ffffffULL;
    h[3] += c;
    c = h[3] >> 26;
    h[3] &= 0x3ffffffULL;
    h[4] += c;
    c = h[4] >> 26;
    h[4] &= 0x3ffffffULL;
    h[0] += c * 5;
    c = h[0] >> 26;
    h[0] &= 0x3ffffffULL;
    h[1] += c;

    /* compute h + -p */
    {
        unsigned long long g[5];
        unsigned long long mask;
        unsigned long long carry;

        g[0] = h[0] + 5;
        carry = g[0] >> 26;
        g[0] &= 0x3ffffffULL;
        g[1] = h[1] + carry;
        carry = g[1] >> 26;
        g[1] &= 0x3ffffffULL;
        g[2] = h[2] + carry;
        carry = g[2] >> 26;
        g[2] &= 0x3ffffffULL;
        g[3] = h[3] + carry;
        carry = g[3] >> 26;
        g[3] &= 0x3ffffffULL;
        g[4] = h[4] + carry - (1ULL << 26);

        /* select h if h < p, or h + -p if h >= p */
        mask = (g[4] >> 63) - 1; /* all ones if g[4] negative (h < p) */
        g[0] &= mask;
        g[1] &= mask;
        g[2] &= mask;
        g[3] &= mask;
        g[4] &= mask;
        mask = ~mask;
        h[0] = (h[0] & mask) | g[0];
        h[1] = (h[1] & mask) | g[1];
        h[2] = (h[2] & mask) | g[2];
        h[3] = (h[3] & mask) | g[3];
        h[4] = (h[4] & mask) | g[4];
    }

    /* h = h % (2^128): repack the five 26-bit limbs into two 64-bit halves */
    h[0] = h[0] | (h[1] << 26) | ((h[2] & 0xfffULL) << 52);
    h[1] = (h[2] >> 12) | (h[3] << 14) | (h[4] << 40);

    /* mac = (h + pad) % (2^128) */
    {
        uint64_t pad[4];
        uint64_t f;
        uint32_t w[4];

        for(int j = 0; j < 4; j++)
            pad[j] = load_le32(key + 16 + j * 4);
        f = (h[0] & 0xffffffffULL) + pad[0];
        w[0] = (uint32_t)f;
        f = (h[0] >> 32) + pad[1] + (f >> 32);
        w[1] = (uint32_t)f;
        f = (h[1] & 0xffffffffULL) + pad[2] + (f >> 32);
        w[2] = (uint32_t)f;
        f = (h[1] >> 32) + pad[3] + (f >> 32);
        w[3] = (uint32_t)f;

        store_le32(tag, w[0]);
        store_le32(tag + 4, w[1]);
        store_le32(tag + 8, w[2]);
        store_le32(tag + 12, w[3]);
    }
}

static void
poly1305_key_gen(const uint8_t key[32], const uint8_t nonce[12], uint8_t mac_key[32])
{
    uint8_t block[64];

    chacha20_block(key, nonce, 0, block);
    memcpy(mac_key, block, 32);
}

static int
timing_safe_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;

    for(size_t i = 0; i < len; i++)
        diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

int
KsyncCryptoChaCha20Poly1305Seal(const uint8_t key[32], const uint8_t nonce[12],
                               const uint8_t *plain, size_t plain_len,
                               const uint8_t *aad, size_t aad_len,
                               uint8_t *ciphertext_and_tag)
{
    uint8_t mac_key[32];
    uint8_t tag[16];
    uint8_t *buf;
    size_t total;
    size_t off;

    if(key == NULL || nonce == NULL || ciphertext_and_tag == NULL)
        return 0;
    if(plain == NULL)
        plain_len = 0;
    if(aad == NULL)
        aad_len = 0;
    poly1305_key_gen(key, nonce, mac_key);
    chacha20_xor_stream(key, nonce, plain, ciphertext_and_tag, plain_len);
    total = aad_len + ((16 - (aad_len % 16)) % 16) +
            plain_len + ((16 - (plain_len % 16)) % 16) + 16;
    buf = (uint8_t *)malloc(total);
    if(buf == NULL)
        return 0;
    off = 0;
    memcpy(buf + off, aad, aad_len);
    off += aad_len;
    memset(buf + off, 0, (16 - (aad_len % 16)) % 16);
    off += (16 - (aad_len % 16)) % 16;
    memcpy(buf + off, ciphertext_and_tag, plain_len);
    off += plain_len;
    memset(buf + off, 0, (16 - (plain_len % 16)) % 16);
    off += (16 - (plain_len % 16)) % 16;
    store_le32(buf + off, (uint32_t)aad_len);
    store_le32(buf + off + 4, 0);
    store_le32(buf + off + 8, (uint32_t)plain_len);
    store_le32(buf + off + 12, 0);
    poly1305_mac(mac_key, buf, total, tag);
    free(buf);
    memcpy(ciphertext_and_tag + plain_len, tag, 16);
    return 1;
}

int
KsyncCryptoChaCha20Poly1305Open(const uint8_t key[32], const uint8_t nonce[12],
                               const uint8_t *ciphertext_and_tag, size_t total_len,
                               const uint8_t *aad, size_t aad_len,
                               uint8_t *plain)
{
    uint8_t mac_key[32];
    uint8_t tag[16];
    size_t plain_len;
    size_t total;
    uint8_t *buf;
    size_t off;

    if(key == NULL || nonce == NULL || ciphertext_and_tag == NULL)
        return 0;
    if(aad == NULL)
        aad_len = 0;
    if(total_len < 16)
        return 0;
    plain_len = total_len - 16;
    poly1305_key_gen(key, nonce, mac_key);
    total = aad_len + ((16 - (aad_len % 16)) % 16) +
            plain_len + ((16 - (plain_len % 16)) % 16) + 16;
    buf = (uint8_t *)malloc(total);
    if(buf == NULL)
        return 0;
    off = 0;
    memcpy(buf + off, aad, aad_len);
    off += aad_len;
    memset(buf + off, 0, (16 - (aad_len % 16)) % 16);
    off += (16 - (aad_len % 16)) % 16;
    memcpy(buf + off, ciphertext_and_tag, plain_len);
    off += plain_len;
    memset(buf + off, 0, (16 - (plain_len % 16)) % 16);
    off += (16 - (plain_len % 16)) % 16;
    store_le32(buf + off, (uint32_t)aad_len);
    store_le32(buf + off + 4, 0);
    store_le32(buf + off + 8, (uint32_t)plain_len);
    store_le32(buf + off + 12, 0);
    poly1305_mac(mac_key, buf, total, tag);
    free(buf);
    if(!timing_safe_equal(tag, ciphertext_and_tag + plain_len, 16))
        return 0;
    if(plain_len > 0) {
        chacha20_xor_stream(key, nonce, ciphertext_and_tag, plain, plain_len);
    }
    return 1;
}

void
KsyncCryptoRandom(uint8_t *out, size_t len)
{
    static uint32_t fallback_state;
    FILE *file;
    size_t got;

    if(out == NULL || len == 0)
        return;
    file = fopen("/dev/urandom", "rb");
    if(file != NULL) {
        got = fread(out, 1, len, file);
        fclose(file);
        if(got == len)
            return;
    }
    /* fallback: hash of time + counter, adequate only as last resort */
    {
        uint8_t seed[16];
        uint32_t tick = (uint32_t)time(NULL);
        for(size_t off = 0; off < len; off += 32) {
            size_t n = len - off < 32 ? len - off : 32;
            uint8_t digest[32];
            fallback_state = fallback_state * 1103515245U + 12345U + 1;
            store_le32(seed, tick);
            store_le32(seed + 4, fallback_state);
            store_le32(seed + 8, (uint32_t)(uintptr_t)&fallback_state);
            store_le32(seed + 12, (uint32_t)off);
            KsyncCryptoSha256(seed, sizeof(seed), digest);
            memcpy(out + off, digest, n);
        }
    }
}

int
KsyncCryptoBytesToHex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    if(bytes == NULL || out == NULL || out_size < len * 2 + 1)
        return 0;
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
    return 1;
}

int
KsyncCryptoHexToBytes(const char *hex, uint8_t *out, size_t out_len)
{
    static const char digits[] = "0123456789abcdef";
    size_t len;

    if(hex == NULL || out == NULL)
        return 0;
    len = strlen(hex);
    if(len != out_len * 2)
        return 0;
    for(size_t i = 0; i < out_len; i++) {
        const char *hi = strchr(digits, tolower((unsigned char)hex[i * 2]));
        const char *lo = strchr(digits, tolower((unsigned char)hex[i * 2 + 1]));
        if(hi == NULL || lo == NULL)
            return 0;
        out[i] = (uint8_t)((hi - digits) << 4 | (lo - digits));
    }
    return 1;
}
