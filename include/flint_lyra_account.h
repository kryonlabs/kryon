#ifndef FLINT_LYRA_ACCOUNT_H
#define FLINT_LYRA_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

enum {
    FLINT_LYRA_PUBLIC_ID_HEX_SIZE = 65,
    FLINT_LYRA_PUBLIC_KEY_HEX_SIZE = 2625,
    FLINT_LYRA_PRIVATE_KEY_HEX_SIZE = 5121,
    FLINT_LYRA_SIGNATURE_HEX_SIZE = 4841,
    FLINT_LYRA_ACCOUNT_EXPORT_TEXT_SIZE = 8200
};

typedef struct FlintLyraAccount {
    char public_id[FLINT_LYRA_PUBLIC_ID_HEX_SIZE];
    char public_key_hex[FLINT_LYRA_PUBLIC_KEY_HEX_SIZE];
    char private_key_hex[FLINT_LYRA_PRIVATE_KEY_HEX_SIZE];
} FlintLyraAccount;

int flint_lyra_account_available(void);
int flint_lyra_account_has_values(const FlintLyraAccount *account);
int flint_lyra_account_create(FlintLyraAccount *account);
int flint_lyra_account_validate(FlintLyraAccount *account);
int flint_lyra_account_parse_text(const char *text, FlintLyraAccount *account);
int flint_lyra_account_export_text(const FlintLyraAccount *account, char *out, size_t out_size);
int flint_lyra_account_import_file(const char *filename, FlintLyraAccount *account);
int flint_lyra_account_export_file(const FlintLyraAccount *account, const char *filename);
void flint_lyra_sha256_hex(const uint8_t *data, size_t len, char out_hex[FLINT_LYRA_PUBLIC_ID_HEX_SIZE]);
int flint_lyra_account_sign_hex(const FlintLyraAccount *account, const uint8_t *message,
                                size_t message_len, char *out_signature_hex,
                                size_t out_size);

#endif
