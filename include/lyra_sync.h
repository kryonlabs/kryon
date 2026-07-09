#ifndef LYRA_SYNC_H
#define LYRA_SYNC_H

#include "lyra_account.h"

#include <stddef.h>

typedef enum LyraSyncResult {
    LYRA_SYNC_OK = 0,
    LYRA_SYNC_INVALID_URL,
    LYRA_SYNC_NO_ACCOUNT,
    LYRA_SYNC_PAYLOAD_FAILED,
    LYRA_SYNC_CHALLENGE_FAILED,
    LYRA_SYNC_SIGN_FAILED,
    LYRA_SYNC_REQUEST_FAILED,
    LYRA_SYNC_AUTH_FAILED
} LyraSyncResult;

typedef struct LyraSyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} LyraSyncBuffer;

typedef int (*LyraSyncHttpRequestFn)(const char *method, const char *url,
                                          const char *body,
                                          const char *const *headers,
                                          int header_count,
                                          LyraSyncBuffer *response,
                                          long *status, void *user);
typedef const char *(*LyraSyncGetTextFn)(const char *key, void *user);
typedef void (*LyraSyncSetTextFn)(const char *key, const char *value, void *user);
typedef char *(*LyraSyncBuildPayloadFn)(const char *user_id_hash,
                                             const char *public_key_hex, void *user);
typedef void (*LyraSyncFreePayloadFn)(char *payload, void *user);
typedef int (*LyraSyncApplyResponseFn)(const char *response_json, void *user);
typedef void (*LyraSyncVoidFn)(void *user);
typedef void (*LyraSyncLogFn)(const char *step, long status,
                                   const char *response, void *user);

typedef struct LyraSyncConfig {
    const char *base_url;
    const LyraAccount *account;
    const char *client_id;
    LyraSyncHttpRequestFn http_request;
    LyraSyncGetTextFn get_text;
    LyraSyncSetTextFn set_text;
    LyraSyncBuildPayloadFn build_payload;
    LyraSyncFreePayloadFn free_payload;
    LyraSyncApplyResponseFn apply_response;
    LyraSyncVoidFn purge_synced_deleted;
    LyraSyncLogFn log_http_failure;
    void *user;
} LyraSyncConfig;

const char *GetLyraSyncResultName(LyraSyncResult result);
int IsLyraSyncURLValid(const char *url);
int NormalizeLyraSyncURL(const char *input, char *out, size_t out_size);
int JoinLyraSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinLyraSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
int AppendLyraSyncBuffer(LyraSyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendLyraSyncBufferJSONString(LyraSyncBuffer *buffer,
                                              const char *text);
void FreeLyraSyncBuffer(LyraSyncBuffer *buffer);
int FindLyraSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindLyraSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
void ClearLyraSyncAuthToken(const LyraSyncConfig *cfg);
LyraSyncResult LoginLyraSync(const LyraSyncConfig *cfg);
LyraSyncResult RunLyraSync(const LyraSyncConfig *cfg);
LyraSyncResult RequestLyraSyncBearer(const LyraSyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
LyraSyncResult DeleteLyraSyncAccount(const LyraSyncConfig *cfg);

#endif
