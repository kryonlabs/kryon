/*
 * Kryon Window Manager Service API
 * C89/C90 compliant
 *
 * Provides filesystem-based interface to window management
 * Exports /mnt/wm to Marrow for external access
 */

#ifndef KRYON_WM_H
#define KRYON_WM_H

#include "kryon.h"

/*
 * Forward declarations
 */
struct KryonWindow;
struct KryonWidget;

/*
 * Initialize window manager filesystem
 * Creates the /mnt/wm directory structure
 *
 * Returns 0 on success, -1 on error
 */
int wm_service_init(P9Node *root);

/*
 * Get window manager root node
 * Returns the root of the /mnt/wm tree for service mounting
 *
 * Returns the root node, or NULL on error
 */
P9Node *wm_get_root(void);

/*
 * Create filesystem entries for a window
 * Called by window_create() to expose window via /mnt/wm
 *
 * Returns 0 on success, -1 on error
 */
int wm_create_window_entry(struct KryonWindow *win);

/*
 * Remove filesystem entries for a window
 * Called by window_destroy() to clean up /mnt/wm
 *
 * Returns 0 on success, -1 on error
 */
int wm_remove_window_entry(struct KryonWindow *win);

/*
 * Create filesystem entries for a widget
 * Called by widget_create() to expose widget via /mnt/wm
 *
 * Returns 0 on success, -1 on error
 */
int wm_create_widget_entry(struct KryonWidget *widget);

/*
 * Remove filesystem entries for a widget
 * Called by widget_destroy() to clean up /mnt/wm
 *
 * Returns 0 on success, -1 on error
 */
int wm_remove_widget_entry(struct KryonWidget *widget);

/*
 * Extended filesystem entry creation (supports nesting)
 *
 * Create filesystem entries for a window with explicit parent directory
 * Called by window_create_ex() to support nested windows
 *
 * Parameters:
 *   win - Window to create entries for
 *   parent_dir - Parent directory (e.g., /mnt/wm or /mnt/wm/win/1/win/)
 *
 * Returns 0 on success, -1 on error
 */
int wm_create_window_entry_ex(struct KryonWindow *win, P9Node *parent_dir);

/*
 * Resolve window by path
 *
 * Resolve a window by path (e.g., "/win/1/win/2/win/3")
 * Returns the window structure, or NULL if not found
 *
 * Parameters:
 *   path - Path to window (e.g., "/win/1/win/2")
 *
 * Returns window pointer, or NULL on error
 */
struct KryonWindow *wm_resolve_path(const char *path);

/*
 * Create virtual device entries for a window
 *
 * Creates /dev/draw, /dev/cons, /dev/mouse, /dev/kbd entries
 *
 * Parameters:
 *   win - Window to create virtual devices for
 *
 * Returns 0 on success, -1 on error
 */
int wm_create_vdev_entries(struct KryonWindow *win);

/*
 * Virtual device handlers
 */
ssize_t vdev_draw_new_read(char *buf, size_t count, uint64_t offset, void *data);
ssize_t vdev_cons_write(const char *buf, size_t count, uint64_t offset, void *data);
ssize_t vdev_cons_read(char *buf, size_t count, uint64_t offset, void *data);

#endif /* KRYON_WM_H */
