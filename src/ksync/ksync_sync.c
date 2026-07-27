#include "ksync_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KSYNC_SYNC_PATH "/api/v1/sync"
#define KSYNC_CHALLENGE_PATH "/api/v1/sync/challenge"
#define KSYNC_LOGIN_PATH "/api/v1/sync/login"
#define KSYNC_ACCOUNT_DELETE_WITH_KEY_PATH "/api/v1/account/delete-with-key"
#define KSYNC_SYNC_AUTH_TOKEN_KEY "sync_auth_token"
#define KSYNC_SYNC_AUTH_TOKEN_EXPIRES_KEY "sync_auth_token_expires_at"
#define KSYNC_SYNC_SIGNATURE_CONTEXT "ksync-sync-v1"
#define KSYNC_SYNC_USER_HEADER "X-Ksync-User"
#define KSYNC_SYNC_SIGNATURE_HEADER "X-Ksync-Signature"

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
IsKsyncSyncURLValid(const char *url)
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
            default:
                escaped[0] = *p;
                escaped[1] = '\0';
                if(!AppendKsyncSyncBuffer(buffer, escaped, 1))
                    return 0;
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

int
FindKsyncSyncJSONString(const char *json, const char *key, char *out, size_t out_size)
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
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
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
    ok = cfg->http_request("GET", url, NULL, NULL, 0, &response, &status, cfg->user);
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
    ok = cfg->http_request("POST", url, body.data, headers, 3, &response, &status, cfg->user);
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
    if(!FindKsyncSyncJSONString(response.data, "auth_token", token, sizeof(token))) {
        sync_log_http_failure(cfg, "login payload", status, response.data);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    expires_at = (long long)time(NULL) + expires_in - 30;
    if(expires_at < (long long)time(NULL))
        expires_at = (long long)time(NULL);
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
    ok = cfg->http_request("POST", url, body, headers, 3, &response, &status, cfg->user);
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
    result = sync_send_bearer(cfg, payload, token);
    if(result == KSYNC_SYNC_AUTH_FAILED) {
        ClearKsyncSyncAuthToken(cfg);
        result = LoginKsyncSync(cfg);
        if(result == KSYNC_SYNC_OK && sync_load_valid_auth_token(cfg, token, sizeof(token)))
            result = sync_send_bearer(cfg, payload, token);
        else if(result == KSYNC_SYNC_OK)
            result = KSYNC_SYNC_AUTH_FAILED;
    }
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
    ok = cfg->http_request(method, url, body != NULL ? body : "", headers, header_count,
                           &response, &status, cfg->user);
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

KsyncSyncResult
DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg)
{
    char url[768];
    char exported_key[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
    KsyncSyncBuffer body = {0};
    KsyncSyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int ok;

    if(!sync_config_valid(cfg))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
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
    ok = cfg->http_request("POST", url, body.data, headers, 1, &response, &status, cfg->user);
    FreeKsyncSyncBuffer(&body);
    FreeKsyncSyncBuffer(&response);
    if(!ok)
        return KSYNC_SYNC_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return KSYNC_SYNC_AUTH_FAILED;
    return status >= 200 && status < 300 ? KSYNC_SYNC_OK : KSYNC_SYNC_REQUEST_FAILED;
}
