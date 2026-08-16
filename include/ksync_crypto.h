#ifndef KSYNC_CRYPTO_H
#define KSYNC_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

void KsyncCryptoSha256(const uint8_t *data, size_t len, uint8_t out[32]);
void KsyncCryptoHmacSha256(const uint8_t *key, size_t key_len,
                           const uint8_t *data, size_t data_len,
                           uint8_t out[32]);
void KsyncCryptoPbkdf2Sha256(const uint8_t *password, size_t password_len,
                             const uint8_t *salt, size_t salt_len,
                             unsigned long iterations, uint8_t out[32]);

/* AEAD: ciphertext_and_tag must have plain_len + 16 bytes; returns 0 on
 * forgery or bad input. */
int KsyncCryptoChaCha20Poly1305Seal(const uint8_t key[32], const uint8_t nonce[12],
                                    const uint8_t *plain, size_t plain_len,
                                    const uint8_t *aad, size_t aad_len,
                                    uint8_t *ciphertext_and_tag);
int KsyncCryptoChaCha20Poly1305Open(const uint8_t key[32], const uint8_t nonce[12],
                                    const uint8_t *ciphertext_and_tag, size_t total_len,
                                    const uint8_t *aad, size_t aad_len,
                                    uint8_t *plain);

void KsyncCryptoRandom(uint8_t *out, size_t len);

int KsyncCryptoBytesToHex(const uint8_t *bytes, size_t len, char *out, size_t out_size);
int KsyncCryptoHexToBytes(const char *hex, uint8_t *out, size_t out_len);

#endif
