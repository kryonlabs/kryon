#include "dropdown.h"
#include "../include/ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declare text width estimation function
extern float ir_get_text_width_estimate(const char* text, float font_size);

void layout_dropdown_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float font_size = 14.0f;
    IRSpacing padding = {8, 8, 8, 8, 0};  // Default padding for dropdown

    if (c->style) {
        font_size = (c->style->font.size > 0) ? c->style->font.size : 14.0f;
        padding = c->style->padding;
    }

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

    // Determine final dimensions
    float final_width = intrinsic_width;
    float final_height = intrinsic_height;

    if (c->style) {
        // Apply explicit dimensions if specified
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

    // Respect constraints
    if (constraints.max_width > 0 && final_width > constraints.max_width) {
        final_width = constraints.max_width;
    }
    if (constraints.max_height > 0 && final_height > constraints.max_height) {
        final_height = constraints.max_height;
    }
    if (constraints.min_width > 0 && final_width < constraints.min_width) {
        final_width = constraints.min_width;
    }
    if (constraints.min_height > 0 && final_height < constraints.min_height) {
        final_height = constraints.min_height;
    }

    // Set computed dimensions
    c->layout_state->computed.width = final_width;
    c->layout_state->computed.height = final_height;

    // Determine position
    float container_x = parent_x;
    float container_y = parent_y;

    // Check for absolute positioning
    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        container_x = c->style->absolute_x;
        container_y = c->style->absolute_y;
    }

    // Set computed position
    c->layout_state->computed.x = container_x;
    c->layout_state->computed.y = container_y;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
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
