#include "ksync_account.h"

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
    const char *got = GetKsyncAccountLastError();

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
make_account(KsyncAccount *account)
{
    unsigned char public_key[1312];
    unsigned char private_key[2560];

    memset(account, 0, sizeof(*account));
    for(size_t i = 0; i < sizeof(public_key); i++)
        public_key[i] = (unsigned char)(i * 7U + 11U);
    for(size_t i = 0; i < sizeof(private_key); i++)
        private_key[i] = (unsigned char)(i * 17U + 3U);
    KsyncSha256Hex(public_key, sizeof(public_key), account->public_id);
    bytes_to_hex_local(public_key, sizeof(public_key), account->public_key_hex,
                       sizeof(account->public_key_hex));
    bytes_to_hex_local(private_key, sizeof(private_key), account->private_key_hex,
                       sizeof(account->private_key_hex));
}

static void
test_export_parse_roundtrip(void)
{
    KsyncAccount account;
    KsyncAccount parsed;
    char text[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    check_true("export text", ExportKsyncAccountText(&account, text, sizeof(text)));
    check_true("generic header", strstr(text, "ksync-account-key-v1\n") == text);
    check_true("parse exported text", ParseKsyncAccountText(text, &parsed));
    check_true("roundtrip public id", strcmp(parsed.public_id, account.public_id) == 0);
    check_true("roundtrip public key", strcmp(parsed.public_key_hex, account.public_key_hex) == 0);
    check_true("roundtrip private key",
               strcmp(parsed.private_key_hex, account.private_key_hex) == 0);
}

static void
test_old_imports(void)
{
    KsyncAccount account;
    KsyncAccount parsed;
    char text[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    char json[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE + 128];

    make_account(&account);
    snprintf(text, sizeof(text),
             "account-key-v1\nalgorithm=ML-DSA-44\npublic_key=%s\nsecret_key=%s\n",
             account.public_key_hex, account.private_key_hex);
    check_true("old secret_key import", ParseKsyncAccountText(text, &parsed));
    check_true("old derived public id", strcmp(parsed.public_id, account.public_id) == 0);

    snprintf(text, sizeof(text),
             "account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("old account-key import", ParseKsyncAccountText(text, &parsed));

    snprintf(text, sizeof(text),
             "lyra-account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("old lyra account-key import", ParseKsyncAccountText(text, &parsed));

    snprintf(json, sizeof(json),
             "{\"exported_key\":\"ksync-account-key-v1\\nalgorithm=ML-DSA-44\\npublic_id=%s\\npublic_key=%s\\nprivate_key=%s\\n\"}",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("json exported_key import", ParseKsyncAccountText(json, &parsed));
}

static void
test_reject_mismatch(void)
{
    KsyncAccount account;
    char text[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];

    make_account(&account);
    account.public_id[0] = account.public_id[0] == '0' ? '1' : '0';
    snprintf(text, sizeof(text),
             "ksync-account-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
             account.public_id, account.public_key_hex, account.private_key_hex);
    check_true("reject mismatched public id", !ParseKsyncAccountText(text, &account));
    check_last_error("mismatched public id error", "public_id does not match public_key");
}

static void
test_reject_diagnostics(void)
{
    KsyncAccount account;

    check_true("reject empty text", !ParseKsyncAccountText("", &account));
    check_last_error("empty text error", "account key file is empty");
    check_true("reject encrypted text",
               !ParseKsyncAccountText("ksync-account-key-v2\nciphertext=00\n",
                                      &account));
    check_last_error("encrypted text error",
                     "account key is encrypted and needs a passphrase");
    check_true("reject missing exported key",
               !ParseKsyncAccountText("{\"wrong\":\"field\"}", &account));
    check_last_error("missing exported_key error",
                     "JSON account key is missing exported_key");
    check_true("reject missing public key",
               !ParseKsyncAccountText("ksync-account-key-v1\nprivate_key=aa\n",
                                      &account));
    check_last_error("missing public_key error", "missing public_key");
}

static void
test_encrypted_roundtrip(void)
{
    KsyncAccount account;
    KsyncAccount parsed;
    char text[KSYNC_ACCOUNT_EXPORT_ENCRYPTED_TEXT_SIZE];

    make_account(&account);
    check_true("encrypted export", ExportKsyncAccountTextEncrypted(&account, "hunter2", text, sizeof(text)));
    check_true("encrypted export marks v2 header", strncmp(text, "ksync-account-key-v2", 20) == 0);
    check_true("encrypted export hides private key", strstr(text, account.private_key_hex) == NULL);
    check_true("encrypted import", ParseKsyncAccountTextEncrypted(text, "hunter2", &parsed));
    check_true("encrypted roundtrip id", strcmp(parsed.public_id, account.public_id) == 0);
    check_true("encrypted roundtrip public key", strcmp(parsed.public_key_hex, account.public_key_hex) == 0);
    check_true("encrypted roundtrip private key", strcmp(parsed.private_key_hex, account.private_key_hex) == 0);
    check_true("wrong passphrase rejected", !ParseKsyncAccountTextEncrypted(text, "wrong", &parsed));
    check_true("tampered ciphertext rejected", !ParseKsyncAccountTextEncrypted(text, "hunter2", &parsed) ||
          (text[strlen(text) - 1] ^= 1, !ParseKsyncAccountTextEncrypted(text, "hunter2", &parsed)));
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
    printf("ksync account tests passed\n");
    return 0;
}
