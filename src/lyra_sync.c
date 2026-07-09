#include "lyra_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LYRA_SYNC_PATH "/api/v1/sync"
#define LYRA_CHALLENGE_PATH "/api/v1/sync/challenge"
#define LYRA_LOGIN_PATH "/api/v1/sync/login"
#define LYRA_ACCOUNT_DELETE_WITH_KEY_PATH "/api/v1/account/delete-with-key"
#define LYRA_SYNC_AUTH_TOKEN_KEY "sync_auth_token"
#define LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY "sync_auth_token_expires_at"

const char *
GetLyraSyncResultName(LyraSyncResult result)
{
    switch(result) {
        case LYRA_SYNC_OK:
            return "ok";
        case LYRA_SYNC_INVALID_URL:
            return "invalid_url";
        case LYRA_SYNC_NO_ACCOUNT:
            return "no_account";
        case LYRA_SYNC_PAYLOAD_FAILED:
            return "payload_failed";
        case LYRA_SYNC_CHALLENGE_FAILED:
            return "challenge_failed";
        case LYRA_SYNC_SIGN_FAILED:
            return "sign_failed";
        case LYRA_SYNC_REQUEST_FAILED:
            return "request_failed";
        case LYRA_SYNC_AUTH_FAILED:
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
sync_loopback_authority_valid(const char *authority)
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
    return 0;
}

int
IsLyraSyncURLValid(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return 0;
    if(sync_has_prefix(url, "https://"))
        return url[8] != '\0';
    if(sync_has_prefix(url, "http://"))
        return sync_loopback_authority_valid(url + 7);
    return sync_loopback_authority_valid(url);
}

int
NormalizeLyraSyncURL(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!IsLyraSyncURLValid(input))
        return 0;
    if(sync_has_prefix(input, "https://") || sync_has_prefix(input, "http://"))
        len = snprintf(out, out_size, "%s", input);
    else
        len = snprintf(out, out_size, "http://%s", input);
    return len > 0 && (size_t)len < out_size;
}

int
JoinLyraSyncURL(char *out, size_t out_size, const char *base_url, const char *path)
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
JoinLyraSyncWebSocketURL(char *out, size_t out_size, const char *base_url, const char *path)
{
    char http_url[768];
    const char *body;
    const char *scheme;
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!JoinLyraSyncURL(http_url, sizeof(http_url), base_url, path))
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
AppendLyraSyncBuffer(LyraSyncBuffer *buffer, const void *data, size_t bytes)
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
AppendLyraSyncBufferJSONString(LyraSyncBuffer *buffer, const char *text)
{
    const char *p;

    if(!AppendLyraSyncBuffer(buffer, "\"", 1))
        return 0;
    if(text == NULL)
        text = "";
    for(p = text; *p != '\0'; p++) {
        char escaped[2];
        switch(*p) {
            case '\\':
                if(!AppendLyraSyncBuffer(buffer, "\\\\", 2))
                    return 0;
                break;
            case '"':
                if(!AppendLyraSyncBuffer(buffer, "\\\"", 2))
                    return 0;
                break;
            case '\n':
                if(!AppendLyraSyncBuffer(buffer, "\\n", 2))
                    return 0;
                break;
            case '\r':
                if(!AppendLyraSyncBuffer(buffer, "\\r", 2))
                    return 0;
                break;
            case '\t':
                if(!AppendLyraSyncBuffer(buffer, "\\t", 2))
                    return 0;
                break;
            default:
                escaped[0] = *p;
                escaped[1] = '\0';
                if(!AppendLyraSyncBuffer(buffer, escaped, 1))
                    return 0;
                break;
        }
    }
    return AppendLyraSyncBuffer(buffer, "\"", 1);
}

void
FreeLyraSyncBuffer(LyraSyncBuffer *buffer)
{
    if(buffer == NULL)
        return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

int
FindLyraSyncJSONString(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    char *w;
    size_t remaining;

    if(json == NULL || key == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return 0;
    p += strlen(pattern);
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p++ != ':')
        return 0;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p++ != '"')
        return 0;
    w = out;
    remaining = out_size - 1;
    while(*p != '\0' && *p != '"' && remaining > 0) {
        if(*p == '\\')
            return 0;
        *w++ = *p++;
        remaining--;
    }
    *w = '\0';
    return *p == '"' && out[0] != '\0';
}

long long
FindLyraSyncJSONInt64(const char *json, const char *key, long long fallback)
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
sync_config_valid(const LyraSyncConfig *cfg)
{
    return cfg != NULL && cfg->base_url != NULL && cfg->account != NULL &&
           cfg->client_id != NULL && cfg->http_request != NULL &&
           cfg->get_text != NULL && cfg->set_text != NULL;
}

static void
sync_log_http_failure(const LyraSyncConfig *cfg, const char *step,
                      long status, const char *response)
{
    if(cfg != NULL && cfg->log_http_failure != NULL)
        cfg->log_http_failure(step, status, response, cfg->user);
}

static int
sync_build_message(const char *method, const char *path, const char *nonce_hex,
                   const char *body, char *out, size_t out_size)
{
    char body_hash[65];
    int len;

    if(method == NULL || path == NULL || nonce_hex == NULL || body == NULL ||
       out == NULL || out_size == 0)
        return 0;
    LyraSha256Hex((const uint8_t *)body, strlen(body), body_hash);
    if(body_hash[0] == '\0')
        return 0;
    len = snprintf(out, out_size, "inbe-sync-v1\n%s\n%s\n%s\n%s\n",
                   method, path, body_hash, nonce_hex);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_load_valid_auth_token(const LyraSyncConfig *cfg, char *out, size_t out_size)
{
    const char *token;
    const char *expires_text;
    char token_copy[4096];
    long long expires_at;

    if(!sync_config_valid(cfg) || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    token = cfg->get_text(LYRA_SYNC_AUTH_TOKEN_KEY, cfg->user);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = cfg->get_text(LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, cfg->user);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

void
ClearLyraSyncAuthToken(const LyraSyncConfig *cfg)
{
    if(!sync_config_valid(cfg))
        return;
    cfg->set_text(LYRA_SYNC_AUTH_TOKEN_KEY, "", cfg->user);
    cfg->set_text(LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, "", cfg->user);
}

static LyraSyncResult
sync_fetch_challenge(const LyraSyncConfig *cfg, const char *user_id,
                     char nonce_hex[65])
{
    char url[768];
    LyraSyncBuffer response = {0};
    long status = 0;
    int ok;

    nonce_hex[0] = '\0';
    if(!JoinLyraSyncURL(url, sizeof(url), cfg->base_url,
                                 LYRA_CHALLENGE_PATH))
        return LYRA_SYNC_INVALID_URL;
    if(strlen(url) + strlen(user_id) + 10 >= sizeof(url))
        return LYRA_SYNC_INVALID_URL;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, user_id, sizeof(url) - strlen(url) - 1);
    ok = cfg->http_request("GET", url, NULL, NULL, 0, &response, &status, cfg->user);
    if(!ok || status != 200 ||
       !FindLyraSyncJSONString(response.data, "nonce", nonce_hex, 65) ||
       strlen(nonce_hex) != 64) {
        sync_log_http_failure(cfg, "challenge", status, response.data);
        FreeLyraSyncBuffer(&response);
        return status == 401 ? LYRA_SYNC_AUTH_FAILED : LYRA_SYNC_CHALLENGE_FAILED;
    }
    FreeLyraSyncBuffer(&response);
    return LYRA_SYNC_OK;
}

LyraSyncResult
LoginLyraSync(const LyraSyncConfig *cfg)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[LYRA_SIGNATURE_HEX_SIZE];
    char url[768];
    char user_header[96];
    char signature_header[LYRA_SIGNATURE_HEX_SIZE + 32];
    const char *headers[3];
    LyraSyncBuffer body = {0};
    LyraSyncBuffer response = {0};
    long status = 0;
    LyraSyncResult challenge_result;
    int ok;
    char token[4096];
    long long expires_in;
    long long expires_at;

    if(!sync_config_valid(cfg) || !HasLyraAccountValues(cfg->account))
        return LYRA_SYNC_NO_ACCOUNT;
    if(!AppendLyraSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendLyraSyncBufferJSONString(&body, cfg->account->public_id) ||
       !AppendLyraSyncBuffer(&body, ",\"client_id\":", strlen(",\"client_id\":")) ||
       !AppendLyraSyncBufferJSONString(&body, cfg->client_id) ||
       !AppendLyraSyncBuffer(&body, ",\"public_key\":", strlen(",\"public_key\":")) ||
       !AppendLyraSyncBufferJSONString(&body, cfg->account->public_key_hex) ||
       !AppendLyraSyncBuffer(&body, "}", 1)) {
        FreeLyraSyncBuffer(&body);
        return LYRA_SYNC_PAYLOAD_FAILED;
    }

    challenge_result = sync_fetch_challenge(cfg, cfg->account->public_id, nonce_hex);
    if(challenge_result != LYRA_SYNC_OK) {
        FreeLyraSyncBuffer(&body);
        return challenge_result;
    }
    if(!sync_build_message("POST", LYRA_LOGIN_PATH, nonce_hex,
                           body.data, message, sizeof(message))) {
        FreeLyraSyncBuffer(&body);
        return LYRA_SYNC_SIGN_FAILED;
    }
    if(!SignLyraAccountHex(cfg->account, (const uint8_t *)message, strlen(message),
                                    signature_hex, sizeof(signature_hex))) {
        FreeLyraSyncBuffer(&body);
        return LYRA_SYNC_SIGN_FAILED;
    }
    JoinLyraSyncURL(url, sizeof(url), cfg->base_url, LYRA_LOGIN_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", cfg->account->public_id);
    snprintf(signature_header, sizeof(signature_header), "X-Inbe-Signature: %s", signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = cfg->http_request("POST", url, body.data, headers, 3, &response, &status, cfg->user);
    FreeLyraSyncBuffer(&body);
    if(!ok) {
        sync_log_http_failure(cfg, "login request", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "login auth", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "login", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    expires_in = FindLyraSyncJSONInt64(response.data, "expires_in_seconds", 3600);
    if(!FindLyraSyncJSONString(response.data, "auth_token", token, sizeof(token))) {
        sync_log_http_failure(cfg, "login payload", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_PAYLOAD_FAILED;
    }
    expires_at = (long long)time(NULL) + expires_in - 30;
    if(expires_at < (long long)time(NULL))
        expires_at = (long long)time(NULL);
    {
        char text[32];
        snprintf(text, sizeof(text), "%lld", expires_at);
        cfg->set_text(LYRA_SYNC_AUTH_TOKEN_KEY, token, cfg->user);
        cfg->set_text(LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, text, cfg->user);
    }
    FreeLyraSyncBuffer(&response);
    return LYRA_SYNC_OK;
}

static LyraSyncResult
sync_send_bearer(const LyraSyncConfig *cfg, const char *body, const char *token)
{
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    LyraSyncBuffer response = {0};
    long status = 0;
    int ok;

    JoinLyraSyncURL(url, sizeof(url), cfg->base_url, LYRA_SYNC_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", cfg->account->public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             token != NULL ? token : "");
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    ok = cfg->http_request("POST", url, body, headers, 3, &response, &status, cfg->user);
    if(!ok) {
        sync_log_http_failure(cfg, "sync request", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "sync auth", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "sync", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    if(cfg->apply_response == NULL || !cfg->apply_response(response.data, cfg->user)) {
        sync_log_http_failure(cfg, "sync payload", status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_PAYLOAD_FAILED;
    }
    FreeLyraSyncBuffer(&response);
    return LYRA_SYNC_OK;
}

LyraSyncResult
RunLyraSync(const LyraSyncConfig *cfg)
{
    char *payload;
    LyraSyncResult result;
    char token[4096];

    if(!sync_config_valid(cfg) || cfg->build_payload == NULL || cfg->free_payload == NULL)
        return LYRA_SYNC_PAYLOAD_FAILED;
    if(!IsLyraSyncURLValid(cfg->base_url))
        return LYRA_SYNC_INVALID_URL;
    if(!HasLyraAccountValues(cfg->account))
        return LYRA_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = LoginLyraSync(cfg);
        if(result != LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return LYRA_SYNC_AUTH_FAILED;
    }
    payload = cfg->build_payload(cfg->account->public_id, cfg->account->public_key_hex, cfg->user);
    if(payload == NULL)
        return LYRA_SYNC_PAYLOAD_FAILED;
    result = sync_send_bearer(cfg, payload, token);
    if(result == LYRA_SYNC_AUTH_FAILED) {
        ClearLyraSyncAuthToken(cfg);
        result = LoginLyraSync(cfg);
        if(result == LYRA_SYNC_OK && sync_load_valid_auth_token(cfg, token, sizeof(token)))
            result = sync_send_bearer(cfg, payload, token);
        else if(result == LYRA_SYNC_OK)
            result = LYRA_SYNC_AUTH_FAILED;
    }
    cfg->free_payload(payload, cfg->user);
    if(result == LYRA_SYNC_OK && cfg->purge_synced_deleted != NULL)
        cfg->purge_synced_deleted(cfg->user);
    return result;
}

static int
sync_copy_response_text(const LyraSyncBuffer *response, char *out, size_t out_size)
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

LyraSyncResult
RequestLyraSyncBearer(const LyraSyncConfig *cfg, const char *method,
                               const char *path, const char *body,
                               char *out, size_t out_size)
{
    char token[4096];
    char url[1024];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    int header_count = 0;
    LyraSyncBuffer response = {0};
    long status = 0;
    LyraSyncResult result;
    int ok;
    int retried_auth = 0;

    if(out != NULL && out_size > 0)
        out[0] = '\0';
    if(!sync_config_valid(cfg))
        return LYRA_SYNC_PAYLOAD_FAILED;
    if(!IsLyraSyncURLValid(cfg->base_url))
        return LYRA_SYNC_INVALID_URL;
    if(method == NULL || path == NULL)
        return LYRA_SYNC_PAYLOAD_FAILED;
    if(!HasLyraAccountValues(cfg->account))
        return LYRA_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = LoginLyraSync(cfg);
        if(result != LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return LYRA_SYNC_AUTH_FAILED;
    }

retry:
    if(!JoinLyraSyncURL(url, sizeof(url), cfg->base_url, path))
        return LYRA_SYNC_INVALID_URL;
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", cfg->account->public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    if(body != NULL)
        headers[header_count++] = "Content-Type: application/json";
    headers[header_count++] = user_header;
    headers[header_count++] = auth_header;
    ok = cfg->http_request(method, url, body != NULL ? body : "", headers, header_count,
                           &response, &status, cfg->user);
    if(!ok) {
        sync_log_http_failure(cfg, path, status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        FreeLyraSyncBuffer(&response);
        ClearLyraSyncAuthToken(cfg);
        if(retried_auth)
            return LYRA_SYNC_AUTH_FAILED;
        retried_auth = 1;
        result = LoginLyraSync(cfg);
        if(result != LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return LYRA_SYNC_AUTH_FAILED;
        status = 0;
        header_count = 0;
        goto retry;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, path, status, response.data);
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_REQUEST_FAILED;
    }
    if(!sync_copy_response_text(&response, out, out_size)) {
        FreeLyraSyncBuffer(&response);
        return LYRA_SYNC_PAYLOAD_FAILED;
    }
    FreeLyraSyncBuffer(&response);
    return LYRA_SYNC_OK;
}

LyraSyncResult
DeleteLyraSyncAccount(const LyraSyncConfig *cfg)
{
    char url[768];
    char exported_key[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    LyraSyncBuffer body = {0};
    LyraSyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int ok;

    if(!sync_config_valid(cfg))
        return LYRA_SYNC_PAYLOAD_FAILED;
    if(!IsLyraSyncURLValid(cfg->base_url))
        return LYRA_SYNC_INVALID_URL;
    if(!HasLyraAccountValues(cfg->account))
        return LYRA_SYNC_NO_ACCOUNT;
    if(!ExportLyraAccountText(cfg->account, exported_key, sizeof(exported_key)))
        return LYRA_SYNC_PAYLOAD_FAILED;
    if(!AppendLyraSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendLyraSyncBufferJSONString(&body, cfg->account->public_id) ||
       !AppendLyraSyncBuffer(&body, ",\"exported_key\":", strlen(",\"exported_key\":")) ||
       !AppendLyraSyncBufferJSONString(&body, exported_key) ||
       !AppendLyraSyncBuffer(&body, "}", 1)) {
        FreeLyraSyncBuffer(&body);
        return LYRA_SYNC_PAYLOAD_FAILED;
    }

    JoinLyraSyncURL(url, sizeof(url), cfg->base_url,
                             LYRA_ACCOUNT_DELETE_WITH_KEY_PATH);
    ok = cfg->http_request("POST", url, body.data, headers, 1, &response, &status, cfg->user);
    FreeLyraSyncBuffer(&body);
    FreeLyraSyncBuffer(&response);
    if(!ok)
        return LYRA_SYNC_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return LYRA_SYNC_AUTH_FAILED;
    return status >= 200 && status < 300 ? LYRA_SYNC_OK : LYRA_SYNC_REQUEST_FAILED;
}
