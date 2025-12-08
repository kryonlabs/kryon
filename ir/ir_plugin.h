/*
 * Kryon Plugin System
 *
 * Allows extending the IR with new command types (canvas, tilemap, etc.)
 * that work automatically across all frontends and backends.
 */

#ifndef IR_PLUGIN_H
#define IR_PLUGIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Plugin Command Range
// ============================================================================

#define IR_PLUGIN_CMD_START  100
#define IR_PLUGIN_CMD_END    255
#define IR_PLUGIN_CMD_COUNT  (IR_PLUGIN_CMD_END - IR_PLUGIN_CMD_START + 1)

// ============================================================================
// Plugin Handler Signature
// ============================================================================

/**
 * Plugin command handler function signature.
 *
 * @param backend_ctx Backend-specific context (e.g., SDL_Renderer*)
 * @param cmd Command structure containing plugin data
 */
typedef void (*IRPluginHandler)(void* backend_ctx, const void* cmd);

// ============================================================================
// Plugin Metadata
// ============================================================================

#define IR_PLUGIN_NAME_MAX 64
#define IR_PLUGIN_VERSION_MAX 32
#define IR_PLUGIN_DESC_MAX 256

typedef struct {
    char name[IR_PLUGIN_NAME_MAX];              // Plugin name (e.g., "markdown", "canvas")
    char version[IR_PLUGIN_VERSION_MAX];        // Plugin version (e.g., "1.0.0")
    char description[IR_PLUGIN_DESC_MAX];       // Short description
    char kryon_version_min[IR_PLUGIN_VERSION_MAX]; // Minimum Kryon version required
    uint32_t command_id_start;                  // First command ID
    uint32_t command_id_end;                    // Last command ID (inclusive)
    const char** required_capabilities;         // NULL-terminated array of required backend capabilities
    uint32_t capability_count;                  // Number of required capabilities
} IRPluginMetadata;

/**
 * Plugin information structure (returned by query functions)
 */
typedef struct {
    IRPluginMetadata metadata;
    bool is_loaded;                             // Whether plugin is currently loaded
    uint32_t handler_count;                     // Number of registered handlers
} IRPluginInfo;

// ============================================================================
// Plugin Registration
// ============================================================================

/**
 * Register a plugin command handler.
 *
 * @param command_id Command ID (must be in range IR_PLUGIN_CMD_START..IR_PLUGIN_CMD_END)
 * @param handler Handler function
 * @return true on success, false if command_id is out of range or already registered
 */
bool ir_plugin_register_handler(uint32_t command_id, IRPluginHandler handler);

/**
 * Unregister a plugin command handler.
 *
 * @param command_id Command ID to unregister
 */
void ir_plugin_unregister_handler(uint32_t command_id);

/**
 * Check if a command handler is registered.
 *
 * @param command_id Command ID to check
 * @return true if handler is registered, false otherwise
 */
bool ir_plugin_has_handler(uint32_t command_id);

/**
 * Register a plugin with metadata.
 * This is the recommended way to register plugins (instead of registering handlers individually).
 *
 * @param metadata Plugin metadata structure
 * @return true on success, false on error (version incompatible, command ID conflict, etc.)
 */
bool ir_plugin_register(const IRPluginMetadata* metadata);

/**
 * Unregister a plugin by name.
 * Unregisters all handlers associated with the plugin.
 *
 * @param plugin_name Name of plugin to unregister
 * @return true if plugin was found and unregistered, false otherwise
 */
bool ir_plugin_unregister(const char* plugin_name);

/**
 * Get information about a registered plugin.
 *
 * @param plugin_name Name of plugin
 * @param info Output plugin info structure
 * @return true if plugin found, false otherwise
 */
bool ir_plugin_get_info(const char* plugin_name, IRPluginInfo* info);

/**
 * List all registered plugins.
 *
 * @param infos Output array of plugin info structures
 * @param max_count Maximum number of plugins to return
 * @return Number of plugins returned
 */
uint32_t ir_plugin_list_all(IRPluginInfo* infos, uint32_t max_count);

/**
 * Check if a command ID range conflicts with existing plugins.
 *
 * @param start_id First command ID
 * @param end_id Last command ID (inclusive)
 * @param conflicting_plugin Output buffer for conflicting plugin name (if any)
 * @param buffer_size Size of conflicting_plugin buffer
 * @return true if conflict detected, false if range is available
 */
bool ir_plugin_check_command_conflict(uint32_t start_id, uint32_t end_id,
                                      char* conflicting_plugin, size_t buffer_size);

/**
 * Check if a plugin version is compatible with current Kryon version.
 *
 * @param required_version Minimum Kryon version required (e.g., "0.3.0")
 * @return true if compatible, false otherwise
 */
bool ir_plugin_check_version_compat(const char* required_version);

// ============================================================================
// Plugin Dispatch
// ============================================================================

/**
 * Dispatch a plugin command to its registered handler.
 *
 * @param backend_ctx Backend-specific context
 * @param command_type Command type ID
 * @param cmd Command structure
 * @return true if command was dispatched, false if no handler registered
 */
bool ir_plugin_dispatch(void* backend_ctx, uint32_t command_type, const void* cmd);

// ============================================================================
// Backend Capabilities
// ============================================================================

typedef struct {
    bool has_2d_shapes : 1;        // Can render circles, ellipses, polygons
    bool has_transforms : 1;       // Can apply rotations, scales, translations
    bool has_hardware_accel : 1;   // GPU-accelerated rendering
    bool has_blend_modes : 1;      // Custom blend modes (alpha, multiply, etc.)
    bool has_antialiasing : 1;     // Anti-aliased rendering
    bool has_gradients : 1;        // Linear/radial gradients
    bool has_text_rendering : 1;   // Advanced text rendering
    bool has_3d_rendering : 1;     // 3D primitives and transforms
} IRBackendCapabilities;

/**
 * Set backend capabilities.
 * Should be called by backend during initialization.
 *
 * @param caps Backend capabilities structure
 */
void ir_plugin_set_backend_capabilities(const IRBackendCapabilities* caps);

/**
 * Get current backend capabilities.
 *
 * @return Pointer to backend capabilities (read-only)
 */
const IRBackendCapabilities* ir_plugin_get_backend_capabilities(void);

/**
 * Check if backend supports a specific capability.
 *
 * @param capability Capability name (e.g., "2d_shapes", "hardware_accel")
 * @return true if supported, false otherwise
 */
bool ir_plugin_backend_supports(const char* capability);

// ============================================================================
// Plugin Statistics
// ============================================================================

typedef struct {
    uint32_t registered_handlers;   // Number of registered handlers
    uint32_t commands_dispatched;   // Total commands dispatched
    uint32_t unknown_commands;      // Commands with no handler
} IRPluginStats;

/**
 * Get plugin system statistics.
 *
 * @param stats Output statistics structure
 */
void ir_plugin_get_stats(IRPluginStats* stats);

/**
 * Print plugin system statistics to stdout.
 */
void ir_plugin_print_stats(void);

/**
 * Reset statistics counters.
 */
void ir_plugin_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // IR_PLUGIN_H
