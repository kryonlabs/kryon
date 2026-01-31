#include "text.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
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

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
        fprintf(stderr, "[Text] Single-pass layout for text: '%s'\n",
                c->text_content ? c->text_content : "(null)");
    }

    // Get font size from style using helper
    float font_size = layout_get_font_size(c, 16.0f);

    // Evaluate text expression if present (for reactive text)
    char* evaluated_text = NULL;
    const char* text = NULL;

    if (c->text_expression && c->text_expression[0] != '\0') {
        // Need to evaluate text expression to get actual text for measurement
        // Declare the function here since we can't include ir_executor.h
        extern char* ir_executor_eval_text_expression(const char* expression, const char* scope);
        evaluated_text = ir_executor_eval_text_expression(c->text_expression, c->scope);
        text = evaluated_text ? evaluated_text : "";
    } else {
        text = c->text_content ? c->text_content : "";
    }

    // Determine width constraint for text measurement
    float available_width = constraints.max_width;

    // Apply explicit width if set
    float explicit_width;
    if (layout_get_explicit_width(c, &explicit_width)) {
        available_width = explicit_width;
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

    // Get padding using helper
    IRSpacing padding = layout_get_padding(c);

    float intrinsic_width = text_width + padding.left + padding.right;
    float intrinsic_height = text_height + padding.top + padding.bottom;

    // Use full pipeline: compute with explicit dimensions, apply constraints, set final layout
    // Note: text component uses 0,0 for relative position then adds parent offset
    layout_compute_full_pipeline(c, constraints, 0, 0,
                                intrinsic_width, intrinsic_height);

    // Convert to absolute coordinates (text special case)
    c->layout_state->computed.x += parent_x;
    c->layout_state->computed.y += parent_y;

    if (getenv("KRYON_DEBUG_TEXT_LAYOUT")) {
        fprintf(stderr, "[Text] Final layout: x=%.1f, y=%.1f, width=%.1f, height=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
    }

    // Free evaluated text if allocated
    if (evaluated_text) {
        free(evaluated_text);
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
