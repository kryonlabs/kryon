#include "ir_component_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// External text measurement callback (defined in ir_layout.c)
extern IRTextMeasureCallback g_text_measure_callback;

/**
 * Single-pass layout for Text components
 *
 * Algorithm:
 * 1. Get font size and text content
 * 2. Determine available width (from constraints or explicit width)
 * 3. Measure text with wrapping using backend callback
 * 4. Set final dimensions
 * 5. Convert to absolute coordinates
 */
void layout_text_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                             float parent_x, float parent_y) {
    if (!c) return;

    if (getenv("KRYON_DEBUG_TAB_LAYOUT")) {
        fprintf(stderr, "[TEXT_LAYOUT_CALL] ID=%u text='%s' parent_pos=(%.1f, %.1f) constraints=(%.1f x %.1f)\n",
                c->id, c->text_content ? c->text_content : "(null)",
                parent_x, parent_y, constraints.max_width, constraints.max_height);
    }

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
        fprintf(stderr, "[Text] Single-pass layout for text: '%s'\n",
                c->text_content ? c->text_content : "(null)");
    }

    // Get font size from style
    float font_size = (c->style && c->style->font.size > 0) ? c->style->font.size : 16.0f;
    const char* text = c->text_content ? c->text_content : "";

    // Determine width constraint for text measurement
    float available_width = constraints.max_width;

    // Apply explicit width if set
    if (c->style && c->style->width.type == IR_DIMENSION_PX) {
        available_width = c->style->width.value;
    }

    // Apply max_width constraint if set
    if (c->style && c->style->text_effect.max_width.type == IR_DIMENSION_PX) {
        float text_max_width = c->style->text_effect.max_width.value;
        if (text_max_width > 0 && text_max_width < available_width) {
            available_width = text_max_width;
        }
    }

    // Measure text with wrapping
    float text_width = 0;
    float text_height = 0;

    if (g_text_measure_callback) {
        // Use backend measurement (accurate multi-line text sizing)
        g_text_measure_callback(text, font_size, available_width, &text_width, &text_height);

        if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
            fprintf(stderr, "[Text] Measured text with max_width=%.1f -> width=%.1f, height=%.1f\n",
                    available_width, text_width, text_height);
        }
    } else {
        // Heuristic fallback (no backend available)
        float char_width = font_size * 0.5f;
        size_t len = strlen(text);
        text_width = len * char_width;
        text_height = font_size * 1.2f;  // Single line height

        if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
            fprintf(stderr, "[Text] Using fallback measurement (no backend)\n");
        }
    }

    // Add padding if present
    IRSpacing padding = {0};
    if (c->style) {
        padding = c->style->padding;
    }

    float final_width = text_width + padding.left + padding.right;
    float final_height = text_height + padding.top + padding.bottom;

    // Apply explicit dimensions if set (override measured dimensions)
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            final_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            final_height = c->style->height.value;
        }
    }

    // Apply min/max constraints
    final_width = fmaxf(final_width, constraints.min_width);
    final_height = fmaxf(final_height, constraints.min_height);

    if (constraints.max_width > 0) {
        final_width = fminf(final_width, constraints.max_width);
    }
    if (constraints.max_height > 0) {
        final_height = fminf(final_height, constraints.max_height);
    }

    // Set final computed dimensions (relative position first)
    c->layout_state->computed.x = 0;
    c->layout_state->computed.y = 0;
    c->layout_state->computed.width = final_width;
    c->layout_state->computed.height = final_height;

    // Convert to absolute coordinates
    c->layout_state->computed.x += parent_x;
    c->layout_state->computed.y += parent_y;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
        fprintf(stderr, "[Text] Final layout: x=%.1f, y=%.1f, width=%.1f, height=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
    }
}

// Text component layout trait
const IRLayoutTrait IR_TEXT_LAYOUT_TRAIT = {
    .layout_component = layout_text_single_pass,
    .name = "Text"
};

// Initialize and register Text component trait
void ir_text_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_TEXT, &IR_TEXT_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Text component trait initialized\n");
    }
}
