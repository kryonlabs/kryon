/**
 * @file namespace.h
 * @brief Namespace Service - Inferno-style namespace operations
 *
 * Provides access to namespace manipulation:
 * - mount() - Mount 9P filesystems from network or local sources
 * - bind() - Bind directories to create union mounts
 * - unmount() - Unmount filesystems
 * - 9P server creation - Expose app data as filesystems
 *
 * This service is primarily for Inferno, but could be adapted to
 * FUSE-based systems on Linux/macOS in the future.
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_SERVICE_NAMESPACE_H
#define KRYON_SERVICE_NAMESPACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// MOUNT FLAGS
// =============================================================================

/**
 * @brief Mount/bind flags (Inferno-compatible)
 */
typedef enum {
    KRYON_MREPL  = 0x0000,      /**< Replace (default for mount) */
    KRYON_MBEFORE = 0x0001,     /**< Add before existing binding */
    KRYON_MAFTER = 0x0002,      /**< Add after existing binding */
    KRYON_MCREATE = 0x0004,     /**< Create mountpoint if missing */
    KRYON_MCACHE = 0x0010,      /**< Cache file data */
} KryonMountFlags;

// =============================================================================
// 9P FILE SERVER
// =============================================================================

/**
 * @brief Opaque 9P file server handle
 *
 * Represents a 9P file server created by the app to expose data.
 */
typedef struct KryonFileServer KryonFileServer;

/**
 * @brief 9P file server callbacks
 *
 * Apps implement these callbacks to handle file operations on
 * their exported filesystem.
 */
typedef struct {
    /** User context passed to all callbacks */
    void *context;

    /**
     * Called when client walks to a file
     * @param context User context
     * @param path File path being accessed
     * @return true if file exists, false otherwise
     */
    bool (*on_walk)(void *context, const char *path);

    /**
     * Called when client opens a file
     * @param context User context
     * @param path File path
     * @param mode Open mode (OREAD, OWRITE, etc.)
     * @return true on success, false to deny access
     */
    bool (*on_open)(void *context, const char *path, int mode);

    /**
     * Called when client reads from file
     * @param context User context
     * @param path File path
     * @param buf Buffer to fill with data
     * @param count Maximum bytes to read
     * @param offset Offset in file to read from
     * @return Bytes read, or -1 on error
     */
    int (*on_read)(void *context, const char *path, void *buf, int count, int64_t offset);

    /**
     * Called when client writes to file
     * @param context User context
     * @param path File path
     * @param buf Data to write
     * @param count Bytes to write
     * @param offset Offset in file to write to
     * @return Bytes written, or -1 on error
     */
    int (*on_write)(void *context, const char *path, const void *buf, int count, int64_t offset);

    /**
     * Called when client stats a file
     * @param context User context
     * @param path File path
     * @param size Output: file size
     * @param mode Output: file mode
     * @param mtime Output: modification time
     * @return true on success, false on error
     */
    bool (*on_stat)(void *context, const char *path, uint64_t *size, uint32_t *mode, uint64_t *mtime);

    /**
     * Called when client lists directory
     * @param context User context
     * @param path Directory path
     * @param entries Output: array of entry names (plugin allocates)
     * @param count Output: number of entries
     * @return true on success, false on error
     */
    bool (*on_readdir)(void *context, const char *path, char ***entries, int *count);

    /**
     * Called when client closes a file
     * @param context User context
     * @param path File path
     */
    void (*on_close)(void *context, const char *path);

} KryonFileServerCallbacks;

// =============================================================================
// NAMESPACE SERVICE INTERFACE
// =============================================================================

/**
 * @brief Namespace service interface
 *
 * Apps receive this interface via kryon_services_get_interface().
 */
typedef struct {
    /**
     * Mount a filesystem
     *
     * @param fd File descriptor for 9P connection (from dial or pipe)
     * @param mountpoint Where to mount in namespace
     * @param flags Mount flags (KRYON_M*)
     * @param spec Optional mount specification (aname for 9P)
     * @return true on success, false on failure
     *
     * @example
     * // Mount remote 9P filesystem
     * int fd = dial("tcp!server!564", NULL, NULL, NULL);
     * ns->mount(fd, "/mnt/remote", 0, "");
     */
    bool (*mount)(int fd, const char *mountpoint, int flags, const char *spec);

    /**
     * Bind directory to another location
     *
     * @param source Source directory path
     * @param target Target directory path
     * @param flags Bind flags (KRYON_M*)
     * @return true on success, false on failure
     *
     * @example
     * // Create union mount
     * ns->bind("/usr/local/lib", "/lib", KRYON_MAFTER);
     */
    bool (*bind)(const char *source, const char *target, int flags);

    /**
     * Unmount a filesystem
     *
     * @param mountpoint Path that was mounted
     * @return true on success, false on failure
     */
    bool (*unmount)(const char *mountpoint);

    /**
     * Create a 9P file server
     *
     * The server will handle file operations via the provided callbacks,
     * allowing the app to expose internal data as a filesystem.
     *
     * @param name Server name (for identification)
     * @param callbacks Callback functions for file operations
     * @return Server handle, or NULL on failure
     *
     * @example
     * KryonFileServerCallbacks cb = {
     *     .context = my_data,
     *     .on_read = my_read_handler,
     *     .on_write = my_write_handler,
     *     // ...
     * };
     * KryonFileServer *srv = ns->create_server("myapp", &cb);
     */
    KryonFileServer* (*create_server)(const char *name, KryonFileServerCallbacks *callbacks);

    /**
     * Destroy a file server
     *
     * @param server Server to destroy
     */
    void (*destroy_server)(KryonFileServer *server);

    /**
     * Serve a file server (export to /srv)
     *
     * Makes the server accessible at /srv/{name} for other processes
     * to mount and access.
     *
     * @param server Server to export
     * @param srv_path Path in /srv to export to (e.g., "/srv/myapp")
     * @return true on success, false on failure
     *
     * @example
     * ns->serve(server, "/srv/myapp");
     * // Other processes can now: mount /srv/myapp /mnt/myapp
     */
    bool (*serve)(KryonFileServer *server, const char *srv_path);

    /**
     * Get file descriptor for server
     *
     * Returns an FD that can be used for custom serving (e.g., over network).
     *
     * @param server Server to get FD for
     * @return File descriptor, or -1 on error
     */
    int (*server_fd)(KryonFileServer *server);

} KryonNamespaceService;

#ifdef __cplusplus
}
#endif

#endif // KRYON_SERVICE_NAMESPACE_H
