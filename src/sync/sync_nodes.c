#include "sync_nodes.h"
#include "sync_crypto.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct NodeSyncContext {
    const SyncConfig *original;
    const SyncNode *node;
} NodeSyncContext;

static int
node_priority(const SyncNode *node)
{
    if(node->trust == SYNC_NODE_PAIRED && node->local) {
        return 0;
    }
    if(node->trust == SYNC_NODE_PAIRED) {
        return 1;
    }
    return 2;
}

void
InitSyncNodePool(SyncNodePool *pool)
{
    if(pool == NULL) {
        return;
    }
    memset(pool, 0, sizeof(*pool));
    pool->active_index = -1;
}

int
AddSyncNode(SyncNodePool *pool, const char *id, const char *name,
            const char *url, SyncNodeTrust trust, int local)
{
    char normalized_url[SYNC_NODE_URL_SIZE];
    SyncNode *node;
    size_t url_length;

    if(pool == NULL || url == NULL ||
       !NormalizeSyncURL(url, normalized_url, sizeof(normalized_url))) {
        return 0;
    }
    url_length = strlen(normalized_url);
    while(url_length > 0 && normalized_url[url_length - 1] == '/') {
        normalized_url[--url_length] = '\0';
    }
    for(int i = 0; i < pool->count; i++) {
        node = &pool->nodes[i];
        if(strcmp(node->url, normalized_url) == 0) {
            if(id != NULL && id[0] != '\0') {
                snprintf(node->id, sizeof(node->id), "%s", id);
            }
            if(name != NULL && name[0] != '\0') {
                snprintf(node->name, sizeof(node->name), "%s", name);
            }
            if(trust == SYNC_NODE_PAIRED) {
                node->trust = SYNC_NODE_PAIRED;
            }
            if(local) {
                node->local = 1;
            }
            return 1;
        }
    }
    if(pool->count >= SYNC_NODE_POOL_CAPACITY) {
        return 0;
    }
    node = &pool->nodes[pool->count++];
    snprintf(node->id, sizeof(node->id), "%s", id != NULL ? id : "");
    snprintf(node->name, sizeof(node->name), "%s", name != NULL ? name : "");
    snprintf(node->url, sizeof(node->url), "%s", normalized_url);
    node->trust = trust;
    node->local = local != 0;
    return 1;
}

static int
select_node_index(const SyncNodePool *pool, long long now,
                  const int *attempted)
{
    int selected = -1;
    int selected_priority = 0;

    if(pool == NULL) {
        return -1;
    }
    for(int i = 0; i < pool->count; i++) {
        const SyncNode *candidate = &pool->nodes[i];
        int priority;

        if((attempted != NULL && attempted[i]) || candidate->retry_after > now) {
            continue;
        }
        priority = node_priority(candidate);
        if(selected < 0 || priority < selected_priority) {
            selected = i;
            selected_priority = priority;
        }
    }
    return selected;
}

const SyncNode *
SelectSyncNode(const SyncNodePool *pool, long long now)
{
    int index = select_node_index(pool, now, NULL);

    if(index < 0) {
        return NULL;
    }
    return &pool->nodes[index];
}

const SyncNode *
GetActiveSyncNode(const SyncNodePool *pool)
{
    if(pool == NULL || pool->active_index < 0 || pool->active_index >= pool->count) {
        return NULL;
    }
    return &pool->nodes[pool->active_index];
}

void
MarkSyncNodeResult(SyncNodePool *pool, int index, int succeeded, long long now)
{
    SyncNode *node;
    int delay;

    if(pool == NULL || index < 0 || index >= pool->count) {
        return;
    }
    node = &pool->nodes[index];
    if(succeeded) {
        node->consecutive_failures = 0;
        node->retry_after = 0;
        pool->active_index = index;
        return;
    }
    if(node->consecutive_failures < 10) {
        node->consecutive_failures++;
    }
    delay = 1 << node->consecutive_failures;
    if(delay > 300) {
        delay = 300;
    }
    node->retry_after = now + delay;
    if(pool->active_index == index) {
        pool->active_index = -1;
    }
}

int
BuildSyncNodeStorageKey(const SyncNode *node, const char *key,
                        char *out, size_t out_size)
{
    char fallback_id[SYNC_PUBLIC_ID_HEX_SIZE];
    const char *id;
    int written;

    if(node == NULL || key == NULL || out == NULL || out_size == 0) {
        return 0;
    }
    id = node->id;
    if(id[0] == '\0') {
        SyncCryptoSha256Hex((const uint8_t *)node->url, strlen(node->url), fallback_id);
        id = fallback_id;
    }
    written = snprintf(out, out_size, "sync_node_%s_%s", id, key);
    return written > 0 && (size_t)written < out_size;
}

static const char *
node_get_text(const char *key, void *user)
{
    NodeSyncContext *context = user;
    char storage_key[192];

    if(context == NULL || context->original->get_text == NULL) {
        return "";
    }
    if(!BuildSyncNodeStorageKey(context->node, key, storage_key,
                                sizeof(storage_key))) {
        return "";
    }
    return context->original->get_text(storage_key, context->original->user);
}

static void
node_set_text(const char *key, const char *value, void *user)
{
    NodeSyncContext *context = user;
    char storage_key[192];

    if(context == NULL || context->original->set_text == NULL ||
       !BuildSyncNodeStorageKey(context->node, key, storage_key,
                                sizeof(storage_key))) {
        return;
    }
    context->original->set_text(storage_key, value, context->original->user);
}

static int
node_http_request(const char *method, const char *url, const char *body,
                  const char *const *headers, int header_count,
                  SyncBuffer *response, long *status, void *user)
{
    NodeSyncContext *context = user;
    return context->original->http_request(method, url, body, headers,
                                           header_count, response, status,
                                           context->original->user);
}

static char *
node_build_payload(const char *user_id, const char *public_key, void *user)
{
    NodeSyncContext *context = user;
    return context->original->build_payload(user_id, public_key,
                                            context->original->user);
}

static void
node_free_payload(char *payload, void *user)
{
    NodeSyncContext *context = user;
    context->original->free_payload(payload, context->original->user);
}

static int
node_apply_response(const char *response, void *user)
{
    NodeSyncContext *context = user;
    if(context->original->apply_response == NULL) {
        return 1;
    }
    return context->original->apply_response(response, context->original->user);
}

static void
node_purge_deleted(void *user)
{
    NodeSyncContext *context = user;
    if(context->original->purge_synced_deleted != NULL) {
        context->original->purge_synced_deleted(context->original->user);
    }
}

static void
node_log_failure(const char *step, long status, const char *response, void *user)
{
    NodeSyncContext *context = user;
    if(context->original->log_http_failure != NULL) {
        context->original->log_http_failure(step, status, response,
                                            context->original->user);
    }
}

static void
node_sleep(int milliseconds, void *user)
{
    NodeSyncContext *context = user;
    if(context->original->sleep_ms != NULL) {
        context->original->sleep_ms(milliseconds, context->original->user);
    }
}

static SyncResult
run_on_node(const SyncConfig *original, const SyncNode *node)
{
    NodeSyncContext context = {original, node};
    SyncConfig config = *original;

    config.base_url = node->url;
    config.signature_context = "daochi-sync-v1";
    config.user_header_name = "X-Daochi-User";
    config.signature_header_name = "X-Daochi-Signature";
    config.http_request = node_http_request;
    config.get_text = node_get_text;
    config.set_text = node_set_text;
    config.build_payload = node_build_payload;
    config.free_payload = node_free_payload;
    config.apply_response = original->apply_response != NULL ? node_apply_response : NULL;
    config.purge_synced_deleted = original->purge_synced_deleted != NULL ?
                                   node_purge_deleted : NULL;
    config.log_http_failure = original->log_http_failure != NULL ?
                              node_log_failure : NULL;
    config.sleep_ms = original->sleep_ms != NULL ? node_sleep : NULL;
    config.user = &context;
    return RunSync(&config);
}

SyncResult
RunSyncWithNodes(SyncNodePool *pool, const SyncConfig *config)
{
    int attempted[SYNC_NODE_POOL_CAPACITY] = {0};
    long long now = (long long)time(NULL);
    SyncResult last_result = SYNC_REQUEST_FAILED;

    if(pool == NULL || config == NULL) {
        return SYNC_PAYLOAD_FAILED;
    }
    for(int attempt = 0; attempt < pool->count; attempt++) {
        int index = select_node_index(pool, now, attempted);

        if(index < 0) {
            break;
        }
        attempted[index] = 1;
        last_result = run_on_node(config, &pool->nodes[index]);
        if(last_result == SYNC_OK) {
            MarkSyncNodeResult(pool, index, 1, now);
            return last_result;
        }
        MarkSyncNodeResult(pool, index, 0, now);
        if(last_result == SYNC_NO_ACCOUNT ||
           last_result == SYNC_PAYLOAD_FAILED ||
           last_result == SYNC_SIGN_FAILED) {
            return last_result;
        }
    }
    return last_result;
}
