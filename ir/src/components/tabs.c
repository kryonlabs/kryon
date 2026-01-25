/*
#include "../include/ir_core.h"
 * IR Component - Tab Layout Traits
 *
 * Provides layout computation for Tab components (TabGroup, TabBar, Tab, TabContent, TabPanel).
 */

#include "tabs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declaration for dispatch
extern void ir_layout_dispatch(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y);

// Default tab height
#define DEFAULT_TAB_HEIGHT 36.0f
#define DEFAULT_TAB_BAR_HEIGHT 44.0f

// ============================================================================
// TabGroup Layout Trait
// ============================================================================

/**
 * Layout function for TabGroup components.
 * TabGroup is a container that arranges TabBar and TabContent vertically.
 */
void layout_tab_group_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                   float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // TabGroup fills available width, height is determined by children
    float group_width = constraints.max_width > 0 ? constraints.max_width : 400.0f;
    float group_height = constraints.max_height > 0 ? constraints.max_height : 300.0f;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            group_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            group_height = c->style->height.value;
        }
    }

    // Apply min constraints
    group_width = fmaxf(group_width, constraints.min_width);
    group_height = fmaxf(group_height, constraints.min_height);

    // Set group's dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = group_width;
    c->layout_state->computed.height = group_height;

    // Layout children vertically (TabBar first, then TabContent)
    float current_y = parent_y;
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            IRLayoutConstraints child_constraints = {
                .max_width = group_width,
                .max_height = group_height - (current_y - parent_y),
                .min_width = 0.0f,
                .min_height = 0.0f
            };
            ir_layout_dispatch(c->children[i], child_constraints, parent_x, current_y);

            // Update y position for next child
            if (c->children[i]->layout_state && c->children[i]->layout_state->computed.valid) {
                current_y += c->children[i]->layout_state->computed.height;
            }
        }
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
}

const IRLayoutTrait IR_TAB_GROUP_LAYOUT_TRAIT = {
    .layout_component = layout_tab_group_single_pass,
    .name = "TabGroup"
};

// ============================================================================
// TabBar Layout Trait
// ============================================================================

/**
 * Layout function for TabBar components.
 * TabBar arranges Tab children horizontally in a row.
 */
void layout_tab_bar_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // TabBar has default height
    float bar_width = constraints.max_width > 0 ? constraints.max_width : 400.0f;
    float bar_height = DEFAULT_TAB_BAR_HEIGHT;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            bar_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            bar_height = c->style->height.value;
        }
    }

    // Apply constraints
    bar_width = fmaxf(bar_width, constraints.min_width);
    bar_height = fmaxf(bar_height, constraints.min_height);

    // Set bar's dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = bar_width;
    c->layout_state->computed.height = bar_height;

    // Layout tabs horizontally
    float current_x = parent_x;
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            IRLayoutConstraints tab_constraints = {
                .max_width = bar_width - (current_x - parent_x),
                .max_height = bar_height,
                .min_width = 0.0f,
                .min_height = 0.0f
            };
            ir_layout_dispatch(c->children[i], tab_constraints, current_x, parent_y);

            // Update x position for next tab
            if (c->children[i]->layout_state && c->children[i]->layout_state->computed.valid) {
                current_x += c->children[i]->layout_state->computed.width;
            }
        }
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
}

const IRLayoutTrait IR_TAB_BAR_LAYOUT_TRAIT = {
    .layout_component = layout_tab_bar_single_pass,
    .name = "TabBar"
};

// ============================================================================
// Tab Layout Trait
// ============================================================================

/**
 * Layout function for Tab components.
 * Tabs have intrinsic width based on text content.
 */
void layout_tab_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                             float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get style or use defaults
    float font_size = 14.0f;
    float padding_horizontal = 16.0f;

    if (c->style) {
        font_size = (c->style->font.size > 0) ? c->style->font.size : 14.0f;
        padding_horizontal = c->style->padding.left + c->style->padding.right;
    }

    // Compute intrinsic width based on text content or tab_data->title
    float intrinsic_width = 80.0f; // Default minimum
    const char* tab_text = NULL;

    // Check tab_data->title first (from KIR deserialization), then text_content
    if (c->tab_data && c->tab_data->title) {
        tab_text = c->tab_data->title;
    } else if (c->text_content) {
        tab_text = c->text_content;
    }

    if (tab_text) {
        // Improved text width estimate with proper padding for clickability
        float measured_width = strlen(tab_text) * font_size * 0.55f;
        intrinsic_width = measured_width + padding_horizontal + 24.0f; // Extra padding for clickability
    }

    float tab_width = intrinsic_width;
    float tab_height = DEFAULT_TAB_HEIGHT;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            tab_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            tab_height = c->style->height.value;
        }
    }

    // Apply constraints
    tab_width = fmaxf(tab_width, constraints.min_width);
    tab_height = fmaxf(tab_height, constraints.min_height);
    if (constraints.max_width > 0) {
        tab_width = fminf(tab_width, constraints.max_width);
    }
    if (constraints.max_height > 0) {
        tab_height = fminf(tab_height, constraints.max_height);
    }

    // Set final dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = tab_width;
    c->layout_state->computed.height = tab_height;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
}

const IRLayoutTrait IR_TAB_LAYOUT_TRAIT = {
    .layout_component = layout_tab_single_pass,
    .name = "Tab"
};

// ============================================================================
// TabContent Layout Trait
// ============================================================================

/**
 * Layout function for TabContent components.
 * TabContent is a container for TabPanel, fills available space.
 */
void layout_tab_content_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                     float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // TabContent fills available space
    float content_width = constraints.max_width > 0 ? constraints.max_width : 400.0f;
    float content_height = constraints.max_height > 0 ? constraints.max_height : 300.0f;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            content_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            content_height = c->style->height.value;
        }
    }

    // Apply constraints
    content_width = fmaxf(content_width, constraints.min_width);
    content_height = fmaxf(content_height, constraints.min_height);

    // Set content's dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = content_width;
    c->layout_state->computed.height = content_height;

    // Layout children (typically TabPanel)
    IRLayoutConstraints child_constraints = {
        .max_width = content_width,
        .max_height = content_height,
        .min_width = 0.0f,
        .min_height = 0.0f
    };
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            ir_layout_dispatch(c->children[i], child_constraints, parent_x, parent_y);
        }
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
}

const IRLayoutTrait IR_TAB_CONTENT_LAYOUT_TRAIT = {
    .layout_component = layout_tab_content_single_pass,
    .name = "TabContent"
};

// ============================================================================
// TabPanel Layout Trait
// ============================================================================

/**
 * Layout function for TabPanel components.
 * TabPanel fills the parent TabContent space and stacks children vertically.
 * After ForEach expansion, TabPanel may contain multiple Row children that
 * need to be positioned at different Y coordinates.
 */
void layout_tab_panel_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                   float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get padding from style
    float pad_left = c->style ? c->style->padding.left : 0;
    float pad_top = c->style ? c->style->padding.top : 0;
    float pad_right = c->style ? c->style->padding.right : 0;
    float pad_bottom = c->style ? c->style->padding.bottom : 0;

    // TabPanel fills available space
    float panel_width = constraints.max_width > 0 ? constraints.max_width : 400.0f;
    float panel_height = constraints.max_height > 0 ? constraints.max_height : 300.0f;

    // Apply explicit dimensions from style
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            panel_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            panel_height = c->style->height.value;
        }
    }

    // Apply constraints
    panel_width = fmaxf(panel_width, constraints.min_width);
    panel_height = fmaxf(panel_height, constraints.min_height);

    // Set panel's dimensions and position
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;
    c->layout_state->computed.width = panel_width;
    c->layout_state->computed.height = panel_height;

    // Content area (inside padding)
    float content_width = panel_width - pad_left - pad_right;
    float content_height = panel_height - pad_top - pad_bottom;
    float content_x = parent_x + pad_left;
    float content_y = parent_y + pad_top;

    // Get gap from layout if specified
    float gap = (c->layout && c->layout->flex.gap > 0) ? (float)c->layout->flex.gap : 0.0f;

    // Layout children vertically (like a Column), starting from content area
    float current_y = content_y;
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            // Skip invisible children
            if (c->children[i]->style && !c->children[i]->style->visible) continue;

            IRLayoutConstraints child_constraints = {
                .max_width = content_width,
                .max_height = content_height - (current_y - content_y),
                .min_width = 0.0f,
                .min_height = 0.0f
            };
            ir_layout_dispatch(c->children[i], child_constraints, content_x, current_y);

            // Update y position for next child
            if (c->children[i]->layout_state && c->children[i]->layout_state->computed.valid) {
                current_y += c->children[i]->layout_state->computed.height;
                // Add gap between children (but not after the last one)
                if (i < c->child_count - 1) {
                    current_y += gap;
                }
            }
        }
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;
}

const IRLayoutTrait IR_TAB_PANEL_LAYOUT_TRAIT = {
    .layout_component = layout_tab_panel_single_pass,
    .name = "TabPanel"
};

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize Tab component layout traits
 * Called from ir_layout_init_traits()
 */
void ir_tab_components_init(void) {
    ir_layout_register_trait(IR_COMPONENT_TAB_GROUP, &IR_TAB_GROUP_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_TAB_BAR, &IR_TAB_BAR_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_TAB, &IR_TAB_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_TAB_CONTENT, &IR_TAB_CONTENT_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_TAB_PANEL, &IR_TAB_PANEL_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Tab components initialized\n");
    }
}
