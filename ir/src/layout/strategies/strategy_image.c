/**
 * @file strategy_image.c
 * @brief Layout strategy for Image components
 */

#define _GNU_SOURCE
#include "../ir_layout_strategy.h"
#include "../include/ir_core.h"
#include "../layout_helpers.h"
#include <stdlib.h>

static void measure_image(IRComponent* comp, IRLayoutConstraints* constraints) {
    if (!comp || !constraints) return;

    if (!layout_ensure_state(comp)) return;

    // Default image size
    float width = 100;
    float height = 100;

    // Get actual image size from custom_data (if loaded)
    // This would query the image asset

    // Apply style constraints
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

    // Maintain aspect ratio if only one dimension specified
    // ...

    // Apply constraints
    layout_apply_constraints(&width, &height, *constraints);

    // Set final layout dimensions
    layout_set_final_dimensions(comp, width, height);
}

static void layout_image_children(IRComponent* comp) {
    // Images have no children
    (void)comp;
}

void ir_register_image_layout_strategy(void) {
    static IRLayoutStrategy strategy = {
        .type = IR_COMPONENT_IMAGE,
        .measure = measure_image,
        .layout_children = layout_image_children,
        .intrinsic_size = NULL
    };

    ir_layout_register_strategy(&strategy);
}
