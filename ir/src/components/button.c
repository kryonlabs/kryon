#include "button.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void layout_button_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get style values using helpers
    float font_size = layout_get_font_size(c, LAYOUT_DEFAULT_FONT_SIZE);
    IRSpacing padding = layout_get_padding(c);

    // Compute intrinsic button dimensions
    float intrinsic_width = LAYOUT_MIN_BUTTON_WIDTH;
    float intrinsic_height = font_size + padding.top + padding.bottom + LAYOUT_FORM_CHROME_PADDING;

    // If there's text content, measure it
    if (c->text_content) {
        float text_width = ir_get_text_width_estimate(c->text_content, font_size);
        intrinsic_width = text_width + padding.left + padding.right + (LAYOUT_FORM_CHROME_PADDING + 8.0f);
    }

    // Use full pipeline: compute with explicit dimensions, apply constraints, set final layout
    layout_compute_full_pipeline(c, constraints, parent_x, parent_y,
                                intrinsic_width, intrinsic_height);

    // Debug logging
    if (layout_is_debug_enabled("Button")) {
        char extra[128];
        snprintf(extra, sizeof(extra), "(text='%s')", c->text_content ? c->text_content : "");
        layout_debug_log("Button", c, extra);
    }
}

const IRLayoutTrait IR_BUTTON_LAYOUT_TRAIT = {
    .layout_component = layout_button_single_pass,
    .name = "Button"
};

void ir_button_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_BUTTON, &IR_BUTTON_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Button component initialized\n");
    }
}
