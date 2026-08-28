#include "ksync_account.h"
#include "ksync_crypto.h"

#if !defined(HAS_LIBOQS)
#error "Kryon Ksync accounts require HAS_LIBOQS; build and link liboqs instead of disabling account crypto"
#endif

#include <oqs/oqs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KSYNC_ACCOUNT_KEY_HEADER "ksync-account-key-v1"
#define KSYNC_LEGACY_UKU_KEY_HEADER "account-key-v1"
#define KSYNC_LEGACY_INBE_KEY_HEADER "inbe-sync-key-v1"
#define KSYNC_ACCOUNT_KEY_V2_HEADER "ksync-account-key-v2"

static char g_ksync_account_last_error[192];

static void
set_account_error(const char *message)
{
    if(message == NULL)
        message = "unknown account key error";
    snprintf(g_ksync_account_last_error, sizeof(g_ksync_account_last_error),
             "%s", message);
}

const char *
GetKsyncAccountLastError(void)
{
    return g_ksync_account_last_error[0] != '\0' ?
        g_ksync_account_last_error : "unknown account key error";
}

static int
hex_chars_valid(const char *hex)
{
    if(hex == NULL || hex[0] == '\0')
        return 0;
    for(size_t i = 0; hex[i] != '\0'; i++) {
        char c = hex[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

static void
copy_key_value(char *out, size_t out_size, const char *value, size_t value_len)
{
    if(out == NULL || out_size == 0)
        return;
    while(value_len > 0 && (*value == ' ' || *value == '\t')) {
        value++;
        value_len--;
    }
    while(value_len > 0 && (value[value_len - 1] == '\n' || value[value_len - 1] == '\r' ||
                            value[value_len - 1] == ' ' || value[value_len - 1] == '\t'))
        value_len--;
    if(value_len >= out_size)
        value_len = out_size - 1;
    memcpy(out, value, value_len);
    out[value_len] = '\0';
}

static void
parse_account_line(KsyncAccount *account, const char *line, size_t line_len)
{
    if(account == NULL || line == NULL)
        return;
    if(line_len > 0 && line[line_len - 1] == '\r')
        line_len--;
    if(line_len > 10 && strncmp(line, "public_id=", 10) == 0)
        copy_key_value(account->public_id, sizeof(account->public_id), line + 10, line_len - 10);
    else if(line_len > 11 && strncmp(line, "public_key=", 11) == 0)
        copy_key_value(account->public_key_hex, sizeof(account->public_key_hex), line + 11, line_len - 11);
    else if(line_len > 12 && strncmp(line, "private_key=", 12) == 0)
        copy_key_value(account->private_key_hex, sizeof(account->private_key_hex), line + 12, line_len - 12);
    else if(line_len > 11 && strncmp(line, "secret_key=", 11) == 0)
        copy_key_value(account->private_key_hex, sizeof(account->private_key_hex), line + 11, line_len - 11);
}

static int
extract_json_string_field(const char *json, const char *field, char *out, size_t out_size)
{
    char key[64];
    const char *p;
    char *dst;
    char *end;

    if(json == NULL || field == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(strlen(field) + 3 > sizeof(key))
        return 0;

    snprintf(key, sizeof(key), "\"%s\"", field);
    p = strstr(json, key);
    if(p == NULL)
        return 0;
    p += strlen(key);
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p != ':')
        return 0;
    p++;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p != '"')
        return 0;
    p++;

    dst = out;
    end = out + out_size - 1;
    while(*p != '\0' && *p != '"' && dst < end) {
        if(*p == '\\') {
            p++;
            if(*p == 'n')
                *dst++ = '\n';
            else if(*p == 'r')
                *dst++ = '\r';
            else if(*p == 't')
                *dst++ = '\t';
            else if(*p == '\\' || *p == '"')
                *dst++ = *p;
            else
                return 0;
            if(*p != '\0')
                p++;
            continue;
        }
        *dst++ = *p++;
    }
    *dst = '\0';
    return *p == '"';
}

static char *
read_file_text(const char *filename)
{
    FILE *file;
    long len;
    char *body;

    if(filename == NULL || filename[0] == '\0')
        return NULL;
    file = fopen(filename, "rb");
    if(file == NULL)
        return NULL;
    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    len = ftell(file);
    if(len < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    body = (char *)malloc((size_t)len + 1);
    if(body == NULL) {
        fclose(file);
        return NULL;
    }
    if(fread(body, 1, (size_t)len, file) != (size_t)len) {
        free(body);
        fclose(file);
        return NULL;
    }
    body[len] = '\0';
    fclose(file);
    return body;
}

void
KsyncSha256Hex(const uint8_t *data, size_t len, char out_hex[KSYNC_PUBLIC_ID_HEX_SIZE])
{
    uint8_t digest[32];

    if(out_hex == NULL)
        return;
    out_hex[0] = '\0';
    if(data == NULL && len > 0)
        return;
    KsyncCryptoSha256(data, len, digest);
    KsyncCryptoBytesToHex(digest, sizeof(digest), out_hex, KSYNC_PUBLIC_ID_HEX_SIZE);
}

int
IsKsyncAccountAvailable(void)
{
    return 1;
}

int
HasKsyncAccountValues(const KsyncAccount *account)
{
    return account != NULL && account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' && account->private_key_hex[0] != '\0';
}

int
ValidateKsyncAccount(KsyncAccount *account)
{
    uint8_t public_key[1312];
    char expected_public_id[KSYNC_PUBLIC_ID_HEX_SIZE];

    if(account == NULL) {
        set_account_error("no account object was provided");
        return 0;
    }
    if(account->public_key_hex[0] == '\0') {
        set_account_error("missing public_key");
        return 0;
    }
    if(strlen(account->public_key_hex) != 2624) {
        set_account_error("public_key has the wrong length");
        return 0;
    }
    if(!hex_chars_valid(account->public_key_hex)) {
        set_account_error("public_key is not valid hex");
        return 0;
    }
    if(account->private_key_hex[0] == '\0') {
        set_account_error("missing private_key");
        return 0;
    }
    if(strlen(account->private_key_hex) != 5120) {
        set_account_error("private_key has the wrong length");
        return 0;
    }
    if(!hex_chars_valid(account->private_key_hex)) {
        set_account_error("private_key is not valid hex");
        return 0;
    }
    if(!KsyncCryptoHexToBytes(account->public_key_hex, public_key, sizeof(public_key))) {
        set_account_error("public_key could not be decoded");
        return 0;
    }
    KsyncSha256Hex(public_key, sizeof(public_key), expected_public_id);
    if(account->public_id[0] == '\0') {
        snprintf(account->public_id, sizeof(account->public_id), "%s", expected_public_id);
        g_ksync_account_last_error[0] = '\0';
        return 1;
    }
    if(strlen(account->public_id) != 64) {
        set_account_error("public_id has the wrong length");
        return 0;
    }
    if(!hex_chars_valid(account->public_id)) {
        set_account_error("public_id is not valid hex");
        return 0;
    }
    if(strcmp(account->public_id, expected_public_id) != 0) {
        set_account_error("public_id does not match public_key");
        return 0;
    }
    g_ksync_account_last_error[0] = '\0';
    return 1;
}

int
ParseKsyncAccountText(const char *text, KsyncAccount *account)
{
    const char *line;
    const char *next;
    char exported_key[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];

    if(text == NULL || account == NULL) {
        set_account_error("no account key text was provided");
        return 0;
    }
    if(extract_json_string_field(text, "exported_key", exported_key, sizeof(exported_key)))
        text = exported_key;

    memset(account, 0, sizeof(*account));
    line = text;
    if((unsigned char)line[0] == 0xef && (unsigned char)line[1] == 0xbb &&
       (unsigned char)line[2] == 0xbf)
        line += 3;
    if(line[0] == '\0') {
        set_account_error("account key file is empty");
        return 0;
    }
    if(strstr(line, KSYNC_ACCOUNT_KEY_V2_HEADER) == line) {
        set_account_error("account key is encrypted and needs a passphrase");
        return 0;
    }
    if(line[0] == '{' && exported_key[0] == '\0') {
        set_account_error("JSON account key is missing exported_key");
        return 0;
    }
    while(*line != '\0') {
        next = strchr(line, '\n');
        if(next == NULL) {
            parse_account_line(account, line, strlen(line));
            break;
        }
        parse_account_line(account, line, (size_t)(next - line));
        line = next + 1;
    }
    return ValidateKsyncAccount(account);
}

int
ExportKsyncAccountText(const KsyncAccount *account, char *out, size_t out_size)
{
    int len;

    if(!HasKsyncAccountValues(account) || out == NULL || out_size == 0)
        return 0;
    len = snprintf(out, out_size,
                   KSYNC_ACCOUNT_KEY_HEADER
                   "\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
                   account->public_id, account->public_key_hex, account->private_key_hex);
    return len > 0 && (size_t)len < out_size;
}

int
ImportKsyncAccountFile(const char *filename, KsyncAccount *account)
{
    char *body = read_file_text(filename);
    int ok;

    if(filename == NULL || filename[0] == '\0') {
        set_account_error("no account key file was selected");
        return 0;
    }
    if(body == NULL) {
        set_account_error("could not read selected account key file");
        return 0;
    }
    ok = ParseKsyncAccountText(body, account);
    free(body);
    return ok;
}

int
ExportKsyncAccountFile(const KsyncAccount *account, const char *filename)
{
    char body[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    FILE *file;
    size_t len;
    int ok;

    if(filename == NULL || filename[0] == '\0' ||
       !ExportKsyncAccountText(account, body, sizeof(body)))
        return 0;
    file = fopen(filename, "wb");
    if(file == NULL)
        return 0;
    len = strlen(body);
    ok = fwrite(body, 1, len, file) == len;
    if(fclose(file) != 0)
        ok = 0;
    return ok;
}

int
CreateKsyncAccount(KsyncAccount *account)
{
    OQS_SIG *sig;
    uint8_t public_key[1312];
    uint8_t private_key[2560];
    KsyncAccount generated;

    if(account == NULL)
        return 0;
    memset(&generated, 0, sizeof(generated));
    sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_44);
    if(sig == NULL || sig->length_public_key != sizeof(public_key) ||
       sig->length_secret_key != sizeof(private_key)) {
        if(sig != NULL)
            OQS_SIG_free(sig);
        return 0;
    }
    if(OQS_SIG_keypair(sig, public_key, private_key) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return 0;
    }
    OQS_SIG_free(sig);

    KsyncSha256Hex(public_key, sizeof(public_key), generated.public_id);
    KsyncCryptoBytesToHex(public_key, sizeof(public_key), generated.public_key_hex,
                 sizeof(generated.public_key_hex));
    KsyncCryptoBytesToHex(private_key, sizeof(private_key), generated.private_key_hex,
                 sizeof(generated.private_key_hex));
    *account = generated;
    return 1;
}

int
SignKsyncAccountHex(const KsyncAccount *account, const uint8_t *message,
                            size_t message_len, char *out_signature_hex, size_t out_size)
{
    OQS_SIG *sig;
    uint8_t private_key[2560];
    uint8_t signature[2420];
    size_t signature_len = 0;

    if(!HasKsyncAccountValues(account) || out_signature_hex == NULL ||
       out_size < sizeof(signature) * 2 + 1 || (message == NULL && message_len > 0))
        return 0;
    out_signature_hex[0] = '\0';
    if(!KsyncCryptoHexToBytes(account->private_key_hex, private_key, sizeof(private_key)))
        return 0;
    sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_44);
    if(sig == NULL || sig->length_secret_key != sizeof(private_key) ||
       sig->length_signature != sizeof(signature)) {
        if(sig != NULL)
            OQS_SIG_free(sig);
        return 0;
    }
    if(OQS_SIG_sign(sig, signature, &signature_len, message, message_len, private_key) !=
           OQS_SUCCESS ||
       signature_len != sizeof(signature)) {
        OQS_SIG_free(sig);
        return 0;
    }
    OQS_SIG_free(sig);
    KsyncCryptoBytesToHex(signature, sizeof(signature), out_signature_hex, out_size);
    return out_signature_hex[0] != '\0';
}

/* ------------------------------------------------------------------ */
/* Passphrase-protected export (v2)                                    */
/* ------------------------------------------------------------------ */

#define KSYNC_ACCOUNT_SALT_BYTES 16

static int
account_find_line_value(const char *text, const char *prefix, char *out,
                        size_t out_size)
{
    size_t prefix_len = strlen(prefix);
    const char *line = text;

    if(text == NULL || out == NULL || out_size == 0)
        return 0;
    while(line != NULL && *line != '\0') {
        const char *next = strchr(line, '\n');
        size_t line_len = next != NULL ? (size_t)(next - line) : strlen(line);
        if(line_len > 0 && line[line_len - 1] == '\r')
            line_len--;
        if(line_len > prefix_len && strncmp(line, prefix, prefix_len) == 0) {
            copy_key_value(out, out_size, line + prefix_len, line_len - prefix_len);
            return out[0] != '\0';
        }
        line = next != NULL ? next + 1 : NULL;
    }
    return 0;
}

static void
account_derive_passphrase_key(const char *passphrase, const uint8_t *salt,
                              size_t salt_len, unsigned long iterations,
                              uint8_t out[32])
{
    static const char info[] = "ksync-account-key-v2";

    if(passphrase == NULL)
        passphrase = "";
    /* domain-separated salt: fixed label || random salt */
    {
        uint8_t salted[64];
        size_t used = 0;
        size_t info_len = sizeof(info) - 1;
        memcpy(salted + used, info, info_len < sizeof(salted) ? info_len : sizeof(salted));
        used += info_len;
        if(salt != NULL && salt_len > 0 && used < sizeof(salted)) {
            size_t n = salt_len < sizeof(salted) - used ? salt_len : sizeof(salted) - used;
            memcpy(salted + used, salt, n);
            used += n;
        }
        KsyncCryptoPbkdf2Sha256((const uint8_t *)passphrase, strlen(passphrase),
                                salted, used, iterations, out);
    }
}

int
ExportKsyncAccountTextEncrypted(const KsyncAccount *account, const char *passphrase,
                                char *out, size_t out_size)
{
    char plaintext[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    uint8_t salt[KSYNC_ACCOUNT_SALT_BYTES];
    uint8_t nonce[12];
    uint8_t key[32];
    uint8_t *sealed = NULL;
    char salt_hex[KSYNC_ACCOUNT_SALT_BYTES * 2 + 1];
    char nonce_hex[12 * 2 + 1];
    size_t sealed_len;
    int len;

    if(!HasKsyncAccountValues(account) || passphrase == NULL ||
       passphrase[0] == '\0' || out == NULL || out_size == 0)
        return 0;
    if(!ExportKsyncAccountText(account, plaintext, sizeof(plaintext)))
        return 0;
    KsyncCryptoRandom(salt, sizeof(salt));
    KsyncCryptoRandom(nonce, sizeof(nonce));
    account_derive_passphrase_key(passphrase, salt, sizeof(salt),
                                  KSYNC_ACCOUNT_PASSPHRASE_ITERATIONS, key);
    sealed_len = strlen(plaintext) + 16;
    sealed = (uint8_t *)malloc(sealed_len);
    if(sealed == NULL)
        return 0;
    if(!KsyncCryptoChaCha20Poly1305Seal(key, nonce, (const uint8_t *)plaintext,
                                        strlen(plaintext), NULL, 0, sealed)) {
        free(sealed);
        return 0;
    }
    KsyncCryptoBytesToHex(salt, sizeof(salt), salt_hex, sizeof(salt_hex));
    KsyncCryptoBytesToHex(nonce, sizeof(nonce), nonce_hex, sizeof(nonce_hex));
    len = snprintf(out, out_size,
                   KSYNC_ACCOUNT_KEY_V2_HEADER
                   "\nalgorithm=ML-DSA-44\nkdf=PBKDF2-SHA256\niterations=%d\n"
                   "salt=%s\nnonce=%s\nciphertext=",
                   KSYNC_ACCOUNT_PASSPHRASE_ITERATIONS, salt_hex, nonce_hex);
    if(len < 0 || (size_t)len >= out_size) {
        free(sealed);
        return 0;
    }
    {
        char *cursor = out + len;
        size_t remaining = out_size - (size_t)len;
        if(!KsyncCryptoBytesToHex(sealed, sealed_len, cursor, remaining)) {
            free(sealed);
            return 0;
        }
    }
    free(sealed);
    return 1;
}

int
ParseKsyncAccountTextEncrypted(const char *text, const char *passphrase,
                               KsyncAccount *account)
{
    char iterations_text[32];
    char salt_hex[KSYNC_ACCOUNT_SALT_BYTES * 2 + 1];
    char nonce_hex[12 * 2 + 1];
    char *ciphertext_hex = (char *)malloc(KSYNC_ACCOUNT_EXPORT_TEXT_SIZE * 2 + 64);
    char *plaintext;
    uint8_t salt[KSYNC_ACCOUNT_SALT_BYTES];
    uint8_t nonce[12];
    uint8_t key[32];
    uint8_t *sealed = NULL;
    unsigned long iterations;
    size_t sealed_len;
    int ok = 0;

    plaintext = (char *)malloc(KSYNC_ACCOUNT_EXPORT_TEXT_SIZE);
    if(ciphertext_hex == NULL || plaintext == NULL) {
        free(ciphertext_hex);
        free(plaintext);
        return 0;
    }
    if(text == NULL || passphrase == NULL || account == NULL) {
        free(ciphertext_hex);
        free(plaintext);
        return 0;
    }
    if(strstr(text, KSYNC_ACCOUNT_KEY_V2_HEADER) != text) {
        free(ciphertext_hex);
        free(plaintext);
        return 0;
    }
    if(!account_find_line_value(text, "iterations=", iterations_text,
                                sizeof(iterations_text)) ||
       !account_find_line_value(text, "salt=", salt_hex, sizeof(salt_hex)) ||
       !account_find_line_value(text, "nonce=", nonce_hex, sizeof(nonce_hex)) ||
       !account_find_line_value(text, "ciphertext=", ciphertext_hex,
                                KSYNC_ACCOUNT_EXPORT_TEXT_SIZE * 2 + 64))
        goto fail;
    iterations = strtoul(iterations_text, NULL, 10);
    if(iterations == 0 || iterations > 100000000UL)
        goto fail;
    if(!KsyncCryptoHexToBytes(salt_hex, salt, sizeof(salt)) ||
       !KsyncCryptoHexToBytes(nonce_hex, nonce, sizeof(nonce)))
        goto fail;
    sealed_len = strlen(ciphertext_hex) / 2;
    if(sealed_len <= 16 || sealed_len > KSYNC_ACCOUNT_EXPORT_TEXT_SIZE)
        goto fail;
    sealed = (uint8_t *)malloc(sealed_len);
    if(sealed == NULL)
        goto fail;
    if(KsyncCryptoHexToBytes(ciphertext_hex, sealed, sealed_len)) {
        account_derive_passphrase_key(passphrase, salt, sizeof(salt), iterations, key);
        if(KsyncCryptoChaCha20Poly1305Open(key, nonce, sealed, sealed_len,
                                           NULL, 0, (uint8_t *)plaintext)) {
            size_t plain_len = sealed_len - 16;
            plaintext[plain_len] = '\0';
            ok = ParseKsyncAccountText(plaintext, account);
        }
    }
    free(sealed);
    free(ciphertext_hex);
    free(plaintext);
    return ok;
fail:
    free(sealed);
    free(ciphertext_hex);
    free(plaintext);
    return 0;
}

int
ExportKsyncAccountFileEncrypted(const KsyncAccount *account, const char *passphrase,
                                const char *filename)
{
    char body[KSYNC_ACCOUNT_EXPORT_ENCRYPTED_TEXT_SIZE];
    FILE *file;
    size_t len;
    int ok;

    if(filename == NULL || filename[0] == '\0' ||
       !ExportKsyncAccountTextEncrypted(account, passphrase, body, sizeof(body)))
        return 0;
    file = fopen(filename, "wb");
    if(file == NULL)
        return 0;
    len = strlen(body);
    ok = fwrite(body, 1, len, file) == len;
    if(fclose(file) != 0)
        ok = 0;
    return ok;
}

int
ImportKsyncAccountFileEncrypted(const char *filename, const char *passphrase,
                                KsyncAccount *account)
{
    char *body = read_file_text(filename);
    int ok;

    if(body == NULL)
        return 0;
    ok = ParseKsyncAccountTextEncrypted(body, passphrase, account);
    free(body);
    return ok;
}
