/*
 * IR Component - Modal Layout Trait
 *
 * Provides layout computation for Modal components.
 * Modals are overlays that typically center their content.
 */

#include "modal.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
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
    if (!layout_ensure_state(c)) return;

    // Default modal dimensions
    float modal_width = 400.0f;
    float modal_height = 300.0f;

    // Apply explicit dimensions from style
    float explicit_width, explicit_height;
    if (layout_get_explicit_width(c, &explicit_width)) {
        modal_width = explicit_width;
    }
    if (layout_get_explicit_height(c, &explicit_height)) {
        modal_height = explicit_height;
    }

    // Apply min constraints
    layout_apply_min_constraints(&modal_width, &modal_height, constraints);

    // Apply max constraints (with 90% cap for modals)
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
    layout_set_final(c, centered_x, centered_y, modal_width, modal_height);

    // Layout children with modal's content area
    if (c->child_count > 0) {
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

    // Debug logging
    if (layout_is_debug_enabled("Modal")) {
        char extra[64];
        snprintf(extra, sizeof(extra), "(children=%u)", c->child_count);
        layout_debug_log("Modal", c, extra);
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
