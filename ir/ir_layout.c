// IR Layout Engine - Unified flexbox layout system integrated with IR
// Moved from /core/layout.c and adapted for IR architecture

#include "ir_core.h"
#include "ir_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Layout Cache & Dirty Tracking
// ============================================================================

void ir_layout_mark_dirty(IRComponent* component) {
    if (!component) return;

    component->dirty_flags |= IR_DIRTY_LAYOUT;

    // Propagate dirty flag upward to parents
    IRComponent* parent = component->parent;
    while (parent) {
        parent->dirty_flags |= IR_DIRTY_SUBTREE;
        parent = parent->parent;
    }
}

void ir_layout_invalidate_subtree(IRComponent* component) {
    if (!component) return;

    component->dirty_flags |= IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE;
    component->layout_cache.dirty = true;

    // Recursively invalidate all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_layout_invalidate_subtree(component->children[i]);
    }
}

// Invalidate layout cache when properties change
void ir_layout_invalidate_cache(IRComponent* component) {
    if (!component) return;

    component->layout_cache.dirty = true;
    component->layout_cache.cache_generation++;
    ir_layout_mark_dirty(component);
}

// ============================================================================
// Intrinsic Size Calculation (with caching)
// ============================================================================

static float ir_get_text_width_estimate(const char* text, float font_size) {
    if (!text) return 0.0f;

    // Improved text width estimation (same algorithm as old layout.c)
    uint32_t text_len = strlen(text);
    float char_width = font_size * 0.5f;

    return text_len * char_width;
}

static float ir_get_component_intrinsic_width_impl(IRComponent* component) {
    if (!component || !component->style) return 100.0f;

    IRStyle* style = component->style;

    // Use explicit width if set
    if (style->width.type == IR_DIMENSION_PX) {
        return style->width.value;
    }

    // Calculate based on component type and content
    switch (component->type) {
        case IR_COMPONENT_TEXT:
            if (component->text_content) {
                float font_size = style->font.size > 0 ? style->font.size : 16.0f;
                return ir_get_text_width_estimate(component->text_content, font_size);
            }
            return 50.0f;

        case IR_COMPONENT_BUTTON:
            if (component->text_content) {
                float font_size = style->font.size > 0 ? style->font.size : 14.0f;
                float text_width = ir_get_text_width_estimate(component->text_content, font_size);
                float padding = style->padding.left + style->padding.right;
                return text_width + padding + 20.0f; // Extra space for button chrome
            }
            return 80.0f;

        case IR_COMPONENT_INPUT:
            return 200.0f; // Default input width

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // Container width depends on children and layout direction
            if (component->child_count > 0 && component->layout) {
                float total_width = 0.0f;
                float max_width = 0.0f;

                bool is_row = (component->layout->flex.direction == 1);

                for (uint32_t i = 0; i < component->child_count; i++) {
                    IRComponent* child = component->children[i];
                    if (!child->style || !child->style->visible) continue;

                    float child_width = ir_get_component_intrinsic_width_impl(child);
                    child_width += child->style->margin.left + child->style->margin.right;

                    if (is_row) {
                        total_width += child_width;
                        if (i > 0) total_width += component->layout->flex.gap;
                    } else {
                        if (child_width > max_width) max_width = child_width;
                    }
                }

                float result = is_row ? total_width : max_width;
                result += style->padding.left + style->padding.right;
                return result;
            }
            return 100.0f;

        default:
            return 100.0f;
    }
}

float ir_get_component_intrinsic_width(IRComponent* component) {
    if (!component) return 0.0f;

    // Check cache
    if (!component->layout_cache.dirty && component->layout_cache.cached_intrinsic_width > 0) {
        return component->layout_cache.cached_intrinsic_width;
    }

    // Calculate and cache
    float width = ir_get_component_intrinsic_width_impl(component);
    component->layout_cache.cached_intrinsic_width = width;

    return width;
}

static float ir_get_component_intrinsic_height_impl(IRComponent* component) {
    if (!component || !component->style) return 50.0f;

    IRStyle* style = component->style;

    // Use explicit height if set
    if (style->height.type == IR_DIMENSION_PX) {
        return style->height.value;
    }

    // Calculate based on component type
    switch (component->type) {
        case IR_COMPONENT_TEXT:
            return style->font.size > 0 ? style->font.size + 4.0f : 20.0f;

        case IR_COMPONENT_BUTTON: {
            float font_size = style->font.size > 0 ? style->font.size : 14.0f;
            float padding = style->padding.top + style->padding.bottom;
            return font_size + padding + 12.0f; // Extra space for button chrome
        }

        case IR_COMPONENT_INPUT:
            return 30.0f;

        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            if (component->child_count > 0 && component->layout) {
                float total_height = 0.0f;
                float max_height = 0.0f;

                bool is_column = (component->layout->flex.direction == 0);

                for (uint32_t i = 0; i < component->child_count; i++) {
                    IRComponent* child = component->children[i];
                    if (!child->style || !child->style->visible) continue;

                    float child_height = ir_get_component_intrinsic_height_impl(child);
                    child_height += child->style->margin.top + child->style->margin.bottom;

                    if (is_column) {
                        total_height += child_height;
                        if (i > 0) total_height += component->layout->flex.gap;
                    } else {
                        if (child_height > max_height) max_height = child_height;
                    }
                }

                float result = is_column ? total_height : max_height;
                result += style->padding.top + style->padding.bottom;
                return result;
            }
            return 50.0f;

        default:
            return 50.0f;
    }
}

float ir_get_component_intrinsic_height(IRComponent* component) {
    if (!component) return 0.0f;

    // Check cache
    if (!component->layout_cache.dirty && component->layout_cache.cached_intrinsic_height > 0) {
        return component->layout_cache.cached_intrinsic_height;
    }

    // Calculate and cache
    float height = ir_get_component_intrinsic_height_impl(component);
    component->layout_cache.cached_intrinsic_height = height;

    return height;
}

// ============================================================================
// Layout Computation (Flexbox Algorithm)
// ============================================================================

static float resolve_dimension(IRDimension dim, float parent_size) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            return dim.value;
        case IR_DIMENSION_PERCENT:
            return parent_size * (dim.value / 100.0f);
        case IR_DIMENSION_AUTO:
            return 0.0f; // Will be calculated from content
        case IR_DIMENSION_FLEX:
            return 0.0f; // Will be calculated from flex grow/shrink
        default:
            return 0.0f;
    }
}

static void ir_layout_compute_row(IRComponent* container, float available_width, float available_height);
static void ir_layout_compute_column(IRComponent* container, float available_width, float available_height);

void ir_layout_compute(IRComponent* root, float available_width, float available_height) {
    if (!root) return;

    // Skip layout if component is clean (not dirty)
    if ((root->dirty_flags & (IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE)) == 0) {
        return;
    }

    // Ensure style and layout exist
    if (!root->style) {
        root->style = ir_create_style();
    }
    if (!root->layout) {
        root->layout = (IRLayout*)calloc(1, sizeof(IRLayout));
    }

    IRStyle* style = root->style;
    IRLayout* layout = root->layout;

    // Resolve dimensions
    float width = resolve_dimension(style->width, available_width);
    float height = resolve_dimension(style->height, available_height);

    // Use intrinsic size if not explicitly set
    if (width == 0.0f) {
        width = available_width > 0 ? available_width : ir_get_component_intrinsic_width(root);
    }
    if (height == 0.0f) {
        height = available_height > 0 ? available_height : ir_get_component_intrinsic_height(root);
    }

    // Apply aspect ratio constraint
    if (layout->aspect_ratio > 0.0f) {
        if (style->width.type != IR_DIMENSION_AUTO && style->height.type == IR_DIMENSION_AUTO) {
            height = width / layout->aspect_ratio;
        } else if (style->height.type != IR_DIMENSION_AUTO && style->width.type == IR_DIMENSION_AUTO) {
            width = height * layout->aspect_ratio;
        }
    }

    // Set positioned bounds (for absolute positioning)
    if (style->position_mode == IR_POSITION_ABSOLUTE) {
        root->rendered_bounds.x = style->absolute_x;
        root->rendered_bounds.y = style->absolute_y;
    } else {
        root->rendered_bounds.x = 0.0f;
        root->rendered_bounds.y = 0.0f;
    }

    root->rendered_bounds.width = width;
    root->rendered_bounds.height = height;
    root->rendered_bounds.valid = true;

    // Calculate inner dimensions for children (subtract padding)
    float inner_width = width - (style->padding.left + style->padding.right);
    float inner_height = height - (style->padding.top + style->padding.bottom);

    if (inner_width < 0) inner_width = 0;
    if (inner_height < 0) inner_height = 0;

    // Layout children based on flex direction
    if (root->child_count > 0) {
        if (layout->flex.direction == 1) {
            // Row layout
            ir_layout_compute_row(root, inner_width, inner_height);
        } else {
            // Column layout (direction == 0 or default)
            ir_layout_compute_column(root, inner_width, inner_height);
        }
    }

    // Recursively layout children
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        if (child->style && child->style->visible) {
            ir_layout_compute(child, child->rendered_bounds.width, child->rendered_bounds.height);
        }
    }

    // Mark as clean
    root->dirty_flags &= ~(IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE);
    root->layout_cache.dirty = false;
}

static void ir_layout_compute_row(IRComponent* container, float available_width, float available_height) {
    if (!container) return;

    IRLayout* layout = container->layout;
    IRStyle* style = container->style;

    // First pass: calculate intrinsic sizes and total flex grow
    float total_width = 0.0f;
    float total_flex_grow = 0.0f;
    uint32_t visible_count = 0;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        visible_count++;

        float child_width = resolve_dimension(child->style->width, available_width);
        if (child_width == 0.0f) {
            child_width = ir_get_component_intrinsic_width(child);
        }

        child_width += child->style->margin.left + child->style->margin.right;
        total_width += child_width;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        }
    }

    // Add gaps
    if (visible_count > 1) {
        total_width += layout->flex.gap * (visible_count - 1);
    }

    // Second pass: distribute remaining space and position children
    float remaining_width = available_width - total_width;
    float current_x = style->padding.left;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        float child_width = resolve_dimension(child->style->width, available_width);
        if (child_width == 0.0f) {
            child_width = ir_get_component_intrinsic_width(child);
        }

        // Apply flex grow
        if (remaining_width > 0 && total_flex_grow > 0 && child->layout && child->layout->flex.grow > 0) {
            float extra = remaining_width * (child->layout->flex.grow / total_flex_grow);
            child_width += extra;
        }

        float child_height = resolve_dimension(child->style->height, available_height);
        if (child_height == 0.0f) {
            child_height = ir_get_component_intrinsic_height(child);
        }

        // Apply cross-axis alignment (vertical)
        float child_y = style->padding.top + child->style->margin.top;
        if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_y += (available_height - child_height) / 2.0f;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_y += available_height - child_height;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_height = available_height - child->style->margin.top - child->style->margin.bottom;
        }

        // Set child bounds
        child->rendered_bounds.x = current_x + child->style->margin.left;
        child->rendered_bounds.y = child_y;
        child->rendered_bounds.width = child_width;
        child->rendered_bounds.height = child_height;
        child->rendered_bounds.valid = true;

        current_x += child_width + child->style->margin.left + child->style->margin.right + layout->flex.gap;
    }
}

static void ir_layout_compute_column(IRComponent* container, float available_width, float available_height) {
    if (!container) return;

    IRLayout* layout = container->layout;
    IRStyle* style = container->style;

    // First pass: calculate intrinsic sizes and total flex grow
    float total_height = 0.0f;
    float total_flex_grow = 0.0f;
    uint32_t visible_count = 0;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        visible_count++;

        float child_height = resolve_dimension(child->style->height, available_height);
        if (child_height == 0.0f) {
            child_height = ir_get_component_intrinsic_height(child);
        }

        child_height += child->style->margin.top + child->style->margin.bottom;
        total_height += child_height;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        }
    }

    // Add gaps
    if (visible_count > 1) {
        total_height += layout->flex.gap * (visible_count - 1);
    }

    // Second pass: distribute remaining space and position children
    float remaining_height = available_height - total_height;
    float current_y = style->padding.top;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        float child_height = resolve_dimension(child->style->height, available_height);
        if (child_height == 0.0f) {
            child_height = ir_get_component_intrinsic_height(child);
        }

        // Apply flex grow
        if (remaining_height > 0 && total_flex_grow > 0 && child->layout && child->layout->flex.grow > 0) {
            float extra = remaining_height * (child->layout->flex.grow / total_flex_grow);
            child_height += extra;
        }

        float child_width = resolve_dimension(child->style->width, available_width);
        if (child_width == 0.0f) {
            child_width = ir_get_component_intrinsic_width(child);
        }

        // Apply cross-axis alignment (horizontal)
        float child_x = style->padding.left + child->style->margin.left;
        if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_x += (available_width - child_width) / 2.0f;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_x += available_width - child_width;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_width = available_width - child->style->margin.left - child->style->margin.right;
        }

        // Set child bounds
        child->rendered_bounds.x = child_x;
        child->rendered_bounds.y = current_y + child->style->margin.top;
        child->rendered_bounds.width = child_width;
        child->rendered_bounds.height = child_height;
        child->rendered_bounds.valid = true;

        current_y += child_height + child->style->margin.top + child->style->margin.bottom + layout->flex.gap;
    }
}
