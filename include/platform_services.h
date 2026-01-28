/**
 * @file platform_services.h
 * @brief Kryon Platform Services - Optional OS-specific features beyond windowing/rendering
 *
 * This header defines the platform services plugin system that allows kryon applications
 * to optionally access OS-specific features when available (e.g., Inferno's 9P filesystems,
 * device files, namespace operations) while remaining fully portable.
 *
 * Key Design Principles:
 * - Non-invasive: Additive layer, doesn't modify core runtime
 * - Opt-in: Apps explicitly request services via API
 * - Compile-time: Plugins compiled in based on build target
 * - Zero overhead: No penalty when plugins not used
 * - Capability detection: Apps check feature availability
 * - Graceful fallback: Missing features don't crash
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_PLATFORM_SERVICES_H
#define KRYON_PLATFORM_SERVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// SERVICE TYPES
// =============================================================================

/**
 * @brief Available platform service types
 *
 * Services provide optional OS-specific features that apps can detect
 * and use when available. Apps that don't use services remain portable.
 */
typedef enum {
    KRYON_SERVICE_NONE = 0,

    /**
     * Extended File I/O - Access beyond standard C library
     * - Device files (Inferno: #c/cons, #p/PID/ctl, etc.)
     * - Special filesystems and mount points
     * - Extended file metadata (Qid, etc.)
     */
    KRYON_SERVICE_EXTENDED_FILE_IO = 1,

    /**
     * Namespace - Inferno-style namespace operations
     * - mount() - Mount 9P filesystems
     * - bind() - Bind directories
     * - unmount() - Unmount filesystems
     * - 9P server creation and export
     */
    KRYON_SERVICE_NAMESPACE = 2,

    /**
     * Process Control - Advanced process management
     * - Process enumeration beyond standard
     * - Control files (Inferno: /prog/PID/ctl)
     * - Process status and monitoring
     */
    KRYON_SERVICE_PROCESS_CONTROL = 3,

    /**
     * Inter-Process Communication - Platform-specific IPC
     * - Named pipes, shared memory
     * - Message queues
     * - Platform-specific mechanisms
     */
    KRYON_SERVICE_IPC = 4,

    KRYON_SERVICE_COUNT
} KryonServiceType;

// =============================================================================
// PLATFORM SERVICES PLUGIN
// =============================================================================

/**
 * @brief Platform services plugin descriptor
 *
 * Each platform plugin (Inferno, POSIX, Windows, etc.) provides this
 * structure describing its capabilities and providing service interfaces.
 */
typedef struct KryonPlatformServices {
    /** Plugin name (e.g., "inferno", "posix", "windows") */
    const char *name;

    /** Plugin version string */
    const char *version;

    /**
     * Initialize the plugin
     * @param config Optional configuration data (plugin-specific)
     * @return true on success, false on failure
     */
    bool (*init)(void *config);

    /**
     * Shutdown the plugin and cleanup resources
     */
    void (*shutdown)(void);

    /**
     * Check if a service is available in this plugin
     * @param service Service type to check
     * @return true if available, false otherwise
     */
    bool (*has_service)(KryonServiceType service);

    /**
     * Get a service interface from this plugin
     * @param service Service type to get
     * @return Service interface pointer, or NULL if not available
     */
    void* (*get_service)(KryonServiceType service);
} KryonPlatformServices;

// =============================================================================
// SERVICES REGISTRY API
// =============================================================================

/**
 * @brief Register a platform services plugin
 *
 * Plugins register themselves using this function, typically via
 * __attribute__((constructor)) for automatic registration at load time.
 *
 * @param services Plugin descriptor to register
 * @return true on success, false if registration fails
 *
 * @example
 * static KryonPlatformServices inferno_plugin = { ... };
 *
 * __attribute__((constructor))
 * static void register_inferno_plugin(void) {
 *     kryon_services_register(&inferno_plugin);
 * }
 */
bool kryon_services_register(KryonPlatformServices *services);

/**
 * @brief Get the currently active platform services plugin
 *
 * @return Active plugin, or NULL if none registered
 */
KryonPlatformServices* kryon_services_get(void);

/**
 * @brief Check if a service is available
 *
 * This is the primary API apps use to detect service availability.
 *
 * @param service Service type to check
 * @return true if service available, false otherwise
 *
 * @example
 * if (kryon_services_available(KRYON_SERVICE_EXTENDED_FILE_IO)) {
 *     // Can use extended file I/O
 * } else {
 *     // Fall back to standard file I/O
 * }
 */
bool kryon_services_available(KryonServiceType service);

/**
 * @brief Get a service interface
 *
 * Returns a pointer to the service interface structure. The type of
 * structure depends on the service type (see service headers in
 * include/services/ directory).
 *
 * @param service Service type to get
 * @return Service interface pointer, or NULL if not available
 *
 * @example
 * KryonExtendedFileIO *xfio = kryon_services_get_interface(
 *     KRYON_SERVICE_EXTENDED_FILE_IO
 * );
 * if (xfio) {
 *     KryonExtendedFile *file = xfio->open("#c/cons", O_RDWR);
 *     // ...
 * }
 */
void* kryon_services_get_interface(KryonServiceType service);

/**
 * @brief Initialize the services system
 *
 * Called by runtime initialization. Apps don't need to call this directly.
 *
 * @return true on success
 */
bool kryon_services_init(void);

/**
 * @brief Shutdown the services system
 *
 * Called by runtime shutdown. Apps don't need to call this directly.
 */
void kryon_services_shutdown(void);

/**
 * @brief Get service type name string
 *
 * @param service Service type
 * @return Human-readable service name
 */
const char* kryon_service_type_name(KryonServiceType service);

#ifdef __cplusplus
}
#endif

#endif // KRYON_PLATFORM_SERVICES_H
