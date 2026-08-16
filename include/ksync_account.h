#ifndef KSYNC_ACCOUNT_H
#define KSYNC_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

enum {
    KSYNC_PUBLIC_ID_HEX_SIZE = 65,
    KSYNC_PUBLIC_KEY_HEX_SIZE = 2625,
    KSYNC_PRIVATE_KEY_HEX_SIZE = 5121,
    KSYNC_SIGNATURE_HEX_SIZE = 4841,
    KSYNC_ACCOUNT_EXPORT_TEXT_SIZE = 8200,
    /* v2 encrypted export: ciphertext is the v1 text plus a 16-byte tag,
     * hex encoded, plus header lines */
    KSYNC_ACCOUNT_EXPORT_ENCRYPTED_TEXT_SIZE = 17664,
    KSYNC_ACCOUNT_PASSPHRASE_ITERATIONS = 600000
};

typedef struct KsyncAccount {
    char public_id[KSYNC_PUBLIC_ID_HEX_SIZE];
    char public_key_hex[KSYNC_PUBLIC_KEY_HEX_SIZE];
    char private_key_hex[KSYNC_PRIVATE_KEY_HEX_SIZE];
} KsyncAccount;

int IsKsyncAccountAvailable(void);
int HasKsyncAccountValues(const KsyncAccount *account);
int CreateKsyncAccount(KsyncAccount *account);
int ValidateKsyncAccount(KsyncAccount *account);
int ParseKsyncAccountText(const char *text, KsyncAccount *account);
int ExportKsyncAccountText(const KsyncAccount *account, char *out, size_t out_size);
int ImportKsyncAccountFile(const char *filename, KsyncAccount *account);
int ExportKsyncAccountFile(const KsyncAccount *account, const char *filename);
/* Passphrase-protected export: PBKDF2-SHA256 key derivation plus
 * ChaCha20-Poly1305 authenticated encryption. The v1 plaintext export
 * remains available for compatibility. */
int ExportKsyncAccountTextEncrypted(const KsyncAccount *account, const char *passphrase,
                                    char *out, size_t out_size);
int ExportKsyncAccountFileEncrypted(const KsyncAccount *account, const char *passphrase,
                                    const char *filename);
int ParseKsyncAccountTextEncrypted(const char *text, const char *passphrase,
                                   KsyncAccount *account);
int ImportKsyncAccountFileEncrypted(const char *filename, const char *passphrase,
                                    KsyncAccount *account);
void KsyncSha256Hex(const uint8_t *data, size_t len, char out_hex[KSYNC_PUBLIC_ID_HEX_SIZE]);
int SignKsyncAccountHex(const KsyncAccount *account, const uint8_t *message,
                                size_t message_len, char *out_signature_hex,
                                size_t out_size);

#endif
