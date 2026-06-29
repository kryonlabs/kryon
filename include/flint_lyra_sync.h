#ifndef FLINT_LYRA_SYNC_H
#define FLINT_LYRA_SYNC_H

#include "flint_lyra_account.h"

#include <stddef.h>

typedef enum FlintLyraSyncResult {
    FLINT_LYRA_SYNC_OK = 0,
    FLINT_LYRA_SYNC_INVALID_URL,
    FLINT_LYRA_SYNC_NO_ACCOUNT,
    FLINT_LYRA_SYNC_PAYLOAD_FAILED,
    FLINT_LYRA_SYNC_CHALLENGE_FAILED,
    FLINT_LYRA_SYNC_SIGN_FAILED,
    FLINT_LYRA_SYNC_REQUEST_FAILED,
    FLINT_LYRA_SYNC_AUTH_FAILED
} FlintLyraSyncResult;

typedef struct FlintLyraSyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} FlintLyraSyncBuffer;

typedef int (*FlintLyraSyncHttpRequestFn)(const char *method, const char *url,
                                          const char *body,
                                          const char *const *headers,
                                          int header_count,
                                          FlintLyraSyncBuffer *response,
                                          long *status, void *user);
typedef const char *(*FlintLyraSyncGetTextFn)(const char *key, void *user);
typedef void (*FlintLyraSyncSetTextFn)(const char *key, const char *value, void *user);
typedef char *(*FlintLyraSyncBuildPayloadFn)(const char *user_id_hash,
                                             const char *public_key_hex, void *user);
typedef void (*FlintLyraSyncFreePayloadFn)(char *payload, void *user);
typedef int (*FlintLyraSyncApplyResponseFn)(const char *response_json, void *user);
typedef void (*FlintLyraSyncVoidFn)(void *user);
typedef void (*FlintLyraSyncLogFn)(const char *step, long status,
                                   const char *response, void *user);

typedef struct FlintLyraSyncConfig {
    const char *base_url;
    const FlintLyraAccount *account;
    const char *client_id;
    FlintLyraSyncHttpRequestFn http_request;
    FlintLyraSyncGetTextFn get_text;
    FlintLyraSyncSetTextFn set_text;
    FlintLyraSyncBuildPayloadFn build_payload;
    FlintLyraSyncFreePayloadFn free_payload;
    FlintLyraSyncApplyResponseFn apply_response;
    FlintLyraSyncVoidFn purge_synced_deleted;
    FlintLyraSyncLogFn log_http_failure;
    void *user;
} FlintLyraSyncConfig;

const char *flint_lyra_sync_result_name(FlintLyraSyncResult result);
int flint_lyra_sync_url_valid(const char *url);
int flint_lyra_sync_normalize_url(const char *input, char *out, size_t out_size);
int flint_lyra_sync_join_url(char *out, size_t out_size,
                             const char *base_url, const char *path);
int flint_lyra_sync_join_ws_url(char *out, size_t out_size,
                                const char *base_url, const char *path);
int flint_lyra_sync_buffer_append(FlintLyraSyncBuffer *buffer,
                                  const void *data, size_t bytes);
int flint_lyra_sync_buffer_append_json_string(FlintLyraSyncBuffer *buffer,
                                              const char *text);
void flint_lyra_sync_buffer_free(FlintLyraSyncBuffer *buffer);
int flint_lyra_sync_find_json_string(const char *json, const char *key,
                                     char *out, size_t out_size);
long long flint_lyra_sync_find_json_int64(const char *json, const char *key,
                                          long long fallback);
void flint_lyra_sync_clear_auth_token(const FlintLyraSyncConfig *cfg);
FlintLyraSyncResult flint_lyra_sync_login(const FlintLyraSyncConfig *cfg);
FlintLyraSyncResult flint_lyra_sync_run(const FlintLyraSyncConfig *cfg);
FlintLyraSyncResult flint_lyra_sync_bearer_request(const FlintLyraSyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
FlintLyraSyncResult flint_lyra_sync_delete_account(const FlintLyraSyncConfig *cfg);

#endif
