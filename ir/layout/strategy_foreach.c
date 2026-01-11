/**
 * @file strategy_foreach.c
 * @brief Layout strategy for ForEach components
 */

#define _GNU_SOURCE
#include "ir_layout_strategy.h"
#include "../ir_core.h"
#include <stdlib.h>

// Forward declarations from ir_layout.c
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

static void measure_foreach(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!comp->layout_state) {
        comp->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // ForEach behaves like a container - width from constraints, height from children
    float width = constraints->max_width;
    float height = 0;  // Will be determined by children

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

static void layout_foreach_children(IRComponent* comp) {
    // ForEach child layout is handled by the main layout system
    // The ForEach component just acts as a container
    (void)comp;
}

void ir_register_foreach_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_FOR_EACH,
        .measure = measure_foreach,
        .layout_children = layout_foreach_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
