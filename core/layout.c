#include "include/kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Layout Constants and Enums
// ============================================================================

typedef enum {
    KRYON_LAYOUT_ROW = 0,
    KRYON_LAYOUT_COLUMN = 1,
    KRYON_LAYOUT_ABSOLUTE = 2
} kryon_layout_direction_t;

// ============================================================================
// Layout Context for Pass Information
// ============================================================================

typedef struct {
    kryon_fp_t available_width;
    kryon_fp_t available_height;
    kryon_fp_t current_x;
    kryon_fp_t current_y;
    kryon_fp_t line_height;
    uint8_t child_index;
    bool line_break_pending;
} kryon_layout_context_t;

static bool kryon_layout_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        const char* env = getenv("KRYON_TRACE_LAYOUT");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
#endif
    }
    return enabled;
}

// ============================================================================
// Layout Helper Functions
// ============================================================================

static kryon_fp_t get_component_intrinsic_width(kryon_component_t* component) {
    if (component == NULL) return 0;

    // Use explicit width if set
    if (component->width > 0) {
        return component->width;
    }

    // Calculate based on content (simplified)
    // Use string comparison to identify component types
    if (component->ops == &kryon_text_ops) {
        kryon_text_state_t* text_state = (kryon_text_state_t*)component->state;
        if (text_state && text_state->text) {
            // Estimate text width (platform-specific implementation needed)
            uint32_t text_len = strlen(text_state->text);
            return KRYON_FP_FROM_INT(text_len * 8); // 8 pixels per character (estimate)
        }
        return KRYON_FP_FROM_INT(50); // Default text width
    }
    else if (component->ops == &kryon_button_ops) {
        kryon_button_state_t* button_state = (kryon_button_state_t*)component->state;
        if (button_state && button_state->text) {
            uint32_t text_len = strlen(button_state->text);
            return KRYON_FP_FROM_INT(text_len * 8 + 20); // Text width + padding
        }
        return KRYON_FP_FROM_INT(80); // Default button width
    }

    return KRYON_FP_FROM_INT(100); // Default width for containers
}

static kryon_fp_t get_component_intrinsic_height(kryon_component_t* component) {
    if (component == NULL) return 0;

    // Use explicit height if set
    if (component->height > 0) {
        return component->height;
    }

    // Calculate based on content (simplified)
    if (component->ops == &kryon_text_ops) {
        return KRYON_FP_FROM_INT(20); // Standard text height
    }
    else if (component->ops == &kryon_button_ops) {
        return KRYON_FP_FROM_INT(30); // Standard button height
    }

    return KRYON_FP_FROM_INT(50); // Default height for containers
}

static kryon_fp_t calculate_total_flex_grow(kryon_component_t* container) {
    if (container == NULL) return 0;

    kryon_fp_t total_grow = 0;
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (child->visible && child->flex_grow > 0) {
            total_grow += KRYON_FP_FROM_INT(child->flex_grow);
        }
    }

    return total_grow;
}

static void apply_alignment(kryon_component_t* component, kryon_fp_t available_space,
                           kryon_alignment_t alignment, bool is_main_axis) {
    if (component == NULL) return;

    switch (alignment) {
        case KRYON_ALIGN_START:
            // No offset needed
            break;

        case KRYON_ALIGN_CENTER:
            if (is_main_axis) {
                component->x += available_space / 2;
            } else {
                component->y += available_space / 2;
            }
            break;

        case KRYON_ALIGN_END:
            if (is_main_axis) {
                component->x += available_space;
            } else {
                component->y += available_space;
            }
            break;

        case KRYON_ALIGN_STRETCH:
            if (is_main_axis) {
                component->width += available_space;
            } else {
                component->height += available_space;
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// Row Layout Implementation
// ============================================================================

static void layout_row(kryon_component_t* container, kryon_layout_context_t* ctx) {
    if (container == NULL || ctx == NULL) return;

    kryon_fp_t container_width = container->width;
    kryon_fp_t container_height = container->height;
    kryon_fp_t current_x = KRYON_FP_FROM_INT(container->margin_left);
    kryon_fp_t current_y = KRYON_FP_FROM_INT(container->margin_top);
    kryon_fp_t line_height = 0;
    kryon_fp_t used_width = current_x;

    // First pass: calculate intrinsic sizes and handle wrapping
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        kryon_fp_t child_width = get_component_intrinsic_width(child);
        kryon_fp_t child_height = get_component_intrinsic_height(child);

        // Add margins
        child_width += KRYON_FP_FROM_INT(child->margin_left + child->margin_right);
        child_height += KRYON_FP_FROM_INT(child->margin_top + child->margin_bottom);

        // Check if we need to wrap to next line
        if (used_width + child_width > container_width && i > 0) {
            // Move to next line
            current_x = KRYON_FP_FROM_INT(container->margin_left);
            current_y += line_height;
            used_width = current_x;
            line_height = 0;
        }

        // Position child
        child->x = current_x + KRYON_FP_FROM_INT(child->margin_left);
        child->y = current_y + KRYON_FP_FROM_INT(child->margin_top);
        child->width = child_width - KRYON_FP_FROM_INT(child->margin_left + child->margin_right);
        child->height = child_height - KRYON_FP_FROM_INT(child->margin_top + child->margin_bottom);

        // Update line tracking
        if (child_height > line_height) {
            line_height = child_height;
        }

        current_x += child_width;
        used_width += child_width;
    }

    // Second pass: apply flex grow and alignment
    kryon_fp_t remaining_width = container_width - used_width;
    kryon_fp_t total_flex_grow = calculate_total_flex_grow(container);

    if (total_flex_grow > 0 && remaining_width > 0) {
        // Distribute remaining space to flexible children
        for (uint8_t i = 0; i < container->child_count; i++) {
            kryon_component_t* child = container->children[i];
            if (!child->visible || child->flex_grow == 0) continue;

            kryon_fp_t extra_space = KRYON_FP_MUL(remaining_width,
                KRYON_FP_DIV(KRYON_FP_FROM_INT(child->flex_grow), total_flex_grow));
            child->width += extra_space;
        }
    }

    // Apply cross-axis alignment (vertical centering for now)
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        kryon_fp_t extra_height = line_height - child->height;
        if (extra_height > 0) {
            child->y += extra_height / 2; // Center vertically
        }
    }
}

// ============================================================================
// Column Layout Implementation
// ============================================================================

static void layout_column(kryon_component_t* container, kryon_layout_context_t* ctx) {
    if (container == NULL || ctx == NULL) return;

    kryon_fp_t padding_left = KRYON_FP_FROM_INT(container->padding_left);
    kryon_fp_t padding_right = KRYON_FP_FROM_INT(container->padding_right);
    kryon_fp_t padding_top = KRYON_FP_FROM_INT(container->padding_top);
    kryon_fp_t padding_bottom = KRYON_FP_FROM_INT(container->padding_bottom);

    kryon_fp_t container_width = container->width;
    if (container_width == 0) {
        container_width = ctx->available_width;
    }

    kryon_fp_t inner_width = container_width - padding_left - padding_right;
    if (inner_width < 0) inner_width = 0;

    kryon_fp_t origin_x = container->x + padding_left;
    kryon_fp_t origin_y = container->y + padding_top;

    kryon_fp_t total_content_height = 0;
    kryon_fp_t total_flex_grow = 0;

    // Measure children and assign base dimensions
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        kryon_fp_t margin_left = KRYON_FP_FROM_INT(child->margin_left);
        kryon_fp_t margin_right = KRYON_FP_FROM_INT(child->margin_right);
        kryon_fp_t margin_top = KRYON_FP_FROM_INT(child->margin_top);
        kryon_fp_t margin_bottom = KRYON_FP_FROM_INT(child->margin_bottom);

        kryon_fp_t available_width = inner_width - (margin_left + margin_right);
        if (available_width < 0) available_width = 0;

        kryon_fp_t child_width = child->width;
        if (child_width == 0) {
            child_width = get_component_intrinsic_width(child);
        }
        if (child_width == 0 || child_width > available_width) {
            child_width = available_width;
        }

        kryon_fp_t child_height = child->height;
        if (child_height == 0) {
            child_height = get_component_intrinsic_height(child);
        }
        if (child_height == 0) {
            child_height = KRYON_FP_FROM_INT(20);
        }

        child->width = child_width;
        child->height = child_height;

        total_content_height += child->height + margin_top + margin_bottom;

        if (child->flex_grow > 0) {
            total_flex_grow += KRYON_FP_FROM_INT(child->flex_grow);
        }
    }

    kryon_fp_t target_inner_height = container->height > 0
        ? container->height - padding_top - padding_bottom
        : ctx->available_height > 0
            ? ctx->available_height - padding_top - padding_bottom
            : total_content_height;

    if (target_inner_height < 0) {
        target_inner_height = total_content_height;
    }

    kryon_fp_t remaining_height = target_inner_height - total_content_height;
    if (remaining_height < 0) {
        remaining_height = 0;
    }

    if (remaining_height > 0 && total_flex_grow > 0) {
        for (uint8_t i = 0; i < container->child_count; i++) {
            kryon_component_t* child = container->children[i];
            if (!child->visible || child->flex_grow == 0) continue;

            kryon_fp_t ratio = KRYON_FP_DIV(KRYON_FP_FROM_INT(child->flex_grow), total_flex_grow);
            kryon_fp_t extra = KRYON_FP_MUL(remaining_height, ratio);
            child->height += extra;
            total_content_height += extra;
        }
    }

    // Handle flexible width for children with flexGrow
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible || child->flex_grow == 0) continue;

        // If child has flex_grow, make it fill the available width
        child->width = inner_width;
    }

    // Determine alignment offsets
    kryon_fp_t available_height_for_alignment = target_inner_height;
    if (available_height_for_alignment < total_content_height) {
        available_height_for_alignment = total_content_height;
    }

    kryon_fp_t alignment_offset = 0;
    switch (container->justify_content) {
        case KRYON_ALIGN_CENTER:
            if (available_height_for_alignment > total_content_height) {
                alignment_offset = (available_height_for_alignment - total_content_height) / 2;
            }
            break;
        case KRYON_ALIGN_END:
            if (available_height_for_alignment > total_content_height) {
                alignment_offset = available_height_for_alignment - total_content_height;
            }
            break;
        case KRYON_ALIGN_STRETCH:
        default:
            break;
    }

    // Position children
    kryon_fp_t cursor_y = origin_y + alignment_offset;
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        const bool has_explicit_x = (child->layout_flags & KRYON_COMPONENT_FLAG_HAS_X) != 0;
        const bool has_explicit_y = (child->layout_flags & KRYON_COMPONENT_FLAG_HAS_Y) != 0;
        const kryon_fp_t explicit_x = child->explicit_x;
        const kryon_fp_t explicit_y = child->explicit_y;

        kryon_fp_t margin_left = KRYON_FP_FROM_INT(child->margin_left);
        kryon_fp_t margin_right = KRYON_FP_FROM_INT(child->margin_right);
        kryon_fp_t margin_top = KRYON_FP_FROM_INT(child->margin_top);
        kryon_fp_t margin_bottom = KRYON_FP_FROM_INT(child->margin_bottom);

        kryon_fp_t available_width = inner_width - (margin_left + margin_right);
        if (available_width < 0) available_width = 0;

        switch (container->align_items) {
            case KRYON_ALIGN_CENTER:
                child->x = origin_x + margin_left + (available_width > child->width
                    ? (available_width - child->width) / 2 : 0);
                break;
            case KRYON_ALIGN_END:
                child->x = origin_x + margin_left + (available_width > child->width
                    ? (available_width - child->width) : 0);
                break;
            case KRYON_ALIGN_STRETCH:
                child->x = origin_x + margin_left;
                child->width = available_width;
                break;
            case KRYON_ALIGN_START:
            default:
                child->x = origin_x + margin_left;
                break;
        }

        if (has_explicit_x) {
            child->x += explicit_x;
        }

        cursor_y += margin_top;
        child->y = cursor_y;
        if (has_explicit_y) {
            child->y += explicit_y;
        }
        cursor_y += child->height + margin_bottom;
    }

    if (container->height == 0) {
        container->height = (cursor_y - container->y) + padding_bottom;
    }
}

// ============================================================================
// Absolute Layout Implementation
// ============================================================================

static void layout_absolute(kryon_component_t* container, kryon_layout_context_t* ctx) {
    if (container == NULL || ctx == NULL) return;

    kryon_fp_t container_width = container->width;
    kryon_fp_t container_height = container->height;

    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        // Use explicit position if set, otherwise use current position
        kryon_fp_t child_x = (child->x > 0) ? child->x : KRYON_FP_FROM_INT(child->margin_left);
        kryon_fp_t child_y = (child->y > 0) ? child->y : KRYON_FP_FROM_INT(child->margin_top);

        // Calculate size
        kryon_fp_t child_width = get_component_intrinsic_width(child);
        kryon_fp_t child_height = get_component_intrinsic_height(child);

        // Clamp to container bounds
        if (child_x + child_width > container_width) {
            child_width = container_width - child_x;
        }
        if (child_y + child_height > container_height) {
            child_height = container_height - child_y;
        }

        child->x = child_x;
        child->y = child_y;
        child->width = child_width;
        child->height = child_height;
    }
}

// ============================================================================
// Public column layout entry point (exported helper)
// ============================================================================

void kryon_layout_apply_column(kryon_component_t* container,
                               kryon_fp_t available_width,
                               kryon_fp_t available_height) {
    if (container == NULL) return;

    kryon_layout_context_t ctx = {0};
    ctx.available_width = (available_width > 0) ? available_width : container->width;
    ctx.available_height = (available_height > 0) ? available_height : container->height;

    layout_column(container, &ctx);
}

// ============================================================================
// Main Layout Engine
// ============================================================================

static void layout_component_internal(kryon_component_t* component, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (component == NULL) return;

    // Respect explicit dimensions while still using available space when unset
    if (component->width == 0 && available_width > 0) {
        component->width = available_width;
    }
    if (component->height == 0 && available_height > 0) {
        component->height = available_height;
    }

    kryon_fp_t effective_width = component->width > 0 ? component->width : available_width;
    kryon_fp_t effective_height = component->height > 0 ? component->height : available_height;

    // Determine layout direction based on component type or properties
    // For now, use simple heuristics
    kryon_layout_direction_t direction = KRYON_LAYOUT_COLUMN; // Default

    // Create layout context
    kryon_layout_context_t ctx = {0};
    ctx.available_width = effective_width;
    ctx.available_height = effective_height;

    // Layout children based on direction
    switch (direction) {
        case KRYON_LAYOUT_ROW:
            layout_row(component, &ctx);
            break;

        case KRYON_LAYOUT_COLUMN:
            layout_column(component, &ctx);
            break;

        case KRYON_LAYOUT_ABSOLUTE:
            layout_absolute(component, &ctx);
            break;

        default:
            layout_column(component, &ctx);
            break;
    }

    // Recursively layout children
    for (uint8_t i = 0; i < component->child_count; i++) {
        kryon_component_t* child = component->children[i];
        if (child->visible && child->dirty) {
            layout_component_internal(child, child->width, child->height);
        }
    }

    kryon_component_mark_clean(component);
}

// ============================================================================
// Public Layout API
// ============================================================================

void kryon_layout_tree(kryon_component_t* root, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (root == NULL) return;

    // Start layout from root
    layout_component_internal(root, available_width, available_height);

    if (kryon_layout_should_trace()) {
        fprintf(stderr,
                "[kryon][layout] root w=%d h=%d (available=%d x %d raw=%f x %f) sizeof_fp=%zu no_float=%d\n",
                KRYON_FP_TO_INT(root->width),
                KRYON_FP_TO_INT(root->height),
                KRYON_FP_TO_INT(available_width),
                KRYON_FP_TO_INT(available_height),
                (double)available_width,
                (double)available_height,
                sizeof(kryon_fp_t),
                KRYON_NO_FLOAT);
        for (uint8_t i = 0; i < root->child_count; i++) {
            kryon_component_t* child = root->children[i];
            fprintf(stderr,
                    "[kryon][layout] child[%u] w=%d h=%d x=%d y=%d\n",
                    i,
                    KRYON_FP_TO_INT(child->width),
                    KRYON_FP_TO_INT(child->height),
                    KRYON_FP_TO_INT(child->x),
                    KRYON_FP_TO_INT(child->y));
        }
    }
}

void kryon_layout_component(kryon_component_t* component, kryon_fp_t available_width, kryon_fp_t available_height) {
    if (component == NULL) return;

    layout_component_internal(component, available_width, available_height);
}

void kryon_layout_invalidate(kryon_component_t* component) {
    if (component == NULL) return;

    // Mark component and all children as dirty
    kryon_component_mark_dirty(component);
    for (uint8_t i = 0; i < component->child_count; i++) {
        kryon_layout_invalidate(component->children[i]);
    }
}

// ============================================================================
// Layout Utility Functions
// ============================================================================

bool kryon_layout_is_point_in_component(kryon_component_t* component, int16_t x, int16_t y) {
    if (component == NULL || !component->visible) return false;

    int16_t comp_x = KRYON_FP_TO_INT(component->x);
    int16_t comp_y = KRYON_FP_TO_INT(component->y);
    int16_t comp_w = KRYON_FP_TO_INT(component->width);
    int16_t comp_h = KRYON_FP_TO_INT(component->height);

    return kryon_point_in_rect(x, y, comp_x, comp_y, comp_w, comp_h);
}

kryon_component_t* kryon_layout_find_component_at_point(kryon_component_t* root, int16_t x, int16_t y) {
    if (root == NULL || !kryon_layout_is_point_in_component(root, x, y)) {
        return NULL;
    }

    // Search children first (top-to-bottom for proper z-order)
    for (int8_t i = (int8_t)root->child_count - 1; i >= 0; i--) {
        kryon_component_t* child = root->children[i];
        kryon_component_t* found = kryon_layout_find_component_at_point(child, x, y);
        if (found != NULL) {
            return found;
        }
    }

    // No child contains the point, return this component
    return root;
}

void kryon_layout_get_bounds(kryon_component_t* component, int16_t* x, int16_t* y, uint16_t* w, uint16_t* h) {
    if (component == NULL) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }

    if (x) *x = KRYON_FP_TO_INT(component->x);
    if (y) *y = KRYON_FP_TO_INT(component->y);
    if (w) *w = KRYON_FP_TO_INT(component->width);
    if (h) *h = KRYON_FP_TO_INT(component->height);
}
