#include "checkbox.h"
#include "../include/ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void layout_checkbox_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float font_size = 14.0f;
    IRSpacing padding = {0};

    if (c->style) {
        font_size = (c->style->font.size > 0) ? c->style->font.size : 14.0f;
        padding = c->style->padding;
    }

    // Checkbox box size (typically 16-20px square)
    float checkbox_size = 20.0f;
    float gap_between = 8.0f; // Gap between checkbox and label

    // Compute intrinsic dimensions
    float intrinsic_width = checkbox_size + gap_between;
    float intrinsic_height = fmaxf(checkbox_size, font_size);

    // If there's label text, add its width
    if (c->text_content) {
        float text_width = ir_get_text_width_estimate(c->text_content, font_size);
        intrinsic_width += text_width;
    }

    // Add padding
    intrinsic_width += padding.left + padding.right;
    intrinsic_height += padding.top + padding.bottom;

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

    // Determine position (check for absolute positioning)
    float pos_x = parent_x;
    float pos_y = parent_y;

    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        pos_x = c->style->absolute_x;
        pos_y = c->style->absolute_y;
    }

    // Set final dimensions and position
    c->layout_state->computed.x = pos_x;
    c->layout_state->computed.y = pos_y;
    c->layout_state->computed.width = final_width;
    c->layout_state->computed.height = final_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_CHECKBOX")) {
        fprintf(stderr, "[Checkbox] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f (label='%s')\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height,
                c->text_content ? c->text_content : "");
    }
}

const IRLayoutTrait IR_CHECKBOX_LAYOUT_TRAIT = {
    .layout_component = layout_checkbox_single_pass,
    .name = "Checkbox"
};

void ir_checkbox_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_CHECKBOX, &IR_CHECKBOX_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Checkbox component initialized\n");
    }
}
