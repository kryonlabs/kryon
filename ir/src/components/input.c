/*
 * IR Component - Input Layout Trait
 *
 * Provides layout computation for Input components (text fields, textareas).
 */

#include "input.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Forward declarations for text measurement
extern float ir_get_text_width_estimate(const char* text, float font_size);

/**
 * Layout function for Input components.
 *
 * Input fields size based on their placeholder/value text, with explicit
 * width/height from style taking precedence.
 */
void layout_input_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get style values using helpers
    float font_size = layout_get_font_size(c, LAYOUT_DEFAULT_FONT_SIZE);
    IRSpacing padding = layout_get_padding(c);

    // Compute intrinsic input dimensions
    float intrinsic_width = LAYOUT_MIN_INPUT_WIDTH;
    float intrinsic_height = font_size + padding.top + padding.bottom + 10.0f;

    // If there's text content or placeholder, measure it
    const char* text_to_measure = c->text_content;
    if (!text_to_measure || text_to_measure[0] == '\0') {
        // Try to get placeholder from custom_data if available
        if (c->custom_data) {
            text_to_measure = (const char*)c->custom_data;
        }
    }

    if (text_to_measure) {
        // Estimate text width and add minimum for editability
        float text_width = ir_get_text_width_estimate(text_to_measure, font_size);
        intrinsic_width = fmaxf(LAYOUT_MIN_INPUT_WIDTH, text_width + padding.left + padding.right + 16.0f);
    }

    // Use full pipeline: compute with explicit dimensions, apply constraints, set final layout
    layout_compute_full_pipeline(c, constraints, parent_x, parent_y,
                                intrinsic_width, intrinsic_height);

    // Debug logging
    if (layout_is_debug_enabled("Input")) {
        char extra[128];
        snprintf(extra, sizeof(extra), "(text='%s')", c->text_content ? c->text_content : "");
        layout_debug_log("Input", c, extra);
    }
}

const IRLayoutTrait IR_INPUT_LAYOUT_TRAIT = {
    .layout_component = layout_input_single_pass,
    .name = "Input"
};

/**
 * Initialize Input component layout trait
 * Called from ir_layout_init_traits()
 */
void ir_input_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_INPUT, &IR_INPUT_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Input component initialized\n");
    }
}
