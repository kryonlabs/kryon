#ifndef SYNC_ACCOUNT_H
#define SYNC_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

enum {
    SYNC_PUBLIC_ID_HEX_SIZE = 65,
    SYNC_PUBLIC_KEY_HEX_SIZE = 2625,
    SYNC_PRIVATE_KEY_HEX_SIZE = 5121,
    SYNC_SIGNATURE_HEX_SIZE = 4841,
    SYNC_ACCOUNT_EXPORT_TEXT_SIZE = 8200,
    /* v2 encrypted export: ciphertext is the v1 text plus a 16-byte tag,
     * hex encoded, plus header lines */
    SYNC_ACCOUNT_EXPORT_ENCRYPTED_TEXT_SIZE = 17664,
    SYNC_ACCOUNT_PASSPHRASE_ITERATIONS = 600000
};

typedef struct SyncAccount {
    char public_id[SYNC_PUBLIC_ID_HEX_SIZE];
    char public_key_hex[SYNC_PUBLIC_KEY_HEX_SIZE];
    char private_key_hex[SYNC_PRIVATE_KEY_HEX_SIZE];
} SyncAccount;

int IsSyncAccountAvailable(void);
int HasSyncAccountValues(const SyncAccount *account);
int CreateSyncAccount(SyncAccount *account);
int ValidateSyncAccount(SyncAccount *account);
const char *GetSyncAccountLastError(void);
int ParseSyncAccountText(const char *text, SyncAccount *account);
int ExportSyncAccountText(const SyncAccount *account, char *out, size_t out_size);
int ImportSyncAccountFile(const char *filename, SyncAccount *account);
int ExportSyncAccountFile(const SyncAccount *account, const char *filename);
/* Passphrase-protected export: PBKDF2-SHA256 key derivation plus
 * ChaCha20-Poly1305 authenticated encryption. The v1 plaintext export
 * remains available for compatibility. */
int ExportSyncAccountTextEncrypted(const SyncAccount *account, const char *passphrase,
                                    char *out, size_t out_size);
int ExportSyncAccountFileEncrypted(const SyncAccount *account, const char *passphrase,
                                    const char *filename);
int ParseSyncAccountTextEncrypted(const char *text, const char *passphrase,
                                   SyncAccount *account);
int ImportSyncAccountFileEncrypted(const char *filename, const char *passphrase,
                                    SyncAccount *account);
int SignSyncAccountHex(const SyncAccount *account, const uint8_t *message,
                                size_t message_len, char *out_signature_hex,
                                size_t out_size);

#endif
