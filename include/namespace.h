/*
 * Per-Window Namespace Support
 * C89/C90 compliant
 *
 * Implements rio-style per-window namespaces with network transparency.
 * Each window can mount remote WMs, filesystems, or run nested WM instances.
 */

#ifndef KRYON_NAMESPACE_H
#define KRYON_NAMESPACE_H

#include "kryon.h"

/*
 * Namespace types
 */
typedef enum {
    NS_LOCAL = 0,            /* Empty namespace (default) */
    NS_REMOTE_WM,            /* Connected to remote WM server */
    NS_NESTED_WM,            /* Running nested WM instance */
    NS_FILESYSTEM            /* Mounted filesystem */
} NamespaceType;

/*
 * Initialize namespace for a window
 *
 * Parameters:
 *   win - Window to initialize namespace for
 *
 * Returns 0 on success, -1 on error
 */
int window_namespace_init(struct KryonWindow *win);

/*
 * Mount a remote WM or filesystem into window
 *
 * Spec formats:
 *   "local!"              - Empty local namespace
 *   "tcp!host!port"       - Connect to remote WM
 *   "nest!widthxheight"   - Run nested WM (future)
 *   "local!/path"         - Mount local filesystem (future)
 *
 * Parameters:
 *   win - Window to mount into
 *   spec - Mount specification string
 *
 * Returns 0 on success, -1 on error
 */
int window_namespace_mount(struct KryonWindow *win, const char *spec);

/*
 * Unmount/cleanup namespace
 *
 * Parameters:
 *   win - Window to unmount
 *
 * Returns 0 on success, -1 on error
 */
int window_namespace_unmount(struct KryonWindow *win);

/*
 * Resolve path in window's namespace
 *
 * Parameters:
 *   win - Window
 *   path - Path to resolve
 *
 * Returns P9Node on success, NULL on error
 */
P9Node *window_namespace_lookup(struct KryonWindow *win, const char *path);

#endif /* KRYON_NAMESPACE_H */
