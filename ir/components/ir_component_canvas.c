/*
 * IR Component - Canvas Layout Trait
 *
 * Provides layout computation for Canvas components.
 * Canvas components typically have explicit dimensions.
 */

#include "ir_component_canvas.h"
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
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Default canvas dimensions (same as HTML canvas default)
    float canvas_width = 300.0f;
    float canvas_height = 150.0f;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            canvas_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            canvas_height = c->style->height.value;
        }
    }

    // Apply min constraints
    canvas_width = fmaxf(canvas_width, constraints.min_width);
    canvas_height = fmaxf(canvas_height, constraints.min_height);

    // Apply max constraints
    if (constraints.max_width > 0) {
        canvas_width = fminf(canvas_width, constraints.max_width);
    }
    if (constraints.max_height > 0) {
        canvas_height = fminf(canvas_height, constraints.max_height);
    }

    // Set final dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = canvas_width;
    c->layout_state->computed.height = canvas_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_CANVAS")) {
        fprintf(stderr, "[Canvas] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
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
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Default canvas dimensions (same as HTML canvas default)
    float canvas_width = 300.0f;
    float canvas_height = 150.0f;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            canvas_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            canvas_height = c->style->height.value;
        }
    }

    // Apply min constraints
    canvas_width = fmaxf(canvas_width, constraints.min_width);
    canvas_height = fmaxf(canvas_height, constraints.min_height);

    // Apply max constraints
    if (constraints.max_width > 0) {
        canvas_width = fminf(canvas_width, constraints.max_width);
    }
    if (constraints.max_height > 0) {
        canvas_height = fminf(canvas_height, constraints.max_height);
    }

    // Set final dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = canvas_width;
    c->layout_state->computed.height = canvas_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_NATIVE_CANVAS")) {
        fprintf(stderr, "[NativeCanvas] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
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
