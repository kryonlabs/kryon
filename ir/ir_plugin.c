/*
 * Kryon Plugin System Implementation
 */

#define _GNU_SOURCE
#include "ir_plugin.h"
#include "ir_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Version Constants
// ============================================================================

#define KRYON_VERSION_MAJOR 0
#define KRYON_VERSION_MINOR 3
#define KRYON_VERSION_PATCH 0
#define KRYON_VERSION_STRING "0.3.0"

// ============================================================================
// Plugin Registry Entry
// ============================================================================

#define MAX_PLUGINS 32

typedef struct {
    IRPluginMetadata metadata;
    bool is_active;
    uint32_t handler_count;
} PluginRegistryEntry;

// ============================================================================
// Global State
// ============================================================================

// Named struct to match forward declaration in header
typedef struct PluginSystem {
    // Handler table (256 entries, only 100-255 used for plugins)
    IRPluginHandler handlers[256];
    uint32_t registered_count;

    // Component renderer table (indexed by IRComponentType)
    IRPluginComponentRenderer component_renderers[64];  // Support up to 64 component types
    uint32_t component_renderer_count;

    // Plugin registry
    PluginRegistryEntry plugins[MAX_PLUGINS];
    uint32_t plugin_count;

    // Backend capabilities
    IRBackendCapabilities backend_caps;
    bool caps_initialized;

    // Statistics
    uint32_t commands_dispatched;
    uint32_t unknown_commands;
    uint32_t components_rendered;

    // Callback bridges (indexed by IRComponentType)
    IRPluginCallbackBridge callback_bridges[32];
    uint32_t callback_bridge_count;

    // Web component renderers (indexed by IRComponentType)
    IRPluginWebComponentRenderer web_renderers[64];
    IRPluginWebCSSGenerator web_css_generators[64];
    uint32_t web_renderer_count;

    // Plugin event type registry (100-255 range)
    IRPluginEventType event_types[156];  // 156 slots for plugin events (100-255 inclusive)
    uint32_t event_type_count;

    // Current plugin requirements (from deserialized IR file)
    char** current_requirements;
    uint32_t current_requirement_count;

    // Loaded plugin handles (for dynamic symbol lookup)
    IRPluginHandle* loaded_handles[MAX_PLUGINS];
    uint32_t loaded_handle_count;
} PluginSystem;

// Global plugin system instance - shared across all dynamically loaded libraries
// Removed 'static' keyword to make symbol globally visible
PluginSystem g_plugin_system = {0};

// ============================================================================
// Version Comparison Helpers
// ============================================================================

static int parse_version(const char* version, int* major, int* minor, int* patch) {
    if (!version) return -1;
    return sscanf(version, "%d.%d.%d", major, minor, patch);
}

static bool version_compare(const char* required, const char* current) {
    int req_major = 0, req_minor = 0, req_patch = 0;
    int cur_major = 0, cur_minor = 0, cur_patch = 0;

    if (parse_version(required, &req_major, &req_minor, &req_patch) < 1) return false;
    if (parse_version(current, &cur_major, &cur_minor, &cur_patch) < 1) return false;

    if (cur_major > req_major) return true;
    if (cur_major < req_major) return false;
    if (cur_minor > req_minor) return true;
    if (cur_minor < req_minor) return false;
    return cur_patch >= req_patch;
}

// ============================================================================
// Enhanced Plugin Registration
// ============================================================================

bool ir_plugin_register(const IRPluginMetadata* metadata) {
    if (!metadata) {
        fprintf(stderr, "[kryon][plugin] NULL metadata provided\n");
        return false;
    }

    // Check if we have space for more plugins
    if (g_plugin_system.plugin_count >= MAX_PLUGINS) {
        fprintf(stderr, "[kryon][plugin] Maximum number of plugins (%d) reached\n", MAX_PLUGINS);
        return false;
    }

    // Check version compatibility
    if (metadata->kryon_version_min[0] != '\0') {
        if (!version_compare(metadata->kryon_version_min, KRYON_VERSION_STRING)) {
            fprintf(stderr, "[kryon][plugin] Plugin '%s' requires Kryon %s (current: %s)\n",
                    metadata->name, metadata->kryon_version_min, KRYON_VERSION_STRING);
            return false;
        }
    }

    // Check command ID range validity (allow 0-0 for plugins with no commands)
    bool has_commands = (metadata->command_id_start != 0 || metadata->command_id_end != 0);
    if (has_commands) {
        if (metadata->command_id_start < IR_PLUGIN_CMD_START ||
            metadata->command_id_end > IR_PLUGIN_CMD_END ||
            metadata->command_id_start > metadata->command_id_end) {
            fprintf(stderr, "[kryon][plugin] Plugin '%s' has invalid command ID range (%u-%u)\n",
                    metadata->name, metadata->command_id_start, metadata->command_id_end);
            return false;
        }

        // Check for command ID conflicts (only if plugin has commands)
        char conflict_buf[IR_PLUGIN_NAME_MAX];
        if (ir_plugin_check_command_conflict(metadata->command_id_start, metadata->command_id_end,
                                             conflict_buf, sizeof(conflict_buf))) {
            fprintf(stderr, "[kryon][plugin] Plugin '%s' command ID range conflicts with '%s'\n",
                    metadata->name, conflict_buf);
            return false;
        }
    }

    // Check for duplicate plugin name
    for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
        if (g_plugin_system.plugins[i].is_active &&
            strcmp(g_plugin_system.plugins[i].metadata.name, metadata->name) == 0) {
            fprintf(stderr, "[kryon][plugin] Plugin '%s' is already registered\n", metadata->name);
            return false;
        }
    }

    // Check required backend capabilities
    if (metadata->required_capabilities && g_plugin_system.caps_initialized) {
        for (uint32_t i = 0; i < metadata->capability_count; i++) {
            const char* cap = metadata->required_capabilities[i];
            if (!ir_plugin_backend_supports(cap)) {
                fprintf(stderr, "[kryon][plugin] Plugin '%s' requires capability '%s' which is not supported\n",
                        metadata->name, cap);
                return false;
            }
        }
    }

    // Add plugin to registry
    PluginRegistryEntry* entry = &g_plugin_system.plugins[g_plugin_system.plugin_count];
    entry->metadata = *metadata;
    entry->is_active = true;
    entry->handler_count = 0;
    g_plugin_system.plugin_count++;

    printf("[kryon][plugin] Registered plugin '%s' v%s (commands %u-%u)\n",
           metadata->name, metadata->version,
           metadata->command_id_start, metadata->command_id_end);

    return true;
}

bool ir_plugin_unregister(const char* plugin_name) {
    if (!plugin_name) return false;

    for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
        PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
        if (entry->is_active && strcmp(entry->metadata.name, plugin_name) == 0) {
            // Unregister all handlers in this plugin's range
            for (uint32_t cmd_id = entry->metadata.command_id_start;
                 cmd_id <= entry->metadata.command_id_end;
                 cmd_id++) {
                if (g_plugin_system.handlers[cmd_id] != NULL) {
                    g_plugin_system.handlers[cmd_id] = NULL;
                    g_plugin_system.registered_count--;
                    entry->handler_count--;
                }
            }

            entry->is_active = false;
            printf("[kryon][plugin] Unregistered plugin '%s'\n", plugin_name);
            return true;
        }
    }

    return false;
}

bool ir_plugin_get_info(const char* plugin_name, IRPluginInfo* info) {
    if (!plugin_name || !info) return false;

    for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
        const PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
        if (entry->is_active && strcmp(entry->metadata.name, plugin_name) == 0) {
            info->metadata = entry->metadata;
            info->is_loaded = true;
            info->handler_count = entry->handler_count;
            return true;
        }
    }

    return false;
}

uint32_t ir_plugin_list_all(IRPluginInfo* infos, uint32_t max_count) {
    if (!infos || max_count == 0) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_plugin_system.plugin_count && count < max_count; i++) {
        const PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
        if (entry->is_active) {
            infos[count].metadata = entry->metadata;
            infos[count].is_loaded = true;
            infos[count].handler_count = entry->handler_count;
            count++;
        }
    }

    return count;
}

bool ir_plugin_check_command_conflict(uint32_t start_id, uint32_t end_id,
                                      char* conflicting_plugin, size_t buffer_size) {
    for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
        const PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
        if (!entry->is_active) continue;

        // Check if ranges overlap
        if (start_id <= entry->metadata.command_id_end &&
            end_id >= entry->metadata.command_id_start) {
            if (conflicting_plugin && buffer_size > 0) {
                snprintf(conflicting_plugin, buffer_size, "%s", entry->metadata.name);
            }
            return true;
        }
    }

    return false;
}

bool ir_plugin_check_version_compat(const char* required_version) {
    return version_compare(required_version, KRYON_VERSION_STRING);
}

// ============================================================================
// Simple Handler Registration (Legacy)
// ============================================================================

bool ir_plugin_register_handler(uint32_t command_id, IRPluginHandler handler) {
    // Validate command ID range
    if (command_id < IR_PLUGIN_CMD_START || command_id > IR_PLUGIN_CMD_END) {
        fprintf(stderr, "[kryon][plugin] Command ID %u out of range (%d-%d)\n",
                command_id, IR_PLUGIN_CMD_START, IR_PLUGIN_CMD_END);
        return false;
    }

    // Check if already registered
    if (g_plugin_system.handlers[command_id] != NULL) {
        fprintf(stderr, "[kryon][plugin] Command ID %u already registered\n", command_id);
        return false;
    }

    // Register handler
    g_plugin_system.handlers[command_id] = handler;
    g_plugin_system.registered_count++;

    // Update handler count for owning plugin (if any)
    for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
        PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
        if (entry->is_active &&
            command_id >= entry->metadata.command_id_start &&
            command_id <= entry->metadata.command_id_end) {
            entry->handler_count++;
            break;
        }
    }

    return true;
}

void ir_plugin_unregister_handler(uint32_t command_id) {
    if (command_id >= IR_PLUGIN_CMD_START && command_id <= IR_PLUGIN_CMD_END) {
        if (g_plugin_system.handlers[command_id] != NULL) {
            g_plugin_system.handlers[command_id] = NULL;
            g_plugin_system.registered_count--;
        }
    }
}

bool ir_plugin_has_handler(uint32_t command_id) {
    if (command_id < IR_PLUGIN_CMD_START || command_id > IR_PLUGIN_CMD_END) {
        return false;
    }
    return g_plugin_system.handlers[command_id] != NULL;
}

// ============================================================================
// Plugin Dispatch
// ============================================================================

bool ir_plugin_dispatch(void* backend_ctx, uint32_t command_type, const void* cmd) {
    // Check if command is in plugin range
    if (command_type < IR_PLUGIN_CMD_START || command_type > IR_PLUGIN_CMD_END) {
        return false;
    }

    // Get handler
    IRPluginHandler handler = g_plugin_system.handlers[command_type];
    if (handler == NULL) {
        g_plugin_system.unknown_commands++;
        return false;
    }

    // Dispatch to handler
    handler(backend_ctx, cmd);
    g_plugin_system.commands_dispatched++;
    return true;
}

// ============================================================================
// Backend Capabilities
// ============================================================================

void ir_plugin_set_backend_capabilities(const IRBackendCapabilities* caps) {
    if (caps) {
        g_plugin_system.backend_caps = *caps;
        g_plugin_system.caps_initialized = true;
    }
}

const IRBackendCapabilities* ir_plugin_get_backend_capabilities(void) {
    if (!g_plugin_system.caps_initialized) {
        // Return default capabilities (all false)
        static const IRBackendCapabilities default_caps = {0};
        return &default_caps;
    }
    return &g_plugin_system.backend_caps;
}

bool ir_plugin_backend_supports(const char* capability) {
    if (!capability || !g_plugin_system.caps_initialized) {
        return false;
    }

    const IRBackendCapabilities* caps = &g_plugin_system.backend_caps;

    // String comparison for capability names
    if (strcmp(capability, "2d_shapes") == 0) {
        return caps->has_2d_shapes;
    } else if (strcmp(capability, "transforms") == 0) {
        return caps->has_transforms;
    } else if (strcmp(capability, "hardware_accel") == 0) {
        return caps->has_hardware_accel;
    } else if (strcmp(capability, "blend_modes") == 0) {
        return caps->has_blend_modes;
    } else if (strcmp(capability, "antialiasing") == 0) {
        return caps->has_antialiasing;
    } else if (strcmp(capability, "gradients") == 0) {
        return caps->has_gradients;
    } else if (strcmp(capability, "text_rendering") == 0) {
        return caps->has_text_rendering;
    } else if (strcmp(capability, "3d_rendering") == 0) {
        return caps->has_3d_rendering;
    }

    return false;
}

// ============================================================================
// Plugin Event Type Registration
// ============================================================================

bool ir_plugin_register_event_type(const char* plugin_name, const char* event_type_name,
                                    uint32_t event_type_id, const char* description) {
    if (!plugin_name || !event_type_name) {
        fprintf(stderr, "[plugin] Invalid parameters for event type registration\n");
        return false;
    }

    // Validate ID range (100-255)
    if (event_type_id < 100 || event_type_id > 255) {
        fprintf(stderr, "[plugin] Invalid event type ID %u (must be 100-255)\n", event_type_id);
        return false;
    }

    // Check for duplicate event type name
    for (uint32_t i = 0; i < g_plugin_system.event_type_count; i++) {
        if (strcmp(g_plugin_system.event_types[i].event_type_name, event_type_name) == 0) {
            fprintf(stderr, "[plugin] Duplicate event type '%s'\n", event_type_name);
            return false;
        }
    }

    // Check for duplicate event type ID
    for (uint32_t i = 0; i < g_plugin_system.event_type_count; i++) {
        if (g_plugin_system.event_types[i].event_type_id == event_type_id) {
            fprintf(stderr, "[plugin] Event type ID %u already used\n", event_type_id);
            return false;
        }
    }

    // Register event type
    if (g_plugin_system.event_type_count >= 156) {
        fprintf(stderr, "[plugin] Event type registry full (max 156 plugin events)\n");
        return false;
    }

    IRPluginEventType* event_type = &g_plugin_system.event_types[g_plugin_system.event_type_count];
    event_type->plugin_name = strdup(plugin_name);
    event_type->event_type_name = strdup(event_type_name);
    event_type->event_type_id = event_type_id;
    event_type->description = description ? strdup(description) : NULL;
    g_plugin_system.event_type_count++;

    fprintf(stderr, "[plugin] Registered event type '%s' (ID=%u) from plugin '%s'\n",
            event_type_name, event_type_id, plugin_name);
    return true;
}

uint32_t ir_plugin_get_event_type_id(const char* event_type_name) {
    if (!event_type_name) return 0;

    for (uint32_t i = 0; i < g_plugin_system.event_type_count; i++) {
        if (strcmp(g_plugin_system.event_types[i].event_type_name, event_type_name) == 0) {
            return g_plugin_system.event_types[i].event_type_id;
        }
    }
    return 0;  // Not found
}

const char* ir_plugin_get_event_type_name(uint32_t event_type_id) {
    for (uint32_t i = 0; i < g_plugin_system.event_type_count; i++) {
        if (g_plugin_system.event_types[i].event_type_id == event_type_id) {
            return g_plugin_system.event_types[i].event_type_name;
        }
    }
    return NULL;
}

bool ir_plugin_has_event_type(const char* event_type_name) {
    return ir_plugin_get_event_type_id(event_type_name) != 0;
}

// ============================================================================
// Statistics
// ============================================================================

void ir_plugin_get_stats(IRPluginStats* stats) {
    if (!stats) return;

    stats->registered_handlers = g_plugin_system.registered_count;
    stats->commands_dispatched = g_plugin_system.commands_dispatched;
    stats->unknown_commands = g_plugin_system.unknown_commands;
}

void ir_plugin_print_stats(void) {
    printf("=== Plugin System Statistics ===\n");
    printf("Registered Handlers: %u\n", g_plugin_system.registered_count);
    printf("Commands Dispatched: %u\n", g_plugin_system.commands_dispatched);
    printf("Unknown Commands: %u\n", g_plugin_system.unknown_commands);

    // Show registered plugins
    if (g_plugin_system.plugin_count > 0) {
        printf("\n=== Registered Plugins (%u) ===\n", g_plugin_system.plugin_count);
        for (uint32_t i = 0; i < g_plugin_system.plugin_count; i++) {
            const PluginRegistryEntry* entry = &g_plugin_system.plugins[i];
            if (entry->is_active) {
                printf("  • %s v%s\n", entry->metadata.name, entry->metadata.version);
                printf("    Commands: %u-%u (%u handlers)\n",
                       entry->metadata.command_id_start,
                       entry->metadata.command_id_end,
                       entry->handler_count);
                if (entry->metadata.description[0] != '\0') {
                    printf("    Description: %s\n", entry->metadata.description);
                }
            }
        }
    }

    if (g_plugin_system.caps_initialized) {
        printf("\n=== Backend Capabilities ===\n");
        const IRBackendCapabilities* caps = &g_plugin_system.backend_caps;
        printf("2D Shapes: %s\n", caps->has_2d_shapes ? "yes" : "no");
        printf("Transforms: %s\n", caps->has_transforms ? "yes" : "no");
        printf("Hardware Accel: %s\n", caps->has_hardware_accel ? "yes" : "no");
        printf("Blend Modes: %s\n", caps->has_blend_modes ? "yes" : "no");
        printf("Antialiasing: %s\n", caps->has_antialiasing ? "yes" : "no");
        printf("Gradients: %s\n", caps->has_gradients ? "yes" : "no");
        printf("Text Rendering: %s\n", caps->has_text_rendering ? "yes" : "no");
        printf("3D Rendering: %s\n", caps->has_3d_rendering ? "yes" : "no");
    }

    printf("\n");
}

void ir_plugin_reset_stats(void) {
    g_plugin_system.commands_dispatched = 0;
    g_plugin_system.unknown_commands = 0;
    g_plugin_system.components_rendered = 0;
}

// ============================================================================
// Component Renderer Registration
// ============================================================================

bool ir_plugin_register_component_renderer(uint32_t component_type, IRPluginComponentRenderer renderer) {
    if (!renderer) {
        fprintf(stderr, "[kryon][plugin] NULL component renderer provided\n");
        return false;
    }

    if (component_type >= 64) {
        fprintf(stderr, "[kryon][plugin] Component type %u out of range (max 63)\n", component_type);
        return false;
    }

    // Check if already registered
    if (g_plugin_system.component_renderers[component_type] != NULL) {
        fprintf(stderr, "[kryon][plugin] Component type %u already has a renderer\n", component_type);
        return false;
    }

    // Register renderer
    g_plugin_system.component_renderers[component_type] = renderer;
    g_plugin_system.component_renderer_count++;

    printf("[kryon][plugin] Registered component renderer for type %u\n", component_type);
    return true;
}

void ir_plugin_unregister_component_renderer(uint32_t component_type) {
    if (component_type < 64 && g_plugin_system.component_renderers[component_type] != NULL) {
        g_plugin_system.component_renderers[component_type] = NULL;
        g_plugin_system.component_renderer_count--;
    }
}

bool ir_plugin_has_component_renderer(uint32_t component_type) {
    if (component_type >= 64) {
        return false;
    }
    return g_plugin_system.component_renderers[component_type] != NULL;
}

bool ir_plugin_dispatch_component_render(void* backend_ctx, uint32_t component_type,
                                         const IRComponent* component,
                                         float x, float y, float width, float height) {
    if (component_type >= 32) {
        fprintf(stderr, "[kryon][plugin] Component type %u out of range (>= 32)\n", component_type);
        return false;
    }

    IRPluginComponentRenderer renderer = g_plugin_system.component_renderers[component_type];
    if (renderer == NULL) {
        fprintf(stderr, "[kryon][plugin] No renderer for component type %u (count=%u)\n",
                component_type, g_plugin_system.component_renderer_count);
        return false;
    }

    // Dispatch to renderer
    renderer(backend_ctx, component, x, y, width, height);
    g_plugin_system.components_rendered++;
    return true;
}

// ============================================================================
// Web Component Renderer Registration
// ============================================================================

bool ir_plugin_register_web_renderer(uint32_t component_type,
                                     IRPluginWebComponentRenderer renderer,
                                     IRPluginWebCSSGenerator css_gen) {
    if (!renderer) {
        fprintf(stderr, "[kryon][plugin] NULL web component renderer provided\n");
        return false;
    }

    if (component_type >= 64) {
        fprintf(stderr, "[kryon][plugin] Component type %u out of range for web renderer (max 63)\n", component_type);
        return false;
    }

    // Check if already registered
    if (g_plugin_system.web_renderers[component_type] != NULL) {
        fprintf(stderr, "[kryon][plugin] Component type %u already has a web renderer\n", component_type);
        return false;
    }

    // Register renderer and optional CSS generator
    g_plugin_system.web_renderers[component_type] = renderer;
    g_plugin_system.web_css_generators[component_type] = css_gen;  // Can be NULL
    g_plugin_system.web_renderer_count++;

    printf("[kryon][plugin] Registered web renderer for component type %u\n", component_type);
    return true;
}

void ir_plugin_unregister_web_renderer(uint32_t component_type) {
    if (component_type < 64 && g_plugin_system.web_renderers[component_type] != NULL) {
        g_plugin_system.web_renderers[component_type] = NULL;
        g_plugin_system.web_css_generators[component_type] = NULL;
        g_plugin_system.web_renderer_count--;
    }
}

bool ir_plugin_has_web_renderer(uint32_t component_type) {
    if (component_type >= 64) {
        return false;
    }
    return g_plugin_system.web_renderers[component_type] != NULL;
}

char* ir_plugin_render_web_component(const IRComponent* component, const char* theme) {
    if (!component) {
        return NULL;
    }

    uint32_t component_type = component->type;
    if (component_type >= 32) {
        return NULL;
    }

    IRPluginWebComponentRenderer renderer = g_plugin_system.web_renderers[component_type];
    if (renderer == NULL) {
        return NULL;
    }

    // Dispatch to renderer
    return renderer(component, theme ? theme : "light");
}

char* ir_plugin_get_web_css(uint32_t component_type, const char* theme) {
    if (component_type >= 32) {
        return NULL;
    }

    IRPluginWebCSSGenerator css_gen = g_plugin_system.web_css_generators[component_type];
    if (css_gen == NULL) {
        return NULL;
    }

    return css_gen(theme ? theme : "light");
}

// ============================================================================
// Callback Bridge Registration
// ============================================================================

bool ir_plugin_register_callback_bridge(uint32_t component_type, IRPluginCallbackBridge bridge) {
    if (!bridge || component_type >= 32) {
        return false;
    }

    if (g_plugin_system.callback_bridges[component_type] != NULL) {
        fprintf(stderr, "[kryon][plugin] Component type %u already has a callback bridge\n", component_type);
        return false;
    }

    g_plugin_system.callback_bridges[component_type] = bridge;
    g_plugin_system.callback_bridge_count++;
    printf("[kryon][plugin] Registered callback bridge for component type %u\n", component_type);
    return true;
}

void ir_plugin_unregister_callback_bridge(uint32_t component_type) {
    if (component_type < 32 && g_plugin_system.callback_bridges[component_type] != NULL) {
        g_plugin_system.callback_bridges[component_type] = NULL;
        g_plugin_system.callback_bridge_count--;
    }
}

bool ir_plugin_has_callback_bridge(uint32_t component_type) {
    return (component_type < 32) && (g_plugin_system.callback_bridges[component_type] != NULL);
}

bool ir_plugin_dispatch_callback(uint32_t component_type, uint32_t component_id) {
    if (component_type >= 32) {
        return false;
    }

    IRPluginCallbackBridge bridge = g_plugin_system.callback_bridges[component_type];
    if (!bridge) {
        return false;
    }

    bridge(component_id);
    return true;
}
// ============================================================================
// Dynamic Plugin Loading Implementation
// ============================================================================

#ifndef __ANDROID__
// Dynamic plugin loading not supported on Android
#include <dlfcn.h>
#include <dirent.h>
#include <glob.h>
#include <toml.h>
#include <sys/stat.h>

// Plugin search paths (in priority order)
static const char* PLUGIN_SEARCH_PATHS[] = {
    "~/.kryon/plugins/",
    "/usr/local/lib/kryon/plugins/",
    "/usr/lib/kryon/plugins/",
    "../kryon-plugin-*/build/",  // Development mode
    NULL
};

// Helper: Expand tilde in path (simplified, no wildcards for now)
static char** expand_path(const char* path, int* count) {
    *count = 0;

    // Handle tilde expansion manually
    if (path[0] == '~' && path[1] == '/') {
        const char* home = getenv("HOME");
        if (!home) {
            return NULL;
        }

        char* expanded = malloc(strlen(home) + strlen(path) + 1);
        sprintf(expanded, "%s%s", home, path + 1);

        char** result = malloc(sizeof(char*));
        result[0] = expanded;
        *count = 1;
        return result;
    }

    // For wildcard paths (like ../kryon-plugin-*/build/), use glob
    if (strchr(path, '*')) {
        glob_t glob_result;
        memset(&glob_result, 0, sizeof(glob_result));

        int ret = glob(path, GLOB_TILDE, NULL, &glob_result);
        if (ret != 0) {
            return NULL;
        }

        *count = glob_result.gl_pathc;
        char** paths = malloc(sizeof(char*) * *count);
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            paths[i] = strdup(glob_result.gl_pathv[i]);
        }

        globfree(&glob_result);
        return paths;
    }

    // Regular path - just duplicate it
    char** result = malloc(sizeof(char*));
    result[0] = strdup(path);
    *count = 1;
    return result;
}

// Helper: Check if file exists
static bool file_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

// Helper: Parse plugin.toml file
static bool parse_plugin_toml(const char* toml_path, IRPluginDiscoveryInfo* info) {
    FILE* fp = fopen(toml_path, "r");
    if (!fp) {
        return false;
    }

    // Read entire file into memory
    // Note: Using toml_parse() instead of toml_parse_file() because the latter
    // doesn't work correctly when called from within a shared library
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* file_contents = malloc(file_size + 1);
    if (!file_contents) {
        fclose(fp);
        return false;
    }

    size_t read_size = fread(file_contents, 1, file_size, fp);
    file_contents[read_size] = '\0';
    fclose(fp);

    char errbuf[200] = {0};
    toml_table_t* conf = toml_parse(file_contents, errbuf, sizeof(errbuf));
    free(file_contents);

    if (!conf) {
        fprintf(stderr, "[kryon][plugin] Error parsing %s: %s\n", toml_path, errbuf);
        return false;
    }

    // Parse [plugin] section
    toml_table_t* plugin_table = toml_table_in(conf, "plugin");
    if (!plugin_table) {
        toml_free(conf);
        return false;
    }

    // Get plugin name
    toml_datum_t name = toml_string_in(plugin_table, "name");
    if (name.ok) {
        strncpy(info->name, name.u.s, sizeof(info->name) - 1);
        free(name.u.s);
    }

    // Get plugin version
    toml_datum_t version = toml_string_in(plugin_table, "version");
    if (version.ok) {
        strncpy(info->version, version.u.s, sizeof(info->version) - 1);
        free(version.u.s);
    }

    // Parse [capabilities] section
    toml_table_t* caps_table = toml_table_in(conf, "capabilities");
    if (caps_table) {
        // Get command_ids array
        toml_array_t* cmd_ids = toml_array_in(caps_table, "command_ids");
        if (cmd_ids) {
            int cmd_count = toml_array_nelem(cmd_ids);
            info->command_count = cmd_count;
            info->command_ids = malloc(sizeof(uint32_t) * cmd_count);

            for (int i = 0; i < cmd_count; i++) {
                toml_datum_t id = toml_int_at(cmd_ids, i);
                if (id.ok) {
                    info->command_ids[i] = (uint32_t)id.u.i;
                }
            }
        }

        // Get backends array
        toml_array_t* backends = toml_array_in(caps_table, "backends");
        if (backends) {
            int backend_count = toml_array_nelem(backends);
            info->backend_count = backend_count;
            info->backends = malloc(sizeof(char*) * backend_count);

            for (int i = 0; i < backend_count; i++) {
                toml_datum_t backend = toml_string_at(backends, i);
                if (backend.ok) {
                    info->backends[i] = backend.u.s;  // Takes ownership
                }
            }
        }
    }

    toml_free(conf);
    return true;
}

// Discover plugins in a single directory
static void discover_in_directory(const char* dir_path, IRPluginDiscoveryInfo*** plugins, uint32_t* count) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Check for plugin.toml in subdirectory
            char toml_path[1024];
            snprintf(toml_path, sizeof(toml_path), "%s/%s/plugin.toml", dir_path, entry->d_name);

            if (file_exists(toml_path)) {
                IRPluginDiscoveryInfo* info = calloc(1, sizeof(IRPluginDiscoveryInfo));
                strncpy(info->toml_path, toml_path, sizeof(info->toml_path) - 1);

                if (parse_plugin_toml(toml_path, info)) {
                    // Find .so file in same directory
                    char so_path[1024];
                    snprintf(so_path, sizeof(so_path), "%s/%s/libkryon_%s.so", dir_path, entry->d_name, info->name);

                    if (file_exists(so_path)) {
                        strncpy(info->path, so_path, sizeof(info->path) - 1);

                        // Add to results
                        *plugins = realloc(*plugins, sizeof(IRPluginDiscoveryInfo*) * (*count + 1));
                        (*plugins)[*count] = info;
                        (*count)++;
                    } else {
                        // .so not found, free info
                        free(info->command_ids);
                        for (uint32_t i = 0; i < info->backend_count; i++) {
                            free(info->backends[i]);
                        }
                        free(info->backends);
                        free(info);
                    }
                } else {
                    free(info);
                }
            }
        }
    }

    closedir(dir);
}

IRPluginDiscoveryInfo** ir_plugin_discover(const char* search_path, uint32_t* count) {
    *count = 0;
    IRPluginDiscoveryInfo** plugins = NULL;

    if (search_path) {
        // Search in provided path only
        discover_in_directory(search_path, &plugins, count);
    } else {
        // Search in default paths
        for (int i = 0; PLUGIN_SEARCH_PATHS[i] != NULL; i++) {
            int expanded_count = 0;
            char** expanded_paths = expand_path(PLUGIN_SEARCH_PATHS[i], &expanded_count);

            if (expanded_paths) {
                for (int j = 0; j < expanded_count; j++) {
                    discover_in_directory(expanded_paths[j], &plugins, count);
                    free(expanded_paths[j]);
                }
                free(expanded_paths);
            }
        }
    }

    return plugins;
}

void ir_plugin_free_discovery(IRPluginDiscoveryInfo** plugins, uint32_t count) {
    if (!plugins) return;

    for (uint32_t i = 0; i < count; i++) {
        if (plugins[i]) {
            free(plugins[i]->command_ids);
            for (uint32_t j = 0; j < plugins[i]->backend_count; j++) {
                free(plugins[i]->backends[j]);
            }
            free(plugins[i]->backends);
            free(plugins[i]);
        }
    }
    free(plugins);
}

IRPluginHandle* ir_plugin_load(const char* plugin_path, const char* plugin_name) {
    return ir_plugin_load_with_metadata(plugin_path, plugin_name, NULL);
}

IRPluginHandle* ir_plugin_load_with_metadata(const char* plugin_path, const char* plugin_name,
                                              const IRPluginDiscoveryInfo* discovery_info) {
    // If discovery info is provided, register the plugin first
    if (discovery_info) {
        IRPluginMetadata metadata = {0};
        strncpy(metadata.name, discovery_info->name, sizeof(metadata.name) - 1);
        strncpy(metadata.version, discovery_info->version, sizeof(metadata.version) - 1);

        // Set command ID range from discovery info
        if (discovery_info->command_count > 0) {
            metadata.command_id_start = discovery_info->command_ids[0];
            metadata.command_id_end = discovery_info->command_ids[discovery_info->command_count - 1];
        }

        // Register the plugin with metadata
        if (!ir_plugin_register(&metadata)) {
            fprintf(stderr, "[kryon][plugin] Failed to register plugin '%s'\n", plugin_name);
            return NULL;
        }
    }

    IRPluginHandle* handle = calloc(1, sizeof(IRPluginHandle));
    if (!handle) {
        return NULL;
    }

    // Load shared library with RTLD_GLOBAL for symbol visibility to Lua FFI
    handle->dl_handle = dlopen(plugin_path, RTLD_LAZY | RTLD_GLOBAL);
    if (!handle->dl_handle) {
        fprintf(stderr, "[kryon][plugin] Error loading %s: %s\n", plugin_path, dlerror());
        free(handle);
        return NULL;
    }

    // Store plugin info
    strncpy(handle->name, plugin_name, sizeof(handle->name) - 1);
    strncpy(handle->path, plugin_path, sizeof(handle->path) - 1);
    if (discovery_info) {
        strncpy(handle->version, discovery_info->version, sizeof(handle->version) - 1);

        // Copy command IDs
        if (discovery_info->command_count > 0) {
            handle->command_count = discovery_info->command_count;
            handle->command_ids = calloc(discovery_info->command_count, sizeof(uint32_t));
            if (handle->command_ids) {
                memcpy(handle->command_ids, discovery_info->command_ids,
                       discovery_info->command_count * sizeof(uint32_t));
            }
        }
    }

    // Resolve init function (e.g., "kryon_canvas_plugin_init")
    char init_name[128];
    snprintf(init_name, sizeof(init_name), "kryon_%s_plugin_init", plugin_name);
    handle->init_func = dlsym(handle->dl_handle, init_name);

    if (!handle->init_func) {
        fprintf(stderr, "[kryon][plugin] Warning: Plugin %s has no init function (%s)\n", plugin_name, init_name);
    }

    // Resolve shutdown function
    char shutdown_name[128];
    snprintf(shutdown_name, sizeof(shutdown_name), "kryon_%s_plugin_shutdown", plugin_name);
    handle->shutdown_func = dlsym(handle->dl_handle, shutdown_name);

    if (!handle->shutdown_func) {
        fprintf(stderr, "[kryon][plugin] Warning: Plugin %s has no shutdown function (%s)\n", plugin_name, shutdown_name);
    }

    // Track this handle for symbol lookup
    if (g_plugin_system.loaded_handle_count < MAX_PLUGINS) {
        g_plugin_system.loaded_handles[g_plugin_system.loaded_handle_count++] = handle;
    }

    return handle;
}

void ir_plugin_unload(IRPluginHandle* handle) {
    if (!handle) return;

    if (handle->shutdown_func) {
        handle->shutdown_func();
    }

    if (handle->dl_handle) {
        dlclose(handle->dl_handle);
    }

    free(handle->command_ids);
    free(handle);
}

void* ir_plugin_get_symbol(const char* plugin_name, const char* symbol_name) {
    if (!plugin_name || !symbol_name) return NULL;

    // Find the loaded plugin by name
    for (uint32_t i = 0; i < g_plugin_system.loaded_handle_count; i++) {
        IRPluginHandle* handle = g_plugin_system.loaded_handles[i];
        if (handle && strcmp(handle->name, plugin_name) == 0) {
            // Found the plugin, look up the symbol
            void* sym = dlsym(handle->dl_handle, symbol_name);
            if (!sym) {
                fprintf(stderr, "[kryon][plugin] Symbol '%s' not found in plugin '%s': %s\n",
                        symbol_name, plugin_name, dlerror());
            }
            return sym;
        }
    }

    // Plugin not loaded
    return NULL;
}

bool ir_plugin_is_loaded(const char* plugin_name) {
    if (!plugin_name) return false;

    for (uint32_t i = 0; i < g_plugin_system.loaded_handle_count; i++) {
        IRPluginHandle* handle = g_plugin_system.loaded_handles[i];
        if (handle && strcmp(handle->name, plugin_name) == 0) {
            return true;
        }
    }
    return false;
}

const char* ir_plugin_find_by_capability(const char* symbol_name) {
    if (!symbol_name) return NULL;

    // Search through all loaded plugins for one that provides the symbol
    for (uint32_t i = 0; i < g_plugin_system.loaded_handle_count; i++) {
        IRPluginHandle* handle = g_plugin_system.loaded_handles[i];
        if (!handle) continue;

        // Check if this plugin has the symbol
        void* sym = dlsym(handle->dl_handle, symbol_name);
        if (sym) {
            // Found it - return the plugin name
            return handle->name;
        }
    }

    return NULL;
}

#else  // __ANDROID__
// Stub implementations for Android

IRPluginDiscoveryInfo** ir_plugin_discover(const char* search_path, uint32_t* count) {
    (void)search_path;
    *count = 0;
    return NULL;
}

void ir_plugin_free_discovery(IRPluginDiscoveryInfo** plugins, uint32_t count) {
    (void)plugins;
    (void)count;
}

IRPluginHandle* ir_plugin_load(const char* plugin_path, const char* plugin_name) {
    (void)plugin_path;
    (void)plugin_name;
    fprintf(stderr, "[kryon][plugin] Dynamic plugin loading not supported on Android\n");
    return NULL;
}

void ir_plugin_unload(IRPluginHandle* handle) {
    (void)handle;
}

void* ir_plugin_get_symbol(const char* plugin_name, const char* symbol_name) {
    (void)plugin_name;
    (void)symbol_name;
    return NULL;
}

bool ir_plugin_is_loaded(const char* plugin_name) {
    (void)plugin_name;
    return false;
}

#endif  // __ANDROID__

// Helper: Recursively scan IR tree for plugin commands
// NOTE: Currently disabled as IRComponent doesn't have plugin_command field yet.
// This will be implemented when plugin commands are added to the IR structure.
static void scan_commands_recursive(IRComponent* comp, uint32_t** used_commands, uint32_t* used_count) {
    if (!comp) return;

    // TODO: Implement plugin command scanning when IRComponent has plugin_command field
    // For now, just recursively scan children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        scan_commands_recursive(comp->children[i], used_commands, used_count);
    }
}

// Helper: Check if command is in range
static bool has_command_in_range(uint32_t* commands, uint32_t count, uint32_t start, uint32_t end) {
    for (uint32_t i = 0; i < count; i++) {
        if (commands[i] >= start && commands[i] <= end) {
            return true;
        }
    }
    return false;
}

char** ir_plugin_scan_requirements(IRComponent* root, uint32_t* count) {
    *count = 0;

    if (!root) {
        return NULL;
    }

    // Collect all unique plugin command types
    uint32_t* used_commands = NULL;
    uint32_t used_count = 0;
    scan_commands_recursive(root, &used_commands, &used_count);

    // Map command IDs to plugin names
    // Canvas: 100-102, Markdown: 200-202
    char** plugin_names = NULL;

    // Check if canvas commands are used
    if (has_command_in_range(used_commands, used_count, 100, 102)) {
        plugin_names = realloc(plugin_names, sizeof(char*) * (*count + 1));
        plugin_names[*count] = strdup("canvas");
        (*count)++;
    }

    // Check if markdown commands are used
    if (has_command_in_range(used_commands, used_count, 200, 202)) {
        plugin_names = realloc(plugin_names, sizeof(char*) * (*count + 1));
        plugin_names[*count] = strdup("markdown");
        (*count)++;
    }

    free(used_commands);
    return plugin_names;
}

void ir_plugin_free_requirements(char** plugin_names, uint32_t count) {
    if (!plugin_names) return;

    for (uint32_t i = 0; i < count; i++) {
        free(plugin_names[i]);
    }
    free(plugin_names);
}

// ============================================================================
// Plugin Requirements Storage (for deserialized IR files)
// ============================================================================

void ir_plugin_set_requirements(char** plugin_names, uint32_t count) {
    // Clear any existing requirements
    ir_plugin_clear_requirements();

    // Store new requirements
    g_plugin_system.current_requirements = plugin_names;
    g_plugin_system.current_requirement_count = count;

    if (count > 0) {
        fprintf(stderr, "[kryon][plugin] IR file requires %u plugin(s): ", count);
        for (uint32_t i = 0; i < count; i++) {
            fprintf(stderr, "%s%s", plugin_names[i], (i < count - 1) ? ", " : "");
        }
        fprintf(stderr, "\n");
    }
}

const char* const* ir_plugin_get_requirements(uint32_t* count) {
    if (count) {
        *count = g_plugin_system.current_requirement_count;
    }
    return (const char* const*)g_plugin_system.current_requirements;
}

void ir_plugin_clear_requirements(void) {
    if (g_plugin_system.current_requirements) {
        for (uint32_t i = 0; i < g_plugin_system.current_requirement_count; i++) {
            free(g_plugin_system.current_requirements[i]);
        }
        free(g_plugin_system.current_requirements);
        g_plugin_system.current_requirements = NULL;
        g_plugin_system.current_requirement_count = 0;
    }
}
