// IR Hit Test Module
// Hit testing functions extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_hit_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Hit Testing - Find component at a given point
// ============================================================================

bool ir_is_point_in_component(IRComponent* component, float x, float y) {
    if (!component || !component->rendered_bounds.valid) {
        return false;
    }

    float comp_x = component->rendered_bounds.x;
    float comp_y = component->rendered_bounds.y;
    float comp_w = component->rendered_bounds.width;
    float comp_h = component->rendered_bounds.height;

    return (x >= comp_x && x < comp_x + comp_w &&
            y >= comp_y && y < comp_y + comp_h);
}

IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y) {
    if (!root) {
        return NULL;
    }

    // Skip invisible components - they shouldn't be interactive
    if (root->style && !root->style->visible) {
        return NULL;
    }

    if (!ir_is_point_in_component(root, x, y)) {
        return NULL;
    }

    // Find the child with highest z-index that contains the point
    IRComponent* best_target = NULL;
    uint32_t best_z_index = 0;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        if (!child) continue;

        // Check if point is in this child's bounds
        if (ir_is_point_in_component(child, x, y)) {
            // Recursively find target in this child's subtree
            IRComponent* child_target = ir_find_component_at_point(child, x, y);
            if (child_target != NULL) {
                // Get the effective z-index
                uint32_t effective_z = child_target->style ? child_target->style->z_index : 0;

                if (best_target == NULL || effective_z > best_z_index) {
                    best_z_index = effective_z;
                    best_target = child_target;
                }
            }
        }
    }

    if (best_target != NULL) {
        return best_target;
    }

    // No child handled the event, return this component
    return root;
}

void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height) {
    if (!component) return;

    component->rendered_bounds.x = x;
    component->rendered_bounds.y = y;
    component->rendered_bounds.width = width;
    component->rendered_bounds.height = height;
    component->rendered_bounds.valid = true;
}
