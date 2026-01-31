/*
 * IR Component - Image Layout Trait
 *
 * Provides layout computation for Image components using intrinsic size.
 */

#include "image.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Forward declarations for intrinsic size functions
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

/**
 * Layout function for Image components.
 *
 * Images use their intrinsic dimensions when available, or fall back to
 * the constraint sizes when no intrinsic size is set.
 */
void layout_image_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get intrinsic size (may return 0 if not set)
    float intrinsic_width = ir_get_component_intrinsic_width(c);
    float intrinsic_height = ir_get_component_intrinsic_height(c);

    // Determine final size based on constraints and intrinsic size
    float final_width, final_height;

    if (intrinsic_width > 0 && intrinsic_height > 0) {
        // Use intrinsic size, but respect constraints (maintain aspect ratio)
        final_width = intrinsic_width;
        final_height = intrinsic_height;

        // Apply max constraints (maintain aspect ratio)
        if (constraints.max_width > 0 && final_width > constraints.max_width) {
            float scale = constraints.max_width / final_width;
            final_width = constraints.max_width;
            final_height *= scale;
        }
        if (constraints.max_height > 0 && final_height > constraints.max_height) {
            float scale = constraints.max_height / final_height;
            final_height = constraints.max_height;
            final_width *= scale;
        }

        // Apply min constraints
        final_width = fmaxf(final_width, constraints.min_width);
        final_height = fmaxf(final_height, constraints.min_height);
    } else {
        // No intrinsic size - use constraint defaults or reasonable image placeholder size
        final_width = (constraints.max_width > 0) ? fminf(constraints.max_width, 200.0f) : 200.0f;
        final_height = (constraints.max_height > 0) ? fminf(constraints.max_height, 150.0f) : 150.0f;

        // Apply min constraints
        final_width = fmaxf(final_width, constraints.min_width);
        final_height = fmaxf(final_height, constraints.min_height);
    }

    // Apply explicit dimensions from style (overrides intrinsic size)
    float explicit_width, explicit_height;
    if (layout_get_explicit_width(c, &explicit_width)) {
        final_width = explicit_width;
    }
    if (layout_get_explicit_height(c, &explicit_height)) {
        final_height = explicit_height;
    }

    // Set final layout
    layout_set_final_with_parent(c, parent_x, parent_y, final_width, final_height);
}

const IRLayoutTrait IR_IMAGE_LAYOUT_TRAIT = {
    .layout_component = layout_image_single_pass,
    .name = "Image"
};

/**
 * Initialize Image component layout trait
 * Called from ir_layout_init_traits()
 */
void ir_image_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_IMAGE, &IR_IMAGE_LAYOUT_TRAIT);
}
