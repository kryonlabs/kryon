#include "ir_native_canvas.h"
#include "ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Global Callback Registry
// ============================================================================

// Maximum number of NativeCanvas components that can have callbacks
#define MAX_NATIVE_CANVAS_CALLBACKS 256

typedef struct {
    uint32_t component_id;
    void (*callback)(uint32_t);
    void* user_data;
    bool active;
} NativeCanvasCallbackEntry;

// Global callback registry
static NativeCanvasCallbackEntry g_canvas_callbacks[MAX_NATIVE_CANVAS_CALLBACKS];
static uint32_t g_callback_count = 0;
static bool g_registry_initialized = false;

// Global root component for component lookup by ID
static IRComponent* g_canvas_root = NULL;

// Initialize the callback registry
static void init_callback_registry(void) {
    if (g_registry_initialized) return;

    memset(g_canvas_callbacks, 0, sizeof(g_canvas_callbacks));
    g_callback_count = 0;
    g_registry_initialized = true;
}

// Find callback entry by component ID
static NativeCanvasCallbackEntry* find_callback_entry(uint32_t component_id) {
    for (uint32_t i = 0; i < g_callback_count; i++) {
        if (g_canvas_callbacks[i].active && g_canvas_callbacks[i].component_id == component_id) {
            return &g_canvas_callbacks[i];
        }
    }
    return NULL;
}

// Create or update callback entry
static NativeCanvasCallbackEntry* get_or_create_callback_entry(uint32_t component_id) {
    init_callback_registry();

    // Check if entry already exists
    NativeCanvasCallbackEntry* entry = find_callback_entry(component_id);
    if (entry) return entry;

    // Find first inactive slot
    for (uint32_t i = 0; i < MAX_NATIVE_CANVAS_CALLBACKS; i++) {
        if (!g_canvas_callbacks[i].active) {
            g_canvas_callbacks[i].component_id = component_id;
            g_canvas_callbacks[i].callback = NULL;
            g_canvas_callbacks[i].user_data = NULL;
            g_canvas_callbacks[i].active = true;

            if (i >= g_callback_count) {
                g_callback_count = i + 1;
            }

            return &g_canvas_callbacks[i];
        }
    }

    // Registry full
    fprintf(stderr, "[NativeCanvas] Callback registry full (max %d callbacks)\n",
            MAX_NATIVE_CANVAS_CALLBACKS);
    return NULL;
}

// ============================================================================
// Component Creation and Management
// ============================================================================

IRComponent* ir_create_native_canvas(uint16_t width, uint16_t height) {
    // Create component
    IRComponent* comp = ir_create_component(IR_COMPONENT_NATIVE_CANVAS);
    if (!comp) {
        fprintf(stderr, "[NativeCanvas] Failed to create component\n");
        return NULL;
    }

    // Allocate and initialize canvas data
    IRNativeCanvasData* data = (IRNativeCanvasData*)malloc(sizeof(IRNativeCanvasData));
    if (!data) {
        fprintf(stderr, "[NativeCanvas] Failed to allocate canvas data\n");
        // Note: We should free comp here, but ir_create_component doesn't provide a destroy function
        return NULL;
    }

    memset(data, 0, sizeof(IRNativeCanvasData));
    data->width = width;
    data->height = height;
    data->background_color = 0x000000FF; // Black with full alpha
    data->render_callback = NULL;
    data->user_data = NULL;

    comp->custom_data = (char*)data;

    // Ensure style exists and set dimensions
    ir_get_style(comp);  // Creates style if it doesn't exist

    // If width/height are 0, use 100% to fill parent
    // Otherwise use explicit pixel dimensions
    if (width == 0) {
        ir_set_width(comp, IR_DIMENSION_PERCENT, 100.0f);
    } else {
        ir_set_width(comp, IR_DIMENSION_PX, (float)width);
    }

    if (height == 0) {
        ir_set_height(comp, IR_DIMENSION_PERCENT, 100.0f);
    } else {
        ir_set_height(comp, IR_DIMENSION_PX, (float)height);
    }

    printf("[CANVAS] Created native canvas: %s width, %s height\n",
           width == 0 ? "100%" : "px", height == 0 ? "100%" : "px");

    return comp;
}

// ============================================================================
// Callback Management
// ============================================================================

void ir_native_canvas_set_callback(uint32_t component_id, void (*callback)(uint32_t)) {
    NativeCanvasCallbackEntry* entry = get_or_create_callback_entry(component_id);
    if (entry) {
        entry->callback = callback;
    }
}

void ir_native_canvas_set_user_data(uint32_t component_id, void* user_data) {
    NativeCanvasCallbackEntry* entry = get_or_create_callback_entry(component_id);
    if (entry) {
        entry->user_data = user_data;
    }
}

void* ir_native_canvas_get_user_data(uint32_t component_id) {
    NativeCanvasCallbackEntry* entry = find_callback_entry(component_id);
    return entry ? entry->user_data : NULL;
}

bool ir_native_canvas_invoke_callback(uint32_t component_id) {
    NativeCanvasCallbackEntry* entry = find_callback_entry(component_id);

    if (entry && entry->callback) {
        entry->callback(component_id);
        return true;
    }

    return false;
}

// ============================================================================
// Property Setters/Getters
// ============================================================================

// Set the root component for component lookup by ID
void ir_native_canvas_set_root(IRComponent* root) {
    g_canvas_root = root;
}

void ir_native_canvas_set_background_color(uint32_t component_id, uint32_t color) {
    if (!g_canvas_root) {
        fprintf(stderr, "[NativeCanvas] Error: Root component not set. Call ir_native_canvas_set_root() first.\n");
        return;
    }

    // Look up component by ID using the existing IR builder function
    IRComponent* component = ir_find_component_by_id(g_canvas_root, component_id);
    if (!component) {
        fprintf(stderr, "[NativeCanvas] Error: Component with ID %u not found\n", component_id);
        return;
    }

    if (component->type != IR_COMPONENT_NATIVE_CANVAS) {
        fprintf(stderr, "[NativeCanvas] Error: Component %u is not a NativeCanvas\n", component_id);
        return;
    }

    IRNativeCanvasData* data = ir_native_canvas_get_data(component);
    if (data) {
        data->background_color = color;
    }
}

IRNativeCanvasData* ir_native_canvas_get_data(IRComponent* component) {
    if (!component || component->type != IR_COMPONENT_NATIVE_CANVAS) {
        return NULL;
    }

    return (IRNativeCanvasData*)component->custom_data;
}

// ============================================================================
// Cleanup (optional, for future use)
// ============================================================================

/**
 * Remove callback entry for a component
 * This should be called when a NativeCanvas component is destroyed
 */
void ir_native_canvas_remove_callback(uint32_t component_id) {
    NativeCanvasCallbackEntry* entry = find_callback_entry(component_id);
    if (entry) {
        entry->active = false;
        entry->callback = NULL;
        entry->user_data = NULL;
    }
}

/**
 * Clear all callback entries
 * This can be called when shutting down the application
 */
void ir_native_canvas_clear_all_callbacks(void) {
    memset(g_canvas_callbacks, 0, sizeof(g_canvas_callbacks));
    g_callback_count = 0;
    g_registry_initialized = false;
}
