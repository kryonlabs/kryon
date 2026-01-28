/**
 * @file namespace.c
 * @brief Inferno Namespace Service Implementation
 *
 * Implements namespace operations using Inferno syscalls:
 * - mount() - Mount 9P filesystems
 * - bind() - Bind directories
 * - unmount() - Unmount filesystems
 * - 9P server creation (basic implementation)
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifdef KRYON_PLUGIN_INFERNO

#include "services/namespace.h"
#include "inferno_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// =============================================================================
// FILE SERVER
// =============================================================================

/**
 * File server handle
 */
struct KryonFileServer {
    char *name;                         /**< Server name */
    KryonFileServerCallbacks callbacks; /**< Callback functions */
    int fd;                             /**< File descriptor for serving */
};

// =============================================================================
// NAMESPACE OPERATIONS
// =============================================================================

/**
 * Mount a filesystem
 */
static bool inferno_ns_mount(int fd, const char *mountpoint, int flags, const char *spec) {
    if (fd < 0 || !mountpoint) {
        return false;
    }

    // Convert flags
    int inferno_flags = 0;
    if (flags & KRYON_MBEFORE) {
        inferno_flags |= MBEFORE;
    }
    if (flags & KRYON_MAFTER) {
        inferno_flags |= MAFTER;
    }
    if (flags & KRYON_MCREATE) {
        inferno_flags |= MCREATE;
    }
    if (flags & KRYON_MCACHE) {
        inferno_flags |= MCACHE;
    }

    // Call Inferno mount syscall wrapper
    const char *aname = spec ? spec : "";
    int result = inferno_mount(fd, -1, mountpoint, inferno_flags, aname);

    return result >= 0;
}

/**
 * Bind directory
 */
static bool inferno_ns_bind(const char *source, const char *target, int flags) {
    if (!source || !target) {
        return false;
    }

    // Convert flags
    int inferno_flags = 0;
    if (flags & KRYON_MBEFORE) {
        inferno_flags |= MBEFORE;
    }
    if (flags & KRYON_MAFTER) {
        inferno_flags |= MAFTER;
    }
    if (flags & KRYON_MCREATE) {
        inferno_flags |= MCREATE;
    }

    // Call Inferno bind syscall wrapper
    int result = inferno_bind(source, target, inferno_flags);

    return result >= 0;
}

/**
 * Unmount a filesystem
 */
static bool inferno_ns_unmount(const char *mountpoint) {
    if (!mountpoint) {
        return false;
    }

    // Call Inferno unmount syscall wrapper
    int result = inferno_unmount(NULL, mountpoint);

    return result >= 0;
}

// =============================================================================
// FILE SERVER OPERATIONS
// =============================================================================

/**
 * Create a 9P file server
 *
 * Note: This is a simplified implementation. A full 9P server would
 * require implementing the complete 9P2000 protocol. This provides
 * basic file server functionality for simple use cases.
 */
static KryonFileServer* inferno_create_server(const char *name, KryonFileServerCallbacks *callbacks) {
    if (!name || !callbacks) {
        return NULL;
    }

    KryonFileServer *server = malloc(sizeof(KryonFileServer));
    if (!server) {
        return NULL;
    }

    server->name = strdup(name);
    server->callbacks = *callbacks;
    server->fd = -1;

    return server;
}

/**
 * Destroy a file server
 */
static void inferno_destroy_server(KryonFileServer *server) {
    if (!server) {
        return;
    }

    if (server->fd >= 0) {
        inferno_close(server->fd);
    }

    if (server->name) {
        free(server->name);
    }

    free(server);
}

/**
 * Serve a file server (export to /srv)
 *
 * Note: This is a placeholder implementation. A full implementation
 * would use file2chan() or implement a 9P server. For now, we just
 * indicate that the functionality is available but not fully implemented.
 */
static bool inferno_serve(KryonFileServer *server, const char *srv_path) {
    if (!server || !srv_path) {
        return false;
    }

    // TODO: Implement 9P server using file2chan or similar
    // For now, this is a stub that indicates the interface exists
    fprintf(stderr, "[Kryon Namespace] 9P server serving not yet fully implemented\n");
    fprintf(stderr, "[Kryon Namespace] Would serve '%s' at '%s'\n",
            server->name, srv_path);

    return false; // Not yet implemented
}

/**
 * Get file descriptor for server
 */
static int inferno_server_fd(KryonFileServer *server) {
    if (!server) {
        return -1;
    }

    return server->fd;
}

// =============================================================================
// SERVICE INTERFACE
// =============================================================================

/**
 * Namespace service interface for Inferno
 */
static KryonNamespaceService inferno_namespace_service = {
    .mount = inferno_ns_mount,
    .bind = inferno_ns_bind,
    .unmount = inferno_ns_unmount,
    .create_server = inferno_create_server,
    .destroy_server = inferno_destroy_server,
    .serve = inferno_serve,
    .server_fd = inferno_server_fd,
};

/**
 * Get the Namespace service
 */
KryonNamespaceService* inferno_get_namespace_service(void) {
    return &inferno_namespace_service;
}

#endif // KRYON_PLUGIN_INFERNO
