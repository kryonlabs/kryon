/**
 * @file services_registry.c
 * @brief Platform Services Registry Implementation
 *
 * Manages registration and lookup of platform service plugins.
 * Plugins register themselves at load time using __attribute__((constructor)).
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#include "platform_services.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// GLOBAL STATE
// =============================================================================

/**
 * Currently registered platform services plugin.
 * Only one plugin can be active at a time.
 */
static KryonPlatformServices *g_active_plugin = NULL;

/**
 * Services initialization state
 */
static bool g_services_initialized = false;

// =============================================================================
// SERVICE TYPE NAMES
// =============================================================================

/**
 * Get human-readable name for service type
 */
const char* kryon_service_type_name(KryonServiceType service) {
    switch (service) {
        case KRYON_SERVICE_NONE:
            return "None";
        case KRYON_SERVICE_EXTENDED_FILE_IO:
            return "Extended File I/O";
        case KRYON_SERVICE_NAMESPACE:
            return "Namespace";
        case KRYON_SERVICE_PROCESS_CONTROL:
            return "Process Control";
        case KRYON_SERVICE_IPC:
            return "IPC";
        case KRYON_SERVICE_COUNT:
            return "Count";
        default:
            return "Unknown";
    }
}

// =============================================================================
// REGISTRATION API
// =============================================================================

/**
 * Register a platform services plugin
 */
bool kryon_services_register(KryonPlatformServices *services) {
    if (!services) {
        fprintf(stderr, "kryon_services_register: NULL services pointer\n");
        return false;
    }

    if (!services->name) {
        fprintf(stderr, "kryon_services_register: Plugin has no name\n");
        return false;
    }

    // Check if plugin already registered
    if (g_active_plugin) {
        fprintf(stderr, "kryon_services_register: Plugin '%s' already registered, "
                        "ignoring '%s'\n",
                g_active_plugin->name, services->name);
        return false;
    }

    // Register the plugin
    g_active_plugin = services;

    // Initialize if services system is already initialized
    if (g_services_initialized && services->init) {
        if (!services->init(NULL)) {
            fprintf(stderr, "kryon_services_register: Failed to initialize '%s' plugin\n",
                    services->name);
            g_active_plugin = NULL;
            return false;
        }
    }

    printf("[Kryon Services] Registered plugin: %s v%s\n",
           services->name,
           services->version ? services->version : "unknown");

    return true;
}

/**
 * Get the currently active plugin
 */
KryonPlatformServices* kryon_services_get(void) {
    return g_active_plugin;
}

// =============================================================================
// SERVICE QUERY API
// =============================================================================

/**
 * Check if a service is available
 */
bool kryon_services_available(KryonServiceType service) {
    if (!g_active_plugin) {
        return false;
    }

    if (!g_active_plugin->has_service) {
        return false;
    }

    return g_active_plugin->has_service(service);
}

/**
 * Get a service interface
 */
void* kryon_services_get_interface(KryonServiceType service) {
    if (!g_active_plugin) {
        return NULL;
    }

    if (!g_active_plugin->get_service) {
        return NULL;
    }

    return g_active_plugin->get_service(service);
}

// =============================================================================
// LIFECYCLE
// =============================================================================

/**
 * Initialize services system
 */
bool kryon_services_init(void) {
    if (g_services_initialized) {
        return true;
    }

    g_services_initialized = true;

    // Initialize active plugin if present
    if (g_active_plugin && g_active_plugin->init) {
        if (!g_active_plugin->init(NULL)) {
            fprintf(stderr, "[Kryon Services] Failed to initialize plugin: %s\n",
                    g_active_plugin->name);
            return false;
        }
        printf("[Kryon Services] Initialized plugin: %s\n", g_active_plugin->name);
    } else if (!g_active_plugin) {
        printf("[Kryon Services] No plugin registered, services unavailable\n");
    }

    return true;
}

/**
 * Shutdown services system
 */
void kryon_services_shutdown(void) {
    if (!g_services_initialized) {
        return;
    }

    // Shutdown active plugin
    if (g_active_plugin && g_active_plugin->shutdown) {
        g_active_plugin->shutdown();
        printf("[Kryon Services] Shutdown plugin: %s\n", g_active_plugin->name);
    }

    g_services_initialized = false;
}
