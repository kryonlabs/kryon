#include "lyra_account.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void
check_true(const char *label, int value)
{
    if(!value) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
}

static void
bytes_to_hex_local(const unsigned char *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    if(out == NULL || out_size < len * 2 + 1)
        return;
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static void
make_account(LyraAccount *account)
{
    unsigned char public_key[1312];
    unsigned char private_key[2560];

    memset(account, 0, sizeof(*account));
    for(size_t i = 0; i < sizeof(public_key); i++)
        public_key[i] = (unsigned char)(i * 7U + 11U);
    for(size_t i = 0; i < sizeof(private_key); i++)
        private_key[i] = (unsigned char)(i * 17U + 3U);
    LyraSha256Hex(public_key, sizeof(public_key), account->public_id);
    bytes_to_hex_local(public_key, sizeof(public_key), account->public_key_hex,
                       sizeof(account->public_key_hex));
    bytes_to_hex_local(private_key, sizeof(private_key), account->private_key_hex,
                       sizeof(account->private_key_hex));
}

static void
test_export_parse_roundtrip(void)
{
    LyraAccount account;
    LyraAccount parsed;
    char text[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    check_true("export text", ExportLyraAccountText(&account, text, sizeof(text)));
    check_true("generic header", strstr(text, "lyra-account-key-v1\n") == text);
    check_true("parse exported text", ParseLyraAccountText(text, &parsed));
    check_true("roundtrip public id", strcmp(parsed.public_id, account.public_id) == 0);
    check_true("roundtrip public key", strcmp(parsed.public_key_hex, account.public_key_hex) == 0);
    check_true("roundtrip private key",
               strcmp(parsed.private_key_hex, account.private_key_hex) == 0);
}

static void
test_old_imports(void)
{
    LyraAccount account;
    LyraAccount parsed;
    char text[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    char json[LYRA_ACCOUNT_EXPORT_TEXT_SIZE + 128];

    make_account(&account);
    snprintf(text, sizeof(text),
             "inbe-sync-key-v1\nalgorithm=ML-DSA-44\npublic_key=%s\nsecret_key=%s\n",
             account.public_key_hex, account.private_key_hex);
    check_true("old secret_key import", ParseLyraAccountText(text, &parsed));
    check_true("old derived public id", strcmp(parsed.public_id, account.public_id) == 0);

    snprintf(text, sizeof(text),
             "account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("old account-key import", ParseLyraAccountText(text, &parsed));

    snprintf(json, sizeof(json),
             "{\"exported_key\":\"lyra-account-key-v1\\nalgorithm=ML-DSA-44\\npublic_id=%s\\npublic_key=%s\\nprivate_key=%s\\n\"}",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("json exported_key import", ParseLyraAccountText(json, &parsed));
}

static void
test_reject_mismatch(void)
{
    LyraAccount account;
    char text[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    account.public_id[0] = account.public_id[0] == '0' ? '1' : '0';
    snprintf(text, sizeof(text),
             "lyra-account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("reject mismatched public id", !ParseLyraAccountText(text, &account));
}

int
main(void)
{
    test_export_parse_roundtrip();
    test_old_imports();
    test_reject_mismatch();
    if(failures != 0)
        return 1;
    printf("lyra account tests passed\n");
    return 0;
}
