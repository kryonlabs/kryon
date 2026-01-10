/*
 * IR Component - Modal Layout Trait
 *
 * Provides layout computation for Modal components.
 * Modals are overlays that typically center their content.
 */

#include "ir_component_modal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Forward declaration for dispatch
extern void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

/**
 * Layout function for Modal components.
 *
 * Modals typically overlay their content, centered within the available space.
 * They often have a backdrop/overlay behind them.
 */
void layout_modal_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float modal_width = 400.0f;  // Default modal width
    float modal_height = 300.0f; // Default modal height

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            modal_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            modal_height = c->style->height.value;
        }
    }

    // Apply constraints (but maintain reasonable minimums for modals)
    modal_width = fmaxf(modal_width, constraints.min_width);
    modal_height = fmaxf(modal_height, constraints.min_height);

    if (constraints.max_width > 0 && modal_width > constraints.max_width) {
        modal_width = fmaxf(constraints.max_width * 0.9f, constraints.min_width);
    }
    if (constraints.max_height > 0 && modal_height > constraints.max_height) {
        modal_height = fmaxf(constraints.max_height * 0.9f, constraints.min_height);
    }

    // Center the modal within the available space
    float centered_x = parent_x + (constraints.max_width - modal_width) / 2.0f;
    float centered_y = parent_y + (constraints.max_height - modal_height) / 2.0f;

    // Set modal's dimensions and position
    c->layout_state->computed.x = centered_x;
    c->layout_state->computed.y = centered_y;
    c->layout_state->computed.width = modal_width;
    c->layout_state->computed.height = modal_height;

    // Layout children with modal's content area
    if (c->child_count > 0) {
        // Create constraints for modal content
        IRLayoutConstraints content_constraints = {
            .max_width = modal_width,
            .max_height = modal_height,
            .min_width = 0.0f,
            .min_height = 0.0f
        };

        for (uint32_t i = 0; i < c->child_count; i++) {
            if (c->children[i]) {
                ir_layout_dispatch(c->children[i], content_constraints, centered_x, centered_y);
            }
        }
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_MODAL")) {
        fprintf(stderr, "[Modal] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f (children=%u)\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height,
                c->child_count);
    }
}

const IRLayoutTrait IR_MODAL_LAYOUT_TRAIT = {
    .layout_component = layout_modal_single_pass,
    .name = "Modal"
};

/**
 * Initialize Modal component layout trait
 * Called from ir_layout_init_traits()
 */
void ir_modal_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_MODAL, &IR_MODAL_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Modal component initialized\n");
    }
}
