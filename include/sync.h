#ifndef SYNC_H
#define SYNC_H

#include "sync/account.h"

#include <stddef.h>

typedef enum SyncResult {
    SYNC_OK = 0,
    SYNC_INVALID_URL,
    SYNC_NO_ACCOUNT,
    SYNC_PAYLOAD_FAILED,
    SYNC_CHALLENGE_FAILED,
    SYNC_SIGN_FAILED,
    SYNC_REQUEST_FAILED,
    SYNC_AUTH_FAILED
} SyncResult;

typedef struct SyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} SyncBuffer;

typedef int (*SyncHttpRequestFn)(const char *method, const char *url,
                                          const char *body,
                                          const char *const *headers,
                                          int header_count,
                                          SyncBuffer *response,
                                          long *status, void *user);
typedef const char *(*SyncGetTextFn)(const char *key, void *user);
typedef void (*SyncSetTextFn)(const char *key, const char *value, void *user);
typedef char *(*SyncBuildPayloadFn)(const char *user_id_hash,
                                             const char *public_key_hex, void *user);
typedef void (*SyncFreePayloadFn)(char *payload, void *user);
typedef int (*SyncApplyResponseFn)(const char *response_json, void *user);
typedef void (*SyncVoidFn)(void *user);
typedef void (*SyncLogFn)(const char *step, long status,
                                   const char *response, void *user);

typedef struct SyncConfig {
    const char *base_url;
    const SyncAccount *account;
    const char *client_id;
    const char *app_id;
    int protocol_version;
    const char *signature_context;
    const char *user_header_name;
    const char *signature_header_name;
    SyncHttpRequestFn http_request;
    SyncGetTextFn get_text;
    SyncSetTextFn set_text;
    SyncBuildPayloadFn build_payload;
    SyncFreePayloadFn free_payload;
    SyncApplyResponseFn apply_response;
    SyncVoidFn purge_synced_deleted;
    SyncLogFn log_http_failure;
    /* Retry policy for transient failures (transport errors, 429, 5xx).
     * Retries only happen when sleep_ms is provided; without it the single
     * attempt behaviour is unchanged. Delay grows exponentially:
     * retry_delay_ms << attempt, capped at 30s. */
    int retry_max;
    int retry_delay_ms;
    void (*sleep_ms)(int ms, void *user);
    /* When set, RunSync encrypts the outgoing payload with a key
     * derived from the account and decrypts envelope responses before
     * apply_response (server sees opaque ciphertext). */
    int encrypt_payload;
    void *user;
} SyncConfig;

const char *GetSyncResultName(SyncResult result);
int IsSyncURLValid(const char *url);
int NormalizeSyncURL(const char *input, char *out, size_t out_size);
int JoinSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
int AppendSyncBuffer(SyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendSyncBufferJSONString(SyncBuffer *buffer,
                                              const char *text);
void FreeSyncBuffer(SyncBuffer *buffer);
int FindSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
void ClearSyncAuthToken(const SyncConfig *cfg);
SyncResult LoginSync(const SyncConfig *cfg);
SyncResult RunSync(const SyncConfig *cfg);
SyncResult RequestSyncBearer(const SyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
SyncResult DeleteSyncAccount(const SyncConfig *cfg);
/* Opt-in end-to-end payload encryption. v1 envelopes seal plaintext JSON.
 * v2 envelopes compress plaintext first when that reduces size, then seal
 * the compressed bytes. Both versions use ChaCha20-Poly1305 under a key
 * derived from the account private key. Unwrap returns 0 for input that is
 * not an envelope. Caller frees *out. */
int WrapSyncPayload(const SyncAccount *account, const char *payload, char **out);
int UnwrapSyncPayload(const SyncAccount *account, const char *envelope_json, char **out);
int DefaultSyncHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers,
                            int header_count,
                            SyncBuffer *response,
                            long *status, void *user);
SyncResult WaitForRemoteSyncEvent(const SyncConfig *cfg,
                                     const char *path);
#if defined(__EMSCRIPTEN__)
int StartWebSync(const SyncConfig *cfg);
int PollWebSync(SyncResult *result, int *changed);
int StartWebRemoteEvents(const SyncConfig *cfg, const char *path);
int PollWebRemoteEvents(void);
#endif

#endif
