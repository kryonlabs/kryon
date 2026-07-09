#ifndef LYRA_ACCOUNT_H
#define LYRA_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

enum {
    LYRA_PUBLIC_ID_HEX_SIZE = 65,
    LYRA_PUBLIC_KEY_HEX_SIZE = 2625,
    LYRA_PRIVATE_KEY_HEX_SIZE = 5121,
    LYRA_SIGNATURE_HEX_SIZE = 4841,
    LYRA_ACCOUNT_EXPORT_TEXT_SIZE = 8200
};

typedef struct LyraAccount {
    char public_id[LYRA_PUBLIC_ID_HEX_SIZE];
    char public_key_hex[LYRA_PUBLIC_KEY_HEX_SIZE];
    char private_key_hex[LYRA_PRIVATE_KEY_HEX_SIZE];
} LyraAccount;

int IsLyraAccountAvailable(void);
int HasLyraAccountValues(const LyraAccount *account);
int CreateLyraAccount(LyraAccount *account);
int ValidateLyraAccount(LyraAccount *account);
int ParseLyraAccountText(const char *text, LyraAccount *account);
int ExportLyraAccountText(const LyraAccount *account, char *out, size_t out_size);
int ImportLyraAccountFile(const char *filename, LyraAccount *account);
int ExportLyraAccountFile(const LyraAccount *account, const char *filename);
void LyraSha256Hex(const uint8_t *data, size_t len, char out_hex[LYRA_PUBLIC_ID_HEX_SIZE]);
int SignLyraAccountHex(const LyraAccount *account, const uint8_t *message,
                                size_t message_len, char *out_signature_hex,
                                size_t out_size);

#endif
