/*
 * Kryon Plugin System Implementation
 */

#include "ir_plugin.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

typedef struct {
    // Handler table (256 entries, only 100-255 used for plugins)
    IRPluginHandler handlers[256];
    uint32_t registered_count;

    // Backend capabilities
    IRBackendCapabilities backend_caps;
    bool caps_initialized;

    // Statistics
    uint32_t commands_dispatched;
    uint32_t unknown_commands;
} PluginSystem;

static PluginSystem g_plugin_system = {0};

// ============================================================================
// Plugin Registration
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
}
