#include "include/kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Layout Constants and Enums
// ============================================================================

typedef enum {
    KRYON_LAYOUT_COLUMN = 0,
    KRYON_LAYOUT_ROW = 1,
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

static bool kryon_coordinates_should_trace(void) {
    static bool initialized = false;
    static bool enabled = false;

    if (!initialized) {
        initialized = true;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
        const char* env = getenv("KRYON_TRACE_COORDINATES_FLOW");
        enabled = (env != NULL && env[0] != '\0' && env[0] != '0');
#endif
    }
    return enabled;
}

static void trace_coordinate_flow(const char* stage, kryon_component_t* component,
                                  kryon_fp_t x, kryon_fp_t y, kryon_fp_t width, kryon_fp_t height) {
    if (!kryon_coordinates_should_trace()) return;

    const char* component_type = "unknown";
    if (component->ops == &kryon_container_ops) component_type = "container";
    else if (component->ops == &kryon_text_ops) component_type = "text";
    else if (component->ops == &kryon_button_ops) component_type = "button";

    fprintf(stderr, "[kryon][coordinates] %s: component=%p type=%s "
            "x_fp=%f (int:%d) y_fp=%f (int:%d) w_fp=%f (int:%d) h_fp=%f (int:%d)\n",
            stage, (void*)component, component_type,
            kryon_fp_to_float(x), KRYON_FP_TO_INT(x),
            kryon_fp_to_float(y), KRYON_FP_TO_INT(y),
            kryon_fp_to_float(width), KRYON_FP_TO_INT(width),
            kryon_fp_to_float(height), KRYON_FP_TO_INT(height));
}

// ============================================================================
// Layout Helper Functions
// ============================================================================

static kryon_fp_t get_component_intrinsic_width(kryon_component_t* component) {
    if (component == NULL) return 0;

    // Use explicit width ONLY if user explicitly set it (not layout-calculated width)
    if ((component->layout_flags & KRYON_COMPONENT_FLAG_HAS_WIDTH) != 0) {
        return component->width;
    }

    // Calculate based on content (simplified)
    // Use string comparison to identify component types
    if (component->ops == &kryon_text_ops) {
        kryon_text_state_t* text_state = (kryon_text_state_t*)component->state;

        // Check if this Text component has inline children (rich text mode)
        if (component->child_count > 0 && text_state) {
            // Rich text mode - sum up children widths
            // Trigger layout to calculate proper dimensions
            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[kryon][layout] Text with %d inline children - triggering layout\n",
                        component->child_count);
            }

            // Call text_layout to flatten children and calculate dimensions
            // This will set component->width and component->height
            if (component->ops->layout) {
                component->ops->layout(component, 0, 0);  // Let it auto-size
            }

            return component->width > 0 ? component->width : KRYON_FP_FROM_INT(100);
        }

        if (text_state && text_state->text) {
            // Improved text width estimation based on font size and weight
            uint32_t text_len = strlen(text_state->text);
            uint8_t font_size = text_state->font_size > 0 ? text_state->font_size : 16; // Default 16px

            // Use 0.5 * font_size per character (better approximation than fixed 8px)
            float char_width = font_size * 0.5f;

            // Account for bold (20% wider) and italic (5% wider)
            if (text_state->font_weight == KRYON_FONT_WEIGHT_BOLD) {
                char_width *= 1.2f;
            }
            if (text_state->font_style == KRYON_FONT_STYLE_ITALIC) {
                char_width *= 1.05f;
            }

            int width = (int)(text_len * char_width);

            // Debug trace
            if (getenv("KRYON_TRACE_LAYOUT")) {
                fprintf(stderr, "[kryon][layout] get_intrinsic_width text='%s' len=%u font_size=%u weight=%u style=%u char_width=%.2f calculated_width=%d\n",
                        text_state->text, text_len, font_size, text_state->font_weight, text_state->font_style, char_width, width);
            }

            return KRYON_FP_FROM_INT(width);
        }
        return KRYON_FP_FROM_INT(50); // Default text width
    }
    else if (component->ops == &kryon_button_ops) {
        kryon_button_state_t* button_state = (kryon_button_state_t*)component->state;
        if (button_state && button_state->text) {
            uint32_t text_len = strlen(button_state->text);
            uint16_t padding = (uint16_t)(component->padding_left + component->padding_right);
            uint16_t total_padding = padding > 0 ? padding : 20;
            return KRYON_FP_FROM_INT((int)(text_len * 8 + total_padding)); // Text width + measured padding
        }
        return KRYON_FP_FROM_INT(80); // Default button width
    }

    return KRYON_FP_FROM_INT(100); // Default width for containers
}

static kryon_fp_t get_component_intrinsic_height(kryon_component_t* component) {
    if (component == NULL) return 0;

    // Use explicit height ONLY if user explicitly set it (not layout-calculated height)
    if ((component->layout_flags & KRYON_COMPONENT_FLAG_HAS_HEIGHT) != 0) {
        return component->height;
    }

    // Calculate based on content (simplified)
    if (component->ops == &kryon_text_ops) {
        // If this Text component has inline children (rich text mode), use calculated height
        if (component->child_count > 0) {
            // Layout should have been called during width calculation
            return component->height > 0 ? component->height : KRYON_FP_FROM_INT(20);
        }
        return KRYON_FP_FROM_INT(20); // Standard text height
    }
    else if (component->ops == &kryon_button_ops) {
        uint16_t vertical_padding = (uint16_t)(component->padding_top + component->padding_bottom);
        uint16_t base_height = 30;
        if (vertical_padding > 0) {
            uint16_t padding_height = (uint16_t)(vertical_padding + 14); // include text baseline
            if (padding_height > base_height) base_height = padding_height;
        }
        return KRYON_FP_FROM_INT(base_height); // Button height reflects padding
    }

    // Estimate container height from children when possible
    if (component->child_count > 0) {
        uint16_t padding = (uint16_t)(component->padding_top + component->padding_bottom);
        uint16_t computed_height = 0;
        kryon_layout_direction_t direction = (kryon_layout_direction_t)component->layout_direction;
        if (direction == KRYON_LAYOUT_ROW) {
            uint16_t max_child = 0;
            for (uint8_t i = 0; i < component->child_count; i++) {
                kryon_component_t* child = component->children[i];
                if (child == NULL || !child->visible) {
                    continue;
                }
                int child_height = KRYON_FP_TO_INT(child->height);
                if (child_height == 0) {
                    child_height = KRYON_FP_TO_INT(get_component_intrinsic_height(child));
                }
                if (child_height > max_child) {
                    max_child = (uint16_t)child_height;
                }
            }
            computed_height = max_child;
        } else if (direction == KRYON_LAYOUT_COLUMN) {
            uint16_t total = 0;
            uint8_t visible_count = 0;
            for (uint8_t i = 0; i < component->child_count; i++) {
                kryon_component_t* child = component->children[i];
                if (child == NULL || !child->visible) {
                    continue;
                }
                int child_height = KRYON_FP_TO_INT(child->height);
                if (child_height == 0) {
                    child_height = KRYON_FP_TO_INT(get_component_intrinsic_height(child));
                }
                total += (uint16_t)child_height;
                visible_count++;
            }
            if (visible_count > 1) {
                total += (uint16_t)(component->gap * (visible_count - 1));
            }
            computed_height = total;
        }
        uint16_t final_height = (uint16_t)(padding + computed_height);
        if (final_height > 0) {
            return KRYON_FP_FROM_INT(final_height);
        }
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

static kryon_fp_t calculate_space_offset(kryon_alignment_t alignment, kryon_fp_t available_space,
                                        kryon_fp_t total_item_size, uint8_t item_count, uint8_t item_index,
                                        kryon_fp_t cumulative_size_before) {
    if (item_count <= 1 || available_space <= 0) {
        return cumulative_size_before;
    }

    kryon_fp_t remaining_space = available_space - total_item_size;
    if (remaining_space <= 0) {
        return cumulative_size_before;
    }

    switch (alignment) {
        case KRYON_ALIGN_SPACE_EVENLY: {
            // Equal space before, between, and after each item
            // Total segments = item_count + 1
            kryon_fp_t segment_size = remaining_space / KRYON_FP_FROM_INT(item_count + 1);
            kryon_fp_t gap_offset = segment_size * KRYON_FP_FROM_INT(item_index + 1);
            return gap_offset + cumulative_size_before;
        }

        case KRYON_ALIGN_SPACE_AROUND: {
            // Equal space before and after each item (half at edges)
            // Total segments = item_count * 2
            kryon_fp_t segment_size = remaining_space / KRYON_FP_FROM_INT(item_count * 2);
            kryon_fp_t gap_offset = segment_size * KRYON_FP_FROM_INT(item_index * 2 + 1);
            return gap_offset + cumulative_size_before;
        }

        case KRYON_ALIGN_SPACE_BETWEEN: {
            // Equal space between items, no space at edges
            // Total segments = item_count - 1
            if (item_index == 0) {
                return cumulative_size_before; // First item has no gap offset
            }
            kryon_fp_t segment_size = remaining_space / KRYON_FP_FROM_INT(item_count - 1);
            kryon_fp_t gap_offset = segment_size * KRYON_FP_FROM_INT(item_index);
            return gap_offset + cumulative_size_before;
        }

        default:
            return cumulative_size_before;
    }
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

        case KRYON_ALIGN_SPACE_EVENLY:
            // Space evenly: equal space before, between, and after each item
            // This is handled at the container level in layout_row/layout_column
            break;

        case KRYON_ALIGN_SPACE_AROUND:
            // Space around: equal space before and after each item (half at edges)
            // This is handled at the container level in layout_row/layout_column
            break;

        case KRYON_ALIGN_SPACE_BETWEEN:
            // Space between: equal space between items, no space at edges
            // This is handled at the container level in layout_row/layout_column
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

    kryon_fp_t padding_left = KRYON_FP_FROM_INT(container->padding_left);
    kryon_fp_t padding_right = KRYON_FP_FROM_INT(container->padding_right);
    kryon_fp_t padding_top = KRYON_FP_FROM_INT(container->padding_top);
    kryon_fp_t padding_bottom = KRYON_FP_FROM_INT(container->padding_bottom);

    // Use available width from context, not container->width
    // This allows rows to fill their parent's space
    kryon_fp_t container_width = ctx->available_width > 0 ? ctx->available_width : container->width;
    kryon_fp_t container_height = ctx->available_height > 0 ? ctx->available_height : container->height;

    // Use relative coordinate space - all child positions are relative to this container
    kryon_fp_t current_x = padding_left;
    kryon_fp_t current_y = padding_top;
    kryon_fp_t line_height = 0;
    kryon_fp_t used_width = current_x;
    kryon_fp_t gap_fp = KRYON_FP_FROM_INT(container->gap);

    // Track visible children for space distribution
    uint8_t visible_child_count = 0;
    kryon_fp_t total_children_width = 0;  // Width including gaps (for start/center/end alignment)
    kryon_fp_t total_children_width_no_gap = 0;  // Width without gaps (for space distribution)
    uint8_t child_indices[16]; // Track visible child indices for space distribution

    // First pass: calculate intrinsic sizes and handle wrapping
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        kryon_fp_t child_width = get_component_intrinsic_width(child);
        kryon_fp_t child_height = get_component_intrinsic_height(child);

        // Add margins
        child_width += KRYON_FP_FROM_INT(child->margin_left + child->margin_right);
        child_height += KRYON_FP_FROM_INT(child->margin_top + child->margin_bottom);

        // Check if we need to wrap to next line (account for gap and right padding)
        kryon_fp_t gap_needed = (visible_child_count > 0) ? gap_fp : 0;
        if (used_width + child_width + gap_needed + padding_right > container_width && i > 0) {
            // Apply space distribution to previous line before wrapping
            if (visible_child_count > 0 && (container->justify_content == KRYON_ALIGN_SPACE_EVENLY ||
                container->justify_content == KRYON_ALIGN_SPACE_AROUND ||
                container->justify_content == KRYON_ALIGN_SPACE_BETWEEN)) {

                kryon_fp_t available_width = container_width - padding_left - padding_right;

                // Re-position children with space distribution
                // Use total width WITHOUT gaps since space distribution adds its own spacing
                kryon_fp_t cumulative_width = 0;
                for (uint8_t j = 0; j < visible_child_count; j++) {
                    uint8_t child_idx = child_indices[j];
                    kryon_component_t* visible_child = container->children[child_idx];

                    kryon_fp_t space_offset = calculate_space_offset(
                        container->justify_content, available_width, total_children_width_no_gap,
                        visible_child_count, j, cumulative_width
                    );

                    kryon_fp_t base_x = padding_left + space_offset;
                    visible_child->x = base_x + KRYON_FP_FROM_INT(visible_child->margin_left);

                    cumulative_width += visible_child->width + KRYON_FP_FROM_INT(visible_child->margin_left) + KRYON_FP_FROM_INT(visible_child->margin_right);
                }
            }

            // Reset for new line
            visible_child_count = 0;
            total_children_width = 0;
            total_children_width_no_gap = 0;
            current_x = padding_left;
            current_y += line_height;
            used_width = current_x;
            line_height = 0;
        }

        // Store child index for space distribution
        if (visible_child_count < 16) {
            child_indices[visible_child_count] = i;
        }

        // Add gap before positioning (except for first child)
        if (visible_child_count > 0) {
            current_x += gap_fp;
            used_width += gap_fp;
        }

        // Initial position (will be adjusted for space distribution later)
        child->x = current_x + KRYON_FP_FROM_INT(child->margin_left);
        child->y = current_y + KRYON_FP_FROM_INT(child->margin_top);
        child->width = child_width - KRYON_FP_FROM_INT(child->margin_left + child->margin_right);
        child->height = child_height - KRYON_FP_FROM_INT(child->margin_top + child->margin_bottom);

        // Debug trace for text components
        if (getenv("KRYON_TRACE_LAYOUT") && child->ops == &kryon_text_ops) {
            kryon_text_state_t* text_state = (kryon_text_state_t*)child->state;
            if (text_state && text_state->text) {
                fprintf(stderr, "[kryon][layout] ROW_POSITION text='%s' x=%d y=%d width=%d height=%d current_x=%d gap=%d\n",
                        text_state->text, KRYON_FP_TO_INT(child->x), KRYON_FP_TO_INT(child->y),
                        KRYON_FP_TO_INT(child->width), KRYON_FP_TO_INT(child->height),
                        KRYON_FP_TO_INT(current_x), KRYON_FP_TO_INT(gap_fp));
            }
        }

        // Track total width for alignment calculations
        total_children_width_no_gap += child_width;  // Without gaps (for space distribution)
        total_children_width += child_width;  // With gaps (for regular alignment)
        if (visible_child_count > 0) {
            total_children_width += gap_fp;
        }
        visible_child_count++;

        // Update line tracking
        if (child_height > line_height) {
            line_height = child_height;
        }

        current_x += child_width;
        used_width += child_width;
    }

    // Determine alignment offsets including space distribution
    // Use INNER width (excluding padding) for alignment calculations
    kryon_fp_t inner_width = container_width - padding_left - padding_right;
    kryon_fp_t available_width_for_alignment = inner_width;
    if (available_width_for_alignment < total_children_width) {
        available_width_for_alignment = total_children_width;
    }

    kryon_fp_t alignment_offset = 0;
    switch (container->justify_content) {
        case KRYON_ALIGN_CENTER:
            if (available_width_for_alignment > total_children_width) {
                alignment_offset = (available_width_for_alignment - total_children_width) / 2;
            }
            break;
        case KRYON_ALIGN_END:
            if (available_width_for_alignment > total_children_width) {
                alignment_offset = available_width_for_alignment - total_children_width;
            }
            break;
        case KRYON_ALIGN_SPACE_EVENLY:
        case KRYON_ALIGN_SPACE_AROUND:
        case KRYON_ALIGN_SPACE_BETWEEN:
            // Space distribution will be handled per-child in the positioning loop
            alignment_offset = 0;
            break;
        case KRYON_ALIGN_STRETCH:
        default:
            break;
    }

    // Apply alignment offset and space distribution to last line if needed
    if (visible_child_count > 0) {
        if (container->justify_content == KRYON_ALIGN_SPACE_EVENLY ||
            container->justify_content == KRYON_ALIGN_SPACE_AROUND ||
            container->justify_content == KRYON_ALIGN_SPACE_BETWEEN) {

            kryon_fp_t available_width = container_width - padding_left - padding_right;

            // Re-position children with space distribution
            // Use total width WITHOUT gaps since space distribution adds its own spacing
            kryon_fp_t cumulative_width = 0;
            for (uint8_t j = 0; j < visible_child_count; j++) {
                uint8_t child_idx = child_indices[j];
                kryon_component_t* visible_child = container->children[child_idx];

                kryon_fp_t space_offset = calculate_space_offset(
                    container->justify_content, available_width, total_children_width_no_gap,
                    visible_child_count, j, cumulative_width
                );

                kryon_fp_t base_x = padding_left + space_offset;
                visible_child->x = base_x + KRYON_FP_FROM_INT(visible_child->margin_left);

                cumulative_width += visible_child->width + KRYON_FP_FROM_INT(visible_child->margin_left) + KRYON_FP_FROM_INT(visible_child->margin_right);
            }
        } else {
            // Apply main-axis alignment (START, CENTER, END)
            kryon_fp_t cumulative_width = 0;
            for (uint8_t j = 0; j < visible_child_count; j++) {
                uint8_t child_idx = child_indices[j];
                kryon_component_t* visible_child = container->children[child_idx];

                kryon_fp_t base_x = padding_left + alignment_offset + cumulative_width;
                visible_child->x = base_x + KRYON_FP_FROM_INT(visible_child->margin_left);

                cumulative_width += visible_child->width + KRYON_FP_FROM_INT(visible_child->margin_left) + KRYON_FP_FROM_INT(visible_child->margin_right);

                // Add gap between children (but not after the last child)
                if (j < visible_child_count - 1) {
                    cumulative_width += KRYON_FP_FROM_INT(container->gap);
                }
            }
        }
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

    // Apply cross-axis alignment (vertical alignment)
    for (uint8_t i = 0; i < container->child_count; i++) {
        kryon_component_t* child = container->children[i];
        if (!child->visible) continue;

        kryon_fp_t extra_height = line_height - child->height;
        if (extra_height > 0) {
            switch (container->align_items) {
                case KRYON_ALIGN_START:
                    // No offset needed
                    break;
                case KRYON_ALIGN_CENTER:
                    child->y += extra_height / 2;
                    break;
                case KRYON_ALIGN_END:
                    child->y += extra_height;
                    break;
                case KRYON_ALIGN_STRETCH:
                    child->height = line_height;
                    break;
                default:
                    child->y += extra_height / 2; // Default to center
                    break;
            }
        }
    }

    // Set container dimensions if not explicitly provided
    if (container->height == 0) {
        container->height = current_y + line_height + padding_bottom;
    }
    if (container_width > 0) {
        container->width = container_width;
    } else if (used_width > container->width) {
        container->width = used_width;
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
    kryon_fp_t gap_fp = KRYON_FP_FROM_INT(container->gap);

    kryon_fp_t container_width = container->width;
    if (container_width == 0) {
        container_width = ctx->available_width;
    }

    kryon_fp_t inner_width = container_width - padding_left - padding_right;
    if (inner_width < 0) inner_width = 0;

    // Use relative coordinate space - all child positions are relative to this container
    // The rendering system (calculate_absolute_position) will handle transformation to screen coordinates
    kryon_fp_t origin_x = padding_left;
    kryon_fp_t origin_y = padding_top;

    kryon_fp_t total_content_height = 0;
    kryon_fp_t total_flex_grow = 0;

    // Count visible children first
    uint8_t visible_child_count = 0;
    for (uint8_t i = 0; i < container->child_count; i++) {
        if (container->children[i]->visible) {
            visible_child_count++;
        }
    }

    // Measure children and assign base dimensions
    uint8_t visible_index = 0;
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
        // Only clamp width if not explicitly set
        if ((child->layout_flags & KRYON_COMPONENT_FLAG_HAS_WIDTH) == 0) {
            if (child_width == 0 || child_width > available_width) {
                child_width = available_width;
            }
        }

        kryon_fp_t child_height = child->height;
        // Don't set intrinsic height for containers - let them calculate from content
        if (child_height == 0 && child->ops != &kryon_container_ops) {
            child_height = get_component_intrinsic_height(child);
        }
        if (child_height == 0 && child->ops != &kryon_container_ops) {
            child_height = KRYON_FP_FROM_INT(20);
        }

        child->width = child_width;
        // Only assign height if we calculated one (containers keep height=0 for auto-calculation)
        if (child_height > 0) {
            child->height = child_height;
        }

        total_content_height += child->height + margin_top + margin_bottom;

        // Add gap to total content height (except for last child)
        if (visible_index < visible_child_count - 1) {
            total_content_height += gap_fp;
        }

        if (child->flex_grow > 0) {
            total_flex_grow += KRYON_FP_FROM_INT(child->flex_grow);
        }

        visible_index++;
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

    // Determine alignment offsets including space distribution
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
        case KRYON_ALIGN_SPACE_EVENLY:
        case KRYON_ALIGN_SPACE_AROUND:
        case KRYON_ALIGN_SPACE_BETWEEN:
            // Space distribution will be handled per-child in the positioning loop
            alignment_offset = 0;
            break;
        case KRYON_ALIGN_STRETCH:
        default:
            break;
    }

    // Position children
    kryon_fp_t cursor_y = origin_y + alignment_offset;
    kryon_fp_t cumulative_height = 0; // Track cumulative height for space distribution
    uint8_t current_visible_index = 0; // Track visible child index for space distribution

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

        // Handle cross-axis alignment (horizontal)
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
                // Only stretch width if not explicitly set
                if ((child->layout_flags & KRYON_COMPONENT_FLAG_HAS_WIDTH) == 0) {
                    child->width = available_width;
                }
                break;
            case KRYON_ALIGN_START:
            default:
                child->x = origin_x + margin_left;
                break;
        }

        // If explicit X position is set, use it (relative to container)
        if (has_explicit_x) {
            child->x = explicit_x;
        }

        // Handle main-axis space distribution (vertical)
        kryon_fp_t child_y = cursor_y + margin_top;

        if (container->justify_content == KRYON_ALIGN_SPACE_EVENLY ||
            container->justify_content == KRYON_ALIGN_SPACE_AROUND ||
            container->justify_content == KRYON_ALIGN_SPACE_BETWEEN) {

            kryon_fp_t space_offset = calculate_space_offset(
                container->justify_content, target_inner_height, total_content_height,
                visible_child_count, current_visible_index, cumulative_height
            );
            child_y = origin_y + space_offset + margin_top;
        }

        child->y = child_y;
        // If explicit Y position is set, use it (relative to container)
        if (has_explicit_y) {
            child->y = explicit_y;
        }

        // Update cursor and cumulative height for next child
        if (container->justify_content == KRYON_ALIGN_SPACE_EVENLY ||
            container->justify_content == KRYON_ALIGN_SPACE_AROUND ||
            container->justify_content == KRYON_ALIGN_SPACE_BETWEEN) {
            // For space distribution, track cumulative height
            cumulative_height += child->height + margin_top + margin_bottom;
            current_visible_index++;
        } else {
            // For non-space alignments, add gap between children
            cursor_y += child->height + margin_top + margin_bottom;
            if (current_visible_index < visible_child_count - 1) {
                cursor_y += gap_fp;
            }
            current_visible_index++;
        }
    }

    // Calculate container height if not explicitly set
    if (container->height == 0) {
        // Always calculate from actual content, not cursor position
        // cursor_y may not be accurate when using space distribution
        kryon_fp_t total_height = padding_top;
        uint8_t height_calc_index = 0;

        for (uint8_t i = 0; i < container->child_count; i++) {
            kryon_component_t* child = container->children[i];
            if (!child->visible) continue;

            kryon_fp_t child_contribution = child->height +
                                           KRYON_FP_FROM_INT(child->margin_top + child->margin_bottom);

            total_height += child_contribution;

            // Add gap between children (but not after the last one)
            if (height_calc_index > 0) {
                total_height += gap_fp;
            }
            height_calc_index++;
        }

        total_height += padding_bottom;
        container->height = total_height;
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

        // Check if explicit positions are set using flags
        const bool has_explicit_x = (child->layout_flags & KRYON_COMPONENT_FLAG_HAS_X) != 0;
        const bool has_explicit_y = (child->layout_flags & KRYON_COMPONENT_FLAG_HAS_Y) != 0;

        // Use explicit position if set, otherwise use margin as default
        // Positions are relative to the container, not screen-absolute
        kryon_fp_t child_x = has_explicit_x ? child->explicit_x : KRYON_FP_FROM_INT(child->margin_left);
        kryon_fp_t child_y = has_explicit_y ? child->explicit_y : KRYON_FP_FROM_INT(child->margin_top);

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

    // For root component (parent == NULL), ensure it's positioned at (0,0) and fills the available space
    bool is_root_component = (component->parent == NULL);
    if (is_root_component) {
        component->x = 0;
        component->y = 0;
    }

    // For container components (with children), allow expansion to available space
    // unless they have explicit width constraints that should be respected
    bool is_container = (component->child_count > 0);
    bool has_explicit_width = (component->layout_flags & KRYON_COMPONENT_FLAG_HAS_WIDTH) != 0;
    bool has_explicit_height = (component->layout_flags & KRYON_COMPONENT_FLAG_HAS_HEIGHT) != 0;

    // For components with flex_grow > 0, ensure they can expand to fill available space
    // but don't override explicit dimensions unless they need to grow
    if (component->flex_grow > 0) {
        if (available_width > component->width && !has_explicit_width) {
            component->width = available_width;
        }
        if (available_height > component->height && !has_explicit_height) {
            component->height = available_height;
        }
    }

    // Use available space for components without explicit dimensions
    // This ensures children respect parent's content area (after padding)
    kryon_fp_t effective_width;
    kryon_fp_t effective_height;

    if (has_explicit_width && component->width > 0) {
        effective_width = component->width;
    } else {
        effective_width = available_width > 0 ? available_width : component->width;
    }

    if (has_explicit_height && component->height > 0) {
        effective_height = component->height;
    } else {
        effective_height = available_height > 0 ? available_height : component->height;
    }

    // Determine layout direction based on component property
    kryon_layout_direction_t direction = (kryon_layout_direction_t)component->layout_direction;

    if (direction > KRYON_LAYOUT_ABSOLUTE) {
        direction = KRYON_LAYOUT_COLUMN; // Fallback to column if invalid
    }

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

    // Calculate inner dimensions for children (subtract padding from effective dimensions)
    kryon_fp_t inner_width = effective_width -
        KRYON_FP_FROM_INT(component->padding_left + component->padding_right);
    kryon_fp_t inner_height = effective_height -
        KRYON_FP_FROM_INT(component->padding_top + component->padding_bottom);

    if (inner_width < 0) inner_width = 0;
    if (inner_height < 0) inner_height = 0;

    // Recursively layout children. Width defaults to the parent's inner width
    // (so rows stretch to match their container) unless the child explicitly
    // set its width. Height defaults to the child's measured height (to avoid
    // inflating short components like TabBar rows) or the parent's inner height
    // if no height has been resolved yet.
    for (uint8_t i = 0; i < component->child_count; i++) {
        kryon_component_t* child = component->children[i];
        if (child->visible) {
            kryon_fp_t child_available_width = inner_width;
            kryon_fp_t child_available_height = inner_height;

            if (((child->layout_flags & KRYON_COMPONENT_FLAG_HAS_WIDTH) != 0) && child->width > 0) {
                child_available_width = child->width;
            }
            if (child->height > 0) {
                child_available_height = child->height;
            }

            layout_component_internal(child, child_available_width, child_available_height);
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
