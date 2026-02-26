/*
 * Kryon Marrow Client API
 * C89/C90 compliant
 *
 * Provides functions for Kryon to register with Marrow and mount filesystems
 */

#ifndef KRYON_MARROW_H
#define KRYON_MARROW_H

#include "p9client.h"

/*
 * Register Kryon as a service with Marrow
 *
 * Protocol: Write "register <name> <type>" to /svc/ctl
 * Example: marrow_service_register(mc, "kryon", "window-manager", "Kryon Window Manager")
 *
 * Parameters:
 *   mc - P9Client connection to Marrow
 *   name - Service name (e.g., "kryon")
 *   type - Service type (e.g., "window-manager")
 *   description - Human-readable description (optional, reserved)
 *
 * Returns 0 on success, -1 on error
 */
int marrow_service_register(P9Client *mc, const char *name,
                           const char *type, const char *description);

/*
 * Mount Kryon's filesystem into Marrow's namespace
 *
 * Protocol: Write "mount <path>" to /svc/ctl
 * Example: marrow_namespace_mount(mc, "/mnt/wm")
 *
 * Parameters:
 *   mc - P9Client connection to Marrow
 *   path - Mount path in Marrow's namespace (e.g., "/mnt/wm")
 *
 * Returns 0 on success, -1 on error
 */
int marrow_namespace_mount(P9Client *mc, const char *path);

/*
 * Unregister Kryon service from Marrow
 *
 * Protocol: Write "unregister <name>" to /svc/ctl
 *
 * Parameters:
 *   mc - P9Client connection to Marrow
 *   name - Service name to unregister
 *
 * Returns 0 on success, -1 on error
 */
int marrow_service_unregister(P9Client *mc, const char *name);

/*
 * Unmount Kryon's filesystem from Marrow
 *
 * Note: This is done automatically when client disconnects
 * This function is reserved for future use
 *
 * Parameters:
 *   mc - P9Client connection to Marrow
 *   path - Mount path to unmount
 *
 * Returns 0 on success, -1 on error
 */
int marrow_namespace_unmount(P9Client *mc, const char *path);

#endif /* KRYON_MARROW_H */
