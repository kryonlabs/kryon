/*
 * Signal bus implementation. Connections are stored in a file-scope pool keyed
 * by scene pointer so KryScene stays a fixed struct. Emits walk the pool,
 * match (emitter, signal), and dispatch via the target node kind's registered
 * signal handler.
 */

#include "kry_signal.h"
#include <stdio.h>
#include <string.h>

#define KRY_SIGNAL_POOLS_MAX 8

typedef struct KrySignalPool {
    KryScene *scene;
    KrySignalConnection connections[KRY_SIGNAL_CONNECTIONS_MAX];
    int count;
} KrySignalPool;

static KrySignalPool g_signal_pools[KRY_SIGNAL_POOLS_MAX];
static KrySignalHandlerFn g_signal_handlers[KRY_NODE_CUSTOM + 1];

static KrySignalPool *
kry_signal_pool(KryScene *scene)
{
    int i;
    for(i = 0; i < KRY_SIGNAL_POOLS_MAX; i++) {
        if(g_signal_pools[i].scene == scene)
            return &g_signal_pools[i];
    }
    for(i = 0; i < KRY_SIGNAL_POOLS_MAX; i++) {
        if(g_signal_pools[i].scene == NULL) {
            g_signal_pools[i].scene = scene;
            g_signal_pools[i].count = 0;
            return &g_signal_pools[i];
        }
    }
    return NULL;
}

void
KryNodeKindRegisterSignalHandler(KryNodeKind kind, KrySignalHandlerFn fn)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return;
    g_signal_handlers[kind] = fn;
}

int
KrySignalConnect(KryScene *scene, KryNodeId emitter, const char *signal,
                 KryNodeId target, const char *handler)
{
    KrySignalPool *pool;
    int i;

    if(scene == NULL || signal == NULL || handler == NULL)
        return 0;
    pool = kry_signal_pool(scene);
    if(pool == NULL)
        return 0;
    for(i = 0; i < pool->count; i++) {
        if(pool->connections[i].alive &&
           pool->connections[i].emitter == emitter &&
           pool->connections[i].target == target &&
           strcmp(pool->connections[i].signal, signal) == 0 &&
           strcmp(pool->connections[i].handler, handler) == 0)
            return 1; /* already connected */
    }
    if(pool->count >= KRY_SIGNAL_CONNECTIONS_MAX)
        return 0;
    pool->connections[pool->count].emitter = emitter;
    pool->connections[pool->count].target = target;
    snprintf(pool->connections[pool->count].signal, KRY_SIGNAL_NAME_MAX, "%s", signal);
    snprintf(pool->connections[pool->count].handler, KRY_SIGNAL_NAME_MAX, "%s", handler);
    pool->connections[pool->count].alive = 1;
    pool->count++;
    return 1;
}

int
KrySignalEmit(KryScene *scene, KryNodeId emitter, const char *signal,
              KryonPropertyValue arg)
{
    KrySignalPool *pool;
    int i;
    int fired = 0;

    if(scene == NULL || signal == NULL)
        return 0;
    pool = kry_signal_pool(scene);
    if(pool == NULL)
        return 0;
    for(i = 0; i < pool->count; i++) {
        KrySignalConnection *c = &pool->connections[i];
        KryNode *target;
        KrySignalHandlerFn handler_fn;

        if(!c->alive || c->emitter != emitter ||
           strcmp(c->signal, signal) != 0)
            continue;
        target = KryNodeGet(scene, c->target);
        if(target == NULL || !(target->flags & KRY_NODE_FLAG_ALIVE))
            continue;
        handler_fn = g_signal_handlers[target->kind];
        if(handler_fn != NULL) {
            handler_fn(scene, c->target, emitter, c->handler, arg);
            fired++;
        }
    }
    return fired;
}

void
KrySignalDisconnectNode(KryScene *scene, KryNodeId node)
{
    KrySignalPool *pool;
    int i;
    if(scene == NULL)
        return;
    pool = kry_signal_pool(scene);
    if(pool == NULL)
        return;
    for(i = 0; i < pool->count; i++) {
        if(pool->connections[i].alive &&
           (pool->connections[i].emitter == node ||
            pool->connections[i].target == node))
            pool->connections[i].alive = 0;
    }
}
