#ifndef KRY_SIGNAL_H
#define KRY_SIGNAL_H

/*
 * Signals: a declarative event bus for the scene tree. A node declares a
 * signal by name; another node connects that (emitter, signal) pair to one of
 * its handler functions; emit fires all matching connections. Compile-time
 * only at the .kry level (k2c lowers signal/emit/connect to these calls); no
 * runtime scripting VM.
 */

#include "scene_tree.h"
#include "kryon_property.h" /* KryonPropertyValue as the signal argument */

#define KRY_SIGNAL_NAME_MAX 64
#define KRY_SIGNAL_CONNECTIONS_MAX 256

typedef struct KrySignalConnection {
    KryNodeId emitter;
    char signal[KRY_SIGNAL_NAME_MAX];
    KryNodeId target;
    char handler[KRY_SIGNAL_NAME_MAX];
    int alive;
} KrySignalConnection;

/* Connect (emitter.signal) to (target.handler). Subsequent emits of that
 * signal on that emitter invoke the target's handler. Duplicate connects are
 * ignored. Returns 1 if the connection was added, 0 on full/invalid. */
int KrySignalConnect(KryScene *scene, KryNodeId emitter, const char *signal,
                     KryNodeId target, const char *handler);

/* Emit `signal` on `emitter`, passing `arg` to every connected handler. Each
 * connection's target node's kind ops may provide a `handle_signal` hook; the
 * matching handler name is dispatched through it. Returns the number of
 * handlers invoked. */
int KrySignalEmit(KryScene *scene, KryNodeId emitter, const char *signal,
                  KryonPropertyValue arg);

/* Disconnect all connections involving `node` (as emitter or target). Called
 * automatically by KryNodeRemove via the scene tree so dead nodes don't
 * accumulate connections. */
void KrySignalDisconnectNode(KryScene *scene, KryNodeId node);

/* Per-kind signal handler hook. If a kind registers one, KrySignalEmit looks
 * up the target node's kind ops and calls this when a connection matches. */
typedef void (*KrySignalHandlerFn)(KryScene *scene, KryNodeId target,
                                   KryNodeId emitter, const char *handler,
                                   KryonPropertyValue arg);
void KryNodeKindRegisterSignalHandler(KryNodeKind kind, KrySignalHandlerFn fn);

#endif
