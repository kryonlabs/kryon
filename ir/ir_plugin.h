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

// Forward declare IRComponent from ir_core.h
#ifndef IR_CORE_H
typedef struct IRComponent IRComponent;
#endif

/**
 * Backend context for plugin rendering.
 * Contains pointers to backend-specific rendering resources.
 */
typedef struct {
    void* renderer;      // Backend renderer (e.g., SDL_Renderer*)
    void* font;          // Default font (e.g., TTF_Font*)
    void* user_data;     // Additional backend-specific data
} IRPluginBackendContext;

/**
 * Plugin command handler function signature.
 *
 * @param backend_ctx Backend-specific context (e.g., SDL_Renderer*)
 * @param cmd Command structure containing plugin data
 */
typedef void (*IRPluginHandler)(void* backend_ctx, const void* cmd);

/**
 * Plugin component renderer function signature.
 * Allows plugins to handle rendering of specific component types.
 *
 * @param backend_ctx Backend-specific context (e.g., SDL_Renderer*, TTF_Font*)
 * @param component Component to render
 * @param x X position
 * @param y Y position
 * @param width Component width
 * @param height Component height
 */
typedef void (*IRPluginComponentRenderer)(void* backend_ctx, const IRComponent* component,
                                          float x, float y, float width, float height);

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
// Global Plugin System State
// ============================================================================

// Forward declaration of internal plugin system structure
typedef struct PluginSystem PluginSystem;

// Global plugin system instance - shared across all dynamically loaded libraries
// This ensures that plugin registrations made in one library (e.g., canvas plugin)
// are visible to all other libraries (e.g., desktop backend)
extern PluginSystem g_plugin_system;

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
// Component Renderer Registration
// ============================================================================

/**
 * Register a component renderer for a specific component type.
 * This allows plugins to handle rendering of custom or extended component types.
 *
 * @param component_type Component type (e.g., IR_COMPONENT_MARKDOWN)
 * @param renderer Renderer function
 * @return true on success, false if component_type already has a renderer
 */
bool ir_plugin_register_component_renderer(uint32_t component_type, IRPluginComponentRenderer renderer);

/**
 * Unregister a component renderer.
 *
 * @param component_type Component type to unregister
 */
void ir_plugin_unregister_component_renderer(uint32_t component_type);

/**
 * Check if a component type has a registered renderer.
 *
 * @param component_type Component type to check
 * @return true if renderer is registered, false otherwise
 */
bool ir_plugin_has_component_renderer(uint32_t component_type);

/**
 * Dispatch component rendering to registered plugin renderer.
 *
 * @param backend_ctx Backend-specific context
 * @param component_type Component type
 * @param component Component to render
 * @param x X position
 * @param y Y position
 * @param width Component width
 * @param height Component height
 * @return true if dispatched, false if no renderer registered
 */
bool ir_plugin_dispatch_component_render(void* backend_ctx, uint32_t component_type,
                                         const IRComponent* component,
                                         float x, float y, float width, float height);

// ============================================================================
// Web Component Renderer Registration
// ============================================================================

/**
 * Web component renderer function signature.
 * Returns HTML string representation of the component.
 * The returned string must be freed by the caller.
 *
 * @param component Component to render
 * @param theme Theme name (e.g., "light", "dark")
 * @return HTML string (caller must free), or NULL on failure
 */
typedef char* (*IRPluginWebComponentRenderer)(const IRComponent* component, const char* theme);

/**
 * Web component CSS generator function signature.
 * Returns CSS string for styling the component.
 * The returned string must be freed by the caller.
 *
 * @param theme Theme name (e.g., "light", "dark")
 * @return CSS string (caller must free), or NULL if no CSS needed
 */
typedef char* (*IRPluginWebCSSGenerator)(const char* theme);

/**
 * Register a web component renderer for a specific component type.
 * This allows plugins to generate HTML for custom component types.
 *
 * @param component_type Component type (e.g., IR_COMPONENT_MARKDOWN)
 * @param renderer HTML renderer function
 * @param css_gen CSS generator function (can be NULL)
 * @return true on success, false if component_type already has a renderer
 */
bool ir_plugin_register_web_renderer(uint32_t component_type,
                                     IRPluginWebComponentRenderer renderer,
                                     IRPluginWebCSSGenerator css_gen);

/**
 * Unregister a web component renderer.
 *
 * @param component_type Component type to unregister
 */
void ir_plugin_unregister_web_renderer(uint32_t component_type);

/**
 * Check if a component type has a registered web renderer.
 *
 * @param component_type Component type to check
 * @return true if web renderer is registered, false otherwise
 */
bool ir_plugin_has_web_renderer(uint32_t component_type);

/**
 * Render a component to HTML using registered plugin renderer.
 *
 * @param component Component to render
 * @param theme Theme name (e.g., "light", "dark")
 * @return HTML string (caller must free), or NULL if no renderer registered
 */
char* ir_plugin_render_web_component(const IRComponent* component, const char* theme);

/**
 * Get CSS for a component type from registered plugin.
 *
 * @param component_type Component type
 * @param theme Theme name
 * @return CSS string (caller must free), or NULL if no CSS generator
 */
char* ir_plugin_get_web_css(uint32_t component_type, const char* theme);

// ============================================================================
// Component Callback Bridge Registration
// ============================================================================

/**
 * Callback bridge function signature.
 * Allows plugins to register a bridge that gets called during component rendering.
 * This enables plugins to handle user-defined callbacks (e.g., onDraw for canvas).
 */
typedef void (*IRPluginCallbackBridge)(uint32_t component_id);

/**
 * Register a callback bridge for a specific component type.
 * This allows plugins to handle user-defined callbacks.
 *
 * @param component_type Component type ID
 * @param bridge Bridge function to call
 * @return true if registered successfully, false otherwise
 */
bool ir_plugin_register_callback_bridge(uint32_t component_type, IRPluginCallbackBridge bridge);

/**
 * Unregister a callback bridge for a component type.
 *
 * @param component_type Component type ID
 */
void ir_plugin_unregister_callback_bridge(uint32_t component_type);

/**
 * Check if a component type has a registered callback bridge.
 *
 * @param component_type Component type ID
 * @return true if bridge is registered, false otherwise
 */
bool ir_plugin_has_callback_bridge(uint32_t component_type);

/**
 * Dispatch callback invocation to registered plugin bridge.
 *
 * @param component_type Component type ID
 * @param component_id Component instance ID
 * @return true if callback was dispatched, false otherwise
 */
bool ir_plugin_dispatch_callback(uint32_t component_type, uint32_t component_id);

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
// Plugin Event Type Registration
// ============================================================================

/**
 * Plugin event type metadata.
 * Plugins can register custom event types for component callbacks.
 */
typedef struct {
    char* plugin_name;           // Plugin name (e.g., "canvas")
    char* event_type_name;       // Event type name (e.g., "canvas_draw")
    uint32_t event_type_id;      // Event type ID (must be 100-255)
    char* description;           // Optional description
} IRPluginEventType;

/**
 * Register a custom event type for a plugin.
 * Plugins can register event types in the range 100-255.
 * Core events (0-99) are reserved for Kryon core.
 *
 * @param plugin_name Plugin name (e.g., "canvas")
 * @param event_type_name Event type name (e.g., "canvas_draw")
 * @param event_type_id Event type ID (must be 100-255)
 * @param description Optional description (can be NULL)
 * @return true on success, false if ID out of range or already registered
 */
bool ir_plugin_register_event_type(const char* plugin_name, const char* event_type_name,
                                    uint32_t event_type_id, const char* description);

/**
 * Get event type ID by name.
 *
 * @param event_type_name Event type name (e.g., "canvas_draw")
 * @return Event type ID, or 0 if not found
 */
uint32_t ir_plugin_get_event_type_id(const char* event_type_name);

/**
 * Get event type name by ID.
 *
 * @param event_type_id Event type ID
 * @return Event type name, or NULL if not found
 */
const char* ir_plugin_get_event_type_name(uint32_t event_type_id);

/**
 * Check if an event type is registered.
 *
 * @param event_type_name Event type name
 * @return true if registered, false otherwise
 */
bool ir_plugin_has_event_type(const char* event_type_name);

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

// ============================================================================
// Dynamic Plugin Loading (dlopen)
// ============================================================================

/**
 * Handle for a dynamically loaded plugin.
 */
typedef struct IRPluginHandle {
    void* dl_handle;                    // dlopen handle
    char name[64];                      // Plugin name from plugin.toml
    char version[16];                   // Plugin version
    char path[512];                     // Path to .so file
    uint32_t* command_ids;              // Array of supported command IDs
    uint32_t command_count;             // Number of command IDs
    bool (*init_func)(void*);           // Init function pointer
    void (*shutdown_func)(void);        // Shutdown function pointer
} IRPluginHandle;

/**
 * Plugin discovery information (from plugin.toml scanning).
 */
typedef struct IRPluginDiscoveryInfo {
    char path[512];                     // Full path to .so file
    char toml_path[512];                // Path to plugin.toml
    char name[64];                      // Plugin name
    char version[16];                   // Plugin version
    uint32_t* command_ids;              // Supported command IDs
    uint32_t command_count;             // Count of command IDs
    char** backends;                    // Supported backends
    uint32_t backend_count;             // Count of backends
} IRPluginDiscoveryInfo;

/**
 * Discover plugins by scanning directories for plugin.toml files.
 * If search_path is NULL, scans default paths:
 *   1. ~/.kryon/plugins/
 *   2. /usr/local/lib/kryon/plugins/
 *   3. /usr/lib/kryon/plugins/
 *   4. ../kryon-plugin-STAR/build/ (development mode, where STAR is wildcard)
 *
 * @param search_path Directory to search, or NULL for defaults
 * @param count Output: number of plugins discovered
 * @return Array of plugin info (caller must free with ir_plugin_free_discovery)
 */
IRPluginDiscoveryInfo** ir_plugin_discover(const char* search_path, uint32_t* count);

/**
 * Free memory allocated by ir_plugin_discover().
 *
 * @param plugins Array returned by ir_plugin_discover
 * @param count Number of plugins in array
 */
void ir_plugin_free_discovery(IRPluginDiscoveryInfo** plugins, uint32_t count);

/**
 * Load a plugin from a .so file using dlopen().
 * Resolves init and shutdown functions:
 *   - kryon_{name}_plugin_init
 *   - kryon_{name}_plugin_shutdown
 *
 * @param plugin_path Full path to plugin .so file
 * @param plugin_name Plugin name (for function resolution)
 * @return Plugin handle, or NULL on error
 */
IRPluginHandle* ir_plugin_load(const char* plugin_path, const char* plugin_name);

/**
 * Unload a dynamically loaded plugin.
 * Calls shutdown function if present, then dlclose().
 *
 * @param handle Plugin handle to unload
 */
void ir_plugin_unload(IRPluginHandle* handle);

/**
 * Get a symbol from a loaded plugin by name.
 * This allows runtime lookup of plugin functions without compile-time linking.
 *
 * @param plugin_name Name of the loaded plugin (e.g., "syntax")
 * @param symbol_name Name of the symbol to look up (e.g., "syntax_tokenize")
 * @return Function pointer, or NULL if plugin not loaded or symbol not found
 */
void* ir_plugin_get_symbol(const char* plugin_name, const char* symbol_name);

/**
 * Check if a plugin is currently loaded.
 *
 * @param plugin_name Name of the plugin to check
 * @return true if the plugin is loaded, false otherwise
 */
bool ir_plugin_is_loaded(const char* plugin_name);

/**
 * Scan an IR tree and return list of required plugin names.
 * Analyzes plugin_command types in the tree and maps them to plugin names
 * based on command ID ranges.
 *
 * @param root Root component of IR tree
 * @param count Output: number of required plugins
 * @return Array of plugin names (caller must free)
 */
char** ir_plugin_scan_requirements(struct IRComponent* root, uint32_t* count);

/**
 * Free memory allocated by ir_plugin_scan_requirements().
 *
 * @param plugin_names Array of plugin names
 * @param count Number of plugins
 */
void ir_plugin_free_requirements(char** plugin_names, uint32_t count);

/**
 * Set plugin requirements from deserialized IR file.
 * This is called by the deserializer to store plugin requirements globally.
 *
 * @param plugin_names Array of plugin names (takes ownership)
 * @param count Number of plugins
 */
void ir_plugin_set_requirements(char** plugin_names, uint32_t count);

/**
 * Get plugin requirements from the most recently deserialized IR file.
 * This allows backends to access plugin requirements after loading a .kir file.
 *
 * @param count Output: number of required plugins
 * @return Array of plugin names (do not free, owned by plugin system)
 */
const char* const* ir_plugin_get_requirements(uint32_t* count);

/**
 * Clear stored plugin requirements.
 * Should be called when unloading an IR file.
 */
void ir_plugin_clear_requirements(void);

#ifdef __cplusplus
}
#endif

#endif // IR_PLUGIN_H
