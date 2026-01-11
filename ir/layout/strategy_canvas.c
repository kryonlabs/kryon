/**
 * @file strategy_canvas.c
 * @brief Layout strategy for Canvas components
 */

#define _GNU_SOURCE
#include "ir_layout_strategy.h"
#include "../ir_core.h"
#include <stdlib.h>

static void measure_canvas(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!comp->layout_state) {
        comp->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Native canvas always has explicit dimensions set during creation
    float width = 800.0f;  // Fallback default
    float height = 600.0f; // Fallback default

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

static void layout_canvas_children(IRComponent* comp) {
    // Canvas has no children
    (void)comp;
}

void ir_register_canvas_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_NATIVE_CANVAS,
        .measure = measure_canvas,
        .layout_children = layout_canvas_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
