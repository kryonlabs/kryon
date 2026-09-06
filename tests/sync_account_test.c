#include "sync/account.h"
#include "sync_crypto.h"

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
check_last_error(const char *label, const char *want)
{
    const char *got = GetSyncAccountLastError();

    if(got != NULL && want != NULL && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "FAIL: %s got \"%s\" want \"%s\"\n", label,
            got != NULL ? got : "(null)", want != NULL ? want : "(null)");
    failures++;
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
make_account(SyncAccount *account)
{
    unsigned char public_key[1312];
    unsigned char private_key[2560];

    memset(account, 0, sizeof(*account));
    for(size_t i = 0; i < sizeof(public_key); i++)
        public_key[i] = (unsigned char)(i * 7U + 11U);
    for(size_t i = 0; i < sizeof(private_key); i++)
        private_key[i] = (unsigned char)(i * 17U + 3U);
    SyncCryptoSha256Hex(public_key, sizeof(public_key), account->public_id);
    bytes_to_hex_local(public_key, sizeof(public_key), account->public_key_hex,
                       sizeof(account->public_key_hex));
    bytes_to_hex_local(private_key, sizeof(private_key), account->private_key_hex,
                       sizeof(account->private_key_hex));
}

static void
test_export_parse_roundtrip(void)
{
    SyncAccount account;
    SyncAccount parsed;
    char text[SYNC_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    check_true("export text", ExportSyncAccountText(&account, text, sizeof(text)));
    check_true("generic header", strstr(text, "account-key-v1\n") == text);
    check_true("parse exported text", ParseSyncAccountText(text, &parsed));
    check_true("roundtrip public id", strcmp(parsed.public_id, account.public_id) == 0);
    check_true("roundtrip public key", strcmp(parsed.public_key_hex, account.public_key_hex) == 0);
    check_true("roundtrip private key",
               strcmp(parsed.private_key_hex, account.private_key_hex) == 0);
}

static void
test_old_imports(void)
{
    SyncAccount account;
    SyncAccount parsed;
    char text[SYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    char json[SYNC_ACCOUNT_EXPORT_TEXT_SIZE + 128];

    make_account(&account);
    snprintf(text, sizeof(text),
             "account-key-v1\nalgorithm=ML-DSA-44\npublic_key=%s\nsecret_key=%s\n",
             account.public_key_hex, account.private_key_hex);
    check_true("old secret_key import", ParseSyncAccountText(text, &parsed));
    check_true("old derived public id", strcmp(parsed.public_id, account.public_id) == 0);

    snprintf(text, sizeof(text),
             "account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("old account-key import", ParseSyncAccountText(text, &parsed));

    snprintf(text, sizeof(text),
             "lyra-account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("old lyra account-key import", ParseSyncAccountText(text, &parsed));

    snprintf(json, sizeof(json),
             "{\"exported_key\":\"ksync-account-key-v1\\nalgorithm=ML-DSA-44\\npublic_id=%s\\npublic_key=%s\\nprivate_key=%s\\n\"}",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("json exported_key import", ParseSyncAccountText(json, &parsed));
}

static void
test_reject_mismatch(void)
{
    SyncAccount account;
    char text[SYNC_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    account.public_id[0] = account.public_id[0] == '0' ? '1' : '0';
    snprintf(text, sizeof(text),
             "ksync-account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("reject mismatched public id", !ParseSyncAccountText(text, &account));
    check_last_error("mismatched public id error", "public_id does not match public_key");
}

static void
test_reject_diagnostics(void)
{
    SyncAccount account;

    check_true("reject empty text", !ParseSyncAccountText("", &account));
    check_last_error("empty text error", "account key file is empty");
    check_true("reject encrypted text",
               !ParseSyncAccountText("ksync-account-key-v2\nciphertext=00\n",
                                      &account));
    check_last_error("encrypted text error",
                     "account key is encrypted and needs a passphrase");
    check_true("reject missing exported key",
               !ParseSyncAccountText("{\"wrong\":\"field\"}", &account));
    check_last_error("missing exported_key error",
                     "JSON account key is missing exported_key");
    check_true("reject missing public key",
               !ParseSyncAccountText("ksync-account-key-v1\nprivate_key=aa\n",
                                      &account));
    check_last_error("missing public_key error", "missing public_key");
}

static void
test_encrypted_roundtrip(void)
{
    SyncAccount account;
    SyncAccount parsed;
    char text[SYNC_ACCOUNT_EXPORT_ENCRYPTED_TEXT_SIZE];

    make_account(&account);
    check_true("encrypted export", ExportSyncAccountTextEncrypted(&account, "hunter2", text, sizeof(text)));
    check_true("encrypted export marks v2 header",
               strncmp(text, "ksync-account-key-v2",
                       strlen("ksync-account-key-v2")) == 0);
    check_true("encrypted export hides private key", strstr(text, account.private_key_hex) == NULL);
    check_true("encrypted import", ParseSyncAccountTextEncrypted(text, "hunter2", &parsed));
    check_true("encrypted roundtrip id", strcmp(parsed.public_id, account.public_id) == 0);
    check_true("encrypted roundtrip public key", strcmp(parsed.public_key_hex, account.public_key_hex) == 0);
    check_true("encrypted roundtrip private key", strcmp(parsed.private_key_hex, account.private_key_hex) == 0);
    check_true("wrong passphrase rejected", !ParseSyncAccountTextEncrypted(text, "wrong", &parsed));
    check_true("tampered ciphertext rejected", !ParseSyncAccountTextEncrypted(text, "hunter2", &parsed) ||
          (text[strlen(text) - 1] ^= 1, !ParseSyncAccountTextEncrypted(text, "hunter2", &parsed)));
}

int
main(void)
{
    test_export_parse_roundtrip();
    test_old_imports();
    test_reject_mismatch();
    test_reject_diagnostics();
    test_encrypted_roundtrip();
    if(failures != 0)
        return 1;
    printf("sync account tests passed\n");
    return 0;
}
