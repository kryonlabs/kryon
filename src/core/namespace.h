/*
 * Kryon Namespace Manager - Plan 9 Namespace Operations
 * C89/C90 compliant
 *
 * Handles namespace mounting, binding, and /mnt/term management
 * for Plan 9 CPU server functionality.
 */

#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <stddef.h>
#include <stdint.h>
#include "kryon.h"

/*
 * Maximum path length
 */
#define NS_MAX_PATH 512

/*
 * Maximum number of bind mounts
 */
#define NS_MAX_BINDS 64

/*
 * Bind mount type
 */
typedef enum {
    NS_BIND_REPLACE,   /* Replace existing file */
    NS_BIND_BEFORE,    /* Insert before existing file */
    NS_BIND_AFTER      /* Insert after existing file */
} NSBindType;

/*
 * Bind mount entry
 */
typedef struct {
    char src[NS_MAX_PATH];     /* Source path */
    char dst[NS_MAX_PATH];     /* Destination path */
    NSBindType type;           /* Bind type */
    int active;                /* Bind is active */
} NSBind;

/*
 * Initialize namespace manager
 * Returns 0 on success, -1 on error
 */
int namespace_init(void);

/*
 * Cleanup namespace manager
 */
void namespace_cleanup(void);

/*
 * Create /mnt/term structure for a client
 * Returns the mnt_term root node, or NULL on error
 */
P9Node *namespace_create_mnt_term(P9Node *root, int client_id);

/*
 * Mount client's exported namespace
 * This is called when a CPU client connects
 * Returns 0 on success, -1 on error
 */
int namespace_mount_export(P9Node *mnt_term, int client_fd);

/*
 * Bind device to path (like Plan 9 bind command)
 * Returns 0 on success, -1 on error
 */
int namespace_bind(P9Node *root, const char *device, const char *path,
                   NSBindType type);

/*
 * Unbind a previously bound mount
 * Returns 0 on success, -1 on error
 */
int namespace_unbind(const char *path);

/*
 * Lookup a node in the namespace
 * Returns the node, or NULL if not found
 */
P9Node *namespace_lookup(P9Node *root, const char *path);

/*
 * Create a symlink in the namespace
 * Returns 0 on success, -1 on error
 */
int namespace_symlink(P9Node *root, const char *target, const char *linkpath);

/*
 * Get /mnt/term for a client
 * Returns the mnt_term node, or NULL if not found
 */
P9Node *namespace_get_mnt_term(int client_id);

/*
 * Delete /mnt/term for a client
 * Returns 0 on success, -1 on error
 */
int namespace_delete_mnt_term(int client_id);

#endif /* NAMESPACE_H */
