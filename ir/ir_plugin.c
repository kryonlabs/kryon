/*
 * Kryon Plugin System Implementation
 */

#include "ir_plugin.h"
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

typedef struct {
    // Handler table (256 entries, only 100-255 used for plugins)
    IRPluginHandler handlers[256];
    uint32_t registered_count;

    // Component renderer table (indexed by IRComponentType)
    IRPluginComponentRenderer component_renderers[32];  // Support up to 32 component types
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
} PluginSystem;

static PluginSystem g_plugin_system = {0};

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

    // Check command ID range validity
    if (metadata->command_id_start < IR_PLUGIN_CMD_START ||
        metadata->command_id_end > IR_PLUGIN_CMD_END ||
        metadata->command_id_start > metadata->command_id_end) {
        fprintf(stderr, "[kryon][plugin] Plugin '%s' has invalid command ID range (%u-%u)\n",
                metadata->name, metadata->command_id_start, metadata->command_id_end);
        return false;
    }

    // Check for command ID conflicts
    char conflict_buf[IR_PLUGIN_NAME_MAX];
    if (ir_plugin_check_command_conflict(metadata->command_id_start, metadata->command_id_end,
                                         conflict_buf, sizeof(conflict_buf))) {
        fprintf(stderr, "[kryon][plugin] Plugin '%s' command ID range conflicts with '%s'\n",
                metadata->name, conflict_buf);
        return false;
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

    if (component_type >= 32) {
        fprintf(stderr, "[kryon][plugin] Component type %u out of range (max 31)\n", component_type);
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
    if (component_type < 32 && g_plugin_system.component_renderers[component_type] != NULL) {
        g_plugin_system.component_renderers[component_type] = NULL;
        g_plugin_system.component_renderer_count--;
    }
}

bool ir_plugin_has_component_renderer(uint32_t component_type) {
    if (component_type >= 32) {
        return false;
    }
    return g_plugin_system.component_renderers[component_type] != NULL;
}

bool ir_plugin_dispatch_component_render(void* backend_ctx, uint32_t component_type,
                                         const IRComponent* component,
                                         float x, float y, float width, float height) {
    if (component_type >= 32) {
        return false;
    }

    IRPluginComponentRenderer renderer = g_plugin_system.component_renderers[component_type];
    if (renderer == NULL) {
        return false;
    }

    // Dispatch to renderer
    renderer(backend_ctx, component, x, y, width, height);
    g_plugin_system.components_rendered++;
    return true;
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
