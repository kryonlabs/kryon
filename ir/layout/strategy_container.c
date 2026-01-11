/**
 * @file strategy_container.c
 * @brief Layout strategy for Container components
 */

#define _GNU_SOURCE
#include "ir_layout_strategy.h"
#include "../ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declaration from ir_layout.c
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

static void measure_container(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!comp->layout_state) {
        comp->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // Container uses its size constraints or defaults
    float width = constraints->max_width;
    float height = 0;  // Height depends on children

    // Check for explicit size in style
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

static void layout_container_children(IRComponent* comp) {
    // Container layout is handled by the main layout system
    // This is a placeholder for any container-specific child layout
    (void)comp;
}

void ir_register_container_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_CONTAINER,
        .measure = measure_container,
        .layout_children = layout_container_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
