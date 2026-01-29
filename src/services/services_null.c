/**

 * @file services_null.c
 * @brief Null Platform Services Implementation
 *
 * Default implementation when no platform plugin is available.
 * All services return NULL/false, allowing apps to gracefully
 * fall back to standard functionality.
 *
 * This file is intentionally minimal - it's a placeholder for when
 * no specific platform plugin is compiled in.
 *
 * @version 1.0.0
 * @author Kryon Labs
 */
#include "lib9.h"


#include "platform_services.h"

// =============================================================================
// NULL PLUGIN IMPLEMENTATION
// =============================================================================

/**
 * Null plugin has no services available
 */
static bool null_has_service(KryonServiceType service) {
    (void)service; // Unused
    return false;
}

/**
 * Null plugin returns NULL for all services
 */
static void* null_get_service(KryonServiceType service) {
    (void)service; // Unused
    return NULL;
}

/**
 * Null plugin initialization (no-op)
 */
static bool null_init(void *config) {
    (void)config; // Unused
    return true;
}

/**
 * Null plugin shutdown (no-op)
 */
static void null_shutdown(void) {
    // Nothing to cleanup
}

// =============================================================================
// NULL PLUGIN DESCRIPTOR
// =============================================================================

/**
 * Null plugin descriptor
 *
 * This is NOT auto-registered. It exists only as a fallback if apps
 * explicitly want to register it, but normally apps just check if
 * services are available before using them.
 */
static KryonPlatformServices null_plugin = {
    .name = "null",
    .version = "1.0.0",
    .init = null_init,
    .shutdown = null_shutdown,
    .has_service = null_has_service,
    .get_service = null_get_service,
};

/**
 * Get the null plugin (if apps need to explicitly register it)
 */
KryonPlatformServices* kryon_get_null_plugin(void) {
    return &null_plugin;
}
