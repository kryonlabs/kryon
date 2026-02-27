/*
 * Per-Window Namespace Support
 * C89/C90 compliant
 *
 * Implements rio-style per-window namespaces.
 * Each window can spawn nested WM instances or mount filesystems.
 */

#ifndef KRYON_NAMESPACE_H
#define KRYON_NAMESPACE_H

#include "kryon.h"

/*
 * Forward declarations
 */
struct KryonWindow;

/*
 * Namespace types
 */
typedef enum {
    NS_LOCAL = 0,            /* Empty namespace (default) */
    NS_NESTED_WM,            /* Running nested WM instance */
    NS_FILESYSTEM            /* Mounted filesystem */
} NamespaceType;

/*
 * Initialize namespace for a window
 */
int window_namespace_init(struct KryonWindow *win);

/*
 * Spawn a nested WM instance in a window
 *
 * The nested WM will use this window's virtual framebuffer.
 *
 * Returns 0 on success, -1 on error
 */
int window_spawn_nested_wm(struct KryonWindow *win);

/*
 * Mount filesystem into window's namespace
 *
 * Spec formats:
 *   "nest!"              - Run nested WM instance
 *   "local!/path"        - Mount local filesystem
 *
 * Returns 0 on success, -1 on error
 */
int window_namespace_mount(struct KryonWindow *win, const char *spec);

/*
 * Unmount/cleanup namespace (kill nested WM, unmount fs)
 */
int window_namespace_unmount(struct KryonWindow *win);

#endif /* KRYON_NAMESPACE_H */
