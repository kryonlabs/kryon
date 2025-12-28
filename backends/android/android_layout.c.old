/*
 * Android Layout Computation
 *
 * Computes render bounds for IR components before rendering.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "android_internal.h"

// ============================================================================
// LAYOUT COMPUTATION
// ============================================================================

void compute_component_layout(IRComponent* component,
                              float parent_width,
                              float parent_height) {
    if (!component) return;

    // Default to full parent size
    component->render_bounds.x = 0;
    component->render_bounds.y = 0;
    component->render_bounds.width = parent_width;
    component->render_bounds.height = parent_height;

    // Apply explicit width/height from style
    if (component->style) {
        if (component->style->width > 0) {
            component->render_bounds.width = component->style->width;
        }
        if (component->style->height > 0) {
            component->render_bounds.height = component->style->height;
        }
    }

    // Layout children based on component type
    if (!component->children || component->children_count == 0) {
        return;
    }

    float content_width = component->render_bounds.width;
    float content_height = component->render_bounds.height;

    // Apply padding
    float padding = component->style ? component->style->padding : 0.0f;
    float padding_left = component->style ? component->style->padding_left : padding;
    float padding_right = component->style ? component->style->padding_right : padding;
    float padding_top = component->style ? component->style->padding_top : padding;
    float padding_bottom = component->style ? component->style->padding_bottom : padding;

    float available_width = content_width - padding_left - padding_right;
    float available_height = content_height - padding_top - padding_bottom;
    float gap = component->style ? component->style->gap : 0.0f;

    switch (component->type) {
        case IR_COMPONENT_ROW: {
            // Horizontal layout
            float x_offset = padding_left;
            float total_flex = 0.0f;
            float fixed_width = 0.0f;

            // Calculate total flex and fixed width
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];
                if (child->style && child->style->flex_grow > 0) {
                    total_flex += child->style->flex_grow;
                } else if (child->style && child->style->width > 0) {
                    fixed_width += child->style->width;
                } else {
                    // Default child width
                    fixed_width += 100.0f;
                }
                if (i > 0) fixed_width += gap;
            }

            float flex_space = available_width - fixed_width;

            // Layout children
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];

                if (i > 0) x_offset += gap;

                float child_width;
                if (child->style && child->style->flex_grow > 0 && total_flex > 0) {
                    child_width = (flex_space * child->style->flex_grow) / total_flex;
                } else if (child->style && child->style->width > 0) {
                    child_width = child->style->width;
                } else {
                    child_width = 100.0f;
                }

                float child_height = child->style && child->style->height > 0 ?
                    child->style->height : available_height;

                compute_component_layout(child, child_width, child_height);

                child->render_bounds.x = x_offset;
                child->render_bounds.y = padding_top;

                x_offset += child_width;
            }
            break;
        }

        case IR_COMPONENT_COLUMN: {
            // Vertical layout
            float y_offset = padding_top;
            float total_flex = 0.0f;
            float fixed_height = 0.0f;

            // Calculate total flex and fixed height
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];
                if (child->style && child->style->flex_grow > 0) {
                    total_flex += child->style->flex_grow;
                } else if (child->style && child->style->height > 0) {
                    fixed_height += child->style->height;
                } else {
                    // Default child height
                    fixed_height += 50.0f;
                }
                if (i > 0) fixed_height += gap;
            }

            float flex_space = available_height - fixed_height;

            // Layout children
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];

                if (i > 0) y_offset += gap;

                float child_height;
                if (child->style && child->style->flex_grow > 0 && total_flex > 0) {
                    child_height = (flex_space * child->style->flex_grow) / total_flex;
                } else if (child->style && child->style->height > 0) {
                    child_height = child->style->height;
                } else {
                    child_height = 50.0f;
                }

                float child_width = child->style && child->style->width > 0 ?
                    child->style->width : available_width;

                compute_component_layout(child, child_width, child_height);

                child->render_bounds.x = padding_left;
                child->render_bounds.y = y_offset;

                y_offset += child_height;
            }
            break;
        }

        case IR_COMPONENT_STACK: {
            // Stack layout - children overlap at 0,0
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];

                float child_width = child->style && child->style->width > 0 ?
                    child->style->width : available_width;
                float child_height = child->style && child->style->height > 0 ?
                    child->style->height : available_height;

                compute_component_layout(child, child_width, child_height);

                child->render_bounds.x = padding_left;
                child->render_bounds.y = padding_top;
            }
            break;
        }

        case IR_COMPONENT_CENTER: {
            // Center single child
            if (component->children_count > 0) {
                IRComponent* child = &component->children[0];

                float child_width = child->style && child->style->width > 0 ?
                    child->style->width : 100.0f;
                float child_height = child->style && child->style->height > 0 ?
                    child->style->height : 50.0f;

                compute_component_layout(child, child_width, child_height);

                child->render_bounds.x = (content_width - child_width) / 2.0f;
                child->render_bounds.y = (content_height - child_height) / 2.0f;
            }
            break;
        }

        default:
            // Default layout - fill parent
            for (int i = 0; i < component->children_count; i++) {
                IRComponent* child = &component->children[i];

                float child_width = child->style && child->style->width > 0 ?
                    child->style->width : available_width;
                float child_height = child->style && child->style->height > 0 ?
                    child->style->height : available_height;

                compute_component_layout(child, child_width, child_height);

                child->render_bounds.x = padding_left;
                child->render_bounds.y = padding_top;
            }
            break;
    }
}
