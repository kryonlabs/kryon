/**
 * @file strategy_column.c
 * @brief Layout strategy for Column components (vertical flexbox)
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

static void measure_column(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!comp->layout_state) {
        comp->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    float width = constraints->max_width;
    float height = 0;  // Determined by children

    if (comp->style) {
        if (comp->style->width.type == IR_DIMENSION_PX) {
            width = comp->style->width.value;
        } else if (comp->style->width.type == IR_DIMENSION_PERCENT) {
            width = constraints->max_width * (comp->style->width.value / 100.0f);
        }
        if (comp->style->height.type == IR_DIMENSION_PX) {
            height = comp->style->height.value;
        } else if (comp->style->height.type == IR_DIMENSION_PERCENT) {
            height = constraints->max_height * (comp->style->height.value / 100.0f);
        }
    }

    comp->layout_state->computed.width = width;
    comp->layout_state->computed.height = height;
    comp->layout_state->layout_valid = true;
}

static void layout_column_children(IRComponent* comp) {
    if (!comp || !comp->layout_state || !comp->layout) return;

    IRLayout* layout = comp->layout;
    IRStyle* style = comp->style;

    float available_width = comp->layout_state->computed.width;
    float available_height = comp->layout_state->computed.height;

    float current_y = style ? style->padding.top : 0;
    float max_width = 0;

    float gap = layout ? layout->flex.gap : 0;

    for (uint32_t i = 0; i < comp->child_count; i++) {
        IRComponent* child = comp->children[i];
        if (!child->style || !child->style->visible) continue;

        float child_height = 0;
        if (child->style->height.type == IR_DIMENSION_PX) {
            child_height = child->style->height.value;
        } else if (child->style->height.type == IR_DIMENSION_PERCENT) {
            child_height = available_height * (child->style->height.value / 100.0f);
        } else {
            child_height = ir_get_component_intrinsic_height(child);
        }

        float child_width = 0;
        if (child->style->width.type == IR_DIMENSION_PX) {
            child_width = child->style->width.value;
        } else if (child->style->width.type == IR_DIMENSION_PERCENT) {
            child_width = available_width * (child->style->width.value / 100.0f);
        } else {
            child_width = ir_get_component_intrinsic_width(child);
        }

        // Apply cross-axis alignment (horizontal)
        float child_x = style ? style->padding.left : 0;
        child_x += child->style->margin.left;

        if (layout && layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_x += (available_width - child_width) / 2.0f;
        } else if (layout && layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_x += available_width - child_width;
        } else if (layout && layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_width = available_width - child->style->margin.left - child->style->margin.right;
        }

        // Set child bounds
        if (child->layout_state) {
            child->layout_state->computed.x = child_x;
            child->layout_state->computed.y = current_y + child->style->margin.top;
            child->layout_state->computed.width = child_width;
            child->layout_state->computed.height = child_height;
            child->layout_state->computed.valid = true;
        }

        if (child_width > max_width) {
            max_width = child_width;
        }

        current_y += child_height + child->style->margin.bottom + gap;
    }
}

void ir_register_column_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_COLUMN,
        .measure = measure_column,
        .layout_children = layout_column_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
