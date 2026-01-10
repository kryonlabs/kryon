/*
 * IR Component - Markdown and Sprite Layout Traits
 *
 * Provides layout computation for Markdown and Sprite components.
 */

#include "ir_component_misc.h"
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
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float font_size = 14.0f;
    float line_height = 1.5f;

    if (c->style) {
        font_size = (c->style->font.size > 0) ? c->style->font.size : 14.0f;
    }

    // Compute intrinsic width (constrained by parent)
    float intrinsic_width = constraints.max_width > 0 ? constraints.max_width : 800.0f;
    float intrinsic_height = font_size * line_height; // Minimum height for one line

    // If there's text content, estimate height
    if (c->text_content) {
        // Rough estimate: assume ~60 chars per line, count newlines
        int lines = 1;
        const char* p = c->text_content;
        while (*p) {
            if (*p == '\n') lines++;
            p++;
        }
        intrinsic_height = font_size * line_height * lines;
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

    if (getenv("KRYON_DEBUG_MARKDOWN")) {
        fprintf(stderr, "[Markdown] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
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
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get intrinsic size (may return 0 if not set)
    float intrinsic_width = ir_get_component_intrinsic_width(c);
    float intrinsic_height = ir_get_component_intrinsic_height(c);

    // Determine final size based on constraints and intrinsic size
    float final_width, final_height;

    if (intrinsic_width > 0 && intrinsic_height > 0) {
        // Use intrinsic size, but respect constraints
        final_width = intrinsic_width;
        final_height = intrinsic_height;

        // Apply max constraints
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
        // No intrinsic size - use constraint defaults or placeholder
        final_width = (constraints.max_width > 0) ? fminf(constraints.max_width, 32.0f) : 32.0f;
        final_height = (constraints.max_height > 0) ? fminf(constraints.max_height, 32.0f) : 32.0f;

        // Apply min constraints
        final_width = fmaxf(final_width, constraints.min_width);
        final_height = fmaxf(final_height, constraints.min_height);
    }

    // Apply explicit dimensions from style (overrides intrinsic size)
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            final_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            final_height = c->style->height.value;
        }
    }

    // Set final dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = final_width;
    c->layout_state->computed.height = final_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_SPRITE")) {
        fprintf(stderr, "[Sprite] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
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
