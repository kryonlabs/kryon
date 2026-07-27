#ifndef KSYNC_SYNC_H
#define KSYNC_SYNC_H

#include "ksync_account.h"

#include <stddef.h>

typedef enum KsyncSyncResult {
    KSYNC_SYNC_OK = 0,
    KSYNC_SYNC_INVALID_URL,
    KSYNC_SYNC_NO_ACCOUNT,
    KSYNC_SYNC_PAYLOAD_FAILED,
    KSYNC_SYNC_CHALLENGE_FAILED,
    KSYNC_SYNC_SIGN_FAILED,
    KSYNC_SYNC_REQUEST_FAILED,
    KSYNC_SYNC_AUTH_FAILED
} KsyncSyncResult;

typedef struct KsyncSyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} KsyncSyncBuffer;

typedef int (*KsyncSyncHttpRequestFn)(const char *method, const char *url,
                                          const char *body,
                                          const char *const *headers,
                                          int header_count,
                                          KsyncSyncBuffer *response,
                                          long *status, void *user);
typedef const char *(*KsyncSyncGetTextFn)(const char *key, void *user);
typedef void (*KsyncSyncSetTextFn)(const char *key, const char *value, void *user);
typedef char *(*KsyncSyncBuildPayloadFn)(const char *user_id_hash,
                                             const char *public_key_hex, void *user);
typedef void (*KsyncSyncFreePayloadFn)(char *payload, void *user);
typedef int (*KsyncSyncApplyResponseFn)(const char *response_json, void *user);
typedef void (*KsyncSyncVoidFn)(void *user);
typedef void (*KsyncSyncLogFn)(const char *step, long status,
                                   const char *response, void *user);

typedef struct KsyncSyncConfig {
    const char *base_url;
    const KsyncAccount *account;
    const char *client_id;
    const char *signature_context;
    const char *user_header_name;
    const char *signature_header_name;
    KsyncSyncHttpRequestFn http_request;
    KsyncSyncGetTextFn get_text;
    KsyncSyncSetTextFn set_text;
    KsyncSyncBuildPayloadFn build_payload;
    KsyncSyncFreePayloadFn free_payload;
    KsyncSyncApplyResponseFn apply_response;
    KsyncSyncVoidFn purge_synced_deleted;
    KsyncSyncLogFn log_http_failure;
    void *user;
} KsyncSyncConfig;

const char *GetKsyncSyncResultName(KsyncSyncResult result);
int IsKsyncSyncURLValid(const char *url);
int NormalizeKsyncSyncURL(const char *input, char *out, size_t out_size);
int JoinKsyncSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinKsyncSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
int AppendKsyncSyncBuffer(KsyncSyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendKsyncSyncBufferJSONString(KsyncSyncBuffer *buffer,
                                              const char *text);
void FreeKsyncSyncBuffer(KsyncSyncBuffer *buffer);
int FindKsyncSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindKsyncSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
void ClearKsyncSyncAuthToken(const KsyncSyncConfig *cfg);
KsyncSyncResult LoginKsyncSync(const KsyncSyncConfig *cfg);
KsyncSyncResult RunKsyncSync(const KsyncSyncConfig *cfg);
KsyncSyncResult RequestKsyncSyncBearer(const KsyncSyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
KsyncSyncResult DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg);

#endif
