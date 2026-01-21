/**
 * @file ir_capability.h
 * @brief Kryon Capability Registry - Internal Header
 *
 * This header is for Kryon internal use only.
 * Plugins should include <kryon/capability.h> instead.
 *
 * The capability registry manages plugin capabilities and routes
 * rendering/data access requests to the appropriate plugins.
 */

#ifndef IR_CAPABILITY_H
#define IR_CAPABILITY_H

#include "ir_types.h"
#include <kryon/capability.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Capability API Types
// ============================================================================
// Note: KryonDataHandle and KryonCapabilityAPI are defined in <kryon/capability.h>
// This internal header adds IR-specific functions that use those types.

// Re-export public types for convenience
typedef struct KryonPlugin KryonPlugin;
typedef struct KryonCapabilityAPI KryonCapabilityAPI;
typedef struct KryonDataHandle KryonDataHandle;
typedef struct KryonRenderContext KryonRenderContext;
typedef struct KryonEventContext KryonEventContext;

// ============================================================================
// IR-specific capability functions
// ============================================================================

// Forward declaration
typedef struct IRComponent IRComponent;

/**
 * Initialize the capability registry
 *
 * Must be called before any other capability functions.
 * Sets up the API structure and allocates storage for plugins.
 */
void ir_capability_registry_init(void);

/**
 * Set the root component for global component lookup
 *
 * @param root The root component of the component tree
 *
 * This allows plugins to lookup components by ID through the capability API.
 */
void ir_capability_set_root_component(IRComponent* root);

/**
 * Shutdown and cleanup the capability registry
 *
 * Unloads all plugins and frees all allocated memory.
 */
void ir_capability_registry_shutdown(void);

/**
 * Get the API structure (to pass to plugins)
 *
 * @return Pointer to the capability API structure
 *
 * This API pointer is passed to plugins when they are loaded.
 * Plugins use it to access Kryon capabilities and register their own.
 */
KryonCapabilityAPI* ir_capability_get_api(void);

/**
 * Load a plugin from a .so file
 *
 * @param path Full path to the plugin shared library
 * @param name Plugin name (or NULL to use basename of path)
 * @return true on success, false on failure
 *
 * This function:
 * 1. Loads the shared library via dlopen
 * 2. Finds the kryon_plugin_load entry point
 * 3. Calls the entry point with the capability API
 * 4. Stores the plugin info in the registry
 */
bool ir_capability_load_plugin(const char* path, const char* name);

/**
 * Unload a plugin
 *
 * @param name Name of the plugin to unload
 * @return true on success, false if plugin not found
 *
 * Calls the plugin's kryon_plugin_unload function if present,
 * then unloads the shared library.
 */
bool ir_capability_unload_plugin(const char* name);

/**
 * Render a component to HTML (dispatches to appropriate plugin)
 *
 * @param component_type Component type to render
 * @param handle Opaque data handle for the component
 * @param theme Theme name (e.g., "dark", "light")
 * @return Newly allocated HTML string, or NULL if no renderer registered
 *
 * The caller is responsible for freeing the returned string.
 */
char* ir_capability_render_web(
    uint32_t component_type,
    const KryonDataHandle* handle,
    const char* theme
);

/**
 * Generate CSS for a component type
 *
 * @param component_type Component type
 * @param theme Theme name
 * @return Newly allocated CSS string, or NULL if no generator registered
 *
 * The caller is responsible for freeing the returned string.
 */
char* ir_capability_generate_css(
    uint32_t component_type,
    const char* theme
);

/**
 * Check if a component type has a registered web renderer
 *
 * @param component_type Component type to check
 * @return true if a renderer is registered, false otherwise
 */
bool ir_capability_has_web_renderer(uint32_t component_type);

/**
 * Check if a component type has a registered CSS generator
 *
 * @param component_type Component type to check
 * @return true if a generator is registered, false otherwise
 */
bool ir_capability_has_css_generator(uint32_t component_type);

/**
 * Check if a component type has a registered component renderer
 *
 * @param component_type Component type to check
 * @return true if a renderer is registered, false otherwise
 */
bool ir_capability_has_component_renderer(uint32_t component_type);

/**
 * Create a data handle from component data
 *
 * @param custom_data Pointer to component's custom_data
 * @param data_size Size of custom_data in bytes
 * @param component_type Component type tag
 * @param component_id Component instance ID
 * @return Data handle structure
 *
 * This creates an opaque handle that plugins can use
 * to access component data through the capability API.
 */
KryonDataHandle ir_capability_create_data_handle(
    const void* custom_data,
    size_t data_size,
    uint32_t component_type,
    uint32_t component_id
);

/**
 * List all loaded plugins and registered capabilities
 *
 * Outputs to stdout for debugging/CLI commands.
 */
void ir_capability_list_plugins(void);

/**
 * Get the number of loaded plugins
 *
 * @return Plugin count
 */
uint32_t ir_capability_get_plugin_count(void);

/**
 * Get a plugin name by index
 *
 * @param index Plugin index (0 to get_plugin_count() - 1)
 * @return Plugin name, or NULL if index is invalid
 */
const char* ir_capability_get_plugin_name(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* IR_CAPABILITY_H */
