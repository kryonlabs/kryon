#ifndef DEBUG_BACKEND_H
#define DEBUG_BACKEND_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stdio.h>

// Debug output modes
typedef enum {
    DEBUG_OUTPUT_STDOUT,    // Print to stdout
    DEBUG_OUTPUT_FILE,      // Write to file
    DEBUG_OUTPUT_BUFFER     // Store in buffer for retrieval
} DebugOutputMode;

// Debug renderer configuration
typedef struct DebugRendererConfig {
    DebugOutputMode output_mode;
    const char* output_file;    // For FILE mode
    int max_depth;              // Max tree depth to print (-1 = unlimited)
    bool show_style;            // Include style info
    bool show_bounds;           // Include rendered bounds
    bool show_hidden;           // Include hidden components
    bool compact;               // Compact output (single line per component)
} DebugRendererConfig;

// Debug renderer state
typedef struct DebugRenderer {
    DebugRendererConfig config;
    FILE* output;
    char* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    uint32_t frame_count;
} DebugRenderer;

// Initialize debug renderer with configuration
DebugRenderer* debug_renderer_create(const DebugRendererConfig* config);

// Destroy debug renderer and free resources
void debug_renderer_destroy(DebugRenderer* renderer);

// Render IR tree as text
void debug_render_tree(DebugRenderer* renderer, IRComponent* root);

// Render single frame info (header + tree + footer)
void debug_render_frame(DebugRenderer* renderer, IRComponent* root);

// Get buffered output (if mode is BUFFER)
const char* debug_get_output(DebugRenderer* renderer);

// Clear output buffer
void debug_clear_output(DebugRenderer* renderer);

// Print component type as string
const char* debug_component_type_str(IRComponentType type);

// Print dimension as string
void debug_dimension_to_str(IRDimension dim, char* buffer, size_t size);

// Default configuration
DebugRendererConfig debug_renderer_config_default(void);

// Convenience: render tree to stdout
void debug_print_tree(IRComponent* root);

// Convenience: render tree to file
void debug_print_tree_to_file(IRComponent* root, const char* filename);

#endif // DEBUG_BACKEND_H
