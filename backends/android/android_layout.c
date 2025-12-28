/*
 * Android Layout - Stub Implementation
 * TODO: Implement proper layout engine for Android
 */

#include "android_internal.h"

void android_layout_component(IRComponent* component, float available_width, float available_height) {
    if (!component) return;

    // Stub: Just set the rendered bounds to the available space
    component->rendered_bounds.x = 0;
    component->rendered_bounds.y = 0;
    component->rendered_bounds.width = available_width;
    component->rendered_bounds.height = available_height;

    // Layout children in a simple stack
    float current_y = 0;
    if (component->children) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* child = component->children[i];
            if (child) {
                child->rendered_bounds.x = 0;
                child->rendered_bounds.y = current_y;
                child->rendered_bounds.width = available_width;
                child->rendered_bounds.height = 50; // Fixed height for now
                current_y += 50;

                // Recursively layout children
                android_layout_component(child, available_width, 50);
            }
        }
    }
}
