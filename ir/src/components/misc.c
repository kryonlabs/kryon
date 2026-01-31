/*
 * IR Component - Markdown and Sprite Layout Traits
 *
 * Provides layout computation for Markdown and Sprite components.
 */

#include "misc.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Forward declarations for intrinsic size functions
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

// ============================================================================
// Markdown Layout Trait
// ============================================================================

/**
 * Layout function for Markdown components.
 *
 * Markdown components display rich text and use constraints similar to Text.
 */
void layout_markdown_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get font size using helper
    float font_size = layout_get_font_size(c, 14.0f);
    float line_height = 1.5f;

    // Compute intrinsic width (constrained by parent)
    float intrinsic_width = constraints.max_width > 0 ? constraints.max_width : 800.0f;
    float intrinsic_height = font_size * line_height;

    // If there's text content, estimate height
    if (c->text_content) {
        // Rough estimate: count newlines
        int lines = 1;
        const char* p = c->text_content;
        while (*p) {
            if (*p == '\n') lines++;
            p++;
        }
        intrinsic_height = font_size * line_height * lines;
    }

    // Use full pipeline: compute with explicit dimensions, apply constraints, set final layout
    layout_compute_full_pipeline(c, constraints, parent_x, parent_y,
                                intrinsic_width, intrinsic_height);

    // Debug logging
    if (layout_is_debug_enabled("Markdown")) {
        layout_debug_log("Markdown", c, NULL);
    }
}

const IRLayoutTrait IR_MARKDOWN_LAYOUT_TRAIT = {
    .layout_component = layout_markdown_single_pass,
    .name = "Markdown"
};

// ============================================================================
// Sprite Layout Trait
// ============================================================================

/**
 * Layout function for Sprite components.
 *
 * Sprites use their intrinsic dimensions (from sprite atlas).
 */
void layout_sprite_single_pass(IRComponent* c, IRLayoutConstraints constraints,
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
        // Use intrinsic size, but respect constraints
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
        // No intrinsic size - use constraint defaults or placeholder (32x32)
        final_width = (constraints.max_width > 0) ? fminf(constraints.max_width, 32.0f) : 32.0f;
        final_height = (constraints.max_height > 0) ? fminf(constraints.max_height, 32.0f) : 32.0f;

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

    // Debug logging
    if (layout_is_debug_enabled("Sprite")) {
        layout_debug_log("Sprite", c, NULL);
    }
}

const IRLayoutTrait IR_SPRITE_LAYOUT_TRAIT = {
    .layout_component = layout_sprite_single_pass,
    .name = "Sprite"
};

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize Markdown and Sprite component layout traits
 * Called from ir_layout_init_traits()
 */
void ir_misc_components_init(void) {
    ir_layout_register_trait(IR_COMPONENT_MARKDOWN, &IR_MARKDOWN_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_SPRITE, &IR_SPRITE_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Misc components (Markdown, Sprite) initialized\n");
    }
}
