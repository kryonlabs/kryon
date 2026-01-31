// ir_layout_flex.c
//
// Consolidated flex layout implementation
// Replaces duplicated ir_layout_compute_row() and ir_layout_compute_column()
//
// This file contains the shared layout logic for both row and column layouts,
// with only the axis (horizontal/vertical) as a parameter.

#include "../include/ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declarations from ir_layout.c
extern float resolve_dimension(IRDimension dim, float available);
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

// ============================================================================
// Consolidated Flex Layout Implementation
// ============================================================================

/**
 * Layout direction for flex layout
 */
typedef enum {
    LAYOUT_AXIS_ROW,      // Horizontal (left to right)
    LAYOUT_AXIS_COLUMN    // Vertical (top to bottom)
} LayoutAxis;

/**
 * Consolidated flex layout computation
 *
 * This single implementation handles both row and column layouts by
 * parameterizing the layout axis. The previous implementation had
 * two nearly identical functions (ir_layout_compute_row and
 * ir_layout_compute_column) with ~160 lines each.
 *
 * @param container      Container component to layout
 * @param available_width Available width for layout
 * @param available_height Available height for layout
 * @param axis           Layout axis (ROW or COLUMN)
 */
static void ir_layout_compute_flex(IRComponent* container,
                                   float available_width,
                                   float available_height,
                                   LayoutAxis axis) {
    if (!container) return;

    IRLayout* layout = container->layout;
    IRStyle* style = container->style;

    // Determine main and cross axis based on direction
    bool is_row = (axis == LAYOUT_AXIS_ROW);
    float available_main = is_row ? available_width : available_height;
    float available_cross = is_row ? available_height : available_width;

    // First pass: calculate intrinsic sizes and total flex grow
    float total_main = 0.0f;
    float total_flex_grow = 0.0f;
    uint32_t visible_count = 0;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        visible_count++;

        // Measure child along main axis
        float child_main;
        if (is_row) {
            child_main = resolve_dimension(child->style->width, available_width);
            if (child_main == 0.0f) {
                child_main = ir_get_component_intrinsic_width(child);
            }
        } else {
            child_main = resolve_dimension(child->style->height, available_height);
            if (child_main == 0.0f) {
                child_main = ir_get_component_intrinsic_height(child);
            }
        }

        // Apply max constraint before calculating total
        IRDimension max_dim = is_row ? child->layout->max_width : child->layout->max_height;
        if (child->layout && max_dim.type == IR_DIMENSION_PX && max_dim.value > 0) {
            if (child_main > max_dim.value) {
                child_main = max_dim.value;
            }
        }

        // Add margins along main axis
        if (is_row) {
            child_main += child->style->margin.left + child->style->margin.right;
        } else {
            child_main += child->style->margin.top + child->style->margin.bottom;
        }

        total_main += child_main;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        }
    }

    // Add gaps along main axis
    if (visible_count > 1) {
        total_main += layout->flex.gap * (visible_count - 1);
    }

    // Second pass: distribute remaining space and position children
    float remaining_main = available_main - total_main;
    float current_main = is_row ? style->padding.left : style->padding.top;

    // Apply main-axis alignment (justify_content)
    if (remaining_main > 0 && total_flex_grow == 0 && visible_count > 0) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_CENTER:
                current_main += remaining_main / 2.0f;
                break;
            case IR_ALIGNMENT_END:
                current_main += remaining_main;
                break;
            case IR_ALIGNMENT_SPACE_BETWEEN:
                // Will be handled per-child below
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                current_main += remaining_main / (visible_count * 2);
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                current_main += remaining_main / (visible_count + 1);
                break;
            default:
                break;
        }
    }

    // Calculate gap for space-between/around/evenly
    float extra_gap = 0.0f;
    if (remaining_main > 0 && total_flex_grow == 0 && visible_count > 1) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN:
                extra_gap = remaining_main / (visible_count - 1);
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                extra_gap = remaining_main / visible_count;
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                extra_gap = remaining_main / (visible_count + 1);
                break;
            default:
                break;
        }
    }

    // Position and size children
    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        // Measure child along main axis
        float child_main;
        if (is_row) {
            child_main = resolve_dimension(child->style->width, available_width);
            if (child_main == 0.0f) {
                child_main = ir_get_component_intrinsic_width(child);
            }
        } else {
            child_main = resolve_dimension(child->style->height, available_height);
            if (child_main == 0.0f) {
                child_main = ir_get_component_intrinsic_height(child);
            }
        }

        // Apply flex grow
        if (remaining_main > 0 && total_flex_grow > 0 &&
            child->layout && child->layout->flex.grow > 0) {
            float extra = remaining_main * (child->layout->flex.grow / total_flex_grow);
            child_main += extra;
        }

        // Measure child along cross axis
        float child_cross;
        if (is_row) {
            child_cross = resolve_dimension(child->style->height, available_height);
            if (child_cross == 0.0f) {
                child_cross = ir_get_component_intrinsic_height(child);
            }
        } else {
            child_cross = resolve_dimension(child->style->width, available_width);
            if (child_cross == 0.0f) {
                child_cross = ir_get_component_intrinsic_width(child);
            }
        }

        // Apply max constraint on cross axis BEFORE alignment
        IRDimension max_dim_cross = is_row ? child->layout->max_height : child->layout->max_width;
        if (child->layout && max_dim_cross.type == IR_DIMENSION_PX && max_dim_cross.value > 0) {
            if (child_cross > max_dim_cross.value) {
                child_cross = max_dim_cross.value;
            }
        }

        // Apply cross-axis alignment
        float current_cross;
        if (is_row) {
            // Row layout: vertical alignment
            current_cross = style->padding.top + child->style->margin.top;
            if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
                current_cross += (available_cross - child_cross) / 2.0f;
            } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
                current_cross += available_cross - child_cross;
            } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
                child_cross = available_cross - child->style->margin.top - child->style->margin.bottom;
            }
        } else {
            // Column layout: horizontal alignment
            current_cross = style->padding.left + child->style->margin.left;
            if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
                current_cross += (available_cross - child_cross) / 2.0f;
            } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
                current_cross += available_cross - child_cross;
            } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
                child_cross = available_cross - child->style->margin.left - child->style->margin.right;
            }
        }

        // Apply max constraint on main axis
        IRDimension max_dim_main = is_row ? child->layout->max_width : child->layout->max_height;
        if (child->layout && max_dim_main.type == IR_DIMENSION_PX && max_dim_main.value > 0) {
            if (child_main > max_dim_main.value) {
                child_main = max_dim_main.value;
            }
        }

        // Set child bounds - skip absolutely positioned children
        if (child->style->position_mode == IR_POSITION_ABSOLUTE) {
            child->rendered_bounds.x = child->style->absolute_x;
            child->rendered_bounds.y = child->style->absolute_y;
            child->rendered_bounds.width = is_row ? child_main : child_cross;
            child->rendered_bounds.height = is_row ? child_cross : child_main;
            child->rendered_bounds.valid = true;
            // Don't add to current_main - absolute children are out of flow
            continue;
        }

        // Set bounds based on axis
        if (is_row) {
            child->rendered_bounds.x = current_main + child->style->margin.left;
            child->rendered_bounds.y = current_cross;
            child->rendered_bounds.width = child_main;
            child->rendered_bounds.height = child_cross;
        } else {
            child->rendered_bounds.x = current_cross;
            child->rendered_bounds.y = current_main + child->style->margin.top;
            child->rendered_bounds.width = child_cross;
            child->rendered_bounds.height = child_main;
        }
        child->rendered_bounds.valid = true;

        // Debug output (optional, based on environment variables)
        if (getenv("DEBUG_BUTTON_POSITIONS") && child->type == IR_COMPONENT_BUTTON) {
            const char* text = child->text_content ? child->text_content : "(null)";
            printf("[LAYOUT] Button ID=%u type=%d text='%s' pos=[%.1f, %.1f] size=[%.1fx%.1f] parent_id=%d\n",
                   child->id, (int)child->type, text,
                   child->rendered_bounds.x, child->rendered_bounds.y,
                   child->rendered_bounds.width, child->rendered_bounds.height,
                   container->id);
        }

        #ifdef KRYON_TRACE_LAYOUT
        fprintf(stderr, "║ Child %u: bounds=[%.1f, %.1f, %.1f, %.1f] %s=%.1f\n",
                i, child->rendered_bounds.x, child->rendered_bounds.y,
                child->rendered_bounds.width, child->rendered_bounds.height,
                is_row ? "intrinsic_w" : "intrinsic_h",
                is_row ? ir_get_component_intrinsic_width(child) : ir_get_component_intrinsic_height(child));
        #endif

        // Advance to next position along main axis
        float margin_after = is_row ?
            child->style->margin.right : child->style->margin.bottom;
        current_main += child_main + margin_after + layout->flex.gap + extra_gap;
    }

    // Update container's intrinsic size (main axis only)
    if (is_row) {
        container->layout_cache.cached_intrinsic_width = total_main;
        if (layout) {
            layout->computed_width = total_main;
        }
    } else {
        container->layout_cache.cached_intrinsic_height = total_main;
        if (layout) {
            layout->computed_height = total_main;
        }
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "║ Container intrinsic size updated: %s=%.1f\n",
            is_row ? "width" : "height", total_main);
    #endif
}

// ============================================================================
// Public API Wrappers
// ============================================================================

/**
 * Compute row layout (horizontal)
 * @deprecated Use ir_layout_compute_flex() with LAYOUT_AXIS_ROW directly
 */
void ir_layout_compute_row_public(IRComponent* container,
                                 float available_width,
                                 float available_height) {
    ir_layout_compute_flex(container, available_width, available_height, LAYOUT_AXIS_ROW);
}

/**
 * Compute column layout (vertical)
 * @deprecated Use ir_layout_compute_flex() with LAYOUT_AXIS_COLUMN directly
 */
void ir_layout_compute_column_public(IRComponent* container,
                                   float available_width,
                                   float available_height) {
    ir_layout_compute_flex(container, available_width, available_height, LAYOUT_AXIS_COLUMN);
}
