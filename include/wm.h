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

#endif /* KRYON_WM_H */
