/*
 * Android Layout Integration
 * Layout computation is handled by ir_layout_compute_tree() in IR layer.
 * This file provides the text measurement callback for the layout system.
 */

#include "android_internal.h"
#include <string.h>

// Global renderer reference for text measurement callback
static AndroidIRRenderer* g_android_ir_renderer = NULL;

void android_layout_set_renderer(AndroidIRRenderer* renderer) {
    g_android_ir_renderer = renderer;
}

/**
 * Text measurement callback for IR layout system.
 * Called during ir_layout_compute_tree() to measure text dimensions.
 *
 * IMPORTANT: Must use the same font size scaling as android_rendering.c
 * to ensure layout matches rendering.
 */
static void android_text_measure_callback(const char* text, float font_size,
                                          float max_width,
                                          float* out_width, float* out_height) {
    if (!text || !g_android_ir_renderer || !g_android_ir_renderer->renderer) {
        // Fallback: return estimated dimensions
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    AndroidRenderer* renderer = g_android_ir_renderer->renderer;

    // Apply the same font size scaling as android_rendering.c
    // density * 1.5 mobile boost multiplier
    float density = android_renderer_get_density(renderer);
    int font_size_scaled = (int)(font_size * density * 1.5f + 0.5f);

    // Use AndroidRenderer's text measurement API with scaled font size
    if (!android_renderer_measure_text(renderer, text, NULL, font_size_scaled,
                                       out_width, out_height)) {
        // Fallback estimation if measurement fails
        if (out_width) *out_width = strlen(text) * font_size * 0.5f;
        if (out_height) *out_height = font_size * 1.2f;
    }

    // Convert the measurement back to logical pixels for layout
    // (rendering will scale up again when drawing)
    if (density > 0.0f) {
        if (out_width) *out_width /= density;
        if (out_height) *out_height /= density;
    }
}

/**
 * Register text measurement callback with IR layout system.
 * Must be called during renderer initialization.
 */
void android_register_text_measurement(void) {
    ir_layout_set_text_measure_callback(android_text_measure_callback);
}
