/*
 * kry_sha256.h - Kry standard library: SHA-256 (FIPS 180-4).
 *
 * Streaming interface plus a one-shot file hasher used by kry_update to
 * verify downloaded artifacts against an appcast digest. No external
 * dependencies; the ksync crypto stack intentionally stays separate.
 */
#ifndef KRYON_KRY_SHA256_H
#define KRYON_KRY_SHA256_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int state[8];
    unsigned long long bit_len;
    unsigned char buf[64];
    size_t buf_len;
} KrySha256;

void kry_sha256_init(KrySha256 *ctx);
void kry_sha256_update(KrySha256 *ctx, const void *data, size_t len);
void kry_sha256_final(KrySha256 *ctx, unsigned char out[32]);

/* Lowercase hex (64 chars + NUL) into `out` (65 bytes). Returns `out`. */
char *kry_sha256_hex(const unsigned char digest[32], char out[65]);

/* One-shot: hash a file in chunks. Returns 1 on success, 0 when the file
 * cannot be read. `hex_out` needs 65 bytes. */
int kry_sha256_file(const char *path, char hex_out[65]);

/* Constant-time comparison of two hex digests (case-insensitive). Returns
 * 1 when equal. NULL arguments compare unequal. */
int kry_sha256_hex_equal(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif
