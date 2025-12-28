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

    // Use AndroidRenderer's text measurement API
    if (!android_renderer_measure_text(renderer, text, NULL, (int)font_size,
                                       out_width, out_height)) {
        // Fallback estimation if measurement fails
        if (out_width) *out_width = strlen(text) * font_size * 0.5f;
        if (out_height) *out_height = font_size * 1.2f;
    }
}

/**
 * Register text measurement callback with IR layout system.
 * Must be called during renderer initialization.
 */
void android_register_text_measurement(void) {
    ir_layout_set_text_measure_callback(android_text_measure_callback);
}
