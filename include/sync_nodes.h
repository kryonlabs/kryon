#ifndef SYNC_NODES_H
#define SYNC_NODES_H

#include "sync.h"

#include <stddef.h>

enum {
    SYNC_NODE_POOL_CAPACITY = 8,
    SYNC_NODE_ID_SIZE = 65,
    SYNC_NODE_NAME_SIZE = 80,
    SYNC_NODE_URL_SIZE = 512
};

typedef enum SyncNodeTrust {
    SYNC_NODE_PUBLIC = 0,
    SYNC_NODE_PAIRED = 1
} SyncNodeTrust;

typedef struct SyncNode {
    char id[SYNC_NODE_ID_SIZE];
    char name[SYNC_NODE_NAME_SIZE];
    char url[SYNC_NODE_URL_SIZE];
    SyncNodeTrust trust;
    int local;
    int consecutive_failures;
    long long retry_after;
} SyncNode;

typedef struct SyncNodePool {
    SyncNode nodes[SYNC_NODE_POOL_CAPACITY];
    int count;
    int active_index;
} SyncNodePool;

void InitSyncNodePool(SyncNodePool *pool);
int AddSyncNode(SyncNodePool *pool, const char *id, const char *name,
                const char *url, SyncNodeTrust trust, int local);
const SyncNode *SelectSyncNode(const SyncNodePool *pool, long long now);
const SyncNode *GetActiveSyncNode(const SyncNodePool *pool);
void MarkSyncNodeResult(SyncNodePool *pool, int index, int succeeded,
                        long long now);
int BuildSyncNodeStorageKey(const SyncNode *node, const char *key,
                            char *out, size_t out_size);

/* Tries paired local nodes first, then other paired nodes, then public nodes.
 * Authentication tokens and clock skew are stored under a node-specific key.
 * Payload or account failures are returned immediately; connectivity and
 * authentication failures continue to the next eligible node. */
SyncResult RunSyncWithNodes(SyncNodePool *pool,
                            const SyncConfig *config);

#endif
