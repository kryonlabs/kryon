#ifndef SYNC_CRYPTO_H
#define SYNC_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

enum {
    SYNC_ED25519_PUBLIC_KEY_SIZE = 32,
    SYNC_ED25519_PRIVATE_KEY_SIZE = 64,
    SYNC_ED25519_SIGNATURE_SIZE = 64
};

void SyncCryptoSha256(const uint8_t *data, size_t len, uint8_t out[32]);
void SyncCryptoHmacSha256(const uint8_t *key, size_t key_len,
                           const uint8_t *data, size_t data_len,
                           uint8_t out[32]);
void SyncCryptoPbkdf2Sha256(const uint8_t *password, size_t password_len,
                             const uint8_t *salt, size_t salt_len,
                             unsigned long iterations, uint8_t out[32]);

/* AEAD: ciphertext_and_tag must have plain_len + 16 bytes; returns 0 on
 * forgery or bad input. */
int SyncCryptoChaCha20Poly1305Seal(const uint8_t key[32], const uint8_t nonce[12],
                                    const uint8_t *plain, size_t plain_len,
                                    const uint8_t *aad, size_t aad_len,
                                    uint8_t *ciphertext_and_tag);
int SyncCryptoChaCha20Poly1305Open(const uint8_t key[32], const uint8_t nonce[12],
                                    const uint8_t *ciphertext_and_tag, size_t total_len,
                                    const uint8_t *aad, size_t aad_len,
                                    uint8_t *plain);

void SyncCryptoRandom(uint8_t *out, size_t len);

int SyncCryptoBytesToHex(const uint8_t *bytes, size_t len, char *out, size_t out_size);
int SyncCryptoHexToBytes(const char *hex, uint8_t *out, size_t out_len);
int SyncCryptoCreateDeviceKey(uint8_t private_key[SYNC_ED25519_PRIVATE_KEY_SIZE],
                              uint8_t public_key[SYNC_ED25519_PUBLIC_KEY_SIZE]);
void SyncCryptoSignDevice(const uint8_t private_key[SYNC_ED25519_PRIVATE_KEY_SIZE],
                          const uint8_t *message, size_t message_len,
                          uint8_t signature[SYNC_ED25519_SIGNATURE_SIZE]);
int SyncCryptoVerifyDevice(const uint8_t public_key[SYNC_ED25519_PUBLIC_KEY_SIZE],
                           const uint8_t *message, size_t message_len,
                           const uint8_t signature[SYNC_ED25519_SIGNATURE_SIZE]);

#endif
