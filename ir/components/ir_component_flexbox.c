#include "ir_component_flexbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * Unified Axis-Parameterized Flexbox Layout
 *
 * This single function handles Row, Column, and Container layout by abstracting
 * the layout axis. This eliminates ~200 lines of code duplication.
 *
 * Key insight: Row and Column differ ONLY in which dimension is "main" vs "cross"
 * - Row: main=width (X), cross=height (Y)
 * - Column: main=height (Y), cross=width (X)
 */
void layout_flexbox_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y, LayoutAxis axis) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    if (getenv("KRYON_DEBUG_FLEXBOX")) {
        fprintf(stderr, "[Flexbox] Layout %s (type=%d, children=%d)\n",
                axis == LAYOUT_AXIS_HORIZONTAL ? "ROW" : "COLUMN",
                c->type, c->child_count);
    }

    // Get padding and gap
    IRSpacing padding = {0};
    if (c->style) {
        padding = c->style->padding;
    }

    float gap = (c->layout && c->layout->flex.gap > 0) ? (float)c->layout->flex.gap : 0.0f;

    // Determine available space for children
    // Use explicit dimensions if set, otherwise use constraints
    float container_width = constraints.max_width;
    float container_height = constraints.max_height;

    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            container_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            container_height = c->style->height.value;
        }
    }

    // Available space for children (subtract padding)
    float available_main = (axis == LAYOUT_AXIS_HORIZONTAL)
                          ? container_width - padding.left - padding.right
                          : container_height - padding.top - padding.bottom;

    float available_cross = (axis == LAYOUT_AXIS_HORIZONTAL)
                           ? container_height - padding.top - padding.bottom
                           : container_width - padding.left - padding.right;

    // Compute content area position (parent position + padding)
    float content_x = parent_x + padding.left;
    float content_y = parent_y + padding.top;

    // Track layout along main axis
    float main_position = 0;
    float max_cross_size = 0;
    float total_main_size = 0;

    // Layout all children recursively
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child) continue;

        // Create constraints for child
        IRLayoutConstraints child_constraints = {
            .max_width = (axis == LAYOUT_AXIS_HORIZONTAL) ? available_main : available_cross,
            .max_height = (axis == LAYOUT_AXIS_HORIZONTAL) ? available_cross : available_main,
            .min_width = 0,
            .min_height = 0
        };

        // Compute child's absolute position in content area
        float child_x, child_y;
        if (axis == LAYOUT_AXIS_HORIZONTAL) {
            child_x = content_x + main_position;
            child_y = content_y;  // Will be adjusted for alignment later
        } else {
            child_x = content_x;  // Will be adjusted for alignment later
            child_y = content_y + main_position;
        }

        // RECURSIVELY layout child with its ABSOLUTE position
        ir_layout_single_pass(child, child_constraints, child_x, child_y);

        if (!child->layout_state) continue;

        // Get child's final dimensions
        float child_main = (axis == LAYOUT_AXIS_HORIZONTAL)
                          ? child->layout_state->computed.width
                          : child->layout_state->computed.height;

        float child_cross = (axis == LAYOUT_AXIS_HORIZONTAL)
                           ? child->layout_state->computed.height
                           : child->layout_state->computed.width;

        // Advance main position
        main_position += child_main;
        if (i < c->child_count - 1) {
            main_position += gap;  // Add gap between children
        }

        // Track maximum cross size and total main size
        max_cross_size = fmaxf(max_cross_size, child_cross);
        total_main_size += child_main;
    }

    // Add gaps to total main size
    if (c->child_count > 1) {
        total_main_size += gap * (c->child_count - 1);
    }

    // Determine own dimensions
    // Use explicit dimensions if set (already calculated), otherwise use content size
    float own_main, own_cross;

    if (axis == LAYOUT_AXIS_HORIZONTAL) {
        own_main = (c->style && c->style->width.type == IR_DIMENSION_PX)
                   ? container_width - padding.left - padding.right
                   : total_main_size;
        own_cross = (c->style && c->style->height.type == IR_DIMENSION_PX)
                    ? container_height - padding.top - padding.bottom
                    : max_cross_size;
    } else {
        own_main = (c->style && c->style->height.type == IR_DIMENSION_PX)
                   ? container_height - padding.top - padding.bottom
                   : total_main_size;
        own_cross = (c->style && c->style->width.type == IR_DIMENSION_PX)
                    ? container_width - padding.left - padding.right
                    : max_cross_size;
    }

    // Apply cross-axis alignment to children
    if (c->layout && c->layout->flex.cross_axis != IR_ALIGNMENT_START) {
        for (uint32_t i = 0; i < c->child_count; i++) {
            IRComponent* child = c->children[i];
            if (!child || !child->layout_state) continue;

            float child_cross = (axis == LAYOUT_AXIS_HORIZONTAL)
                               ? child->layout_state->computed.height
                               : child->layout_state->computed.width;

            float cross_offset = 0;
            switch (c->layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    cross_offset = (own_cross - child_cross) / 2.0f;
                    break;
                case IR_ALIGNMENT_END:
                    cross_offset = own_cross - child_cross;
                    break;
                default:
                    break;
            }

            if (axis == LAYOUT_AXIS_HORIZONTAL) {
                child->layout_state->computed.y = content_y + cross_offset;
            } else {
                child->layout_state->computed.x = content_x + cross_offset;
            }
        }
    }

    // Set final computed dimensions (add padding back)
    if (axis == LAYOUT_AXIS_HORIZONTAL) {
        c->layout_state->computed.width = own_main + padding.left + padding.right;
        c->layout_state->computed.height = own_cross + padding.top + padding.bottom;
    } else {
        c->layout_state->computed.width = own_cross + padding.left + padding.right;
        c->layout_state->computed.height = own_main + padding.top + padding.bottom;
    }

    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_FLEXBOX")) {
        fprintf(stderr, "[Flexbox] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
    }
}

// Row layout (horizontal flexbox)
void layout_row_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                            float parent_x, float parent_y) {
    layout_flexbox_single_pass(c, constraints, parent_x, parent_y, LAYOUT_AXIS_HORIZONTAL);
}

// Column layout (vertical flexbox)
void layout_column_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    layout_flexbox_single_pass(c, constraints, parent_x, parent_y, LAYOUT_AXIS_VERTICAL);
}

// Container layout (configurable direction)
void layout_container_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y) {
    // Container can specify direction via layout->flex.direction
    // direction == 1 means row (horizontal), otherwise column (vertical)
    LayoutAxis axis = LAYOUT_AXIS_VERTICAL;  // Default to column

    if (c->layout && c->layout->flex.direction == 1) {
        axis = LAYOUT_AXIS_HORIZONTAL;
    }

    layout_flexbox_single_pass(c, constraints, parent_x, parent_y, axis);
}

// Center layout (centers child both horizontally and vertically)
void layout_center_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Get padding
    IRSpacing padding = {0};
    if (c->style) {
        padding = c->style->padding;
    }

    // Available space for child (subtract padding)
    float available_width = constraints.max_width - padding.left - padding.right;
    float available_height = constraints.max_height - padding.top - padding.bottom;

    // Content area position (parent position + padding)
    float content_x = parent_x + padding.left;
    float content_y = parent_y + padding.top;

    // Layout the child if it exists
    float child_width = 0;
    float child_height = 0;

    if (c->child_count > 0 && c->children[0]) {
        IRComponent* child = c->children[0];

        // Create constraints for child
        IRLayoutConstraints child_constraints = {
            .max_width = available_width,
            .max_height = available_height,
            .min_width = 0,
            .min_height = 0
        };

        // Layout child at temporary position (we'll center it after)
        ir_layout_single_pass(child, child_constraints, content_x, content_y);

        if (child->layout_state) {
            child_width = child->layout_state->computed.width;
            child_height = child->layout_state->computed.height;

            // Center the child
            float offset_x = (available_width - child_width) / 2.0f;
            float offset_y = (available_height - child_height) / 2.0f;

            child->layout_state->computed.x = content_x + offset_x;
            child->layout_state->computed.y = content_y + offset_y;

            if (getenv("KRYON_DEBUG_CENTER")) {
                fprintf(stderr, "[Center] Child centering: avail_w=%.1f, avail_h=%.1f, child_w=%.1f, child_h=%.1f\n",
                        available_width, available_height, child_width, child_height);
                fprintf(stderr, "[Center] Offsets: x=%.1f, y=%.1f -> final child pos: (%.1f, %.1f)\n",
                        offset_x, offset_y, child->layout_state->computed.x, child->layout_state->computed.y);
            }
        }
    }

    // Determine own dimensions
    float own_width = available_width;
    float own_height = available_height;

    // Check for explicit dimensions
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            own_width = c->style->width.value - padding.left - padding.right;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            own_height = c->style->height.value - padding.top - padding.bottom;
        }
    }

    // Set final computed dimensions (add padding back)
    c->layout_state->computed.width = own_width + padding.left + padding.right;
    c->layout_state->computed.height = own_height + padding.top + padding.bottom;
    c->layout_state->computed.x = parent_x;
    c->layout_state->computed.y = parent_y;

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    if (getenv("KRYON_DEBUG_CENTER")) {
        fprintf(stderr, "[Center] Final: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                c->layout_state->computed.x, c->layout_state->computed.y,
                c->layout_state->computed.width, c->layout_state->computed.height);
    }
}

// Component traits
const IRLayoutTrait IR_ROW_LAYOUT_TRAIT = {
    .layout_component = layout_row_single_pass,
    .name = "Row"
};

const IRLayoutTrait IR_COLUMN_LAYOUT_TRAIT = {
    .layout_component = layout_column_single_pass,
    .name = "Column"
};

const IRLayoutTrait IR_CONTAINER_LAYOUT_TRAIT = {
    .layout_component = layout_container_single_pass,
    .name = "Container"
};

const IRLayoutTrait IR_CENTER_LAYOUT_TRAIT = {
    .layout_component = layout_center_single_pass,
    .name = "Center"
};

// Initialize and register flexbox component traits
void ir_flexbox_components_init(void) {
    ir_layout_register_trait(IR_COMPONENT_ROW, &IR_ROW_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_COLUMN, &IR_COLUMN_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_CONTAINER, &IR_CONTAINER_LAYOUT_TRAIT);
    ir_layout_register_trait(IR_COMPONENT_CENTER, &IR_CENTER_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Flexbox components (Row/Column/Container/Center) initialized\n");
    }
}
