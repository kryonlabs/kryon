/**
 * @file ir_capability.c
 * @brief Kryon Capability Registry Implementation
 *
 * Manages plugin capabilities and routes requests to appropriate plugins.
 * This is the core of the new plugin architecture that eliminates header
 * dependencies by using capability-based contracts.
 */

#include "../include/ir_capability.h"
#include "../include/ir_types.h"
#include "../include/ir_log.h"
#include "../include/ir_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define DLCLOSE(handle) FreeLibrary((HMODULE)handle)
#define DLSYM(handle, name) GetProcAddress((HMODULE)handle, name)
#else
#include <dlfcn.h>
#define DLCLOSE(handle) dlclose(handle)
#define DLSYM(handle, name) dlsym(handle, name)
#endif

// ============================================================================
// Constants
// ============================================================================

#define INITIAL_PLUGIN_CAPACITY  16
#define INITIAL_REG_CAPACITY     64
#define MAX_PLUGINS              128

// ============================================================================
// Internal Structures
// ============================================================================

/**
 * Internal plugin representation
 */
typedef struct {
    char                        name[KRYON_PLUGIN_NAME_MAX];
    char                        version[KRYON_PLUGIN_VERSION_MAX];
    KryonPlugin                 metadata;
    void*                       dl_handle;       /* dlopen handle */
    KryonCapabilityAPI*         api;             /* API pointer (not owned) */
    bool                        is_loaded;
} CapabilityPlugin;

/**
 * Capability registration entry
 */
typedef struct {
    uint32_t                    component_type;
    CapabilityPlugin*           plugin;
    kryon_web_renderer_fn       web_renderer;
    kryon_css_generator_fn      css_generator;
    kryon_component_renderer_fn component_renderer;
} CapabilityRegistration;

/**
 * Command handler registration
 */
typedef struct {
    uint32_t                    command_id;
    CapabilityPlugin*           plugin;
    kryon_command_handler_fn    handler;
} CommandRegistration;

/**
 * Event handler registration
 */
typedef struct {
    uint32_t                    event_type;
    CapabilityPlugin*           plugin;
    kryon_event_handler_fn      handler;
} EventRegistration;

// ============================================================================
// Global Registry
// ============================================================================

typedef struct {
    /* Plugin storage */
    CapabilityPlugin*       plugins;
    uint32_t                plugin_count;
    uint32_t                plugin_capacity;

    /* Web/CSS renderer registrations */
    CapabilityRegistration* registrations;
    uint32_t                registration_count;
    uint32_t                registration_capacity;

    /* Command handlers */
    CommandRegistration*     commands;
    uint32_t                command_count;
    uint32_t                command_capacity;

    /* Event handlers */
    EventRegistration*      events;
    uint32_t                event_count;
    uint32_t                event_capacity;

    /* The API structure passed to plugins */
    KryonCapabilityAPI       api;

    /* Initialization state */
    bool                    is_initialized;
} CapabilityRegistry;

static CapabilityRegistry g_registry = {0};

// ============================================================================
// Component Type to String (snake_case for plugin API)
// ============================================================================

static const char* component_type_to_snake_case(uint32_t type) {
    switch ((IRComponentType)type) {
        case IR_COMPONENT_CONTAINER:         return "container";
        case IR_COMPONENT_TEXT:              return "text";
        case IR_COMPONENT_BUTTON:            return "button";
        case IR_COMPONENT_INPUT:             return "input";
        case IR_COMPONENT_CHECKBOX:          return "checkbox";
        case IR_COMPONENT_DROPDOWN:          return "dropdown";
        case IR_COMPONENT_TEXTAREA:          return "textarea";
        case IR_COMPONENT_ROW:               return "row";
        case IR_COMPONENT_COLUMN:            return "column";
        case IR_COMPONENT_CENTER:            return "center";
        case IR_COMPONENT_IMAGE:             return "image";
        case IR_COMPONENT_CANVAS:            return "canvas";
        case IR_COMPONENT_NATIVE_CANVAS:     return "native_canvas";
        case IR_COMPONENT_MARKDOWN:          return "markdown";
        case IR_COMPONENT_SPRITE:            return "sprite";
        case IR_COMPONENT_TAB_GROUP:         return "tab_group";
        case IR_COMPONENT_TAB_BAR:           return "tab_bar";
        case IR_COMPONENT_TAB:               return "tab";
        case IR_COMPONENT_TAB_CONTENT:       return "tab_content";
        case IR_COMPONENT_TAB_PANEL:         return "tab_panel";
        case IR_COMPONENT_MODAL:             return "modal";
        case IR_COMPONENT_TABLE:             return "table";
        case IR_COMPONENT_TABLE_HEAD:        return "table_head";
        case IR_COMPONENT_TABLE_BODY:        return "table_body";
        case IR_COMPONENT_TABLE_FOOT:        return "table_foot";
        case IR_COMPONENT_TABLE_ROW:         return "table_row";
        case IR_COMPONENT_TABLE_CELL:        return "table_cell";
        case IR_COMPONENT_TABLE_HEADER_CELL: return "table_header_cell";
        case IR_COMPONENT_HEADING:           return "heading";
        case IR_COMPONENT_PARAGRAPH:         return "paragraph";
        case IR_COMPONENT_BLOCKQUOTE:        return "blockquote";
        case IR_COMPONENT_CODE_BLOCK:        return "code_block";
        case IR_COMPONENT_HORIZONTAL_RULE:   return "horizontal_rule";
        case IR_COMPONENT_LIST:              return "list";
        case IR_COMPONENT_LIST_ITEM:         return "list_item";
        case IR_COMPONENT_LINK:              return "link";
        case IR_COMPONENT_SPAN:              return "span";
        case IR_COMPONENT_STRONG:            return "strong";
        case IR_COMPONENT_EM:                return "em";
        case IR_COMPONENT_CODE_INLINE:       return "code_inline";
        case IR_COMPONENT_SMALL:             return "small";
        case IR_COMPONENT_MARK:              return "mark";
        case IR_COMPONENT_CUSTOM:            return "custom";
        case IR_COMPONENT_STATIC_BLOCK:      return "static_block";
        case IR_COMPONENT_FOR_LOOP:          return "for_loop";
        case IR_COMPONENT_FOR_EACH:          return "for_each";
        case IR_COMPONENT_VAR_DECL:          return "var_decl";
        case IR_COMPONENT_PLACEHOLDER:       return "placeholder";
        case IR_COMPONENT_FLOWCHART:         return "flowchart";
        case IR_COMPONENT_FLOWCHART_NODE:    return "flowchart_node";
        case IR_COMPONENT_FLOWCHART_EDGE:    return "flowchart_edge";
        case IR_COMPONENT_FLOWCHART_SUBGRAPH:return "flowchart_subgraph";
        case IR_COMPONENT_FLOWCHART_LABEL:   return "flowchart_label";
        default:                             return "unknown";
    }
}

// ============================================================================
// API Implementation - Data Access
// ============================================================================

/**
 * Data contract helpers for known component types
 * These bridge the opaque data handle to the actual IR structures
 */

/* Markdown data contract (not in ir_core.h) */
typedef struct {
    const char* markdown;
    size_t      length;
} IRMarkdownData;

static const char* api_get_data_string(
    const KryonDataHandle* handle,
    const char* key
) {
    if (!handle || !key || !handle->data_ptr) {
        return NULL;
    }

    /* Dispatch based on component type */
    switch (handle->component_type) {
        case IR_COMPONENT_CODE_BLOCK: {
            const IRCodeBlockData* data = (const IRCodeBlockData*)handle->data_ptr;
            if (strcmp(key, "code") == 0) return data->code;
            if (strcmp(key, "language") == 0) return data->language;
            break;
        }
        case IR_COMPONENT_MARKDOWN: {
            const IRMarkdownData* data = (const IRMarkdownData*)handle->data_ptr;
            if (strcmp(key, "markdown") == 0) return data->markdown;
            break;
        }
        case IR_COMPONENT_LINK: {
            const IRLinkData* data = (const IRLinkData*)handle->data_ptr;
            if (strcmp(key, "url") == 0) return data->url;
            if (strcmp(key, "title") == 0) return data->title;
            break;
        }
        case IR_COMPONENT_TEXT: {
            if (strcmp(key, "text") == 0) return (const char*)handle->data_ptr;
            break;
        }
        default:
            break;
    }

    return NULL;
}

static int64_t api_get_data_int(
    const KryonDataHandle* handle,
    const char* key
) {
    if (!handle || !key || !handle->data_ptr) {
        return 0;
    }

    switch (handle->component_type) {
        case IR_COMPONENT_CODE_BLOCK: {
            const IRCodeBlockData* data = (const IRCodeBlockData*)handle->data_ptr;
            if (strcmp(key, "length") == 0) return (int64_t)data->length;
            if (strcmp(key, "start_line") == 0) return data->start_line;
            if (strcmp(key, "show_line_numbers") == 0) return data->show_line_numbers ? 1 : 0;
            break;
        }
        default:
            break;
    }

    return 0;
}

static double api_get_data_float(
    const KryonDataHandle* handle,
    const char* key
) {
    (void)handle;
    (void)key;
    /* No float data contracts yet */
    return 0.0;
}

static bool api_get_data_bool(
    const KryonDataHandle* handle,
    const char* key
) {
    if (!handle || !key || !handle->data_ptr) {
        return false;
    }

    switch (handle->component_type) {
        case IR_COMPONENT_CODE_BLOCK: {
            const IRCodeBlockData* data = (const IRCodeBlockData*)handle->data_ptr;
            if (strcmp(key, "show_line_numbers") == 0) return data->show_line_numbers;
            break;
        }
        default:
            break;
    }

    return false;
}

static const void* api_get_data_ptr(
    const KryonDataHandle* handle,
    size_t* out_size
) {
    if (!handle) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (out_size) *out_size = handle->data_size;
    return handle->data_ptr;
}

// ============================================================================
// API Implementation - String Management
// ============================================================================

static char* api_alloc_string(size_t size) {
    if (size == 0) size = 1;
    return malloc(size);
}

static void api_free_string(char* str) {
    free(str);
}

// ============================================================================
// API Implementation - Rendering (Desktop/Terminal)
// ============================================================================

static void api_draw_text(
    KryonRenderContext* ctx,
    const char* text,
    float x, float y,
    uint32_t color
) {
    (void)ctx;
    (void)text;
    (void)x; (void)y;
    (void)color;
    /* TODO: Implement when desktop backend integration is ready */
    IR_LOG_WARN("capability", "draw_text not yet implemented");
}

static void api_draw_rect(
    KryonRenderContext* ctx,
    float x, float y, float width, float height,
    uint32_t color,
    bool filled
) {
    (void)ctx;
    (void)x; (void)y; (void)width; (void)height;
    (void)color; (void)filled;
    /* TODO: Implement when desktop backend integration is ready */
    IR_LOG_WARN("capability", "draw_rect not yet implemented");
}

static void api_draw_circle(
    KryonRenderContext* ctx,
    float cx, float cy, float radius,
    uint32_t color,
    bool filled
) {
    (void)ctx;
    (void)cx; (void)cy; (void)radius;
    (void)color; (void)filled;
    /* TODO: Implement when desktop backend integration is ready */
    IR_LOG_WARN("capability", "draw_circle not yet implemented");
}

// ============================================================================
// API Implementation - Registration
// ============================================================================

static CapabilityPlugin* find_or_create_plugin(const char* name) {
    /* Search for existing plugin */
    for (uint32_t i = 0; i < g_registry.plugin_count; i++) {
        if (strcmp(g_registry.plugins[i].name, name) == 0) {
            return &g_registry.plugins[i];
        }
    }

    /* Check capacity */
    if (g_registry.plugin_count >= g_registry.plugin_capacity) {
        if (g_registry.plugin_capacity >= MAX_PLUGINS) {
            IR_LOG_ERROR("capability", "Maximum plugin limit reached");
            return NULL;
        }
        uint32_t new_capacity = g_registry.plugin_capacity * 2;
        if (new_capacity > MAX_PLUGINS) new_capacity = MAX_PLUGINS;

        CapabilityPlugin* new_plugins = realloc(
            g_registry.plugins,
            new_capacity * sizeof(CapabilityPlugin)
        );
        if (!new_plugins) return NULL;

        g_registry.plugins = new_plugins;
        g_registry.plugin_capacity = new_capacity;

        /* Zero new entries */
        for (uint32_t i = g_registry.plugin_count; i < new_capacity; i++) {
            memset(&g_registry.plugins[i], 0, sizeof(CapabilityPlugin));
        }
    }

    /* Create new plugin entry */
    CapabilityPlugin* plugin = &g_registry.plugins[g_registry.plugin_count];
    strncpy(plugin->name, name, KRYON_PLUGIN_NAME_MAX - 1);
    plugin->api = &g_registry.api;
    plugin->is_loaded = true;

    g_registry.plugin_count++;
    return plugin;
}

static CapabilityRegistration* find_registration(uint32_t component_type) {
    for (uint32_t i = 0; i < g_registry.registration_count; i++) {
        if (g_registry.registrations[i].component_type == component_type) {
            return &g_registry.registrations[i];
        }
    }
    return NULL;
}

static bool api_register_web_renderer(
    const char* component_name,
    kryon_web_renderer_fn renderer
) {
    if (!component_name) {
        IR_LOG_ERROR("capability", "Cannot register with NULL component name");
        return false;
    }
    if (!renderer) {
        IR_LOG_ERROR("capability", "Cannot register NULL renderer");
        return false;
    }

    /* Convert component name to type ID */
    if (!ir_component_type_name_valid(component_name)) {
        IR_LOG_ERROR("capability", "Unknown component type: %s", component_name);
        return false;
    }
    uint32_t component_type = (uint32_t)ir_component_type_from_snake_case(component_name);

    /* Check if already registered */
    CapabilityRegistration* existing = find_registration(component_type);
    if (existing && existing->web_renderer) {
        IR_LOG_ERROR("capability", "Web renderer already registered for %s (type %u)",
                     component_name, component_type);
        return false;
    }

    /* Find or create registration entry */
    CapabilityRegistration* reg = existing;
    if (!reg) {
        /* Expand capacity if needed */
        if (g_registry.registration_count >= g_registry.registration_capacity) {
            uint32_t new_capacity = g_registry.registration_capacity * 2;
            CapabilityRegistration* new_regs = realloc(
                g_registry.registrations,
                new_capacity * sizeof(CapabilityRegistration)
            );
            if (!new_regs) return false;
            g_registry.registrations = new_regs;
            g_registry.registration_capacity = new_capacity;
        }

        reg = &g_registry.registrations[g_registry.registration_count++];
        reg->component_type = component_type;
        reg->plugin = NULL;  /* Will be set when plugin loads */
    }

    reg->web_renderer = renderer;

    IR_LOG_INFO("capability", "Registered web renderer for %s (type %u)",
                component_name, component_type);

    return true;
}

static bool api_register_css_generator(
    const char* component_name,
    kryon_css_generator_fn generator
) {
    if (!component_name) {
        IR_LOG_ERROR("capability", "Cannot register with NULL component name");
        return false;
    }
    if (!generator) {
        IR_LOG_ERROR("capability", "Cannot register NULL CSS generator");
        return false;
    }

    /* Convert component name to type ID */
    if (!ir_component_type_name_valid(component_name)) {
        IR_LOG_ERROR("capability", "Unknown component type: %s", component_name);
        return false;
    }
    uint32_t component_type = (uint32_t)ir_component_type_from_snake_case(component_name);

    CapabilityRegistration* existing = find_registration(component_type);
    if (existing && existing->css_generator) {
        IR_LOG_ERROR("capability", "CSS generator already registered for %s (type %u)",
                     component_name, component_type);
        return false;
    }

    CapabilityRegistration* reg = existing;
    if (!reg) {
        if (g_registry.registration_count >= g_registry.registration_capacity) {
            uint32_t new_capacity = g_registry.registration_capacity * 2;
            CapabilityRegistration* new_regs = realloc(
                g_registry.registrations,
                new_capacity * sizeof(CapabilityRegistration)
            );
            if (!new_regs) return false;
            g_registry.registrations = new_regs;
            g_registry.registration_capacity = new_capacity;
        }

        reg = &g_registry.registrations[g_registry.registration_count++];
        reg->component_type = component_type;
        reg->plugin = NULL;
    }

    reg->css_generator = generator;

    IR_LOG_INFO("capability", "Registered CSS generator for %s (type %u)",
                component_name, component_type);

    return true;
}

static bool api_register_component_renderer(
    const char* component_name,
    kryon_component_renderer_fn renderer
) {
    if (!component_name) {
        IR_LOG_ERROR("capability", "Cannot register with NULL component name");
        return false;
    }
    if (!renderer) {
        IR_LOG_ERROR("capability", "Cannot register NULL component renderer");
        return false;
    }

    /* Convert component name to type ID */
    if (!ir_component_type_name_valid(component_name)) {
        IR_LOG_ERROR("capability", "Unknown component type: %s", component_name);
        return false;
    }
    uint32_t component_type = (uint32_t)ir_component_type_from_snake_case(component_name);

    CapabilityRegistration* existing = find_registration(component_type);
    if (existing && existing->component_renderer) {
        IR_LOG_ERROR("capability", "Component renderer already registered for %s (type %u)",
                     component_name, component_type);
        return false;
    }

    CapabilityRegistration* reg = existing;
    if (!reg) {
        if (g_registry.registration_count >= g_registry.registration_capacity) {
            uint32_t new_capacity = g_registry.registration_capacity * 2;
            CapabilityRegistration* new_regs = realloc(
                g_registry.registrations,
                new_capacity * sizeof(CapabilityRegistration)
            );
            if (!new_regs) return false;
            g_registry.registrations = new_regs;
            g_registry.registration_capacity = new_capacity;
        }

        reg = &g_registry.registrations[g_registry.registration_count++];
        reg->component_type = component_type;
        reg->plugin = NULL;
    }

    reg->component_renderer = renderer;

    IR_LOG_INFO("capability", "Registered component renderer for %s (type %u)",
                component_name, component_type);

    return true;
}

static uint32_t api_get_component_type_id(const char* component_name) {
    if (!component_name) {
        return UINT32_MAX;
    }

    if (!ir_component_type_name_valid(component_name)) {
        return UINT32_MAX;
    }

    return (uint32_t)ir_component_type_from_snake_case(component_name);
}

static bool api_register_event_handler(
    uint32_t event_type,
    kryon_event_handler_fn handler
) {
    (void)event_type;
    (void)handler;
    /* TODO: Implement event handler registration */
    IR_LOG_WARN("capability", "Event handler registration not yet implemented");
    return true;
}

static bool api_register_command_handler(
    uint32_t command_id,
    kryon_command_handler_fn handler
) {
    (void)command_id;
    (void)handler;
    /* TODO: Implement command handler registration */
    IR_LOG_WARN("capability", "Command handler registration not yet implemented");
    return true;
}

// ============================================================================
// API Implementation - Logging
// ============================================================================

static void api_log_debug(const char* tag, const char* message) {
    IR_LOG_DEBUG(tag, "%s", message);
}

static void api_log_info(const char* tag, const char* message) {
    IR_LOG_INFO(tag, "%s", message);
}

static void api_log_warn(const char* tag, const char* message) {
    IR_LOG_WARN(tag, "%s", message);
}

static void api_log_error(const char* tag, const char* message) {
    IR_LOG_ERROR(tag, "%s", message);
}

// ============================================================================
// API Implementation - Version
// ============================================================================

static const char* api_get_kryon_version(void) {
    return "1.0.0";  /* TODO: Get from version.h or build system */
}

// ============================================================================
// API Implementation - State Manager Access
// ============================================================================

// Helper to get global state manager
static IRStateManager* get_global_state_mgr(void) {
    extern IRStateManager* ir_state_get_global(void);
    return ir_state_get_global();
}

static int64_t api_get_state_int(const char* var_name, const char* scope) {
    IRStateManager* state_mgr = get_global_state_mgr();
    if (!state_mgr) return 0;

    extern IRValue ir_executor_get_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id);
    IRExecutorContext* executor = ir_state_manager_get_executor(state_mgr);
    if (!executor) return 0;

    IRValue val = ir_executor_get_var(executor, var_name, 0);
    if (val.type == VAR_TYPE_INT) {
        return val.int_val;
    }
    return 0;
}

static const char* api_get_state_string(const char* var_name, const char* scope) {
    IRStateManager* state_mgr = get_global_state_mgr();
    if (!state_mgr) return NULL;

    extern IRValue ir_executor_get_var(IRExecutorContext* ctx, const char* name, uint32_t instance_id);
    static char s_string_buf[512];  // Buffer for string return values
    IRExecutorContext* executor = ir_state_manager_get_executor(state_mgr);
    if (!executor) return NULL;

    IRValue val = ir_executor_get_var(executor, var_name, 0);
    if (val.type == VAR_TYPE_STRING && val.string_val) {
        // Copy to buffer for persistence
        strncpy(s_string_buf, val.string_val, sizeof(s_string_buf) - 1);
        s_string_buf[sizeof(s_string_buf) - 1] = '\0';
        return s_string_buf;
    } else if (val.type == VAR_TYPE_INT) {
        snprintf(s_string_buf, sizeof(s_string_buf), "%lld", (long long)val.int_val);
        return s_string_buf;
    }
    return NULL;
}

static bool api_queue_state_update_int(const char* var_name, int64_t value) {
    IRStateManager* state_mgr = get_global_state_mgr();
    if (!state_mgr) return false;

    IRValue val = { .type = VAR_TYPE_INT, .int_val = value };
    return ir_state_queue_set_var(state_mgr, var_name, val, NULL);
}

static bool api_queue_state_update_string(const char* var_name, const char* value) {
    IRStateManager* state_mgr = get_global_state_mgr();
    if (!state_mgr) return false;

    IRValue val = { .type = VAR_TYPE_STRING, .string_val = (char*)value };
    return ir_state_queue_set_var(state_mgr, var_name, val, NULL);
}

static bool api_queue_dirty_mark(uint32_t component_id, uint32_t flags) {
    IRStateManager* state_mgr = get_global_state_mgr();
    if (!state_mgr) return false;

    return ir_state_queue_mark_dirty(state_mgr, component_id, flags, false);
}

// ============================================================================
// Registry Public API
// ============================================================================

void ir_capability_registry_init(void) {
    if (g_registry.is_initialized) {
        IR_LOG_WARN("capability", "Registry already initialized");
        return;
    }

    /* Zero everything */
    memset(&g_registry, 0, sizeof(g_registry));

    /* Initialize API structure */
    g_registry.api.api_version = KRYON_CAPABILITY_API_VERSION;
    g_registry.api.get_data_string = api_get_data_string;
    g_registry.api.get_data_int = api_get_data_int;
    g_registry.api.get_data_float = api_get_data_float;
    g_registry.api.get_data_bool = api_get_data_bool;
    g_registry.api.get_data_ptr = api_get_data_ptr;
    g_registry.api.alloc_string = api_alloc_string;
    g_registry.api.free_string = api_free_string;
    g_registry.api.draw_text = api_draw_text;
    g_registry.api.draw_rect = api_draw_rect;
    g_registry.api.draw_circle = api_draw_circle;
    g_registry.api.register_web_renderer = api_register_web_renderer;
    g_registry.api.register_css_generator = api_register_css_generator;
    g_registry.api.register_component_renderer = api_register_component_renderer;
    g_registry.api.get_component_type_id = api_get_component_type_id;
    g_registry.api.register_event_handler = api_register_event_handler;
    g_registry.api.register_command_handler = api_register_command_handler;
    g_registry.api.log_debug = api_log_debug;
    g_registry.api.log_info = api_log_info;
    g_registry.api.log_warn = api_log_warn;
    g_registry.api.log_error = api_log_error;
    g_registry.api.get_kryon_version = api_get_kryon_version;
    g_registry.api.get_state_int = api_get_state_int;
    g_registry.api.get_state_string = api_get_state_string;
    g_registry.api.queue_state_update_int = api_queue_state_update_int;
    g_registry.api.queue_state_update_string = api_queue_state_update_string;
    g_registry.api.queue_dirty_mark = api_queue_dirty_mark;

    /* Allocate storage */
    g_registry.plugin_capacity = INITIAL_PLUGIN_CAPACITY;
    g_registry.plugins = calloc(
        g_registry.plugin_capacity,
        sizeof(CapabilityPlugin)
    );

    g_registry.registration_capacity = INITIAL_REG_CAPACITY;
    g_registry.registrations = calloc(
        g_registry.registration_capacity,
        sizeof(CapabilityRegistration)
    );

    g_registry.command_capacity = INITIAL_REG_CAPACITY;
    g_registry.commands = calloc(
        g_registry.command_capacity,
        sizeof(CommandRegistration)
    );

    g_registry.event_capacity = INITIAL_REG_CAPACITY;
    g_registry.events = calloc(
        g_registry.event_capacity,
        sizeof(EventRegistration)
    );

    if (!g_registry.plugins || !g_registry.registrations ||
        !g_registry.commands || !g_registry.events) {
        IR_LOG_ERROR("capability", "Failed to allocate registry storage");
        ir_capability_registry_shutdown();
        return;
    }

    g_registry.is_initialized = true;

    IR_LOG_INFO("capability", "Capability registry initialized (API version %u.%u.%u)",
                KRYON_CAPABILITY_API_VERSION_MAJOR,
                KRYON_CAPABILITY_API_VERSION_MINOR,
                KRYON_CAPABILITY_API_VERSION_PATCH);
}

void ir_capability_registry_shutdown(void) {
    if (!g_registry.is_initialized) {
        return;
    }

    /* Unload all plugins */
    for (uint32_t i = 0; i < g_registry.plugin_count; i++) {
        CapabilityPlugin* plugin = &g_registry.plugins[i];
        if (plugin->dl_handle) {
            IR_LOG_INFO("capability", "Unloading plugin: %s", plugin->name);
            DLCLOSE(plugin->dl_handle);
        }
    }

    /* Free storage */
    free(g_registry.plugins);
    free(g_registry.registrations);
    free(g_registry.commands);
    free(g_registry.events);

    /* Zero everything */
    memset(&g_registry, 0, sizeof(g_registry));

    IR_LOG_INFO("capability", "Capability registry shutdown complete");
}

KryonCapabilityAPI* ir_capability_get_api(void) {
    return &g_registry.api;
}

bool ir_capability_load_plugin(const char* path, const char* name) {
    if (!g_registry.is_initialized) {
        IR_LOG_ERROR("capability", "Registry not initialized");
        return false;
    }

    if (!path) {
        IR_LOG_ERROR("capability", "NULL plugin path");
        return false;
    }

    /* Load the shared library */
#if defined(_WIN32) || defined(_WIN64)
    HMODULE handle = LoadLibraryA(path);
    if (!handle) {
        IR_LOG_ERROR("capability", "Failed to load plugin: %s", path);
        return false;
    }
#else
    void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        IR_LOG_ERROR("capability", "Failed to load plugin '%s' from %s: %s",
                     name ? name : "(unknown)", path, dlerror());
        return false;
    }
#endif

    /* Find the entry point */
    kryon_plugin_load_fn entry = (kryon_plugin_load_fn)DLSYM(handle, KRYON_PLUGIN_ENTRY_NAME);
    if (!entry) {
        IR_LOG_ERROR("capability", "Plugin missing entry point: %s", KRYON_PLUGIN_ENTRY_NAME);
        DLCLOSE(handle);
        return false;
    }

    /* Create plugin entry */
    CapabilityPlugin* plugin = find_or_create_plugin(name ? name : path);
    if (!plugin) {
        IR_LOG_ERROR("capability", "Failed to create plugin entry");
        DLCLOSE(handle);
        return false;
    }

    plugin->dl_handle = handle;

    /* Call the plugin's entry point */
    IR_LOG_INFO("capability", "Loading plugin: %s", plugin->name);

    if (!entry(&g_registry.api, &plugin->metadata)) {
        IR_LOG_ERROR("capability", "Plugin %s failed to initialize", plugin->name);
        DLCLOSE(handle);
        plugin->dl_handle = NULL;
        plugin->is_loaded = false;
        return false;
    }

    /* Copy metadata */
    strncpy(plugin->version, plugin->metadata.version, KRYON_PLUGIN_VERSION_MAX - 1);

    IR_LOG_INFO("capability", "Plugin loaded: %s v%s", plugin->name, plugin->version);
    return true;
}

bool ir_capability_unload_plugin(const char* name) {
    if (!name || !g_registry.is_initialized) {
        return false;
    }

    for (uint32_t i = 0; i < g_registry.plugin_count; i++) {
        CapabilityPlugin* plugin = &g_registry.plugins[i];
        if (strcmp(plugin->name, name) == 0) {
            if (plugin->dl_handle) {
                /* Call unload function if present */
#if defined(_WIN32) || defined(_WIN64)
                kryon_plugin_unload_fn unload = (kryon_plugin_unload_fn)
                    GetProcAddress((HMODULE)plugin->dl_handle, "kryon_plugin_unload");
#else
                kryon_plugin_unload_fn unload = (kryon_plugin_unload_fn)
                    dlsym(plugin->dl_handle, "kryon_plugin_unload");
#endif
                if (unload) {
                    unload(&plugin->metadata);
                }

                DLCLOSE(plugin->dl_handle);
                plugin->dl_handle = NULL;
            }

            plugin->is_loaded = false;

            /* Remove registrations for this plugin */
            for (uint32_t j = 0; j < g_registry.registration_count; j++) {
                if (g_registry.registrations[j].plugin == plugin) {
                    /* Shift remaining entries */
                    memmove(&g_registry.registrations[j],
                           &g_registry.registrations[j + 1],
                           (g_registry.registration_count - j - 1) * sizeof(CapabilityRegistration));
                    g_registry.registration_count--;
                    j--;
                }
            }

            IR_LOG_INFO("capability", "Plugin unloaded: %s", name);
            return true;
        }
    }

    IR_LOG_WARN("capability", "Plugin not found: %s", name);
    return false;
}

char* ir_capability_render_web(
    uint32_t component_type,
    const KryonDataHandle* handle,
    const char* theme
) {
    if (!g_registry.is_initialized) {
        return NULL;
    }

    CapabilityRegistration* reg = find_registration(component_type);
    if (!reg || !reg->web_renderer) {
        return NULL;
    }

    return reg->web_renderer(handle, theme);
}

char* ir_capability_generate_css(
    uint32_t component_type,
    const char* theme
) {
    if (!g_registry.is_initialized) {
        return NULL;
    }

    CapabilityRegistration* reg = find_registration(component_type);
    if (!reg || !reg->css_generator) {
        return NULL;
    }

    return reg->css_generator(theme);
}

bool ir_capability_has_web_renderer(uint32_t component_type) {
    if (!g_registry.is_initialized) {
        return false;
    }

    CapabilityRegistration* reg = find_registration(component_type);
    return reg && reg->web_renderer;
}

bool ir_capability_has_css_generator(uint32_t component_type) {
    if (!g_registry.is_initialized) {
        return false;
    }

    CapabilityRegistration* reg = find_registration(component_type);
    return reg && reg->css_generator;
}

bool ir_capability_has_component_renderer(uint32_t component_type) {
    if (!g_registry.is_initialized) {
        return false;
    }

    CapabilityRegistration* reg = find_registration(component_type);
    return reg && reg->component_renderer;
}

KryonDataHandle ir_capability_create_data_handle(
    const void* custom_data,
    size_t data_size,
    uint32_t component_type,
    uint32_t component_id
) {
    KryonDataHandle handle = {
        .data_ptr = custom_data,
        .data_size = data_size,
        .component_type = component_type,
        .component_id = component_id,
        .user_data = NULL
    };
    return handle;
}

void ir_capability_list_plugins(void) {
    if (!g_registry.is_initialized) {
        printf("Capability registry not initialized\n");
        return;
    }

    printf("Loaded Plugins (%u/%u):\n", g_registry.plugin_count, g_registry.plugin_capacity);
    for (uint32_t i = 0; i < g_registry.plugin_count; i++) {
        CapabilityPlugin* plugin = &g_registry.plugins[i];
        if (plugin->is_loaded) {
            printf("  - %s v%s\n", plugin->name, plugin->version);
        }
    }

    printf("\nRegistered Capabilities:\n");
    for (uint32_t i = 0; i < g_registry.registration_count; i++) {
        CapabilityRegistration* reg = &g_registry.registrations[i];
        printf("  - Type %u (%s):\n", reg->component_type,
               component_type_to_snake_case(reg->component_type));
        if (reg->web_renderer) printf("      Web renderer\n");
        if (reg->css_generator) printf("      CSS generator\n");
        if (reg->component_renderer) printf("      Component renderer\n");
    }
}

uint32_t ir_capability_get_plugin_count(void) {
    return g_registry.plugin_count;
}

const char* ir_capability_get_plugin_name(uint32_t index) {
    if (index >= g_registry.plugin_count) {
        return NULL;
    }
    return g_registry.plugins[index].name;
}
