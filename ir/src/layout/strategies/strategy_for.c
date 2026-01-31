/**
 * @file strategy_for.c
 * @brief Layout strategy for For components (unified - handles both compile-time and runtime)
 */

#define _GNU_SOURCE
#include "../ir_layout_strategy.h"
#include "../../include/ir_core.h"
#include "../layout_helpers.h"
#include <stdlib.h>

// Forward declarations from ir_layout.c
extern float ir_get_component_intrinsic_width(IRComponent* component);
extern float ir_get_component_intrinsic_height(IRComponent* component);

static void measure_for(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!layout_ensure_state(comp)) return;

    // For behaves like a container - width from constraints, height from children
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

    layout_set_final_dimensions(comp, width, height);
}

static void layout_for_children(IRComponent* comp) {
    // For child layout is handled by the main layout system
    // The For component just acts as a container
    (void)comp;
}

void ir_register_for_layout_strategy(void) {
    // Register for both IR_COMPONENT_FOR_LOOP (legacy) and the unified type
    // This ensures backward compatibility during the transition
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_FOR_LOOP,
        .measure = measure_for,
        .layout_children = layout_for_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
