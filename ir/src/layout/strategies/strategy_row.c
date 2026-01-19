/**
 * @file strategy_row.c
 * @brief Layout strategy for Row components (horizontal flexbox)
 */

#define _GNU_SOURCE
#include "../include/ir_layout_strategy.h"
#include "../include/ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declarations from ir_layout.c
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

__attribute__((unused)) static void resolve_dimension(IRDimension dim, float parent_size, float* out_value) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            *out_value = dim.value;
            break;
        case IR_DIMENSION_PERCENT:
            *out_value = parent_size * (dim.value / 100.0f);
            break;
        default:
            *out_value = 0.0f;
            break;
    }
}

static void measure_row(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!comp->layout_state) {
        comp->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Row width comes from constraints or style
    float width = constraints->max_width;
    if (comp->style) {
        if (comp->style->width.type == IR_DIMENSION_PX) {
            width = comp->style->width.value;
        } else if (comp->style->width.type == IR_DIMENSION_PERCENT) {
            width = constraints->max_width * (comp->style->width.value / 100.0f);
        }
    }

    // Height depends on children (max child height)
    float height = 0;

    comp->layout_state->computed.width = width;
    comp->layout_state->computed.height = height;
    comp->layout_state->layout_valid = true;
}

static void layout_row_children(IRComponent* comp) {
    if (!comp || !comp->layout_state || !comp->layout) return;

    IRLayout* layout = comp->layout;
    IRStyle* style = comp->style;

    float available_width = comp->layout_state->computed.width;
    float available_height = comp->layout_state->computed.height;

    // First pass: calculate intrinsic sizes and total flex grow
    float total_width = 0.0f;
    float total_flex_grow = 0.0f;
    uint32_t visible_count = 0;

    for (uint32_t i = 0; i < comp->child_count; i++) {
        IRComponent* child = comp->children[i];
        if (!child->style || !child->style->visible) continue;

        visible_count++;

        float child_width = 0;
        if (child->style->width.type == IR_DIMENSION_PX) {
            child_width = child->style->width.value;
        } else if (child->style->width.type == IR_DIMENSION_PERCENT) {
            child_width = available_width * (child->style->width.value / 100.0f);
        } else {
            child_width = ir_get_component_intrinsic_width(child);
        }

        // Apply max_width constraint
        if (child->layout && child->layout->max_width.type == IR_DIMENSION_PX && child->layout->max_width.value > 0) {
            if (child_width > child->layout->max_width.value) {
                child_width = child->layout->max_width.value;
            }
        }

        child_width += child->style->margin.left + child->style->margin.right;
        total_width += child_width;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        }
    }

    // Add gaps
    float gap = layout ? layout->flex.gap : 0;
    if (visible_count > 1) {
        total_width += gap * (visible_count - 1);
    }

    // Second pass: distribute remaining space and position children
    float remaining_width = available_width - total_width;
    float current_x = style ? style->padding.left : 0;

    // Apply main-axis alignment
    if (remaining_width > 0 && total_flex_grow == 0) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_CENTER:
                current_x += remaining_width / 2.0f;
                break;
            case IR_ALIGNMENT_END:
                current_x += remaining_width;
                break;
            default:
                break;
        }
    }

    for (uint32_t i = 0; i < comp->child_count; i++) {
        IRComponent* child = comp->children[i];
        if (!child->style || !child->style->visible) continue;

        float child_width = 0;
        if (child->style->width.type == IR_DIMENSION_PX) {
            child_width = child->style->width.value;
        } else if (child->style->width.type == IR_DIMENSION_PERCENT) {
            child_width = available_width * (child->style->width.value / 100.0f);
        } else {
            child_width = ir_get_component_intrinsic_width(child);
        }

        // Apply flex grow
        if (remaining_width > 0 && total_flex_grow > 0 && child->layout && child->layout->flex.grow > 0) {
            float extra = remaining_width * (child->layout->flex.grow / total_flex_grow);
            child_width += extra;
        }

        float child_height = 0;
        if (child->style->height.type == IR_DIMENSION_PX) {
            child_height = child->style->height.value;
        } else if (child->style->height.type == IR_DIMENSION_PERCENT) {
            child_height = available_height * (child->style->height.value / 100.0f);
        } else {
            child_height = ir_get_component_intrinsic_height(child);
        }

        // Apply cross-axis alignment (vertical)
        float child_y = style ? style->padding.top : 0;
        child_y += child->style->margin.top;

        if (layout && layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_y += (available_height - child_height) / 2.0f;
        } else if (layout && layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_y += available_height - child_height;
        } else if (layout && layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_height = available_height - child->style->margin.top - child->style->margin.bottom;
        }

        // Set child bounds
        if (child->layout_state) {
            child->layout_state->computed.x = current_x + child->style->margin.left;
            child->layout_state->computed.y = child_y;
            child->layout_state->computed.width = child_width;
            child->layout_state->computed.height = child_height;
            child->layout_state->computed.valid = true;
        }

        current_x += child_width + child->style->margin.right + gap;
    }
}

void ir_register_row_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_ROW,
        .measure = measure_row,
        .layout_children = layout_row_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
