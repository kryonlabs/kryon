#include "ir_component_flexbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/**
 * Recursively update positions of all descendants when parent is moved by alignment.
 * This is necessary because children store absolute positions, so when we move a parent,
 * we must also move all its descendants by the same offset.
 */
static void update_descendants_position(IRComponent* c, float dx, float dy) {
    if (!c || !c->layout_state) return;

    // Update this component's position
    c->layout_state->computed.x += dx;
    c->layout_state->computed.y += dy;

    // Recursively update all children
    for (uint32_t i = 0; i < c->child_count; i++) {
        if (c->children[i]) {
            update_descendants_position(c->children[i], dx, dy);
        }
    }
}

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
        fprintf(stderr, "[Flexbox] Layout %s (type=%d, children=%d, ptr=%p, parent_pos=(%.1f,%.1f))\n",
                axis == LAYOUT_AXIS_HORIZONTAL ? "ROW" : "COLUMN",
                c->type, c->child_count, (void*)c, parent_x, parent_y);
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
        } else if (c->style->width.type == IR_DIMENSION_PERCENT) {
            container_width = constraints.max_width * (c->style->width.value / 100.0f);
        }

        if (c->style->height.type == IR_DIMENSION_PX) {
            container_height = c->style->height.value;
        } else if (c->style->height.type == IR_DIMENSION_PERCENT) {
            container_height = constraints.max_height * (c->style->height.value / 100.0f);
        }
    }

    // Available space for children (subtract padding)
    float available_main = (axis == LAYOUT_AXIS_HORIZONTAL)
                          ? container_width - padding.left - padding.right
                          : container_height - padding.top - padding.bottom;

    float available_cross = (axis == LAYOUT_AXIS_HORIZONTAL)
                           ? container_height - padding.top - padding.bottom
                           : container_width - padding.left - padding.right;

    // Determine container's base position (check for absolute positioning first)
    float container_x = parent_x;
    float container_y = parent_y;

    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        container_x = c->style->absolute_x;
        container_y = c->style->absolute_y;
    }

    // Compute content area position (container position + padding)
    float content_x = container_x + padding.left;
    float content_y = container_y + padding.top;

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
    // Main axis: Fill available space ONLY if:
    //   1. Has explicit dimension, OR
    //   2. Has justifyContent alignment that needs space (not START)
    // Otherwise: Shrink-wrap to content
    // Cross axis: Always shrink to content if no explicit dimension
    // Note: Both PX and PERCENT are considered "explicit" dimensions
    float own_main, own_cross;

    // Check if container has main-axis alignment that requires filling available space
    bool has_main_alignment = c->layout && c->layout->flex.justify_content != IR_ALIGNMENT_START;

    if (axis == LAYOUT_AXIS_HORIZONTAL) {
        bool has_explicit_width = c->style && (c->style->width.type == IR_DIMENSION_PX ||
                                                c->style->width.type == IR_DIMENSION_PERCENT);
        bool has_explicit_height = c->style && (c->style->height.type == IR_DIMENSION_PX ||
                                                 c->style->height.type == IR_DIMENSION_PERCENT);

        own_main = has_explicit_width
                   ? container_width - padding.left - padding.right
                   : (has_main_alignment ? available_main : total_main_size);
        own_cross = has_explicit_height
                    ? container_height - padding.top - padding.bottom
                    : max_cross_size;
    } else {
        bool has_explicit_width = c->style && (c->style->width.type == IR_DIMENSION_PX ||
                                                c->style->width.type == IR_DIMENSION_PERCENT);
        bool has_explicit_height = c->style && (c->style->height.type == IR_DIMENSION_PX ||
                                                 c->style->height.type == IR_DIMENSION_PERCENT);

        own_main = has_explicit_height
                   ? container_height - padding.top - padding.bottom
                   : (has_main_alignment ? available_main : total_main_size);
        own_cross = has_explicit_width
                    ? container_width - padding.left - padding.right
                    : max_cross_size;
    }

    // Apply main-axis alignment (justify_content)
    float remaining_main = own_main - total_main_size;
    float main_offset = 0.0f;
    float extra_gap = 0.0f;

    if (remaining_main > 0 && c->layout) {
        if (getenv("KRYON_DEBUG_FLEXBOX")) {
            fprintf(stderr, "[Flexbox] Has layout, justify_content=%d\n", c->layout->flex.justify_content);
        }

        switch (c->layout->flex.justify_content) {
            case IR_ALIGNMENT_START:
                // No adjustment (default)
                break;
            case IR_ALIGNMENT_CENTER:
                main_offset = remaining_main / 2.0f;
                break;
            case IR_ALIGNMENT_END:
                main_offset = remaining_main;
                break;
            case IR_ALIGNMENT_SPACE_BETWEEN:
                if (c->child_count > 1) {
                    extra_gap = remaining_main / (c->child_count - 1);
                }
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                if (c->child_count > 0) {
                    extra_gap = remaining_main / c->child_count;
                    main_offset = extra_gap / 2.0f;
                }
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                if (c->child_count > 0) {
                    extra_gap = remaining_main / (c->child_count + 1);
                    main_offset = extra_gap;
                }
                break;
            default:
                break;
        }

        if (getenv("KRYON_DEBUG_FLEXBOX")) {
            fprintf(stderr, "[Flexbox] Main-axis alignment: remaining=%.1f, offset=%.1f, extra_gap=%.1f\n",
                    remaining_main, main_offset, extra_gap);
        }
    }

    // Adjust child positions along main axis
    float cumulative_offset = main_offset;
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        if (getenv("KRYON_DEBUG_FLEXBOX")) {
            float old_pos = (axis == LAYOUT_AXIS_HORIZONTAL)
                ? child->layout_state->computed.x
                : child->layout_state->computed.y;
            fprintf(stderr, "[Flexbox] Parent=%p Child %d (ptr=%p): before_offset=%.1f, applying_offset=%.1f\n",
                    (void*)c, i, (void*)child, old_pos, cumulative_offset);
        }

        // Move child AND all its descendants by the alignment offset
        // This is necessary because descendants have absolute positions
        if (axis == LAYOUT_AXIS_HORIZONTAL) {
            update_descendants_position(child, cumulative_offset, 0.0f);
        } else {
            update_descendants_position(child, 0.0f, cumulative_offset);
        }

        if (getenv("KRYON_DEBUG_FLEXBOX")) {
            float new_pos = (axis == LAYOUT_AXIS_HORIZONTAL)
                ? child->layout_state->computed.x
                : child->layout_state->computed.y;
            fprintf(stderr, "[Flexbox] Parent=%p Child %d (ptr=%p): after_offset=%.1f\n",
                    (void*)c, i, (void*)child, new_pos);
        }

        if (i < c->child_count - 1) {
            cumulative_offset += extra_gap;
        }
    }

    // Apply cross-axis alignment to children
    if (c->layout && c->layout->flex.cross_axis != IR_ALIGNMENT_START) {
        if (getenv("KRYON_DEBUG_FLEXBOX")) {
            fprintf(stderr, "[Flexbox] Cross-axis alignment: mode=%d, own_cross=%.1f\n",
                    c->layout->flex.cross_axis, own_cross);
        }
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
                    if (getenv("KRYON_DEBUG_FLEXBOX")) {
                        fprintf(stderr, "[Flexbox] Cross-axis CENTER: child_cross=%.1f, offset=%.1f\n",
                                child_cross, cross_offset);
                    }
                    break;
                case IR_ALIGNMENT_END:
                    cross_offset = own_cross - child_cross;
                    break;
                case IR_ALIGNMENT_STRETCH:
                    // Stretch child to fill cross dimension
                    if (axis == LAYOUT_AXIS_HORIZONTAL) {
                        child->layout_state->computed.height = own_cross;
                    } else {
                        child->layout_state->computed.width = own_cross;
                    }
                    // Keep position at content edge (no offset)
                    break;
                default:
                    break;
            }

            // Move child AND all its descendants by the cross-axis offset
            // Calculate how much we need to move from current position
            if (axis == LAYOUT_AXIS_HORIZONTAL) {
                float current_y = child->layout_state->computed.y;
                float target_y = content_y + cross_offset;
                float dy = target_y - current_y;
                if (dy != 0.0f) {
                    update_descendants_position(child, 0.0f, dy);
                }
            } else {
                float current_x = child->layout_state->computed.x;
                float target_x = content_x + cross_offset;
                float dx = target_x - current_x;
                if (dx != 0.0f) {
                    update_descendants_position(child, dx, 0.0f);
                }
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

    // Set final position (already determined at the start)
    c->layout_state->computed.x = container_x;
    c->layout_state->computed.y = container_y;

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

    // Determine container's base position (check for absolute positioning first)
    float container_x = parent_x;
    float container_y = parent_y;

    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        container_x = c->style->absolute_x;
        container_y = c->style->absolute_y;
    }

    // Content area position (container position + padding)
    float content_x = container_x + padding.left;
    float content_y = container_y + padding.top;

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

    // Set final position (already determined at the start)
    c->layout_state->computed.x = container_x;
    c->layout_state->computed.y = container_y;

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
