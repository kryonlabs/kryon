/**
 * @file plugin.c
 * @brief Inferno Platform Services Plugin
 *
 * Provides Inferno-specific services when building for emu:
 * - Extended file I/O using lib9
 * - Namespace operations (mount/bind/9P)
 * - Process control via /prog device
 *
 * This plugin is only compiled when KRYON_PLUGIN_INFERNO is defined.
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifdef KRYON_PLUGIN_INFERNO

#include "platform_services.h"
#include "services/extended_file_io.h"
#include "services/namespace.h"
#include "services/process_control.h"
#include <stdio.h>
#include <string.h>

// Forward declarations from other plugin files
extern KryonExtendedFileIO* inferno_get_extended_file_io(void);
extern KryonNamespaceService* inferno_get_namespace_service(void);
extern KryonProcessControlService* inferno_get_process_control_service(void);

// =============================================================================
// PLUGIN IMPLEMENTATION
// =============================================================================

/**
 * Initialize Inferno plugin
 */
static bool inferno_init(void *config) {
    (void)config; // Unused for now

    // lib9 is typically initialized by the emu environment
    // No explicit initialization needed here

    return true;
}

/**
 * Shutdown Inferno plugin
 */
static void inferno_shutdown(void) {
    // Cleanup if needed
}

/**
 * Check if a service is available
 */
static bool inferno_has_service(KryonServiceType service) {
    switch (service) {
        case KRYON_SERVICE_EXTENDED_FILE_IO:
        case KRYON_SERVICE_NAMESPACE:
        case KRYON_SERVICE_PROCESS_CONTROL:
            return true;

        case KRYON_SERVICE_IPC:
            // IPC not yet implemented for Inferno
            return false;

        default:
            return false;
    }
}

/**
 * Get a service interface
 */
static void* inferno_get_service(KryonServiceType service) {
    switch (service) {
        case KRYON_SERVICE_EXTENDED_FILE_IO:
            return (void*)inferno_get_extended_file_io();

        case KRYON_SERVICE_NAMESPACE:
            return (void*)inferno_get_namespace_service();

        case KRYON_SERVICE_PROCESS_CONTROL:
            return (void*)inferno_get_process_control_service();

        default:
            return NULL;
    }
}

// =============================================================================
// PLUGIN DESCRIPTOR
// =============================================================================

/**
 * Inferno plugin descriptor
 */
static KryonPlatformServices inferno_plugin = {
    .name = "inferno",
    .version = "1.0.0",
    .init = inferno_init,
    .shutdown = inferno_shutdown,
    .has_service = inferno_has_service,
    .get_service = inferno_get_service,
};

// =============================================================================
// AUTO-REGISTRATION
// =============================================================================

/**
 * Auto-register Inferno plugin when module loads
 *
 * Uses GCC constructor attribute to run before main().
 * This ensures the plugin is available as soon as the
 * program starts.
 */
__attribute__((constructor))
static void register_inferno_plugin(void) {
    if (!kryon_services_register(&inferno_plugin)) {
        fprintf(stderr, "[Kryon Inferno Plugin] Failed to register\n");
    }
}

#endif // KRYON_PLUGIN_INFERNO
