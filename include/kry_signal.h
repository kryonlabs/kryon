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
#include "kryon_property.h" /* PropertyValue as the signal argument */

#define SIGNAL_NAME_MAX 64
#define SIGNAL_CONNECTIONS_MAX 256

typedef struct SignalConnection {
    NodeId emitter;
    char signal[SIGNAL_NAME_MAX];
    NodeId target;
    char handler[SIGNAL_NAME_MAX];
    int alive;
} SignalConnection;

/* Connect (emitter.signal) to (target.handler). Subsequent emits of that
 * signal on that emitter invoke the targets handler. Duplicate connects are
 * ignored. Returns 1 if the connection was added, 0 on full/invalid. */
int SignalConnect(Scene *scene, NodeId emitter, const char *signal,
                     NodeId target, const char *handler);

/* Emit `signal` on `emitter`, passing `arg` to every connected handler. Each
 * connections target nodes kind ops may provide a `handle_signal` hook; the
 * matching handler name is dispatched through it. Returns the number of
 * handlers invoked. */
int SignalEmit(Scene *scene, NodeId emitter, const char *signal,
                  PropertyValue arg);

/* Disconnect all connections involving `node` (as emitter or target). Called
 * automatically by NodeRemove via the scene tree so dead nodes dont
 * accumulate connections. */
void SignalDisconnectNode(Scene *scene, NodeId node);

/* Per-kind signal handler hook. If a kind registers one, SignalEmit looks
 * up the target nodes kind ops and calls this when a connection matches. */
typedef void (*SignalHandlerFn)(Scene *scene, NodeId target,
                                   NodeId emitter, const char *handler,
                                   PropertyValue arg);
void NodeKindRegisterSignalHandler(NodeKind kind, SignalHandlerFn fn);

#endif
