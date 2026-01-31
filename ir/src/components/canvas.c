/*
 * IR Component - Canvas Layout Trait
 *
 * Provides layout computation for Canvas components.
 * Canvas components typically have explicit dimensions.
 */

#include "canvas.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/**
 * Layout function for Canvas components.
 *
 * Canvas uses explicit dimensions from style, or falls back to
 * reasonable defaults (300x150 is the HTML canvas default).
 */
void layout_canvas_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Default canvas dimensions (same as HTML canvas default)
    float canvas_width = 300.0f;
    float canvas_height = 150.0f;

    // Apply explicit dimensions from style (with percentage support)
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PERCENT) {
            canvas_width = constraints.max_width * (c->style->width.value / 100.0f);
        } else if (c->style->width.type == IR_DIMENSION_PX) {
            canvas_width = c->style->width.value;
        }

        if (c->style->height.type == IR_DIMENSION_PERCENT) {
            canvas_height = constraints.max_height * (c->style->height.value / 100.0f);
        } else if (c->style->height.type == IR_DIMENSION_PX) {
            canvas_height = c->style->height.value;
        }
    }

    // Apply constraints
    layout_apply_constraints(&canvas_width, &canvas_height, constraints);

    // Set final layout
    layout_set_final_with_parent(c, parent_x, parent_y, canvas_width, canvas_height);

    // Debug logging
    if (layout_is_debug_enabled("Canvas")) {
        layout_debug_log("Canvas", c, NULL);
    }
}

const IRLayoutTrait IR_CANVAS_LAYOUT_TRAIT = {
    .layout_component = layout_canvas_single_pass,
    .name = "Canvas"
};

/**
 * Layout function for NativeCanvas components.
 * Same layout logic as Canvas.
 */
void layout_native_canvas_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                       float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Default canvas dimensions
    float canvas_width = 300.0f;
    float canvas_height = 150.0f;

    // Apply explicit dimensions from style (with percentage support)
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PERCENT) {
            canvas_width = constraints.max_width * (c->style->width.value / 100.0f);
        } else if (c->style->width.type == IR_DIMENSION_PX) {
            canvas_width = c->style->width.value;
        }

        if (c->style->height.type == IR_DIMENSION_PERCENT) {
            canvas_height = constraints.max_height * (c->style->height.value / 100.0f);
        } else if (c->style->height.type == IR_DIMENSION_PX) {
            canvas_height = c->style->height.value;
        }
    }

    // Apply constraints
    layout_apply_constraints(&canvas_width, &canvas_height, constraints);

    // Set final layout
    layout_set_final_with_parent(c, parent_x, parent_y, canvas_width, canvas_height);

    // Debug logging
    if (layout_is_debug_enabled("NativeCanvas")) {
        layout_debug_log("NativeCanvas", c, NULL);
    }
}

const IRLayoutTrait IR_NATIVE_CANVAS_LAYOUT_TRAIT = {
    .layout_component = layout_native_canvas_single_pass,
    .name = "NativeCanvas"
};

/**
 * Initialize Canvas component layout traits
 * Called from ir_layout_init_traits()
 */
void ir_canvas_components_init(void) {
    ir_layout_register_trait(IR_COMPONENT_CANVAS, &IR_CANVAS_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_NATIVE_CANVAS, &IR_NATIVE_CANVAS_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Canvas components initialized\n");
    }
}
