#include "ksync_sync.h"
#include "ksync_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KSYNC_SYNC_PATH "/api/v1/sync"
#define KSYNC_CHALLENGE_PATH "/api/v1/sync/challenge"
#define KSYNC_LOGIN_PATH "/api/v1/sync/login"
#define KSYNC_ACCOUNT_DELETE_WITH_KEY_PATH "/api/v1/account/delete-with-key"
#define KSYNC_ACCOUNT_DELETE_PATH "/api/v1/account/delete"
#define KSYNC_DELETE_SIGNATURE_CONTEXT "ksync-delete-v1"
#define KSYNC_SYNC_PAYLOAD_CONTEXT "ksync-payload-key-v1"
#define KSYNC_SYNC_AUTH_TOKEN_KEY "sync_auth_token"
#define KSYNC_SYNC_AUTH_TOKEN_EXPIRES_KEY "sync_auth_token_expires_at"
#define KSYNC_SYNC_CLOCK_SKEW_KEY "sync_clock_skew"
#define KSYNC_SYNC_SIGNATURE_CONTEXT "ksync-sync-v1"
#define KSYNC_SYNC_USER_HEADER "X-Ksync-User"
#define KSYNC_SYNC_SIGNATURE_HEADER "X-Ksync-Signature"
#define KSYNC_SYNC_PAYLOAD_COMPRESSION "lzss1"

const char *
GetKsyncSyncResultName(KsyncSyncResult result)
{
    switch(result) {
        case KSYNC_SYNC_OK:
            return "ok";
        case KSYNC_SYNC_INVALID_URL:
            return "invalid_url";
        case KSYNC_SYNC_NO_ACCOUNT:
            return "no_account";
        case KSYNC_SYNC_PAYLOAD_FAILED:
            return "payload_failed";
        case KSYNC_SYNC_CHALLENGE_FAILED:
            return "challenge_failed";
        case KSYNC_SYNC_SIGN_FAILED:
            return "sign_failed";
        case KSYNC_SYNC_REQUEST_FAILED:
            return "request_failed";
        case KSYNC_SYNC_AUTH_FAILED:
            return "auth_failed";
        default:
            return "unknown";
    }
}

static int
sync_has_prefix(const char *text, const char *prefix)
{
    return text != NULL && prefix != NULL &&
           strncmp(text, prefix, strlen(prefix)) == 0;
}

static int
sync_url_host_boundary(char ch)
{
    return ch == '\0' || ch == ':' || ch == '/' || ch == '?' || ch == '#';
}

static int
sync_parse_ipv4_host(const char *authority, int octets[4], size_t *host_len)
{
    size_t pos = 0;

    if(authority == NULL || octets == NULL || host_len == NULL)
        return 0;
    for(int part = 0; part < 4; part++) {
        int value = 0;
        int digits = 0;

        while(authority[pos] >= '0' && authority[pos] <= '9') {
            value = value * 10 + (authority[pos] - '0');
            if(value > 255 || ++digits > 3)
                return 0;
            pos++;
        }
        if(digits == 0)
            return 0;
        octets[part] = value;
        if(part < 3) {
            if(authority[pos] != '.')
                return 0;
            pos++;
        }
    }
    if(!sync_url_host_boundary(authority[pos]))
        return 0;
    *host_len = pos;
    return 1;
}

static int
sync_private_ipv4_authority_valid(const char *authority)
{
    int octets[4];
    size_t host_len;

    if(!sync_parse_ipv4_host(authority, octets, &host_len))
        return 0;
    (void)host_len;
    if(octets[0] == 10)
        return 1;
    if(octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31)
        return 1;
    if(octets[0] == 192 && octets[1] == 168)
        return 1;
    if(octets[0] == 169 && octets[1] == 254)
        return 1;
    return 0;
}

static int
sync_local_authority_valid(const char *authority)
{
    static const char *const hosts[] = {"localhost", "127.0.0.1", "10.0.2.2"};

    if(authority == NULL || authority[0] == '\0')
        return 0;
    for(size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
        size_t len = strlen(hosts[i]);
        if(strncmp(authority, hosts[i], len) == 0 &&
           sync_url_host_boundary(authority[len]))
            return 1;
    }
    return sync_private_ipv4_authority_valid(authority);
}

int
IsKsyncSyncURLValid(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return 0;
    if(sync_has_prefix(url, "https://"))
        return url[8] != '\0';
    if(sync_has_prefix(url, "http://"))
        return sync_local_authority_valid(url + 7);
    return sync_local_authority_valid(url);
}

int
NormalizeKsyncSyncURL(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!IsKsyncSyncURLValid(input))
        return 0;
    if(sync_has_prefix(input, "https://") || sync_has_prefix(input, "http://"))
        len = snprintf(out, out_size, "%s", input);
    else
        len = snprintf(out, out_size, "http://%s", input);
    return len > 0 && (size_t)len < out_size;
}

int
JoinKsyncSyncURL(char *out, size_t out_size, const char *base_url, const char *path)
{
    size_t len;
    int written;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(base_url == NULL || path == NULL)
        return 0;
    len = strlen(base_url);
    while(len > 0 && base_url[len - 1] == '/')
        len--;
    written = snprintf(out, out_size, "%.*s%s", (int)len, base_url, path);
    return written > 0 && (size_t)written < out_size;
}

int
JoinKsyncSyncWebSocketURL(char *out, size_t out_size, const char *base_url, const char *path)
{
    char http_url[768];
    const char *body;
    const char *scheme;
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!JoinKsyncSyncURL(http_url, sizeof(http_url), base_url, path))
        return 0;
    if(sync_has_prefix(http_url, "https://")) {
        scheme = "wss://";
        body = http_url + 8;
    } else if(sync_has_prefix(http_url, "http://")) {
        scheme = "ws://";
        body = http_url + 7;
    } else {
        return 0;
    }
    len = snprintf(out, out_size, "%s%s", scheme, body);
    return len > 0 && (size_t)len < out_size;
}

int
AppendKsyncSyncBuffer(KsyncSyncBuffer *buffer, const void *data, size_t bytes)
{
    char *next;
    size_t next_cap;

    if(buffer == NULL || data == NULL || bytes == 0)
        return 1;
    if(buffer->cap == 0 || bytes >= buffer->cap - buffer->len) {
        next_cap = buffer->cap > 0 ? buffer->cap : 1024;
        while(bytes >= next_cap - buffer->len)
            next_cap *= 2;
        next = (char *)realloc(buffer->data, next_cap);
        if(next == NULL)
            return 0;
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, data, bytes);
    buffer->len += bytes;
    buffer->data[buffer->len] = '\0';
    return 1;
}

int
AppendKsyncSyncBufferJSONString(KsyncSyncBuffer *buffer, const char *text)
{
    const char *p;

    if(!AppendKsyncSyncBuffer(buffer, "\"", 1))
        return 0;
    if(text == NULL)
        text = "";
    for(p = text; *p != '\0'; p++) {
        char escaped[2];
        switch(*p) {
            case '\\':
                if(!AppendKsyncSyncBuffer(buffer, "\\\\", 2))
                    return 0;
                break;
            case '"':
                if(!AppendKsyncSyncBuffer(buffer, "\\\"", 2))
                    return 0;
                break;
            case '\n':
                if(!AppendKsyncSyncBuffer(buffer, "\\n", 2))
                    return 0;
                break;
            case '\r':
                if(!AppendKsyncSyncBuffer(buffer, "\\r", 2))
                    return 0;
                break;
            case '\t':
                if(!AppendKsyncSyncBuffer(buffer, "\\t", 2))
                    return 0;
                break;
            case '\b':
                if(!AppendKsyncSyncBuffer(buffer, "\\b", 2))
                    return 0;
                break;
            case '\f':
                if(!AppendKsyncSyncBuffer(buffer, "\\f", 2))
                    return 0;
                break;
            default:
                if((unsigned char)*p < 0x20) {
                    char unicode_escape[8];
                    snprintf(unicode_escape, sizeof(unicode_escape), "\\u%04x",
                             (unsigned char)*p);
                    if(!AppendKsyncSyncBuffer(buffer, unicode_escape, 6))
                        return 0;
                } else {
                    escaped[0] = *p;
                    escaped[1] = '\0';
                    if(!AppendKsyncSyncBuffer(buffer, escaped, 1))
                        return 0;
                }
                break;
        }
    }
    return AppendKsyncSyncBuffer(buffer, "\"", 1);
}

void
FreeKsyncSyncBuffer(KsyncSyncBuffer *buffer)
{
    if(buffer == NULL)
        return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static void
sync_json_skip_ws(const char **pp)
{
    const char *p = *pp;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    *pp = p;
}

static int
sync_json_decode_escape(const char **pp, char *out, size_t out_size,
                        size_t *out_len)
{
    const char *p = *pp + 1; /* past backslash */
    char decoded[5];
    int decoded_len = 0;

    switch(*p) {
        case '"': decoded[0] = '"'; decoded_len = 1; break;
        case '\\': decoded[0] = '\\'; decoded_len = 1; break;
        case '/': decoded[0] = '/'; decoded_len = 1; break;
        case 'b': decoded[0] = '\b'; decoded_len = 1; break;
        case 'f': decoded[0] = '\f'; decoded_len = 1; break;
        case 'n': decoded[0] = '\n'; decoded_len = 1; break;
        case 'r': decoded[0] = '\r'; decoded_len = 1; break;
        case 't': decoded[0] = '\t'; decoded_len = 1; break;
        case 'u': {
            unsigned int cp = 0;
            for(int i = 1; i <= 4; i++) {
                char c = p[i];
                unsigned int v;
                if(c >= '0' && c <= '9')
                    v = (unsigned int)(c - '0');
                else if(c >= 'a' && c <= 'f')
                    v = (unsigned int)(c - 'a') + 10;
                else if(c >= 'A' && c <= 'F')
                    v = (unsigned int)(c - 'A') + 10;
                else
                    return 0;
                cp = cp * 16 + v;
            }
            p += 4;
            if(cp >= 0xd800 && cp <= 0xdbff && p[1] == '\\' && p[2] == 'u') {
                unsigned int lo = 0;

                for(int i = 3; i <= 6; i++) {
                    char c = p[i];
                    unsigned int v;
                    if(c >= '0' && c <= '9')
                        v = (unsigned int)(c - '0');
                    else if(c >= 'a' && c <= 'f')
                        v = (unsigned int)(c - 'a') + 10;
                    else if(c >= 'A' && c <= 'F')
                        v = (unsigned int)(c - 'A') + 10;
                    else
                        return 0;
                    lo = lo * 16 + v;
                }
                if(lo >= 0xdc00 && lo <= 0xdfff) {
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                    p += 6;
                }
            }
            if(cp < 0x80) {
                decoded[0] = (char)cp;
                decoded_len = 1;
            } else if(cp < 0x800) {
                decoded[0] = (char)(0xc0 | (cp >> 6));
                decoded[1] = (char)(0x80 | (cp & 0x3f));
                decoded_len = 2;
            } else if(cp < 0x10000) {
                decoded[0] = (char)(0xe0 | (cp >> 12));
                decoded[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
                decoded[2] = (char)(0x80 | (cp & 0x3f));
                decoded_len = 3;
            } else if(cp <= 0x10ffff) {
                decoded[0] = (char)(0xf0 | (cp >> 18));
                decoded[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
                decoded[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
                decoded[3] = (char)(0x80 | (cp & 0x3f));
                decoded_len = 4;
            } else {
                return 0;
            }
            break;
        }
        default:
            return 0;
    }
    if(*out_len + (size_t)decoded_len >= out_size)
        return 0;
    memcpy(out + *out_len, decoded, (size_t)decoded_len);
    *out_len += (size_t)decoded_len;
    *pp = p + 1;
    return 1;
}

int
FindKsyncSyncJSONString(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *candidate;
    size_t key_len;

    if(json == NULL || key == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    key_len = strlen(key);
    if(key_len + 2 >= sizeof(pattern))
        return 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    candidate = json;
    while((candidate = strstr(candidate, pattern)) != NULL) {
        const char *p = candidate + strlen(pattern);
        /* key must start a token: previous non-ws char is '{' or ',' */
        const char *prev = candidate;
        do {
            prev--;
        } while(prev > json && (*prev == ' ' || *prev == '\t' || *prev == '\r' || *prev == '\n'));
        if(candidate == json || *prev == '{' || *prev == ',') {
            sync_json_skip_ws(&p);
            if(*p == ':') {
                p++;
                sync_json_skip_ws(&p);
                if(*p == '"') {
                    size_t out_len = 0;
                    p++;
                    while(*p != '\0' && *p != '"') {
                        if(*p == '\\') {
                            if(!sync_json_decode_escape(&p, out, out_size, &out_len))
                                break;
                        } else {
                            if(out_len + 1 >= out_size)
                                break;
                            out[out_len++] = *p++;
                        }
                    }
                    if(*p == '"') {
                        out[out_len] = '\0';
                        return out_len > 0;
                    }
                }
            }
        }
        candidate += strlen(pattern);
    }
    out[0] = '\0';
    return 0;
}

long long
FindKsyncSyncJSONInt64(const char *json, const char *key, long long fallback)
{
    const char *p;
    char pattern[64];

    if(json == NULL || key == NULL)
        return fallback;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return fallback;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return fallback;
    p++;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return strtoll(p, NULL, 10);
}

static int
sync_config_valid(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->base_url != NULL && cfg->account != NULL &&
           cfg->client_id != NULL && cfg->http_request != NULL &&
           cfg->get_text != NULL && cfg->set_text != NULL;
}

static void
sync_log_http_failure(const KsyncSyncConfig *cfg, const char *step,
                      long status, const char *response)
{
    if(cfg != NULL && cfg->log_http_failure != NULL)
        cfg->log_http_failure(step, status, response, cfg->user);
}

static int
sync_retry_wait(const KsyncSyncConfig *cfg, int attempt, long status)
{
    int delay;

    if(cfg == NULL || cfg->sleep_ms == NULL || cfg->retry_max <= attempt)
        return 0;
    if(status < 500 && status != 429)
        return 0;
    delay = cfg->retry_delay_ms > 0 ? cfg->retry_delay_ms : 500;
    delay = delay << attempt;
    if(delay > 30000)
        delay = 30000;
    cfg->sleep_ms(delay, cfg->user);
    return 1;
}

static int
sync_http_request_retry(const KsyncSyncConfig *cfg, const char *method,
                        const char *url, const char *body,
                        const char *const *headers, int header_count,
                        KsyncSyncBuffer *response, long *status)
{
    int attempt;

    for(attempt = 0; ; attempt++) {
        FreeKsyncSyncBuffer(response);
        *status = 0;
        if(cfg->http_request(method, url, body, headers, header_count,
                             response, status, cfg->user))
            return 1;
        sync_log_http_failure(cfg, "retryable request", *status, response->data);
        if(!sync_retry_wait(cfg, attempt, *status))
            return 0;
    }
}

static int
sync_build_message(const KsyncSyncConfig *cfg, const char *method, const char *path,
                   const char *nonce_hex, const char *body, char *out,
                   size_t out_size)
{
    char body_hash[65];
    const char *context;
    int len;

    if(cfg == NULL || method == NULL || path == NULL || nonce_hex == NULL ||
       body == NULL || out == NULL || out_size == 0)
        return 0;
    KsyncSha256Hex((const uint8_t *)body, strlen(body), body_hash);
    if(body_hash[0] == '\0')
        return 0;
    context = cfg->signature_context != NULL && cfg->signature_context[0] != '\0'
                  ? cfg->signature_context
                  : KSYNC_SYNC_SIGNATURE_CONTEXT;
    len = snprintf(out, out_size, "%s\n%s\n%s\n%s\n%s\n", context, method,
                   path, body_hash, nonce_hex);
    return len > 0 && (size_t)len < out_size;
}

static const char *
sync_user_header_name(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->user_header_name != NULL &&
                   cfg->user_header_name[0] != '\0'
               ? cfg->user_header_name
               : KSYNC_SYNC_USER_HEADER;
}

static const char *
sync_signature_header_name(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->signature_header_name != NULL &&
                   cfg->signature_header_name[0] != '\0'
               ? cfg->signature_header_name
               : KSYNC_SYNC_SIGNATURE_HEADER;
}

static long long
sync_clock_skew(const KsyncSyncConfig *cfg)
{
    const char *skew_text;
    long long skew;

    if(cfg == NULL || cfg->get_text == NULL)
        return 0;
    skew_text = cfg->get_text(KSYNC_SYNC_CLOCK_SKEW_KEY, cfg->user);
    skew = skew_text != NULL ? atoll(skew_text) : 0;
    if(skew > 86400)
        skew = 86400;
    if(skew < -86400)
        skew = -86400;
    return skew;
}

static int
sync_load_valid_auth_token(const KsyncSyncConfig *cfg, char *out, size_t out_size)
{
    const char *token;
    const char *expires_text;
    char token_copy[4096];
    long long expires_at;

    if(!sync_config_valid(cfg) || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    token = cfg->get_text(KSYNC_SYNC_AUTH_TOKEN_KEY, cfg->user);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = cfg->get_text(KSYNC_SYNC_AUTH_TOKEN_EXPIRES_KEY, cfg->user);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' ||
       expires_at <= (long long)time(NULL) + sync_clock_skew(cfg))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

void
ClearKsyncSyncAuthToken(const KsyncSyncConfig *cfg)
{
    if(!sync_config_valid(cfg))
        return;
    cfg->set_text(KSYNC_SYNC_AUTH_TOKEN_KEY, "", cfg->user);
    cfg->set_text(KSYNC_SYNC_AUTH_TOKEN_EXPIRES_KEY, "", cfg->user);
}

static KsyncSyncResult
sync_fetch_challenge(const KsyncSyncConfig *cfg, const char *user_id,
                     char nonce_hex[65])
{
    char url[768];
    KsyncSyncBuffer response = {0};
    long status = 0;
    int ok;

    nonce_hex[0] = '\0';
    if(!JoinKsyncSyncURL(url, sizeof(url), cfg->base_url,
                                 KSYNC_CHALLENGE_PATH))
        return KSYNC_SYNC_INVALID_URL;
    if(strlen(url) + strlen(user_id) + 10 >= sizeof(url))
        return KSYNC_SYNC_INVALID_URL;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, user_id, sizeof(url) - strlen(url) - 1);
    ok = sync_http_request_retry(cfg, "GET", url, NULL, NULL, 0, &response, &status);
    if(!ok || status != 200 ||
       !FindKsyncSyncJSONString(response.data, "nonce", nonce_hex, 65) ||
       strlen(nonce_hex) != 64) {
        sync_log_http_failure(cfg, "challenge", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return status == 401 ? KSYNC_SYNC_AUTH_FAILED : KSYNC_SYNC_CHALLENGE_FAILED;
    }
    FreeKsyncSyncBuffer(&response);
    return KSYNC_SYNC_OK;
}

KsyncSyncResult
LoginKsyncSync(const KsyncSyncConfig *cfg)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[KSYNC_SIGNATURE_HEX_SIZE];
    char url[768];
    char user_header[96];
    char signature_header[KSYNC_SIGNATURE_HEX_SIZE + 32];
    const char *headers[3];
    KsyncSyncBuffer body = {0};
    KsyncSyncBuffer response = {0};
    long status = 0;
    KsyncSyncResult challenge_result;
    int ok;
    char token[4096];
    long long expires_in;
    long long expires_at;
    long long server_time;

    if(!sync_config_valid(cfg) || !HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
    if(!AppendKsyncSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->account->public_id) ||
       !AppendKsyncSyncBuffer(&body, ",\"client_id\":", strlen(",\"client_id\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->client_id) ||
       !AppendKsyncSyncBuffer(&body, ",\"public_key\":", strlen(",\"public_key\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->account->public_key_hex) ||
       !AppendKsyncSyncBuffer(&body, "}", 1)) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }

    challenge_result = sync_fetch_challenge(cfg, cfg->account->public_id, nonce_hex);
    if(challenge_result != KSYNC_SYNC_OK) {
        FreeKsyncSyncBuffer(&body);
        return challenge_result;
    }
    if(!sync_build_message(cfg, "POST", KSYNC_LOGIN_PATH, nonce_hex,
                           body.data, message, sizeof(message))) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_SIGN_FAILED;
    }
    if(!SignKsyncAccountHex(cfg->account, (const uint8_t *)message, strlen(message),
                                    signature_hex, sizeof(signature_hex))) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_SIGN_FAILED;
    }
    JoinKsyncSyncURL(url, sizeof(url), cfg->base_url, KSYNC_LOGIN_PATH);
    snprintf(user_header, sizeof(user_header), "%s: %s", sync_user_header_name(cfg),
             cfg->account->public_id);
    snprintf(signature_header, sizeof(signature_header), "%s: %s",
             sync_signature_header_name(cfg), signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = sync_http_request_retry(cfg, "POST", url, body.data, headers, 3, &response, &status);
    FreeKsyncSyncBuffer(&body);
    if(!ok) {
        sync_log_http_failure(cfg, "login request", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "login auth", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "login", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    expires_in = FindKsyncSyncJSONInt64(response.data, "expires_in_seconds", 3600);
    server_time = FindKsyncSyncJSONInt64(response.data, "server_time", 0);
    if(!FindKsyncSyncJSONString(response.data, "auth_token", token, sizeof(token))) {
        sync_log_http_failure(cfg, "login payload", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    if(server_time > 0) {
        char skew_text[32];
        long long skew = server_time - (long long)time(NULL);
        snprintf(skew_text, sizeof(skew_text), "%lld", skew);
        cfg->set_text(KSYNC_SYNC_CLOCK_SKEW_KEY, skew_text, cfg->user);
    }
    expires_at = (long long)time(NULL) + sync_clock_skew(cfg) + expires_in - 30;
    if(expires_at < (long long)time(NULL) + sync_clock_skew(cfg))
        expires_at = (long long)time(NULL) + sync_clock_skew(cfg);
    {
        char text[32];
        snprintf(text, sizeof(text), "%lld", expires_at);
        cfg->set_text(KSYNC_SYNC_AUTH_TOKEN_KEY, token, cfg->user);
        cfg->set_text(KSYNC_SYNC_AUTH_TOKEN_EXPIRES_KEY, text, cfg->user);
    }
    FreeKsyncSyncBuffer(&response);
    return KSYNC_SYNC_OK;
}

static KsyncSyncResult
sync_send_bearer(const KsyncSyncConfig *cfg, const char *body, const char *token)
{
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    KsyncSyncBuffer response = {0};
    long status = 0;
    int ok;

    JoinKsyncSyncURL(url, sizeof(url), cfg->base_url, KSYNC_SYNC_PATH);
    snprintf(user_header, sizeof(user_header), "%s: %s", sync_user_header_name(cfg),
             cfg->account->public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             token != NULL ? token : "");
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    ok = sync_http_request_retry(cfg, "POST", url, body, headers, 3, &response, &status);
    if(!ok) {
        sync_log_http_failure(cfg, "sync request", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "sync auth", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "sync", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(cfg->apply_response != NULL && cfg->encrypt_payload) {
        char *plain = NULL;
        if(UnwrapKsyncSyncPayload(cfg->account, response.data, &plain)) {
            int applied = cfg->apply_response(plain, cfg->user);
            free(plain);
            if(!applied) {
                sync_log_http_failure(cfg, "sync payload", status, response.data);
                FreeKsyncSyncBuffer(&response);
                return KSYNC_SYNC_PAYLOAD_FAILED;
            }
            FreeKsyncSyncBuffer(&response);
            return KSYNC_SYNC_OK;
        }
        /* not an envelope: fall through and hand the raw body to the app */
    }
    if(cfg->apply_response == NULL || !cfg->apply_response(response.data, cfg->user)) {
        sync_log_http_failure(cfg, "sync payload", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    FreeKsyncSyncBuffer(&response);
    return KSYNC_SYNC_OK;
}

KsyncSyncResult
RunKsyncSync(const KsyncSyncConfig *cfg)
{
    char *payload;
    int wrapped_payload = 0;
    KsyncSyncResult result;
    char token[4096];

    if(!sync_config_valid(cfg) || cfg->build_payload == NULL || cfg->free_payload == NULL)
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = LoginKsyncSync(cfg);
        if(result != KSYNC_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return KSYNC_SYNC_AUTH_FAILED;
    }
    payload = cfg->build_payload(cfg->account->public_id, cfg->account->public_key_hex, cfg->user);
    if(payload == NULL)
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(cfg->encrypt_payload) {
        char *wrapped = NULL;
        if(!WrapKsyncSyncPayload(cfg->account, payload, &wrapped)) {
            cfg->free_payload(payload, cfg->user);
            return KSYNC_SYNC_PAYLOAD_FAILED;
        }
        cfg->free_payload(payload, cfg->user);
        payload = wrapped;
        wrapped_payload = 1;
    }
    result = sync_send_bearer(cfg, payload, token);
    if(result == KSYNC_SYNC_AUTH_FAILED) {
        ClearKsyncSyncAuthToken(cfg);
        result = LoginKsyncSync(cfg);
        if(result == KSYNC_SYNC_OK && sync_load_valid_auth_token(cfg, token, sizeof(token)))
            result = sync_send_bearer(cfg, payload, token);
        else if(result == KSYNC_SYNC_OK)
            result = KSYNC_SYNC_AUTH_FAILED;
    }
    if(wrapped_payload)
        free(payload);
    else
        cfg->free_payload(payload, cfg->user);
    if(result == KSYNC_SYNC_OK && cfg->purge_synced_deleted != NULL)
        cfg->purge_synced_deleted(cfg->user);
    return result;
}

static int
sync_copy_response_text(const KsyncSyncBuffer *response, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return 1;
    out[0] = '\0';
    if(response == NULL || response->data == NULL)
        return 0;
    if(strlen(response->data) >= out_size)
        return 0;
    snprintf(out, out_size, "%s", response->data);
    return 1;
}

KsyncSyncResult
RequestKsyncSyncBearer(const KsyncSyncConfig *cfg, const char *method,
                               const char *path, const char *body,
                               char *out, size_t out_size)
{
    char token[4096];
    char url[1024];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    int header_count = 0;
    KsyncSyncBuffer response = {0};
    long status = 0;
    KsyncSyncResult result;
    int ok;
    int retried_auth = 0;

    if(out != NULL && out_size > 0)
        out[0] = '\0';
    if(!sync_config_valid(cfg))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(method == NULL || path == NULL)
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = LoginKsyncSync(cfg);
        if(result != KSYNC_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return KSYNC_SYNC_AUTH_FAILED;
    }

retry:
    if(!JoinKsyncSyncURL(url, sizeof(url), cfg->base_url, path))
        return KSYNC_SYNC_INVALID_URL;
    snprintf(user_header, sizeof(user_header), "%s: %s", sync_user_header_name(cfg),
             cfg->account->public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    if(body != NULL)
        headers[header_count++] = "Content-Type: application/json";
    headers[header_count++] = user_header;
    headers[header_count++] = auth_header;
    ok = sync_http_request_retry(cfg, method, url, body != NULL ? body : "",
                                 headers, header_count, &response, &status);
    if(!ok) {
        sync_log_http_failure(cfg, path, status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        FreeKsyncSyncBuffer(&response);
        ClearKsyncSyncAuthToken(cfg);
        if(retried_auth)
            return KSYNC_SYNC_AUTH_FAILED;
        retried_auth = 1;
        result = LoginKsyncSync(cfg);
        if(result != KSYNC_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return KSYNC_SYNC_AUTH_FAILED;
        status = 0;
        header_count = 0;
        goto retry;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, path, status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(!sync_copy_response_text(&response, out, out_size)) {
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    FreeKsyncSyncBuffer(&response);
    return KSYNC_SYNC_OK;
}

static KsyncSyncResult
sync_delete_with_key(const KsyncSyncConfig *cfg)
{
    char url[768];
    char exported_key[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    KsyncSyncBuffer body = {0};
    KsyncSyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int ok;

    if(!ExportKsyncAccountText(cfg->account, exported_key, sizeof(exported_key)))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!AppendKsyncSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->account->public_id) ||
       !AppendKsyncSyncBuffer(&body, ",\"exported_key\":", strlen(",\"exported_key\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, exported_key) ||
       !AppendKsyncSyncBuffer(&body, "}", 1)) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }

    JoinKsyncSyncURL(url, sizeof(url), cfg->base_url,
                             KSYNC_ACCOUNT_DELETE_WITH_KEY_PATH);
    ok = sync_http_request_retry(cfg, "POST", url, body.data, headers, 1,
                                 &response, &status);
    FreeKsyncSyncBuffer(&body);
    FreeKsyncSyncBuffer(&response);
    if(!ok)
        return KSYNC_SYNC_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return KSYNC_SYNC_AUTH_FAILED;
    return status >= 200 && status < 300 ? KSYNC_SYNC_OK : KSYNC_SYNC_REQUEST_FAILED;
}

KsyncSyncResult
DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[KSYNC_SIGNATURE_HEX_SIZE];
    char url[768];
    char user_header[96];
    char signature_header[KSYNC_SIGNATURE_HEX_SIZE + 32];
    const char *headers[3];
    KsyncSyncBuffer body = {0};
    KsyncSyncBuffer response = {0};
    long status = 0;
    KsyncSyncResult result;
    int ok;

    if(!sync_config_valid(cfg))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;

    /* preferred: signed deletion proof that never transmits the key */
    result = sync_fetch_challenge(cfg, cfg->account->public_id, nonce_hex);
    if(result != KSYNC_SYNC_OK)
        return result;
    if(!AppendKsyncSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->account->public_id) ||
       !AppendKsyncSyncBuffer(&body, ",\"public_key\":", strlen(",\"public_key\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, cfg->account->public_key_hex) ||
       !AppendKsyncSyncBuffer(&body, "}", 1)) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    if(!sync_build_message(cfg, "POST", KSYNC_ACCOUNT_DELETE_PATH, nonce_hex,
                           body.data, message, sizeof(message)) ||
       !SignKsyncAccountHex(cfg->account, (const uint8_t *)message, strlen(message),
                            signature_hex, sizeof(signature_hex))) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_SIGN_FAILED;
    }
    JoinKsyncSyncURL(url, sizeof(url), cfg->base_url, KSYNC_ACCOUNT_DELETE_PATH);
    snprintf(user_header, sizeof(user_header), "%s: %s", sync_user_header_name(cfg),
             cfg->account->public_id);
    snprintf(signature_header, sizeof(signature_header), "%s: %s",
             sync_signature_header_name(cfg), signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = sync_http_request_retry(cfg, "POST", url, body.data, headers, 3,
                                 &response, &status);
    FreeKsyncSyncBuffer(&body);
    if(ok && (status == 404 || status == 405)) {
        /* server predates the signed-delete endpoint */
        FreeKsyncSyncBuffer(&response);
        return sync_delete_with_key(cfg);
    }
    FreeKsyncSyncBuffer(&response);
    if(!ok)
        return KSYNC_SYNC_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return KSYNC_SYNC_AUTH_FAILED;
    return status >= 200 && status < 300 ? KSYNC_SYNC_OK : KSYNC_SYNC_REQUEST_FAILED;
}

/* ------------------------------------------------------------------ */
/* Opt-in end-to-end payload encryption                                */
/* ------------------------------------------------------------------ */

static int
sync_payload_key(const KsyncAccount *account, uint8_t out[32])
{
    uint8_t private_key[2560];

    if(!HasKsyncAccountValues(account))
        return 0;
    if(!KsyncCryptoHexToBytes(account->private_key_hex, private_key, sizeof(private_key)))
        return 0;
    KsyncCryptoHmacSha256(private_key, sizeof(private_key),
                          (const uint8_t *)KSYNC_SYNC_PAYLOAD_CONTEXT,
                          strlen(KSYNC_SYNC_PAYLOAD_CONTEXT), out);
    return 1;
}

static unsigned
sync_lzss_hash(const uint8_t *data)
{
    return ((unsigned)data[0] * 251u + (unsigned)data[1] * 31u +
            (unsigned)data[2]) & 4095u;
}

static void
sync_lzss_insert(const uint8_t *data, size_t len, size_t pos,
                 int *head, int *prev)
{
    unsigned h;

    if(pos + 2 >= len)
        return;
    h = sync_lzss_hash(data + pos);
    prev[pos] = head[h];
    head[h] = (int)pos;
}

static int
sync_lzss_find(const uint8_t *data, size_t len, size_t pos,
               const int *head, const int *prev,
               size_t *offset_out, size_t *length_out)
{
    size_t best_len = 0;
    size_t best_offset = 0;
    int candidate;
    int scans = 0;

    if(pos + 2 >= len)
        return 0;
    candidate = head[sync_lzss_hash(data + pos)];
    while(candidate >= 0 && scans++ < 64) {
        size_t cpos = (size_t)candidate;
        size_t offset;
        size_t match_len = 0;

        if(cpos >= pos)
            break;
        offset = pos - cpos;
        if(offset > 4096)
            break;
        while(match_len < 18 && pos + match_len < len &&
              data[cpos + match_len] == data[pos + match_len])
            match_len++;
        if(match_len >= 3 && match_len > best_len) {
            best_len = match_len;
            best_offset = offset;
            if(match_len == 18)
                break;
        }
        candidate = prev[cpos];
    }
    if(best_len < 3)
        return 0;
    *offset_out = best_offset;
    *length_out = best_len;
    return 1;
}

static int
sync_lzss_compress(const uint8_t *data, size_t len,
                   uint8_t **out, size_t *out_len)
{
    uint8_t *compressed;
    int *head = NULL;
    int *prev = NULL;
    size_t cap;
    size_t pos = 0;
    size_t write_pos = 0;

    if(out == NULL || out_len == NULL)
        return 0;
    *out = NULL;
    *out_len = 0;
    if(data == NULL || len == 0 || len > 0x7fffffffu)
        return 0;
    cap = len + len / 8 + 16;
    compressed = (uint8_t *)malloc(cap);
    head = (int *)malloc(4096 * sizeof(int));
    prev = (int *)malloc(len * sizeof(int));
    if(compressed == NULL || head == NULL || prev == NULL) {
        free(compressed);
        free(head);
        free(prev);
        return 0;
    }
    for(size_t i = 0; i < 4096; i++)
        head[i] = -1;
    for(size_t i = 0; i < len; i++)
        prev[i] = -1;

    while(pos < len) {
        size_t control_pos = write_pos++;
        unsigned char control = 0;

        if(write_pos >= cap)
            goto fail;
        for(int bit = 0; bit < 8 && pos < len; bit++) {
            size_t offset = 0;
            size_t length = 0;

            if(sync_lzss_find(data, len, pos, head, prev, &offset, &length)) {
                size_t code_offset = offset - 1;

                if(write_pos + 2 > cap)
                    goto fail;
                control |= (unsigned char)(1u << bit);
                compressed[write_pos++] = (uint8_t)((code_offset >> 4) & 0xffu);
                compressed[write_pos++] =
                    (uint8_t)(((code_offset & 0x0fu) << 4) | (length - 3));
                for(size_t i = 0; i < length; i++)
                    sync_lzss_insert(data, len, pos + i, head, prev);
                pos += length;
            } else {
                if(write_pos + 1 > cap)
                    goto fail;
                compressed[write_pos++] = data[pos];
                sync_lzss_insert(data, len, pos, head, prev);
                pos++;
            }
        }
        compressed[control_pos] = control;
    }

    if(write_pos >= len)
        goto fail;
    free(head);
    free(prev);
    *out = compressed;
    *out_len = write_pos;
    return 1;

fail:
    free(compressed);
    free(head);
    free(prev);
    return 0;
}

static int
sync_lzss_decompress(const uint8_t *data, size_t len, size_t plain_len,
                     char **out)
{
    char *plain;
    size_t read_pos = 0;
    size_t write_pos = 0;

    if(data == NULL || out == NULL || plain_len > 0x7fffffffu)
        return 0;
    *out = NULL;
    plain = (char *)malloc(plain_len + 1);
    if(plain == NULL)
        return 0;
    while(read_pos < len && write_pos < plain_len) {
        unsigned char control = data[read_pos++];

        for(int bit = 0; bit < 8 && write_pos < plain_len; bit++) {
            if((control & (1u << bit)) != 0) {
                size_t code;
                size_t offset;
                size_t length;

                if(read_pos + 2 > len)
                    goto fail;
                code = ((size_t)data[read_pos] << 4) |
                       ((size_t)data[read_pos + 1] >> 4);
                offset = code + 1;
                length = (data[read_pos + 1] & 0x0f) + 3;
                read_pos += 2;
                if(offset > write_pos || write_pos + length > plain_len)
                    goto fail;
                for(size_t i = 0; i < length; i++) {
                    plain[write_pos] = plain[write_pos - offset];
                    write_pos++;
                }
            } else {
                if(read_pos >= len)
                    goto fail;
                plain[write_pos++] = (char)data[read_pos++];
            }
        }
    }
    if(write_pos != plain_len)
        goto fail;
    plain[plain_len] = '\0';
    *out = plain;
    return 1;

fail:
    free(plain);
    return 0;
}

int
WrapKsyncSyncPayload(const KsyncAccount *account, const char *payload, char **out)
{
    uint8_t key[32];
    uint8_t nonce[12];
    uint8_t *sealed;
    uint8_t *compressed = NULL;
    const uint8_t *plain;
    char nonce_hex[25];
    char *sealed_hex;
    size_t payload_len;
    size_t plain_len;
    size_t compressed_len = 0;
    size_t sealed_len;
    KsyncSyncBuffer envelope = {0};
    int ok;
    int compressed_payload = 0;

    if(account == NULL || payload == NULL || out == NULL)
        return 0;
    *out = NULL;
    payload_len = strlen(payload);
    plain = (const uint8_t *)payload;
    plain_len = payload_len;
    if(sync_lzss_compress((const uint8_t *)payload, payload_len,
                          &compressed, &compressed_len)) {
        if(compressed_len + 32 < payload_len) {
            plain = compressed;
            plain_len = compressed_len;
            compressed_payload = 1;
        }
    }
    sealed_len = plain_len + 16;
    sealed = (uint8_t *)malloc(sealed_len);
    sealed_hex = (char *)malloc(sealed_len * 2 + 1);
    if(sealed == NULL || sealed_hex == NULL) {
        free(sealed);
        free(sealed_hex);
        free(compressed);
        return 0;
    }
    if(!sync_payload_key(account, key))
        goto fail;
    KsyncCryptoRandom(nonce, sizeof(nonce));
    if(!KsyncCryptoChaCha20Poly1305Seal(key, nonce, plain, plain_len,
                                        NULL, 0, sealed))
        goto fail;
    if(!KsyncCryptoBytesToHex(sealed, sealed_len, sealed_hex, sealed_len * 2 + 1) ||
       !KsyncCryptoBytesToHex(nonce, sizeof(nonce), nonce_hex, sizeof(nonce_hex)))
        goto fail;
    if(compressed_payload) {
        char size_text[32];

        snprintf(size_text, sizeof(size_text), "%zu", payload_len);
        ok = AppendKsyncSyncBuffer(&envelope, "{\"v\":2,\"compression\":", strlen("{\"v\":2,\"compression\":")) &&
             AppendKsyncSyncBufferJSONString(&envelope, KSYNC_SYNC_PAYLOAD_COMPRESSION) &&
             AppendKsyncSyncBuffer(&envelope, ",\"plain_size\":", strlen(",\"plain_size\":")) &&
             AppendKsyncSyncBuffer(&envelope, size_text, strlen(size_text)) &&
             AppendKsyncSyncBuffer(&envelope, ",\"nonce\":", strlen(",\"nonce\":")) &&
             AppendKsyncSyncBufferJSONString(&envelope, nonce_hex) &&
             AppendKsyncSyncBuffer(&envelope, ",\"ciphertext\":", strlen(",\"ciphertext\":")) &&
             AppendKsyncSyncBufferJSONString(&envelope, sealed_hex) &&
             AppendKsyncSyncBuffer(&envelope, "}", 1);
    } else {
        ok = AppendKsyncSyncBuffer(&envelope, "{\"v\":1,\"nonce\":", strlen("{\"v\":1,\"nonce\":")) &&
             AppendKsyncSyncBufferJSONString(&envelope, nonce_hex) &&
             AppendKsyncSyncBuffer(&envelope, ",\"ciphertext\":", strlen(",\"ciphertext\":")) &&
             AppendKsyncSyncBufferJSONString(&envelope, sealed_hex) &&
             AppendKsyncSyncBuffer(&envelope, "}", 1);
    }
    if(!ok) {
        FreeKsyncSyncBuffer(&envelope);
        goto fail;
    }
    free(sealed);
    free(sealed_hex);
    free(compressed);
    *out = envelope.data; /* ownership moves to caller */
    return 1;
fail:
    free(sealed);
    free(sealed_hex);
    free(compressed);
    return 0;
}

int
UnwrapKsyncSyncPayload(const KsyncAccount *account, const char *envelope_json, char **out)
{
    uint8_t key[32];
    uint8_t nonce[12];
    uint8_t *sealed = NULL;
    char nonce_hex[25];
    static const size_t hex_cap = 4 * 1024 * 1024;
    char *ciphertext_hex;
    char *plain = NULL;
    char compression[16];
    long long version;
    long long plain_size;
    size_t sealed_len;
    int ok = 0;

    if(account == NULL || envelope_json == NULL || out == NULL)
        return 0;
    *out = NULL;
    ciphertext_hex = (char *)malloc(hex_cap);
    if(ciphertext_hex == NULL)
        return 0;
    version = FindKsyncSyncJSONInt64(envelope_json, "v", 0);
    if((version != 1 && version != 2) ||
       !FindKsyncSyncJSONString(envelope_json, "nonce", nonce_hex, sizeof(nonce_hex)) ||
       !FindKsyncSyncJSONString(envelope_json, "ciphertext", ciphertext_hex, hex_cap))
        goto done; /* not an envelope */
    compression[0] = '\0';
    plain_size = 0;
    if(version == 2) {
        plain_size = FindKsyncSyncJSONInt64(envelope_json, "plain_size", -1);
        if(plain_size < 0 || plain_size > 0x7fffffff ||
           !FindKsyncSyncJSONString(envelope_json, "compression",
                                    compression, sizeof(compression)) ||
           strcmp(compression, KSYNC_SYNC_PAYLOAD_COMPRESSION) != 0)
            goto done;
    }
    if(!sync_payload_key(account, key) ||
       !KsyncCryptoHexToBytes(nonce_hex, nonce, sizeof(nonce)))
        goto done;
    sealed_len = strlen(ciphertext_hex) / 2;
    if(sealed_len <= 16)
        goto done;
    sealed = (uint8_t *)malloc(sealed_len);
    plain = (char *)malloc(sealed_len); /* sealed includes 16-byte tag */
    if(sealed == NULL || plain == NULL)
        goto done;
    if(!KsyncCryptoHexToBytes(ciphertext_hex, sealed, sealed_len))
        goto done;
    if(!KsyncCryptoChaCha20Poly1305Open(key, nonce, sealed, sealed_len,
                                        NULL, 0, (uint8_t *)plain))
        goto done;
    if(version == 2) {
        char *decompressed = NULL;

        if(!sync_lzss_decompress((const uint8_t *)plain, sealed_len - 16,
                                 (size_t)plain_size, &decompressed))
            goto done;
        *out = decompressed;
    } else {
        plain[sealed_len - 16] = '\0';
        *out = plain;
        plain = NULL;
    }
    ok = 1;
done:
    free(ciphertext_hex);
    free(sealed);
    free(plain);
    return ok;
}
