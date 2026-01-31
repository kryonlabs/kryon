/**
 * @file strategy_text.c
 * @brief Layout strategy for Text components
 */

#define _GNU_SOURCE
#include "../ir_layout_strategy.h"
#include "../include/ir_core.h"
#include "ir_text_shaping.h"
#include "../layout_helpers.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration from ir_layout.c
extern float ir_get_text_width_estimate(const char* text, float font_size);

static void measure_text(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!layout_ensure_state(comp)) return;

    // Get font size
    float font_size = layout_get_font_size(comp, 16.0f);

    // Estimate text size
    float width = 0;
    float line_height = font_size * 1.2f;  // Default line height
    float height = line_height;

    if (comp->text_content) {
        // Check if max_width is set for text wrapping
        float max_width = 0.0f;
        if (comp->style && comp->style->text_effect.max_width.type == IR_DIMENSION_PX) {
            max_width = comp->style->text_effect.max_width.value;
        }

        // If max_width is set, compute text layout and use it
        if (max_width > 0) {
            // Compute text layout if not cached or if dirty
            if (!comp->text_layout || comp->layout_cache.dirty) {
                ir_text_layout_compute(comp, max_width, font_size);
            }

            if (comp->text_layout) {
                width = ir_text_layout_get_width(comp->text_layout);
                height = ir_text_layout_get_height(comp->text_layout);
            }
        } else {
            // Single-line estimation
            width = ir_get_text_width_estimate(comp->text_content, font_size);
        }
    }

    // Apply constraints
    layout_apply_constraints(&width, &height, *constraints);

    comp->layout_state->computed.width = width;
    comp->layout_state->computed.height = height;
    comp->layout_state->layout_valid = true;
}

static void layout_text_children(IRComponent* comp) {
    // Text has no children
    (void)comp;
}

static void intrinsic_text_size(IRComponent* comp, float* width, float* height) {
    if (!comp || !width || !height) return;

    float font_size = layout_get_font_size(comp, 16.0f);

    *width = 0;
    *height = font_size * 1.2f;

    if (comp->text_content) {
        *width = ir_get_text_width_estimate(comp->text_content, font_size);
    }
}

void ir_register_text_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_TEXT,
        .measure = measure_text,
        .layout_children = layout_text_children,
        .intrinsic_size = intrinsic_text_size
    };

    ir_layout_register_strategy(&strategy);
}
