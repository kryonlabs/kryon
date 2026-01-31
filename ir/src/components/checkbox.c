#include "checkbox.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void layout_checkbox_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get style values using helpers
    float font_size = layout_get_font_size(c, LAYOUT_DEFAULT_FONT_SIZE);
    IRSpacing padding = layout_get_padding(c);

    // Checkbox box size (typically 16-20px square)
    float checkbox_size = 20.0f;
    float gap_between = 8.0f;

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

    // Determine position (check for absolute positioning)
    float pos_x = parent_x;
    float pos_y = parent_y;

    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        pos_x = c->style->absolute_x;
        pos_y = c->style->absolute_y;
    }

    // Use full pipeline: compute with explicit dimensions, apply constraints, set final layout
    layout_compute_full_pipeline(c, constraints, pos_x, pos_y,
                                intrinsic_width, intrinsic_height);

    // Debug logging
    if (layout_is_debug_enabled("Checkbox")) {
        char extra[128];
        snprintf(extra, sizeof(extra), "(label='%s')", c->text_content ? c->text_content : "");
        layout_debug_log("Checkbox", c, extra);
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
