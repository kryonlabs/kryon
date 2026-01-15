/*
 * Kryon Plugin Capability API
 *
 * This is the ONLY header required to build Kryon plugins.
 * Version: 1.0.0
 * Stability: Stable - Changes require major version bump
 *
 * Plugins use this header to:
 * - Define their entry point
 * - Access Kryon capabilities (rendering, data access, etc.)
 * - Register their capabilities with the core
 *
 * No other Kryon headers are needed for plugin development.
 */

#ifndef KRYON_CAPABILITY_H
#define KRYON_CAPABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Version Information
// ============================================================================

#define KRYON_CAPABILITY_API_VERSION_MAJOR  1
#define KRYON_CAPABILITY_API_VERSION_MINOR  0
#define KRYON_CAPABILITY_API_VERSION_PATCH  0

#define KRYON_CAPABILITY_API_VERSION       \
    ((KRYON_CAPABILITY_API_VERSION_MAJOR << 16) | \
     (KRYON_CAPABILITY_API_VERSION_MINOR << 8) | \
     KRYON_CAPABILITY_API_VERSION_PATCH)

// ============================================================================
// Opaque Types
// ============================================================================

typedef struct KryonPlugin          KryonPlugin;
typedef struct KryonCapabilityAPI   KryonCapabilityAPI;
typedef struct KryonDataHandle      KryonDataHandle;
typedef struct KryonRenderContext   KryonRenderContext;
typedef struct KryonEventContext    KryonEventContext;

// ============================================================================
// Component Type Tags
// ============================================================================

typedef enum {
    KRYON_COMPONENT_CONTAINER = 0,
    KRYON_COMPONENT_TEXT = 1,
    KRYON_COMPONENT_BUTTON = 2,
    KRYON_COMPONENT_INPUT = 3,
    KRYON_COMPONENT_CHECKBOX = 4,
    KRYON_COMPONENT_DROPDOWN = 5,
    KRYON_COMPONENT_TEXTAREA = 6,
    KRYON_COMPONENT_ROW = 7,
    KRYON_COMPONENT_COLUMN = 8,
    KRYON_COMPONENT_CENTER = 9,
    KRYON_COMPONENT_IMAGE = 10,
    KRYON_COMPONENT_CANVAS = 11,
    KRYON_COMPONENT_NATIVE_CANVAS = 12,
    KRYON_COMPONENT_MARKDOWN = 13,
    KRYON_COMPONENT_SPRITE = 14,
    KRYON_COMPONENT_TAB_GROUP = 15,
    KRYON_COMPONENT_TAB_BAR = 16,
    KRYON_COMPONENT_TAB = 17,
    KRYON_COMPONENT_TAB_CONTENT = 18,
    KRYON_COMPONENT_TAB_PANEL = 19,
    KRYON_COMPONENT_MODAL = 20,
    KRYON_COMPONENT_TABLE = 21,
    KRYON_COMPONENT_TABLE_HEAD = 22,
    KRYON_COMPONENT_TABLE_BODY = 23,
    KRYON_COMPONENT_TABLE_FOOT = 24,
    KRYON_COMPONENT_TABLE_ROW = 25,
    KRYON_COMPONENT_TABLE_CELL = 26,
    KRYON_COMPONENT_HEADING = 27,
    KRYON_COMPONENT_PARAGRAPH = 28,
    KRYON_COMPONENT_BLOCKQUOTE = 29,
    KRYON_COMPONENT_CODE_BLOCK = 42,     // Used by syntax plugin
    KRYON_COMPONENT_HORIZONTAL_RULE = 43,
    KRYON_COMPONENT_LIST = 44,
    KRYON_COMPONENT_LIST_ITEM = 45,
    KRYON_COMPONENT_LINK = 46,
    KRYON_COMPONENT_SPAN = 47,
    KRYON_COMPONENT_STRONG = 48,
    KRYON_COMPONENT_EM = 49,
    KRYON_COMPONENT_CODE_INLINE = 50,
    KRYON_COMPONENT_SMALL = 51,
    KRYON_COMPONENT_MARK = 52,
    KRYON_COMPONENT_CUSTOM = 255,
    KRYON_COMPONENT_FLOWCHART = 60,
    KRYON_COMPONENT_FLOWCHART_NODE = 61,
    KRYON_COMPONENT_FLOWCHART_EDGE = 62,
    KRYON_COMPONENT_FLOWCHART_SUBGRAPH = 63,
    KRYON_COMPONENT_FLOWCHART_LABEL = 64,
} KryonComponentType;

// Convert component type to string
const char* kryon_component_type_to_string(KryonComponentType type);

// ============================================================================
// Data Handle - Opaque Component Data Access
// ============================================================================

struct KryonDataHandle {
    const void* data_ptr;           /* Pointer to custom_data (opaque to plugin) */
    size_t      data_size;          /* Size of data in bytes */
    uint32_t    component_type;     /* Type tag for validation */
    uint32_t    component_id;       /* Component instance ID */
    void*       user_data;          /* Plugin-specific storage */
};

// ============================================================================
// Render Context - Backend Rendering Information
// ============================================================================

struct KryonRenderContext {
    void*  backend_ptr;             /* Opaque backend (SDL_Renderer*, etc.) */
    float  x, y;                    /* Position */
    float  width, height;           /* Dimensions */
    void*  user_data;               /* Backend-specific data */
};

// ============================================================================
// Event Context - Event Handling Information
// ============================================================================

struct KryonEventContext {
    uint32_t    event_type;         /* Event type ID */
    void*       event_data;         /* Event-specific data */
    uint32_t    component_id;       /* Source component */
    void*       user_data;          /* Plugin-specific storage */
};

// ============================================================================
// Plugin Metadata
// ============================================================================

#define KRYON_PLUGIN_NAME_MAX    64
#define KRYON_PLUGIN_VERSION_MAX 32
#define KRYON_PLUGIN_DESC_MAX    256

struct KryonPlugin {
    char    name[KRYON_PLUGIN_NAME_MAX];       /* Plugin name */
    char    version[KRYON_PLUGIN_VERSION_MAX]; /* Plugin version (semver) */
    char    description[KRYON_PLUGIN_DESC_MAX];/* Short description */
    char    min_kryon_version[KRYON_PLUGIN_VERSION_MAX]; /* Min Kryon version */
    uint32_t api_version;          /* Required API version */
    void*    user_data;            /* Plugin-specific data */
};

// ============================================================================
// Capability Function Types
// ============================================================================

/**
 * Web renderer: generate HTML from component
 *
 * @param handle Opaque data handle for the component
 * @param theme Theme name (e.g., "dark", "light")
 * @return Newly allocated HTML string (caller frees via api->free_string)
 */
typedef char* (*kryon_web_renderer_fn)(
    const KryonDataHandle* handle,
    const char* theme
);

/**
 * CSS generator: generate CSS for a component type
 *
 * @param theme Theme name
 * @return Newly allocated CSS string (caller frees via api->free_string)
 */
typedef char* (*kryon_css_generator_fn)(const char* theme);

/**
 * Component renderer: render to backend (SDL3, terminal, etc.)
 *
 * @param ctx Rendering context with backend pointer and dimensions
 * @param handle Opaque data handle for the component
 */
typedef void (*kryon_component_renderer_fn)(
    const KryonRenderContext* ctx,
    const KryonDataHandle* handle
);

/**
 * Event handler: handle component events
 *
 * @param ctx Event context with event data
 */
typedef void (*kryon_event_handler_fn)(
    const KryonEventContext* ctx
);

/**
 * Layout measure: calculate intrinsic size
 *
 * @param handle Opaque data handle for the component
 * @param available_width Available width for layout
 * @param out_width Output: measured width
 * @param out_height Output: measured height
 */
typedef void (*kryon_layout_measure_fn)(
    const KryonDataHandle* handle,
    float available_width,
    float* out_width,
    float* out_height
);

/**
 * Command handler: handle custom commands
 *
 * @param command_id Command ID (100-255 range for plugins)
 * @param command_data Command data (opaque)
 * @param data_size Size of command data
 */
typedef void (*kryon_command_handler_fn)(
    uint32_t command_id,
    const void* command_data,
    size_t data_size
);

// ============================================================================
// Capability API - All Functions Available to Plugins
// ============================================================================

struct KryonCapabilityAPI {
    /* --- Metadata --- */
    uint32_t api_version;           /* API version (for compatibility check) */

    /* --- Data Access Functions --- */

    /**
     * Get a string value from component data
     * @param handle Data handle
     * @param key Key to look up (e.g., "code", "language", "text")
     * @return String value, or NULL if not found
     */
    const char* (*get_data_string)(
        const KryonDataHandle* handle,
        const char* key
    );

    /**
     * Get an integer value from component data
     */
    int64_t (*get_data_int)(
        const KryonDataHandle* handle,
        const char* key
    );

    /**
     * Get a float value from component data
     */
    double (*get_data_float)(
        const KryonDataHandle* handle,
        const char* key
    );

    /**
     * Get a boolean value from component data
     */
    bool (*get_data_bool)(
        const KryonDataHandle* handle,
        const char* key
    );

    /**
     * Get direct pointer to data (for advanced use)
     * @param handle Data handle
     * @param out_size Output: size of data
     * @return Pointer to data (opaque, do not modify)
     */
    const void* (*get_data_ptr)(
        const KryonDataHandle* handle,
        size_t* out_size
    );

    /* --- String Allocation (for returning values to core) --- */

    /**
     * Allocate a string to return to Kryon
     * Kryon will free this string when done
     */
    char* (*alloc_string)(size_t size);

    /**
     * Free a string allocated by alloc_string
     * (Usually Kryon handles this, but provided for cleanup)
     */
    void (*free_string)(char* str);

    /* --- Rendering Functions (for desktop/terminal backends) --- */

    /**
     * Draw text at position
     * @param ctx Render context
     * @param text Text to draw
     * @param x, y Position (relative to context)
     * @param color RGBA color
     */
    void (*draw_text)(
        KryonRenderContext* ctx,
        const char* text,
        float x, float y,
        uint32_t color
    );

    /**
     * Draw a rectangle
     * @param ctx Render context
     * @param x, y, width, height Dimensions
     * @param color RGBA color
     * @param filled If true, fill; otherwise outline
     */
    void (*draw_rect)(
        KryonRenderContext* ctx,
        float x, float y, float width, float height,
        uint32_t color,
        bool filled
    );

    /**
     * Draw a circle
     * @param ctx Render context
     * @param cx, cy Center position
     * @param radius Circle radius
     * @param color RGBA color
     * @param filled If true, fill; otherwise outline
     */
    void (*draw_circle)(
        KryonRenderContext* ctx,
        float cx, float cy, float radius,
        uint32_t color,
        bool filled
    );

    /* --- Registration Functions --- */

    /**
     * Register a web renderer for a component type
     * @param component_type Component type to handle
     * @param renderer Renderer function
     * @return true on success, false if type already registered
     */
    bool (*register_web_renderer)(
        uint32_t component_type,
        kryon_web_renderer_fn renderer
    );

    /**
     * Register a CSS generator for a component type
     * @param component_type Component type to handle
     * @param generator CSS generator function
     * @return true on success, false if type already registered
     */
    bool (*register_css_generator)(
        uint32_t component_type,
        kryon_css_generator_fn generator
    );

    /**
     * Register a component renderer for desktop/terminal backends
     * @param component_type Component type to handle
     * @param renderer Renderer function
     * @return true on success, false if type already registered
     */
    bool (*register_component_renderer)(
        uint32_t component_type,
        kryon_component_renderer_fn renderer
    );

    /**
     * Register an event handler
     * @param event_type Event type ID
     * @param handler Event handler function
     * @return true on success, false if type already registered
     */
    bool (*register_event_handler)(
        uint32_t event_type,
        kryon_event_handler_fn handler
    );

    /**
     * Register a command handler
     * @param command_id Command ID (100-255 for plugins)
     * @param handler Command handler function
     * @return true on success, false if ID already registered
     */
    bool (*register_command_handler)(
        uint32_t command_id,
        kryon_command_handler_fn handler
    );

    /* --- Logging --- */

    void (*log_debug)(const char* tag, const char* message);
    void (*log_info)(const char* tag, const char* message);
    void (*log_warn)(const char* tag, const char* message);
    void (*log_error)(const char* tag, const char* message);

    /* --- Version Info --- */

    /**
     * Get Kryon version string
     * @return Version string (e.g., "1.0.0")
     */
    const char* (*get_kryon_version)(void);

    /* --- State Manager Access (reactive state) --- */

    /**
     * Get integer state variable value
     * @param var_name Variable name
     * @param scope Variable scope (NULL for global)
     * @return Integer value, or 0 if not found
     */
    int64_t (*get_state_int)(const char* var_name, const char* scope);

    /**
     * Get string state variable value
     * @param var_name Variable name
     * @param scope Variable scope (NULL for global)
     * @return String value (valid until next call), or NULL if not found
     */
    const char* (*get_state_string)(const char* var_name, const char* scope);

    /**
     * Queue a state variable update
     * @param var_name Variable name
     * @param value New integer value
     * @return true on success, false on error
     */
    bool (*queue_state_update_int)(const char* var_name, int64_t value);

    /**
     * Queue a state variable update (string)
     * @param var_name Variable name
     * @param value New string value (copied)
     * @return true on success, false on error
     */
    bool (*queue_state_update_string)(const char* var_name, const char* value);

    /**
     * Queue a dirty flag mark for a component
     * @param component_id Component ID
     * @param flags Dirty flags (IR_DIRTY_* values)
     * @return true on success, false on error
     */
    bool (*queue_dirty_mark)(uint32_t component_id, uint32_t flags);

    /* Reserved for future expansion (maintains ABI stability) */
    void* reserved[11];  // Reduced from 16 to account for new fields
};

// ============================================================================
// Plugin Entry Point
// ============================================================================

/**
 * kryon_plugin_load - Plugin entry point called by Kryon
 *
 * This function MUST be exported by all plugins.
 *
 * @param api Pointer to capability API (valid for plugin lifetime)
 * @param plugin Plugin metadata structure to fill in
 * @return true on success, false on error
 *
 * The plugin should:
 * 1. Save the api pointer for later use (make it static/global)
 * 2. Fill in the plugin metadata (name, version, etc.)
 * 3. Register all capabilities (renderers, handlers, etc.)
 * 4. Return true on success
 */
typedef bool (*kryon_plugin_load_fn)(
    KryonCapabilityAPI* api,
    KryonPlugin* plugin
);

/**
 * kryon_plugin_unload - Called when plugin is unloaded (optional)
 *
 * @param plugin Plugin metadata (as passed to load)
 *
 * Plugins can export this to clean up resources.
 * If not exported, Kryon will simply unload the plugin.
 */
typedef void (*kryon_plugin_unload_fn)(KryonPlugin* plugin);

/* Default entry point name (can be overridden in plugin.toml) */
#define KRYON_PLUGIN_ENTRY_NAME "kryon_plugin_load"

#ifdef __cplusplus
}
#endif

#endif /* KRYON_CAPABILITY_H */
