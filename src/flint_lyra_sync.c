#include "flint_lyra_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FLINT_LYRA_SYNC_PATH "/api/v1/sync"
#define FLINT_LYRA_CHALLENGE_PATH "/api/v1/sync/challenge"
#define FLINT_LYRA_LOGIN_PATH "/api/v1/sync/login"
#define FLINT_LYRA_ACCOUNT_DELETE_WITH_KEY_PATH "/api/v1/account/delete-with-key"
#define FLINT_LYRA_SYNC_AUTH_TOKEN_KEY "sync_auth_token"
#define FLINT_LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY "sync_auth_token_expires_at"

const char *
flint_lyra_sync_result_name(FlintLyraSyncResult result)
{
    switch(result) {
        case FLINT_LYRA_SYNC_OK:
            return "ok";
        case FLINT_LYRA_SYNC_INVALID_URL:
            return "invalid_url";
        case FLINT_LYRA_SYNC_NO_ACCOUNT:
            return "no_account";
        case FLINT_LYRA_SYNC_PAYLOAD_FAILED:
            return "payload_failed";
        case FLINT_LYRA_SYNC_CHALLENGE_FAILED:
            return "challenge_failed";
        case FLINT_LYRA_SYNC_SIGN_FAILED:
            return "sign_failed";
        case FLINT_LYRA_SYNC_REQUEST_FAILED:
            return "request_failed";
        case FLINT_LYRA_SYNC_AUTH_FAILED:
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
flint_lyra_sync_url_valid(const char *url)
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
flint_lyra_sync_normalize_url(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!flint_lyra_sync_url_valid(input))
        return 0;
    if(sync_has_prefix(input, "https://") || sync_has_prefix(input, "http://"))
        len = snprintf(out, out_size, "%s", input);
    else
        len = snprintf(out, out_size, "http://%s", input);
    return len > 0 && (size_t)len < out_size;
}

int
flint_lyra_sync_join_url(char *out, size_t out_size, const char *base_url, const char *path)
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
flint_lyra_sync_join_ws_url(char *out, size_t out_size, const char *base_url, const char *path)
{
    char http_url[768];
    const char *body;
    const char *scheme;
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!flint_lyra_sync_join_url(http_url, sizeof(http_url), base_url, path))
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
flint_lyra_sync_buffer_append(FlintLyraSyncBuffer *buffer, const void *data, size_t bytes)
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
flint_lyra_sync_buffer_append_json_string(FlintLyraSyncBuffer *buffer, const char *text)
{
    const char *p;

    if(!flint_lyra_sync_buffer_append(buffer, "\"", 1))
        return 0;
    if(text == NULL)
        text = "";
    for(p = text; *p != '\0'; p++) {
        char escaped[2];
        switch(*p) {
            case '\\':
                if(!flint_lyra_sync_buffer_append(buffer, "\\\\", 2))
                    return 0;
                break;
            case '"':
                if(!flint_lyra_sync_buffer_append(buffer, "\\\"", 2))
                    return 0;
                break;
            case '\n':
                if(!flint_lyra_sync_buffer_append(buffer, "\\n", 2))
                    return 0;
                break;
            case '\r':
                if(!flint_lyra_sync_buffer_append(buffer, "\\r", 2))
                    return 0;
                break;
            case '\t':
                if(!flint_lyra_sync_buffer_append(buffer, "\\t", 2))
                    return 0;
                break;
            default:
                escaped[0] = *p;
                escaped[1] = '\0';
                if(!flint_lyra_sync_buffer_append(buffer, escaped, 1))
                    return 0;
                break;
        }
    }
    return flint_lyra_sync_buffer_append(buffer, "\"", 1);
}

void
flint_lyra_sync_buffer_free(FlintLyraSyncBuffer *buffer)
{
    if(buffer == NULL)
        return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

int
flint_lyra_sync_find_json_string(const char *json, const char *key, char *out, size_t out_size)
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
flint_lyra_sync_find_json_int64(const char *json, const char *key, long long fallback)
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
sync_config_valid(const FlintLyraSyncConfig *cfg)
{
    return cfg != NULL && cfg->base_url != NULL && cfg->account != NULL &&
           cfg->client_id != NULL && cfg->http_request != NULL &&
           cfg->get_text != NULL && cfg->set_text != NULL;
}

static void
sync_log_http_failure(const FlintLyraSyncConfig *cfg, const char *step,
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
    flint_lyra_sha256_hex((const uint8_t *)body, strlen(body), body_hash);
    if(body_hash[0] == '\0')
        return 0;
    len = snprintf(out, out_size, "inbe-sync-v1\n%s\n%s\n%s\n%s\n",
                   method, path, body_hash, nonce_hex);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_load_valid_auth_token(const FlintLyraSyncConfig *cfg, char *out, size_t out_size)
{
    const char *token;
    const char *expires_text;
    char token_copy[4096];
    long long expires_at;

    if(!sync_config_valid(cfg) || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    token = cfg->get_text(FLINT_LYRA_SYNC_AUTH_TOKEN_KEY, cfg->user);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = cfg->get_text(FLINT_LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, cfg->user);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

void
flint_lyra_sync_clear_auth_token(const FlintLyraSyncConfig *cfg)
{
    if(!sync_config_valid(cfg))
        return;
    cfg->set_text(FLINT_LYRA_SYNC_AUTH_TOKEN_KEY, "", cfg->user);
    cfg->set_text(FLINT_LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, "", cfg->user);
}

static FlintLyraSyncResult
sync_fetch_challenge(const FlintLyraSyncConfig *cfg, const char *user_id,
                     char nonce_hex[65])
{
    char url[768];
    FlintLyraSyncBuffer response = {0};
    long status = 0;
    int ok;

    nonce_hex[0] = '\0';
    if(!flint_lyra_sync_join_url(url, sizeof(url), cfg->base_url,
                                 FLINT_LYRA_CHALLENGE_PATH))
        return FLINT_LYRA_SYNC_INVALID_URL;
    if(strlen(url) + strlen(user_id) + 10 >= sizeof(url))
        return FLINT_LYRA_SYNC_INVALID_URL;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, user_id, sizeof(url) - strlen(url) - 1);
    ok = cfg->http_request("GET", url, NULL, NULL, 0, &response, &status, cfg->user);
    if(!ok || status != 200 ||
       !flint_lyra_sync_find_json_string(response.data, "nonce", nonce_hex, 65) ||
       strlen(nonce_hex) != 64) {
        sync_log_http_failure(cfg, "challenge", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return status == 401 ? FLINT_LYRA_SYNC_AUTH_FAILED : FLINT_LYRA_SYNC_CHALLENGE_FAILED;
    }
    flint_lyra_sync_buffer_free(&response);
    return FLINT_LYRA_SYNC_OK;
}

FlintLyraSyncResult
flint_lyra_sync_login(const FlintLyraSyncConfig *cfg)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[FLINT_LYRA_SIGNATURE_HEX_SIZE];
    char url[768];
    char user_header[96];
    char signature_header[FLINT_LYRA_SIGNATURE_HEX_SIZE + 32];
    const char *headers[3];
    FlintLyraSyncBuffer body = {0};
    FlintLyraSyncBuffer response = {0};
    long status = 0;
    FlintLyraSyncResult challenge_result;
    int ok;
    char token[4096];
    long long expires_in;
    long long expires_at;

    if(!sync_config_valid(cfg) || !flint_lyra_account_has_values(cfg->account))
        return FLINT_LYRA_SYNC_NO_ACCOUNT;
    if(!flint_lyra_sync_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !flint_lyra_sync_buffer_append_json_string(&body, cfg->account->public_id) ||
       !flint_lyra_sync_buffer_append(&body, ",\"client_id\":", strlen(",\"client_id\":")) ||
       !flint_lyra_sync_buffer_append_json_string(&body, cfg->client_id) ||
       !flint_lyra_sync_buffer_append(&body, ",\"public_key\":", strlen(",\"public_key\":")) ||
       !flint_lyra_sync_buffer_append_json_string(&body, cfg->account->public_key_hex) ||
       !flint_lyra_sync_buffer_append(&body, "}", 1)) {
        flint_lyra_sync_buffer_free(&body);
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    }

    challenge_result = sync_fetch_challenge(cfg, cfg->account->public_id, nonce_hex);
    if(challenge_result != FLINT_LYRA_SYNC_OK) {
        flint_lyra_sync_buffer_free(&body);
        return challenge_result;
    }
    if(!sync_build_message("POST", FLINT_LYRA_LOGIN_PATH, nonce_hex,
                           body.data, message, sizeof(message))) {
        flint_lyra_sync_buffer_free(&body);
        return FLINT_LYRA_SYNC_SIGN_FAILED;
    }
    if(!flint_lyra_account_sign_hex(cfg->account, (const uint8_t *)message, strlen(message),
                                    signature_hex, sizeof(signature_hex))) {
        flint_lyra_sync_buffer_free(&body);
        return FLINT_LYRA_SYNC_SIGN_FAILED;
    }
    flint_lyra_sync_join_url(url, sizeof(url), cfg->base_url, FLINT_LYRA_LOGIN_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", cfg->account->public_id);
    snprintf(signature_header, sizeof(signature_header), "X-Inbe-Signature: %s", signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = cfg->http_request("POST", url, body.data, headers, 3, &response, &status, cfg->user);
    flint_lyra_sync_buffer_free(&body);
    if(!ok) {
        sync_log_http_failure(cfg, "login request", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "login auth", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "login", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    expires_in = flint_lyra_sync_find_json_int64(response.data, "expires_in_seconds", 3600);
    if(!flint_lyra_sync_find_json_string(response.data, "auth_token", token, sizeof(token))) {
        sync_log_http_failure(cfg, "login payload", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    }
    expires_at = (long long)time(NULL) + expires_in - 30;
    if(expires_at < (long long)time(NULL))
        expires_at = (long long)time(NULL);
    {
        char text[32];
        snprintf(text, sizeof(text), "%lld", expires_at);
        cfg->set_text(FLINT_LYRA_SYNC_AUTH_TOKEN_KEY, token, cfg->user);
        cfg->set_text(FLINT_LYRA_SYNC_AUTH_TOKEN_EXPIRES_KEY, text, cfg->user);
    }
    flint_lyra_sync_buffer_free(&response);
    return FLINT_LYRA_SYNC_OK;
}

static FlintLyraSyncResult
sync_send_bearer(const FlintLyraSyncConfig *cfg, const char *body, const char *token)
{
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    FlintLyraSyncBuffer response = {0};
    long status = 0;
    int ok;

    flint_lyra_sync_join_url(url, sizeof(url), cfg->base_url, FLINT_LYRA_SYNC_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", cfg->account->public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             token != NULL ? token : "");
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    ok = cfg->http_request("POST", url, body, headers, 3, &response, &status, cfg->user);
    if(!ok) {
        sync_log_http_failure(cfg, "sync request", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure(cfg, "sync auth", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, "sync", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    if(cfg->apply_response == NULL || !cfg->apply_response(response.data, cfg->user)) {
        sync_log_http_failure(cfg, "sync payload", status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    }
    flint_lyra_sync_buffer_free(&response);
    return FLINT_LYRA_SYNC_OK;
}

FlintLyraSyncResult
flint_lyra_sync_run(const FlintLyraSyncConfig *cfg)
{
    char *payload;
    FlintLyraSyncResult result;
    char token[4096];

    if(!sync_config_valid(cfg) || cfg->build_payload == NULL || cfg->free_payload == NULL)
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    if(!flint_lyra_sync_url_valid(cfg->base_url))
        return FLINT_LYRA_SYNC_INVALID_URL;
    if(!flint_lyra_account_has_values(cfg->account))
        return FLINT_LYRA_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = flint_lyra_sync_login(cfg);
        if(result != FLINT_LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return FLINT_LYRA_SYNC_AUTH_FAILED;
    }
    payload = cfg->build_payload(cfg->account->public_id, cfg->account->public_key_hex, cfg->user);
    if(payload == NULL)
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    result = sync_send_bearer(cfg, payload, token);
    if(result == FLINT_LYRA_SYNC_AUTH_FAILED) {
        flint_lyra_sync_clear_auth_token(cfg);
        result = flint_lyra_sync_login(cfg);
        if(result == FLINT_LYRA_SYNC_OK && sync_load_valid_auth_token(cfg, token, sizeof(token)))
            result = sync_send_bearer(cfg, payload, token);
        else if(result == FLINT_LYRA_SYNC_OK)
            result = FLINT_LYRA_SYNC_AUTH_FAILED;
    }
    cfg->free_payload(payload, cfg->user);
    if(result == FLINT_LYRA_SYNC_OK && cfg->purge_synced_deleted != NULL)
        cfg->purge_synced_deleted(cfg->user);
    return result;
}

static int
sync_copy_response_text(const FlintLyraSyncBuffer *response, char *out, size_t out_size)
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

FlintLyraSyncResult
flint_lyra_sync_bearer_request(const FlintLyraSyncConfig *cfg, const char *method,
                               const char *path, const char *body,
                               char *out, size_t out_size)
{
    char token[4096];
    char url[1024];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    int header_count = 0;
    FlintLyraSyncBuffer response = {0};
    long status = 0;
    FlintLyraSyncResult result;
    int ok;
    int retried_auth = 0;

    if(out != NULL && out_size > 0)
        out[0] = '\0';
    if(!sync_config_valid(cfg))
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    if(!flint_lyra_sync_url_valid(cfg->base_url))
        return FLINT_LYRA_SYNC_INVALID_URL;
    if(method == NULL || path == NULL)
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    if(!flint_lyra_account_has_values(cfg->account))
        return FLINT_LYRA_SYNC_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(cfg, token, sizeof(token))) {
        result = flint_lyra_sync_login(cfg);
        if(result != FLINT_LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return FLINT_LYRA_SYNC_AUTH_FAILED;
    }

retry:
    if(!flint_lyra_sync_join_url(url, sizeof(url), cfg->base_url, path))
        return FLINT_LYRA_SYNC_INVALID_URL;
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
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    if(status == 401) {
        flint_lyra_sync_buffer_free(&response);
        flint_lyra_sync_clear_auth_token(cfg);
        if(retried_auth)
            return FLINT_LYRA_SYNC_AUTH_FAILED;
        retried_auth = 1;
        result = flint_lyra_sync_login(cfg);
        if(result != FLINT_LYRA_SYNC_OK)
            return result;
        if(!sync_load_valid_auth_token(cfg, token, sizeof(token)))
            return FLINT_LYRA_SYNC_AUTH_FAILED;
        status = 0;
        header_count = 0;
        goto retry;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure(cfg, path, status, response.data);
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    }
    if(!sync_copy_response_text(&response, out, out_size)) {
        flint_lyra_sync_buffer_free(&response);
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    }
    flint_lyra_sync_buffer_free(&response);
    return FLINT_LYRA_SYNC_OK;
}

FlintLyraSyncResult
flint_lyra_sync_delete_account(const FlintLyraSyncConfig *cfg)
{
    char url[768];
    char exported_key[FLINT_LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    FlintLyraSyncBuffer body = {0};
    FlintLyraSyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int ok;

    if(!sync_config_valid(cfg))
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    if(!flint_lyra_sync_url_valid(cfg->base_url))
        return FLINT_LYRA_SYNC_INVALID_URL;
    if(!flint_lyra_account_has_values(cfg->account))
        return FLINT_LYRA_SYNC_NO_ACCOUNT;
    if(!flint_lyra_account_export_text(cfg->account, exported_key, sizeof(exported_key)))
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    if(!flint_lyra_sync_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !flint_lyra_sync_buffer_append_json_string(&body, cfg->account->public_id) ||
       !flint_lyra_sync_buffer_append(&body, ",\"exported_key\":", strlen(",\"exported_key\":")) ||
       !flint_lyra_sync_buffer_append_json_string(&body, exported_key) ||
       !flint_lyra_sync_buffer_append(&body, "}", 1)) {
        flint_lyra_sync_buffer_free(&body);
        return FLINT_LYRA_SYNC_PAYLOAD_FAILED;
    }

    flint_lyra_sync_join_url(url, sizeof(url), cfg->base_url,
                             FLINT_LYRA_ACCOUNT_DELETE_WITH_KEY_PATH);
    ok = cfg->http_request("POST", url, body.data, headers, 1, &response, &status, cfg->user);
    flint_lyra_sync_buffer_free(&body);
    flint_lyra_sync_buffer_free(&response);
    if(!ok)
        return FLINT_LYRA_SYNC_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return FLINT_LYRA_SYNC_AUTH_FAILED;
    return status >= 200 && status < 300 ? FLINT_LYRA_SYNC_OK : FLINT_LYRA_SYNC_REQUEST_FAILED;
}
