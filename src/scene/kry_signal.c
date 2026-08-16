/*
 * Signal bus implementation. Connections are stored in a file-scope pool keyed
 * by scene pointer so Scene stays a fixed struct. Emits walk the pool,
 * match (emitter, signal), and dispatch via the target node kind's registered
 * signal handler.
 */

#include "kry_signal.h"
#include <stdio.h>
#include <string.h>

#define SIGNAL_POOLS_MAX 8

typedef struct SignalPool {
    Scene *scene;
    SignalConnection connections[SIGNAL_CONNECTIONS_MAX];
    int count;
} SignalPool;

static SignalPool g_signal_pools[SIGNAL_POOLS_MAX];
static SignalHandlerFn g_signal_handlers[NODE_CUSTOM + 1];

static SignalPool *
kry_signal_pool(Scene *scene)
{
    int i;
    for(i = 0; i < SIGNAL_POOLS_MAX; i++) {
        if(g_signal_pools[i].scene == scene)
            return &g_signal_pools[i];
    }
    for(i = 0; i < SIGNAL_POOLS_MAX; i++) {
        if(g_signal_pools[i].scene == NULL) {
            g_signal_pools[i].scene = scene;
            g_signal_pools[i].count = 0;
            return &g_signal_pools[i];
        }
    }
    return NULL;
}

void
NodeKindRegisterSignalHandler(NodeKind kind, SignalHandlerFn fn)
{
    if(kind < 0 || kind > NODE_CUSTOM)
        return;
    g_signal_handlers[kind] = fn;
}

int
SignalConnect(Scene *scene, NodeId emitter, const char *signal,
                 NodeId target, const char *handler)
{
    SignalPool *pool;
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
    if(pool->count >= SIGNAL_CONNECTIONS_MAX)
        return 0;
    pool->connections[pool->count].emitter = emitter;
    pool->connections[pool->count].target = target;
    snprintf(pool->connections[pool->count].signal, SIGNAL_NAME_MAX, "%s", signal);
    snprintf(pool->connections[pool->count].handler, SIGNAL_NAME_MAX, "%s", handler);
    pool->connections[pool->count].alive = 1;
    pool->count++;
    return 1;
}

int
SignalEmit(Scene *scene, NodeId emitter, const char *signal,
              PropertyValue arg)
{
    SignalPool *pool;
    int i;
    int fired = 0;

    if(scene == NULL || signal == NULL)
        return 0;
    pool = kry_signal_pool(scene);
    if(pool == NULL)
        return 0;
    for(i = 0; i < pool->count; i++) {
        SignalConnection *c = &pool->connections[i];
        Node *target;
        SignalHandlerFn handler_fn;

        if(!c->alive || c->emitter != emitter ||
           strcmp(c->signal, signal) != 0)
            continue;
        target = NodeGet(scene, c->target);
        if(target == NULL || !(target->flags & NODE_FLAG_ALIVE))
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
SignalDisconnectNode(Scene *scene, NodeId node)
{
    SignalPool *pool;
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
