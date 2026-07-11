#include "lyra_account.h"

#if !defined(HAS_LIBOQS)
#error "Flint Lyra accounts require HAS_LIBOQS; build and link liboqs instead of disabling account crypto"
#endif

#include <oqs/oqs.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LYRA_ACCOUNT_KEY_HEADER "lyra-account-key-v1"
#define LYRA_LEGACY_UKU_KEY_HEADER "account-key-v1"
#define LYRA_LEGACY_INBE_KEY_HEADER "inbe-sync-key-v1"

typedef struct LyraSha256Ctx {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t data[64];
    size_t data_len;
} LyraSha256Ctx;

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t
rotr32(uint32_t value, uint32_t bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void
sha256_transform(LyraSha256Ctx *ctx, const uint8_t data[64])
{
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h;

    for(int i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               (uint32_t)data[i * 4 + 3];
    }
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(m[i - 15], 7) ^ rotr32(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr32(m[i - 2], 17) ^ rotr32(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for(int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + m[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void
sha256_init(LyraSha256Ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void
sha256_update(LyraSha256Ctx *ctx, const uint8_t *data, size_t len)
{
    for(size_t i = 0; i < len; i++) {
        ctx->data[ctx->data_len++] = data[i];
        if(ctx->data_len == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void
sha256_final(LyraSha256Ctx *ctx, uint8_t hash[32])
{
    size_t i = ctx->data_len;

    ctx->data[i++] = 0x80;
    if(i > 56) {
        while(i < 64)
            ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while(i < 56)
        ctx->data[i++] = 0;

    ctx->bit_len += ctx->data_len * 8;
    for(int j = 0; j < 8; j++)
        ctx->data[63 - j] = (uint8_t)(ctx->bit_len >> (j * 8));
    sha256_transform(ctx, ctx->data);

    for(i = 0; i < 4; i++) {
        for(int j = 0; j < 8; j++)
            hash[j * 4 + i] = (uint8_t)(ctx->state[j] >> (24 - i * 8));
    }
}

static void
bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
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

static int
hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    size_t len;

    if(hex == NULL || out == NULL)
        return 0;
    len = strlen(hex);
    if(len != out_len * 2)
        return 0;
    for(size_t i = 0; i < out_len; i++) {
        unsigned int value;
        if(sscanf(hex + i * 2, "%2x", &value) != 1)
            return 0;
        out[i] = (uint8_t)value;
    }
    return 1;
}

static int
hex_string_valid(const char *hex, size_t expected_len)
{
    if(hex == NULL || strlen(hex) != expected_len)
        return 0;
    for(size_t i = 0; i < expected_len; i++) {
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
parse_account_line(LyraAccount *account, const char *line, size_t line_len)
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
LyraSha256Hex(const uint8_t *data, size_t len, char out_hex[LYRA_PUBLIC_ID_HEX_SIZE])
{
    uint8_t digest[32];
    LyraSha256Ctx sha;

    if(out_hex == NULL)
        return;
    out_hex[0] = '\0';
    if(data == NULL && len > 0)
        return;
    sha256_init(&sha);
    sha256_update(&sha, data, len);
    sha256_final(&sha, digest);
    bytes_to_hex(digest, sizeof(digest), out_hex, LYRA_PUBLIC_ID_HEX_SIZE);
}

int
IsLyraAccountAvailable(void)
{
    return 1;
}

int
HasLyraAccountValues(const LyraAccount *account)
{
    return account != NULL && account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' && account->private_key_hex[0] != '\0';
}

int
ValidateLyraAccount(LyraAccount *account)
{
    uint8_t public_key[1312];
    char expected_public_id[LYRA_PUBLIC_ID_HEX_SIZE];

    if(account == NULL)
        return 0;
    if(!hex_string_valid(account->public_key_hex, 2624) ||
       !hex_string_valid(account->private_key_hex, 5120))
        return 0;
    if(!hex_to_bytes(account->public_key_hex, public_key, sizeof(public_key)))
        return 0;
    LyraSha256Hex(public_key, sizeof(public_key), expected_public_id);
    if(account->public_id[0] == '\0') {
        snprintf(account->public_id, sizeof(account->public_id), "%s", expected_public_id);
        return 1;
    }
    if(!hex_string_valid(account->public_id, 64))
        return 0;
    return strcmp(account->public_id, expected_public_id) == 0;
}

int
ParseLyraAccountText(const char *text, LyraAccount *account)
{
    const char *line;
    const char *next;
    char exported_key[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];

    if(text == NULL || account == NULL)
        return 0;
    if(extract_json_string_field(text, "exported_key", exported_key, sizeof(exported_key)))
        text = exported_key;

    memset(account, 0, sizeof(*account));
    line = text;
    if((unsigned char)line[0] == 0xef && (unsigned char)line[1] == 0xbb &&
       (unsigned char)line[2] == 0xbf)
        line += 3;
    while(*line != '\0') {
        next = strchr(line, '\n');
        if(next == NULL) {
            parse_account_line(account, line, strlen(line));
            break;
        }
        parse_account_line(account, line, (size_t)(next - line));
        line = next + 1;
    }
    return ValidateLyraAccount(account);
}

int
ExportLyraAccountText(const LyraAccount *account, char *out, size_t out_size)
{
    int len;

    if(!HasLyraAccountValues(account) || out == NULL || out_size == 0)
        return 0;
    len = snprintf(out, out_size,
                   LYRA_ACCOUNT_KEY_HEADER
                   "\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
                   account->public_id, account->public_key_hex, account->private_key_hex);
    return len > 0 && (size_t)len < out_size;
}

int
ImportLyraAccountFile(const char *filename, LyraAccount *account)
{
    char *body = read_file_text(filename);
    int ok;

    if(body == NULL)
        return 0;
    ok = ParseLyraAccountText(body, account);
    free(body);
    return ok;
}

int
ExportLyraAccountFile(const LyraAccount *account, const char *filename)
{
    char body[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    FILE *file;
    size_t len;
    int ok;

    if(filename == NULL || filename[0] == '\0' ||
       !ExportLyraAccountText(account, body, sizeof(body)))
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
CreateLyraAccount(LyraAccount *account)
{
    OQS_SIG *sig;
    uint8_t public_key[1312];
    uint8_t private_key[2560];
    LyraAccount generated;

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

    LyraSha256Hex(public_key, sizeof(public_key), generated.public_id);
    bytes_to_hex(public_key, sizeof(public_key), generated.public_key_hex,
                 sizeof(generated.public_key_hex));
    bytes_to_hex(private_key, sizeof(private_key), generated.private_key_hex,
                 sizeof(generated.private_key_hex));
    *account = generated;
    return 1;
}

int
SignLyraAccountHex(const LyraAccount *account, const uint8_t *message,
                            size_t message_len, char *out_signature_hex, size_t out_size)
{
    OQS_SIG *sig;
    uint8_t private_key[2560];
    uint8_t signature[2420];
    size_t signature_len = 0;

    if(!HasLyraAccountValues(account) || out_signature_hex == NULL ||
       out_size < sizeof(signature) * 2 + 1 || (message == NULL && message_len > 0))
        return 0;
    out_signature_hex[0] = '\0';
    if(!hex_to_bytes(account->private_key_hex, private_key, sizeof(private_key)))
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
    bytes_to_hex(signature, sizeof(signature), out_signature_hex, out_size);
    return out_signature_hex[0] != '\0';
}
