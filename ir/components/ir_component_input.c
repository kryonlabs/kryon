/*
 * IR Component - Input Layout Trait
 *
 * Provides layout computation for Input components (text fields, textareas).
 */

#include "ir_component_input.h"
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
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float font_size = 14.0f;
    IRSpacing padding = {0};

    if (c->style) {
        font_size = (c->style->font.size > 0) ? c->style->font.size : 14.0f;
        padding = c->style->padding;
    }

    // Compute intrinsic input dimensions
    float intrinsic_width = 150.0f;  // Default width for text input
    float intrinsic_height = font_size + padding.top + padding.bottom + 10.0f; // Input chrome

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
        intrinsic_width = fmaxf(150.0f, text_width + padding.left + padding.right + 16.0f);
    }

    // Start with intrinsic dimensions
    float final_width = intrinsic_width;
    float final_height = intrinsic_height;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            final_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            final_height = c->style->height.value;
        }
    }

    // Apply constraints
    final_width = fmaxf(final_width, constraints.min_width);
    final_height = fmaxf(final_height, constraints.min_height);

    if (constraints.max_width > 0) {
        final_width = fminf(final_width, constraints.max_width);
    }
    if (constraints.max_height > 0) {
        final_height = fminf(final_height, constraints.max_height);
    }

    // Set final dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = final_width;
    c->layout_state->computed.height = final_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_INPUT")) {
        fprintf(stderr, "[Input] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f (text='%s')\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height,
                c->text_content ? c->text_content : "");
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
