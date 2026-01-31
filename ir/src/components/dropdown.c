#include "dropdown.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declare text width estimation function
extern float ir_get_text_width_estimate(const char* text, float font_size);

void layout_dropdown_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get style values using helpers
    float font_size = layout_get_font_size(c, LAYOUT_DEFAULT_FONT_SIZE);
    IRSpacing padding = layout_get_padding(c);

    // Get dropdown state to check for text
    IRDropdownState* dropdown_state = (IRDropdownState*)c->custom_data;
    const char* display_text = NULL;

    // Determine what text to display
    if (dropdown_state) {
        if (dropdown_state->selected_index >= 0 &&
            dropdown_state->selected_index < (int32_t)dropdown_state->option_count &&
            dropdown_state->options) {
            // Display selected option
            display_text = dropdown_state->options[dropdown_state->selected_index];
        } else if (dropdown_state->placeholder) {
            // Display placeholder
            display_text = dropdown_state->placeholder;
        }
    }

    // Estimate text width
    float text_width = 0.0f;
    if (display_text) {
        text_width = ir_get_text_width_estimate(display_text, font_size);
    }

    // Add space for dropdown arrow indicator (typically ~20px)
    float arrow_space = 20.0f;

    // Compute intrinsic dimensions
    float intrinsic_width = padding.left + text_width + arrow_space + padding.right;
    float intrinsic_height = padding.top + font_size + padding.bottom;

    // Start with intrinsic dimensions
    float final_width = intrinsic_width;
    float final_height = intrinsic_height;

    // Apply explicit dimensions (including percentage support)
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            final_width = c->style->width.value;
        } else if (c->style->width.type == IR_DIMENSION_PERCENT) {
            final_width = constraints.max_width * (c->style->width.value / 100.0f);
        }

        if (c->style->height.type == IR_DIMENSION_PX) {
            final_height = c->style->height.value;
        } else if (c->style->height.type == IR_DIMENSION_PERCENT) {
            final_height = constraints.max_height * (c->style->height.value / 100.0f);
        }
    }

    // Apply constraints
    layout_apply_constraints(&final_width, &final_height, constraints);

    // Determine position (check for absolute positioning)
    float pos_x = parent_x;
    float pos_y = parent_y;

    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        pos_x = c->style->absolute_x;
        pos_y = c->style->absolute_y;
    }

    // Set final layout
    layout_set_final(c, pos_x, pos_y, final_width, final_height);
}

// Define the layout trait
const IRLayoutTrait IR_DROPDOWN_LAYOUT_TRAIT = {
    .name = "Dropdown",
    .layout_component = layout_dropdown_single_pass
};

// Initialize and register the dropdown component
void ir_dropdown_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_DROPDOWN, &IR_DROPDOWN_LAYOUT_TRAIT);
}
