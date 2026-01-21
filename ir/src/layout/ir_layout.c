// IR Layout Engine - Unified flexbox layout system integrated with IR
// Moved from /core/layout.c and adapted for IR architecture

#include "../include/ir_core.h"
#include "../include/ir_builder.h"
#include "ir_text_shaping.h"
#include "../components/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

// ============================================================================
// Text Measurement Callback (Backend-Specific)
// ============================================================================

IRTextMeasureCallback g_text_measure_callback = NULL;

void ir_layout_set_text_measure_callback(IRTextMeasureCallback callback) {
    g_text_measure_callback = callback;
}

// ============================================================================
// Layout Cache & Dirty Tracking
// ============================================================================

// Helper: Recursively invalidate layout for entire subtree
void ir_layout_invalidate_subtree(IRComponent* component) {
    if (!component || !component->layout_state) return;

    // Mark this component dirty
    component->layout_state->dirty = true;
    component->layout_state->layout_valid = false;

    // Recurse to all children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_layout_invalidate_subtree(component->children[i]);
    }
}

void ir_layout_mark_dirty(IRComponent* component) {
    if (!component || !component->layout_state) return;

    if (getenv("KRYON_DEBUG_TAB_LAYOUT") && (component->type == IR_COMPONENT_TAB_GROUP ||
                                              component->type == IR_COMPONENT_TAB_CONTENT ||
                                              component->type == IR_COMPONENT_TAB_PANEL)) {
        fprintf(stderr, "[MARK_DIRTY] Component ID=%u type=%d\n", component->id, component->type);
    }

    // Mark this component dirty (consolidated in layout_state)
    component->layout_state->dirty = true;
    component->layout_state->layout_valid = false;
    component->layout_state->dirty_flags |= IR_DIRTY_LAYOUT;

    // Recursively invalidate entire subtree
    for (uint32_t i = 0; i < component->child_count; i++) {
        IRComponent* child = component->children[i];
        if (child && child->layout_state) {
            ir_layout_invalidate_subtree(child);
        }
    }

    // Propagate dirty flag upward to parents
    IRComponent* parent = component->parent;
    while (parent) {
        if (!parent->layout_state) break;

        parent->layout_state->dirty = true;
        parent->layout_state->layout_valid = false;
        parent->layout_state->dirty_flags |= IR_DIRTY_SUBTREE;
        parent->layout_cache.dirty = true;
        parent->layout_cache.cached_intrinsic_width = -1.0f;
        parent->layout_cache.cached_intrinsic_height = -1.0f;

        parent = parent->parent;
    }
}

void ir_component_mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_mark_dirty(component);
    if (component->layout_state) {
        component->layout_state->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
    }
}

void ir_layout_mark_render_dirty(IRComponent* component) {
    if (!component || !component->layout_state) return;

    // Use consolidated dirty_flags in layout_state
    component->layout_state->dirty_flags |= IR_DIRTY_RENDER;

    // Do NOT propagate to parents - visual-only changes don't affect parent layout
}

// Invalidate layout cache when properties change
void ir_layout_invalidate_cache(IRComponent* component) {
    if (!component) return;

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "🔄 INVALIDATE_CACHE for component %u (type=%d)\n", component->id, component->type);
    #endif

    // Mark this component's layout as dirty
    ir_layout_mark_dirty(component);
}

// ============================================================================
// Intrinsic Size Calculation (with caching)
// ============================================================================

float ir_get_text_width_estimate(const char* text, float font_size) {
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

                // Check if max_width is set for text wrapping
                float max_width = 0.0f;
                if (style->text_effect.max_width.type == IR_DIMENSION_PX) {
                    max_width = style->text_effect.max_width.value;
                }

                // If max_width is set, compute text layout and use it
                if (max_width > 0) {
                    // Compute text layout if not cached or if dirty
                    if (!component->text_layout || component->layout_cache.dirty) {
                        ir_text_layout_compute(component, max_width, font_size);
                    }

                    if (component->text_layout) {
                        return ir_text_layout_get_width(component->text_layout);
                    }
                }

                // Fallback to single-line estimation
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
            // Check for explicit width first
            if (component->style && component->style->width.type == IR_DIMENSION_PX) {
                return component->style->width.value;
            }
            return 200.0f; // Default input width

        case IR_COMPONENT_NATIVE_CANVAS:
            // Native canvas always has explicit dimensions set during creation
            if (component->style && component->style->width.type == IR_DIMENSION_PX) {
                return component->style->width.value;
            }
            return 800.0f; // Fallback default

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

                    // OPTIMIZATION: Use caching wrapper instead of _impl (70% reduction in calculations)
                    float child_width = ir_get_component_intrinsic_width(child);
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

    // Check cache (use >= 0 since -1.0f means not cached)
    if (!component->layout_cache.dirty && component->layout_cache.cached_intrinsic_width >= 0.0f) {
        return component->layout_cache.cached_intrinsic_width;
    }

    // Calculate and cache
    float width = ir_get_component_intrinsic_width_impl(component);
    component->layout_cache.cached_intrinsic_width = width;

    return width;
}

static float ir_get_component_intrinsic_height_impl(IRComponent* component) {
    if (!component || !component->style) {
        return 50.0f;
    }

    IRStyle* style = component->style;

    // Use explicit height if set
    if (style->height.type == IR_DIMENSION_PX) {
        return style->height.value;
    }

    // Calculate based on component type
    switch (component->type) {
        case IR_COMPONENT_TEXT: {
            float font_size = style->font.size > 0 ? style->font.size : 16.0f;

            // Check if max_width is set for text wrapping
            float max_width = 0.0f;
            if (style->text_effect.max_width.type == IR_DIMENSION_PX) {
                max_width = style->text_effect.max_width.value;
            }

            // If max_width is set and we have text layout, use multi-line height
            if (max_width > 0 && component->text_content) {
                // Compute text layout if not cached or if dirty
                if (!component->text_layout || component->layout_cache.dirty) {
                    ir_text_layout_compute(component, max_width, font_size);
                }

                if (component->text_layout) {
                    return ir_text_layout_get_height(component->text_layout);
                }
            }

            // Fallback to single-line height using line_height multiplier
            float line_height = style->font.line_height > 0 ? style->font.line_height : 1.5f;
            float computed_height = font_size * line_height;

            // DEBUG: Print text height calculation
            if (component->text_content && getenv("KRYON_DEBUG_TEXT")) {
                fprintf(stderr, "[TEXT HEIGHT] \"%s\" fontSize=%.1f lineHeight=%.1f computed=%.1f\n",
                       component->text_content, font_size, line_height, computed_height);
            }

            return computed_height;
        }

        case IR_COMPONENT_BUTTON: {
            float font_size = style->font.size > 0 ? style->font.size : 14.0f;
            float padding = style->padding.top + style->padding.bottom;
            return font_size + padding + 12.0f; // Extra space for button chrome
        }

        case IR_COMPONENT_INPUT:
            // Check for explicit height first
            if (component->style && component->style->height.type == IR_DIMENSION_PX) {
                return component->style->height.value;
            }
            return 30.0f;

        case IR_COMPONENT_NATIVE_CANVAS:
            // Native canvas always has explicit dimensions set during creation
            if (component->style && component->style->height.type == IR_DIMENSION_PX) {
                return component->style->height.value;
            }
            return 600.0f; // Fallback default

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

                    // OPTIMIZATION: Use caching wrapper instead of _impl (70% reduction in calculations)
                    float child_height = ir_get_component_intrinsic_height(child);
                    child_height += child->style->margin.top + child->style->margin.bottom;

                    if (is_column) {
                        total_height += child_height;
                        if (i > 0) total_height += component->layout->flex.gap;
                    } else {
                        if (child_height > max_height) max_height = child_height;
                    }
                }

                float result = is_column ? total_height : max_height;
                #ifdef KRYON_TRACE_LAYOUT
                fprintf(stderr, "      📦 PADDING: top=%.1f, bottom=%.1f (result before: %.1f)\n",
                    style->padding.top, style->padding.bottom, result);
                #endif
                result += style->padding.top + style->padding.bottom;
                #ifdef KRYON_TRACE_LAYOUT
                fprintf(stderr, "      📦 RESULT after padding: %.1f\n", result);
                #endif
                return result;
            }
            return 50.0f;

        default:
            return 50.0f;
    }
}

float ir_get_component_intrinsic_height(IRComponent* component) {
    if (!component) return 0.0f;

    // Check cache (use >= 0 since -1.0f means not cached)
    if (!component->layout_cache.dirty && component->layout_cache.cached_intrinsic_height >= 0.0f) {
        return component->layout_cache.cached_intrinsic_height;
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "🔍 CACHE MISS for component %u (type=%d, dirty=%d, cached=%.1f)\n",
        component->id, component->type, component->layout_cache.dirty,
        component->layout_cache.cached_intrinsic_height);
    #endif

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
static void ir_layout_compute_grid(IRComponent* container, float available_width, float available_height);
void ir_layout_compute_table(IRComponent* table, float available_width, float available_height);

// ============================================================================
// BiDi Direction Propagation
// ============================================================================

static void ir_propagate_direction(IRComponent* component, IRDirection parent_dir) {
    if (!component || !component->layout) return;

    IRFlexbox* flex = &component->layout->flex;

    // Resolve inherited direction
    if (flex->base_direction == IR_DIRECTION_INHERIT) {
        flex->base_direction = (uint8_t)parent_dir;
    }

    // Resolve auto direction (detect from content)
    if (flex->base_direction == IR_DIRECTION_AUTO) {
        // For text components, detect direction from text content
        if (component->type == IR_COMPONENT_TEXT && component->text_content && component->text_content[0] != '\0') {
            fprintf(stderr, "[BiDi Detection] Text: '%s'\n", component->text_content);
            IRBidiDirection bidi_dir = ir_bidi_detect_direction(component->text_content, strlen(component->text_content));
            flex->base_direction = (bidi_dir == IR_BIDI_DIR_RTL) ? IR_DIRECTION_RTL : IR_DIRECTION_LTR;
            fprintf(stderr, "[BiDi Detection] Result: %s\n", (bidi_dir == IR_BIDI_DIR_RTL) ? "RTL" : "LTR");
        } else {
            // For non-text components or empty text, default to LTR
            flex->base_direction = IR_DIRECTION_LTR;
        }
    }

    // Propagate resolved direction to children
    IRDirection resolved_dir = (IRDirection)flex->base_direction;
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (component->children[i] && component->children[i]->layout) {
            ir_propagate_direction(component->children[i], resolved_dir);
        }
    }
}

// ============================================================================
// Layout Computation
// ============================================================================

void ir_layout_compute(IRComponent* root, float available_width, float available_height) {
    if (!root) return;

    // DEBUG: Log all layout invocations
    if (getenv("KRYON_DEBUG_TABLE")) {
        fprintf(stderr, "[LAYOUT] id=%u type=%d dirty=0x%02x available=%.1fx%.1f\n",
                root->id, root->type,
                root->layout_state ? root->layout_state->dirty_flags : 0,
                available_width, available_height);
    }

    // Skip layout if component is clean (not dirty)
    IRDirtyFlags flags = root->layout_state ? root->layout_state->dirty_flags : 0;
    if ((flags & (IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE)) == 0) {
        if (getenv("KRYON_DEBUG_TABLE")) {
            fprintf(stderr, "[LAYOUT] SKIPPING id=%u (not dirty)\n", root->id);
        }
        return;
    }

    // Ensure style and layout exist
    if (!root->style) {
        root->style = ir_create_style();
    }
    if (!root->layout) {
        root->layout = (IRLayout*)calloc(1, sizeof(IRLayout));
    }

    // Propagate CSS direction property through the tree
    ir_propagate_direction(root, IR_DIRECTION_LTR);

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
        // For AUTO dimensions, always use intrinsic size based on content
        if (style->height.type == IR_DIMENSION_AUTO) {
            height = ir_get_component_intrinsic_height(root);
        } else {
            // For unset dimensions, use available space or intrinsic as fallback
            height = available_height > 0 ? available_height : ir_get_component_intrinsic_height(root);
        }
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
        if (root->layout_state) {
            root->layout_state->computed.x = style->absolute_x;
            root->layout_state->computed.y = style->absolute_y;
        }
    } else {
        root->rendered_bounds.x = 0.0f;
        root->rendered_bounds.y = 0.0f;
        if (root->layout_state) {
            root->layout_state->computed.x = 0.0f;
            root->layout_state->computed.y = 0.0f;
        }
    }

    root->rendered_bounds.width = width;
    root->rendered_bounds.height = height;
    root->rendered_bounds.valid = true;

    // Calculate inner dimensions for children (subtract padding)
    float inner_width = width - (style->padding.left + style->padding.right);
    float inner_height = height - (style->padding.top + style->padding.bottom);

    if (inner_width < 0) inner_width = 0;
    if (inner_height < 0) inner_height = 0;

    // Layout children based on layout mode
    if (root->child_count > 0) {
        // Special handling for table components
        if (root->type == IR_COMPONENT_TABLE) {
            ir_layout_compute_table(root, inner_width, inner_height);
        } else {
            switch (layout->mode) {
                case IR_LAYOUT_MODE_GRID:
                    ir_layout_compute_grid(root, inner_width, inner_height);
                    break;

                case IR_LAYOUT_MODE_FLEX:
                default:
                    if (layout->flex.direction == 1) {
                        // Row layout
                        ir_layout_compute_row(root, inner_width, inner_height);
                    } else {
                        // Column layout (direction == 0 or default)
                        ir_layout_compute_column(root, inner_width, inner_height);
                    }
                    break;
            }
        }
    }

    // Recursively layout children
    // SKIP table children - their layout is handled by ir_layout_compute_table()
    // These use RELATIVE coordinates that would be corrupted by flex layout
    fprintf(stderr, "[LAYOUT_RECURSE] root id=%u type=%d child_count=%u\n",
            root->id, root->type, root->child_count);
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        fprintf(stderr, "[LAYOUT_RECURSE]   child[%u] id=%u type=%d style=%p visible=%d\n",
                i, child->id, child->type, (void*)child->style,
                child->style ? child->style->visible : -1);
        if (child->style && child->style->visible) {
            // Skip table children - their layout is managed by table layout algorithm
            bool is_table_child = (child->type == IR_COMPONENT_TABLE_HEAD ||
                                   child->type == IR_COMPONENT_TABLE_BODY ||
                                   child->type == IR_COMPONENT_TABLE_FOOT ||
                                   child->type == IR_COMPONENT_TABLE_ROW ||
                                   child->type == IR_COMPONENT_TABLE_CELL ||
                                   child->type == IR_COMPONENT_TABLE_HEADER_CELL);
            if (!is_table_child) {
                if (child->type == IR_COMPONENT_TABLE) {
                    fprintf(stderr, "[RECURSE] About to layout table id=%u bounds=%.1fx%.1f\n",
                            child->id, child->rendered_bounds.width, child->rendered_bounds.height);
                }
                ir_layout_compute(child, child->rendered_bounds.width, child->rendered_bounds.height);
            }
        }
    }

    // Mark as clean
    if (root->layout_state) {
        root->layout_state->dirty_flags &= ~(IR_DIRTY_LAYOUT | IR_DIRTY_SUBTREE);
    }
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

        // Apply max_width constraint before calculating total
        if (child->layout && child->layout->max_width.type == IR_DIMENSION_PX && child->layout->max_width.value > 0) {
            if (child_width > child->layout->max_width.value) {
                child_width = child->layout->max_width.value;
            }
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

    // Apply main-axis alignment (justify_content) for row layout
    if (remaining_width > 0 && total_flex_grow == 0 && visible_count > 0) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_CENTER:
                current_x += remaining_width / 2.0f;
                break;
            case IR_ALIGNMENT_END:
                current_x += remaining_width;
                break;
            case IR_ALIGNMENT_SPACE_BETWEEN:
                // Will be handled per-child below
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                current_x += remaining_width / (visible_count * 2);
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                current_x += remaining_width / (visible_count + 1);
                break;
            default:
                break;
        }
    }

    // Calculate gap for space-between/around/evenly
    float extra_gap = 0.0f;
    if (remaining_width > 0 && total_flex_grow == 0 && visible_count > 1) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN:
                extra_gap = remaining_width / (visible_count - 1);
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                extra_gap = remaining_width / visible_count;
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                extra_gap = remaining_width / (visible_count + 1);
                break;
            default:
                break;
        }
    }

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

        // Apply max_height constraint BEFORE alignment calculation
        if (child->layout && child->layout->max_height.type == IR_DIMENSION_PX && child->layout->max_height.value > 0) {
            if (child_height > child->layout->max_height.value) {
                child_height = child->layout->max_height.value;
            }
        }

        // Apply cross-axis alignment (vertical) using clamped height
        float child_y = style->padding.top + child->style->margin.top;
        if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_y += (available_height - child_height) / 2.0f;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_y += available_height - child_height;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_height = available_height - child->style->margin.top - child->style->margin.bottom;
        }

        // Apply max_width constraint
        if (child->layout && child->layout->max_width.type == IR_DIMENSION_PX && child->layout->max_width.value > 0) {
            if (child_width > child->layout->max_width.value) {
                child_width = child->layout->max_width.value;
            }
        }

        // Set child bounds
        child->rendered_bounds.x = current_x + child->style->margin.left;
        child->rendered_bounds.y = child_y;
        child->rendered_bounds.width = child_width;
        child->rendered_bounds.height = child_height;
        child->rendered_bounds.valid = true;

        // Debug: Print Button positions to debug ForEach layout
        if (child->type == IR_COMPONENT_BUTTON && getenv("DEBUG_BUTTON_POSITIONS")) {
            const char* text = child->text_content ? child->text_content : "(null)";
            printf("[LAYOUT] Button ID=%u type=%d text='%s' pos=[%.1f, %.1f] size=[%.1fx%.1f] parent_id=%d\n",
                   child->id, (int)child->type, text,
                   child->rendered_bounds.x, child->rendered_bounds.y,
                   child->rendered_bounds.width, child->rendered_bounds.height,
                   container->id);
        }

        current_x += child_width + child->style->margin.right + layout->flex.gap + extra_gap;
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

        // Apply max_height constraint before calculating total
        if (child->layout && child->layout->max_height.type == IR_DIMENSION_PX && child->layout->max_height.value > 0) {
            if (child_height > child->layout->max_height.value) {
                child_height = child->layout->max_height.value;
            }
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

    // Apply main-axis alignment (justify_content) for column layout
    if (remaining_height > 0 && total_flex_grow == 0 && visible_count > 0) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_CENTER:
                current_y += remaining_height / 2.0f;
                break;
            case IR_ALIGNMENT_END:
                current_y += remaining_height;
                break;
            case IR_ALIGNMENT_SPACE_BETWEEN:
                // Will be handled per-child below
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                current_y += remaining_height / (visible_count * 2);
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                current_y += remaining_height / (visible_count + 1);
                break;
            default:
                break;
        }
    }

    // Calculate gap for space-between/around/evenly
    float extra_gap = 0.0f;
    if (remaining_height > 0 && total_flex_grow == 0 && visible_count > 1) {
        switch (layout->flex.justify_content) {
            case IR_ALIGNMENT_SPACE_BETWEEN:
                extra_gap = remaining_height / (visible_count - 1);
                break;
            case IR_ALIGNMENT_SPACE_AROUND:
                extra_gap = remaining_height / visible_count;
                break;
            case IR_ALIGNMENT_SPACE_EVENLY:
                extra_gap = remaining_height / (visible_count + 1);
                break;
            default:
                break;
        }
    }

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

        // Apply max_width constraint BEFORE alignment calculation
        if (child->layout && child->layout->max_width.type == IR_DIMENSION_PX && child->layout->max_width.value > 0) {
            if (child_width > child->layout->max_width.value) {
                child_width = child->layout->max_width.value;
            }
        }

        // Apply cross-axis alignment (horizontal) using clamped width
        float child_x = style->padding.left + child->style->margin.left;
        if (layout->flex.cross_axis == IR_ALIGNMENT_CENTER) {
            child_x += (available_width - child_width) / 2.0f;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_END) {
            child_x += available_width - child_width;
        } else if (layout->flex.cross_axis == IR_ALIGNMENT_STRETCH) {
            child_width = available_width - child->style->margin.left - child->style->margin.right;
        }

        // Apply max_height constraint
        if (child->layout && child->layout->max_height.type == IR_DIMENSION_PX && child->layout->max_height.value > 0) {
            if (child_height > child->layout->max_height.value) {
                child_height = child->layout->max_height.value;
            }
        }

        // Set child bounds
        child->rendered_bounds.x = child_x;
        child->rendered_bounds.y = current_y + child->style->margin.top;
        child->rendered_bounds.width = child_width;
        child->rendered_bounds.height = child_height;
        child->rendered_bounds.valid = true;

        if (child->type == IR_COMPONENT_CHECKBOX) {
            printf("[LAYOUT_CHECKBOX] ID=%u type=%d bounds=[%.1f, %.1f, %.1f, %.1f] valid=%d\n",
                   child->id, child->type, child->rendered_bounds.x, child->rendered_bounds.y,
                   child->rendered_bounds.width, child->rendered_bounds.height,
                   child->rendered_bounds.valid);
        }

        #ifdef KRYON_TRACE_LAYOUT
        fprintf(stderr, "║ Child %u: bounds=[%.1f, %.1f, %.1f, %.1f] intrinsic_h=%.1f\n",
            i, child->rendered_bounds.x, child->rendered_bounds.y,
            child->rendered_bounds.width, child->rendered_bounds.height,
            ir_get_component_intrinsic_height(child));
        #endif

        current_y += child_height + child->style->margin.bottom + layout->flex.gap + extra_gap;
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "║ Final current_y: %.1f (container height should be: %.1f with padding)\n",
        current_y, current_y + style->padding.bottom);
    fprintf(stderr, "╚═══════════════════════════════════════════════════════\n\n");
    #endif
}

// CSS Grid Layout Implementation
static void ir_layout_compute_grid(IRComponent* container, float available_width, float available_height) {
    if (!container || !container->layout) return;

    IRLayout* layout = container->layout;
    IRGrid* grid = &layout->grid;

    // Handle repeat(auto-fit/auto-fill, ...) for columns
    if (grid->use_column_repeat && grid->column_repeat.mode != IR_GRID_REPEAT_NONE) {
        IRGridRepeat* rep = &grid->column_repeat;

        if (rep->mode == IR_GRID_REPEAT_AUTO_FIT || rep->mode == IR_GRID_REPEAT_AUTO_FILL) {
            // Calculate how many columns fit based on minimum track size
            float min_size = 100.0f;  // Default fallback
            if (rep->has_minmax && rep->minmax.min_type == IR_GRID_TRACK_PX) {
                min_size = rep->minmax.min_value;
            } else if (!rep->has_minmax && rep->track.type == IR_GRID_TRACK_PX) {
                min_size = rep->track.value;
            }

            // Calculate: n * min_size + (n-1) * gap <= available_width
            // Solving: n <= (available_width + gap) / (min_size + gap)
            float gap = grid->column_gap;
            uint8_t count = (uint8_t)((available_width + gap) / (min_size + gap));
            if (count < 1) count = 1;
            if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;
            grid->column_count = count;

            // Populate columns with the repeat track definition
            for (uint8_t i = 0; i < count; i++) {
                if (rep->has_minmax) {
                    // For minmax, use FR for flexible distribution
                    grid->columns[i].type = IR_GRID_TRACK_FR;
                    grid->columns[i].value = (rep->minmax.max_type == IR_GRID_TRACK_FR)
                        ? rep->minmax.max_value : 1.0f;
                } else {
                    grid->columns[i] = rep->track;
                }
            }
        } else if (rep->mode == IR_GRID_REPEAT_COUNT) {
            // Fixed repeat count
            uint8_t count = rep->count;
            if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;
            grid->column_count = count;
            for (uint8_t i = 0; i < count; i++) {
                if (rep->has_minmax) {
                    grid->columns[i].type = IR_GRID_TRACK_FR;
                    grid->columns[i].value = (rep->minmax.max_type == IR_GRID_TRACK_FR)
                        ? rep->minmax.max_value : 1.0f;
                } else {
                    grid->columns[i] = rep->track;
                }
            }
        }
    }

    // Handle repeat() for rows similarly
    if (grid->use_row_repeat && grid->row_repeat.mode != IR_GRID_REPEAT_NONE) {
        IRGridRepeat* rep = &grid->row_repeat;

        if (rep->mode == IR_GRID_REPEAT_COUNT) {
            uint8_t count = rep->count;
            if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;
            grid->row_count = count;
            for (uint8_t i = 0; i < count; i++) {
                if (rep->has_minmax) {
                    grid->rows[i].type = IR_GRID_TRACK_FR;
                    grid->rows[i].value = (rep->minmax.max_type == IR_GRID_TRACK_FR)
                        ? rep->minmax.max_value : 1.0f;
                } else {
                    grid->rows[i] = rep->track;
                }
            }
        }
        // Note: auto-fit/auto-fill for rows is less common, skip for now
    }

    // Auto-calculate row count based on child count if not specified
    if (grid->row_count == 0 && container->child_count > 0 && grid->column_count > 0) {
        grid->row_count = (container->child_count + grid->column_count - 1) / grid->column_count;
        for (uint8_t i = 0; i < grid->row_count; i++) {
            grid->rows[i].type = IR_GRID_TRACK_AUTO;
            grid->rows[i].value = 0;
        }
    }

    // If no grid template is defined, fall back to auto layout
    if (grid->row_count == 0 || grid->column_count == 0) {
        return;
    }

    // Track sizes for each row and column
    float row_sizes[IR_MAX_GRID_TRACKS] = {0};
    float column_sizes[IR_MAX_GRID_TRACKS] = {0};

    // Calculate total gap space
    float total_row_gap = (grid->row_count > 1) ? (grid->row_count - 1) * grid->row_gap : 0.0f;
    float total_column_gap = (grid->column_count > 1) ? (grid->column_count - 1) * grid->column_gap : 0.0f;

    // Available space for tracks (subtract gaps)
    float track_available_width = available_width - total_column_gap;
    float track_available_height = available_height - total_row_gap;

    // ============================================================================
    // CONTENT-BASED SIZING PASS - Measure intrinsic sizes for AUTO tracks
    // ============================================================================

    // Track maximum content size for each column/row
    float column_content_width[IR_MAX_GRID_TRACKS] = {0};
    float row_content_height[IR_MAX_GRID_TRACKS] = {0};

    // First, determine where each child will be placed (same logic as placement below)
    uint8_t auto_row = 0, auto_col = 0;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child || !child->style || !child->style->visible) continue;

        IRGridItem* item = &child->style->grid_item;

        // Determine grid placement (same logic as placement phase)
        int16_t row_start, col_start;

        if (item->row_start >= 0 && item->column_start >= 0) {
            // Explicit placement
            row_start = item->row_start;
            col_start = item->column_start;
        } else {
            // Auto placement
            if (grid->auto_flow_row) {
                row_start = auto_row;
                col_start = auto_col;
                auto_col++;
                if (auto_col >= grid->column_count) {
                    auto_col = 0;
                    auto_row++;
                }
            } else {
                row_start = auto_row;
                col_start = auto_col;
                auto_row++;
                if (auto_row >= grid->row_count) {
                    auto_row = 0;
                    auto_col++;
                }
            }
        }

        // Clamp to grid bounds
        if (row_start >= grid->row_count) row_start = grid->row_count - 1;
        if (col_start >= grid->column_count) col_start = grid->column_count - 1;

        // Measure child's intrinsic size
        float child_width = ir_get_component_intrinsic_width(child);
        float child_height = ir_get_component_intrinsic_height(child);

        // Add child's padding/margin to content size
        if (child->style) {
            child_width += child->style->margin.left + child->style->margin.right;
            child_height += child->style->margin.top + child->style->margin.bottom;
        }

        // Update column content width (only for AUTO/MIN_CONTENT/MAX_CONTENT columns)
        if (grid->columns[col_start].type == IR_GRID_TRACK_AUTO ||
            grid->columns[col_start].type == IR_GRID_TRACK_MIN_CONTENT ||
            grid->columns[col_start].type == IR_GRID_TRACK_MAX_CONTENT) {
            if (child_width > column_content_width[col_start]) {
                column_content_width[col_start] = child_width;
            }
        }

        // Update row content height (only for AUTO/MIN_CONTENT/MAX_CONTENT rows)
        if (grid->rows[row_start].type == IR_GRID_TRACK_AUTO ||
            grid->rows[row_start].type == IR_GRID_TRACK_MIN_CONTENT ||
            grid->rows[row_start].type == IR_GRID_TRACK_MAX_CONTENT) {
            if (child_height > row_content_height[row_start]) {
                row_content_height[row_start] = child_height;
            }
        }
    }

    // ============================================================================
    // TRACK SIZING PASS - Calculate final track sizes
    // ============================================================================

    float remaining_width = track_available_width;
    float remaining_height = track_available_height;
    float total_fr_width = 0.0f;
    float total_fr_height = 0.0f;

    // Calculate column sizes
    for (uint8_t col = 0; col < grid->column_count; col++) {
        IRGridTrack* track = &grid->columns[col];
        switch (track->type) {
            case IR_GRID_TRACK_PX:
                column_sizes[col] = track->value;
                remaining_width -= track->value;
                break;
            case IR_GRID_TRACK_PERCENT:
                column_sizes[col] = track_available_width * (track->value / 100.0f);
                remaining_width -= column_sizes[col];
                break;
            case IR_GRID_TRACK_FR:
                total_fr_width += track->value;
                break;
            case IR_GRID_TRACK_MIN_CONTENT:
                // Use the minimum content size measured
                column_sizes[col] = column_content_width[col];
                if (column_sizes[col] <= 0) column_sizes[col] = 50.0f; // Minimum fallback
                remaining_width -= column_sizes[col];
                break;
            case IR_GRID_TRACK_MAX_CONTENT:
            case IR_GRID_TRACK_AUTO:
                // Use content size, but allow growth if space available
                column_sizes[col] = column_content_width[col];
                if (column_sizes[col] <= 0) column_sizes[col] = 100.0f; // Reasonable default
                // AUTO tracks can share remaining space (treat like FR with base content size)
                total_fr_width += 1.0f;
                break;
        }
    }

    // Calculate row sizes
    for (uint8_t row = 0; row < grid->row_count; row++) {
        IRGridTrack* track = &grid->rows[row];
        switch (track->type) {
            case IR_GRID_TRACK_PX:
                row_sizes[row] = track->value;
                remaining_height -= track->value;
                break;
            case IR_GRID_TRACK_PERCENT:
                row_sizes[row] = track_available_height * (track->value / 100.0f);
                remaining_height -= row_sizes[row];
                break;
            case IR_GRID_TRACK_FR:
                total_fr_height += track->value;
                break;
            case IR_GRID_TRACK_MIN_CONTENT:
                // Use the minimum content size measured
                row_sizes[row] = row_content_height[row];
                if (row_sizes[row] <= 0) row_sizes[row] = 24.0f; // Minimum fallback
                remaining_height -= row_sizes[row];
                break;
            case IR_GRID_TRACK_MAX_CONTENT:
            case IR_GRID_TRACK_AUTO:
                // Use content size, but allow growth if space available
                row_sizes[row] = row_content_height[row];
                if (row_sizes[row] <= 0) row_sizes[row] = 24.0f; // Reasonable default
                // AUTO tracks can share remaining space (treat like FR with base content size)
                total_fr_height += 1.0f;
                break;
        }
    }

    // Second pass: distribute remaining space to fractional units
    if (remaining_width < 0) remaining_width = 0;
    if (remaining_height < 0) remaining_height = 0;

    float fr_width_unit = (total_fr_width > 0) ? (remaining_width / total_fr_width) : 0.0f;
    float fr_height_unit = (total_fr_height > 0) ? (remaining_height / total_fr_height) : 0.0f;

    for (uint8_t col = 0; col < grid->column_count; col++) {
        if (grid->columns[col].type == IR_GRID_TRACK_FR ||
            grid->columns[col].type == IR_GRID_TRACK_AUTO ||
            grid->columns[col].type == IR_GRID_TRACK_MIN_CONTENT ||
            grid->columns[col].type == IR_GRID_TRACK_MAX_CONTENT) {
            float fr = (grid->columns[col].type == IR_GRID_TRACK_FR) ? grid->columns[col].value : 1.0f;
            column_sizes[col] = fr * fr_width_unit;
        }
    }

    for (uint8_t row = 0; row < grid->row_count; row++) {
        if (grid->rows[row].type == IR_GRID_TRACK_FR ||
            grid->rows[row].type == IR_GRID_TRACK_AUTO ||
            grid->rows[row].type == IR_GRID_TRACK_MIN_CONTENT ||
            grid->rows[row].type == IR_GRID_TRACK_MAX_CONTENT) {
            float fr = (grid->rows[row].type == IR_GRID_TRACK_FR) ? grid->rows[row].value : 1.0f;
            row_sizes[row] = fr * fr_height_unit;
        }
    }

    // Calculate row/column start positions
    float row_positions[IR_MAX_GRID_TRACKS + 1];
    float column_positions[IR_MAX_GRID_TRACKS + 1];

    row_positions[0] = container->style->padding.top;
    for (uint8_t row = 0; row < grid->row_count; row++) {
        row_positions[row + 1] = row_positions[row] + row_sizes[row] + grid->row_gap;
    }

    column_positions[0] = container->style->padding.left;
    for (uint8_t col = 0; col < grid->column_count; col++) {
        column_positions[col + 1] = column_positions[col] + column_sizes[col] + grid->column_gap;
    }

    // Place child components in grid cells
    // Reset auto placement counters for actual placement
    auto_row = 0;
    auto_col = 0;

    for (uint32_t i = 0; i < container->child_count; i++) {
        IRComponent* child = container->children[i];
        if (!child->style || !child->style->visible) continue;

        IRGridItem* item = &child->style->grid_item;

        // Determine grid placement
        int16_t row_start, row_end, col_start, col_end;

        if (item->row_start >= 0 && item->column_start >= 0) {
            // Explicit placement
            row_start = item->row_start;
            col_start = item->column_start;
            row_end = (item->row_end >= 0) ? item->row_end : (row_start + 1);
            col_end = (item->column_end >= 0) ? item->column_end : (col_start + 1);
        } else {
            // Auto placement
            if (grid->auto_flow_row) {
                row_start = auto_row;
                col_start = auto_col;
                row_end = row_start + 1;
                col_end = col_start + 1;

                auto_col++;
                if (auto_col >= grid->column_count) {
                    auto_col = 0;
                    auto_row++;
                }
            } else {
                row_start = auto_row;
                col_start = auto_col;
                row_end = row_start + 1;
                col_end = col_start + 1;

                auto_row++;
                if (auto_row >= grid->row_count) {
                    auto_row = 0;
                    auto_col++;
                }
            }
        }

        // Clamp to grid bounds
        if (row_start >= grid->row_count) row_start = grid->row_count - 1;
        if (col_start >= grid->column_count) col_start = grid->column_count - 1;
        if (row_end > grid->row_count) row_end = grid->row_count;
        if (col_end > grid->column_count) col_end = grid->column_count;

        // Calculate cell dimensions (including spans)
        float cell_x = column_positions[col_start];
        float cell_y = row_positions[row_start];
        float cell_width = column_positions[col_end] - cell_x - grid->column_gap;
        float cell_height = row_positions[row_end] - cell_y - grid->row_gap;

        // Apply margins
        float item_x = cell_x + child->style->margin.left;
        float item_y = cell_y + child->style->margin.top;
        float item_width = cell_width - child->style->margin.left - child->style->margin.right;
        float item_height = cell_height - child->style->margin.top - child->style->margin.bottom;

        if (item_width < 0) item_width = 0;
        if (item_height < 0) item_height = 0;

        // Apply alignment (justify-self and align-self)
        IRAlignment justify = (item->justify_self != IR_ALIGNMENT_START) ?
            item->justify_self : grid->justify_items;
        IRAlignment align = (item->align_self != IR_ALIGNMENT_START) ?
            item->align_self : grid->align_items;

        // Get child intrinsic size for alignment
        float child_width = ir_get_component_intrinsic_width(child);
        float child_height = ir_get_component_intrinsic_height(child);

        // If child has explicit size, use that
        if (child->style->width.type == IR_DIMENSION_PX) {
            child_width = child->style->width.value;
        } else if (child->style->width.type == IR_DIMENSION_PERCENT) {
            child_width = item_width * (child->style->width.value / 100.0f);
        }

        if (child->style->height.type == IR_DIMENSION_PX) {
            child_height = child->style->height.value;
        } else if (child->style->height.type == IR_DIMENSION_PERCENT) {
            child_height = item_height * (child->style->height.value / 100.0f);
        }

        // Apply horizontal alignment
        switch (justify) {
            case IR_ALIGNMENT_CENTER:
                item_x += (item_width - child_width) / 2.0f;
                item_width = child_width;
                break;
            case IR_ALIGNMENT_END:
                item_x += item_width - child_width;
                item_width = child_width;
                break;
            case IR_ALIGNMENT_STRETCH:
                // Use full cell width (default behavior)
                break;
            case IR_ALIGNMENT_START:
            default:
                item_width = child_width;
                break;
        }

        // Apply vertical alignment
        switch (align) {
            case IR_ALIGNMENT_CENTER:
                item_y += (item_height - child_height) / 2.0f;
                item_height = child_height;
                break;
            case IR_ALIGNMENT_END:
                item_y += item_height - child_height;
                item_height = child_height;
                break;
            case IR_ALIGNMENT_STRETCH:
                // Use full cell height (default behavior)
                break;
            case IR_ALIGNMENT_START:
            default:
                item_height = child_height;
                break;
        }

        // Set child bounds
        child->rendered_bounds.x = item_x;
        child->rendered_bounds.y = item_y;
        child->rendered_bounds.width = item_width;
        child->rendered_bounds.height = item_height;
        child->rendered_bounds.valid = true;
    }
}

// ============================================================================
// Text Layout System (Multi-line text wrapping)
// ============================================================================

// Global font metrics (set by backend)
IRFontMetrics* g_ir_font_metrics = NULL;

void ir_set_font_metrics(IRFontMetrics* metrics) {
    g_ir_font_metrics = metrics;
}

// Create a new text layout structure
IRTextLayout* ir_text_layout_create(uint32_t max_lines) {
    IRTextLayout* layout = (IRTextLayout*)calloc(1, sizeof(IRTextLayout));
    if (!layout) return NULL;

    layout->line_widths = (float*)malloc(sizeof(float) * max_lines);
    layout->line_heights = (float*)malloc(sizeof(float) * max_lines);
    layout->line_start_indices = (uint32_t*)malloc(sizeof(uint32_t) * max_lines);
    layout->line_end_indices = (uint32_t*)malloc(sizeof(uint32_t) * max_lines);
    layout->line_count = 0;
    layout->total_height = 0.0f;
    layout->max_line_width = 0.0f;
    layout->needs_reshape = false;
    layout->direction = IR_TEXT_DIR_LTR;

    return layout;
}

// Destroy text layout and free resources
void ir_text_layout_destroy(IRTextLayout* layout) {
    if (!layout) return;

    free(layout->line_widths);
    free(layout->line_heights);
    free(layout->line_start_indices);
    free(layout->line_end_indices);
    free(layout);
}

// Helper: Check if character is whitespace
static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

// Helper: Find next word boundary (returns index after word)
static uint32_t find_next_word_boundary(const char* text, uint32_t start, uint32_t length) {
    uint32_t i = start;

    // Skip leading whitespace
    while (i < length && is_whitespace(text[i])) {
        i++;
    }

    // Find end of word (next whitespace or end of string)
    while (i < length && !is_whitespace(text[i]) && text[i] != '\n') {
        i++;
    }

    return i;
}

// Helper: Measure text width using backend or fallback
static float measure_text_width(const char* text, uint32_t start, uint32_t end,
                                 float font_size, const char* font_family) {
    if (g_ir_font_metrics && g_ir_font_metrics->get_text_width) {
        return g_ir_font_metrics->get_text_width(text + start, end - start, font_size, font_family);
    }

    // Fallback: estimate
    uint32_t char_count = end - start;
    return char_count * font_size * 0.5f;
}

// Compute multi-line text layout with greedy line-breaking
void ir_text_layout_compute(IRComponent* component, float max_width, float font_size) {
    if (!component || component->type != IR_COMPONENT_TEXT || !component->text_content) {
        return;
    }

    const char* text = component->text_content;
    uint32_t text_length = strlen(text);

    if (text_length == 0) {
        return;
    }

    // Destroy existing layout if any
    if (component->text_layout) {
        ir_text_layout_destroy(component->text_layout);
    }

    // Create new layout (estimate max lines as text_length / 10)
    uint32_t max_lines = (text_length / 10) + 10;
    IRTextLayout* layout = ir_text_layout_create(max_lines);
    if (!layout) return;

    // Get font properties
    const char* font_family = (component->style && component->style->font.family)
                              ? component->style->font.family : "sans-serif";

    // Get line height (default 1.2 * font_size)
    float line_height = font_size;
    if (component->style && component->style->font.line_height > 0) {
        line_height = font_size * component->style->font.line_height;
    } else {
        line_height = font_size * 1.2f;
    }

    // Get text direction
    if (component->style && component->style->text_effect.text_direction != IR_TEXT_DIR_AUTO) {
        layout->direction = component->style->text_effect.text_direction;
    } else {
        layout->direction = IR_TEXT_DIR_LTR;  // Default LTR, auto-detect in Phase 3
    }

    // Greedy line-breaking algorithm
    uint32_t line_index = 0;
    uint32_t current_pos = 0;

    while (current_pos < text_length && line_index < max_lines) {
        uint32_t line_start = current_pos;
        uint32_t line_end = line_start;
        float current_line_width = 0.0f;
        uint32_t last_break_point = line_start;
        float last_break_width = 0.0f;

        // Handle explicit line break
        if (text[current_pos] == '\n') {
            // Empty line
            layout->line_start_indices[line_index] = line_start;
            layout->line_end_indices[line_index] = line_start;
            layout->line_widths[line_index] = 0.0f;
            layout->line_heights[line_index] = line_height;
            line_index++;
            current_pos++;
            continue;
        }

        // Build line until we exceed max_width or hit newline
        while (current_pos < text_length) {
            // Check for explicit line break
            if (text[current_pos] == '\n') {
                line_end = current_pos;
                current_pos++;  // Skip newline
                break;
            }

            // Find next word
            uint32_t next_word_end = find_next_word_boundary(text, current_pos, text_length);

            // Measure text from line start to next word end
            float potential_width = measure_text_width(text, line_start, next_word_end,
                                                        font_size, font_family);

            // Check if adding this word would exceed max_width
            if (max_width > 0 && potential_width > max_width) {
                // If this is the first word on the line, we must include it anyway
                if (line_end == line_start) {
                    line_end = next_word_end;
                    current_line_width = potential_width;
                    current_pos = next_word_end;
                } else {
                    // Break at last valid point
                    line_end = last_break_point;
                    current_line_width = last_break_width;
                }
                break;
            }

            // Word fits, update line end
            last_break_point = next_word_end;
            last_break_width = potential_width;
            line_end = next_word_end;
            current_line_width = potential_width;
            current_pos = next_word_end;

            // If we reached end of text, we're done
            if (current_pos >= text_length) {
                break;
            }
        }

        // Trim trailing whitespace from line_end
        while (line_end > line_start && is_whitespace(text[line_end - 1])) {
            line_end--;
            current_line_width = measure_text_width(text, line_start, line_end,
                                                     font_size, font_family);
        }

        // Record line
        layout->line_start_indices[line_index] = line_start;
        layout->line_end_indices[line_index] = line_end;
        layout->line_widths[line_index] = current_line_width;
        layout->line_heights[line_index] = line_height;

        // Update max line width
        if (current_line_width > layout->max_line_width) {
            layout->max_line_width = current_line_width;
        }

        line_index++;
    }

    // Finalize layout
    layout->line_count = line_index;
    layout->total_height = line_index * line_height;

    // Store layout in component
    component->text_layout = layout;

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "📝 TEXT_LAYOUT computed for component %u: %u lines, max_width=%.1f, total_height=%.1f\n",
            component->id, layout->line_count, layout->max_line_width, layout->total_height);
    #endif
}

// Get width of text layout
float ir_text_layout_get_width(IRTextLayout* layout) {
    return layout ? layout->max_line_width : 0.0f;
}

// Get height of text layout
float ir_text_layout_get_height(IRTextLayout* layout) {
    return layout ? layout->total_height : 0.0f;
}

// ============================================================================
// Table Layout
// ============================================================================

// Helper: Get cell data from a cell component
static IRTableCellData* get_cell_data(IRComponent* cell) {
    if (!cell) return NULL;
    if (cell->type != IR_COMPONENT_TABLE_CELL &&
        cell->type != IR_COMPONENT_TABLE_HEADER_CELL) return NULL;
    return (IRTableCellData*)cell->custom_data;
}

// Helper: Count columns in a table by looking at first row
static uint32_t table_count_columns(IRComponent* table) {
    uint32_t max_cols = 0;

    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (section->type != IR_COMPONENT_TABLE_HEAD &&
            section->type != IR_COMPONENT_TABLE_BODY &&
            section->type != IR_COMPONENT_TABLE_FOOT) continue;

        for (uint32_t j = 0; j < section->child_count; j++) {
            IRComponent* row = section->children[j];
            if (row->type != IR_COMPONENT_TABLE_ROW) continue;

            uint32_t col_count = 0;
            for (uint32_t k = 0; k < row->child_count; k++) {
                IRComponent* cell = row->children[k];
                IRTableCellData* data = get_cell_data(cell);
                uint32_t colspan = data ? data->colspan : 1;
                if (colspan == 0) colspan = 1;
                col_count += colspan;
            }
            if (col_count > max_cols) max_cols = col_count;
        }
    }

    return max_cols;
}

// Helper: Count total rows in a table
static uint32_t table_count_rows(IRComponent* table) {
    uint32_t row_count = 0;

    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (section->type != IR_COMPONENT_TABLE_HEAD &&
            section->type != IR_COMPONENT_TABLE_BODY &&
            section->type != IR_COMPONENT_TABLE_FOOT) continue;

        for (uint32_t j = 0; j < section->child_count; j++) {
            if (section->children[j]->type == IR_COMPONENT_TABLE_ROW) {
                row_count++;
            }
        }
    }

    return row_count;
}

// Helper: Get intrinsic cell width
static float table_get_cell_content_width(IRComponent* cell) {
    float width = 0.0f;

    // Calculate content width from children or text
    if (cell->text_content) {
        float font_size = cell->style ? cell->style->font.size : 16.0f;
        if (font_size <= 0) font_size = 16.0f;
        width = ir_get_text_width_estimate(cell->text_content, font_size);
    }

    // Check children
    for (uint32_t i = 0; i < cell->child_count; i++) {
        float child_width = ir_get_component_intrinsic_width(cell->children[i]);
        if (child_width > width) width = child_width;
    }

    // Add cell padding
    if (cell->style) {
        width += cell->style->padding.left + cell->style->padding.right;
    }

    return width > 0 ? width : 50.0f;  // Minimum cell width
}

// Helper: Get intrinsic cell height
static float table_get_cell_content_height(IRComponent* cell) {
    float height = 0.0f;

    // Calculate content height from children or text
    if (cell->text_content) {
        float font_size = cell->style ? cell->style->font.size : 16.0f;
        if (font_size <= 0) font_size = 16.0f;
        height = font_size * 1.5f;  // Line height approximation
    }

    // Check children
    for (uint32_t i = 0; i < cell->child_count; i++) {
        float child_height = ir_get_component_intrinsic_height(cell->children[i]);
        if (child_height > height) height = child_height;
    }

    // Add cell padding
    if (cell->style) {
        height += cell->style->padding.top + cell->style->padding.bottom;
    }

    return height > 0 ? height : 24.0f;  // Minimum cell height
}

/**
 * Compute intrinsic size for Table components
 * Runs table layout algorithm (phases 1-4) to calculate dimensions
 */

// Main table layout function
void ir_layout_compute_table(IRComponent* table, float available_width, float available_height) {
    fprintf(stderr, "[TABLE_LAYOUT_ENTRY] table=%p type=%u available=%.1fx%.1f\n",
            (void*)table, table ? (uint32_t)table->type : 0xFFFFFFFFu, available_width, available_height);

    if (!table || table->type != IR_COMPONENT_TABLE) {
        fprintf(stderr, "[TABLE_LAYOUT_ENTRY] Early return: table=%p type=%u\n",
                (void*)table, table ? (uint32_t)table->type : 0xFFFFFFFFu);
        return;
    }

    IRTableState* state = (IRTableState*)table->custom_data;
    fprintf(stderr, "[TABLE_LAYOUT_ENTRY] state=%p\n", (void*)state);

    // Debug: always trace layout state
    if (getenv("KRYON_TRACE_LAYOUT")) {
        fprintf(stderr, "📊 TABLE_LAYOUT: state=%p layout_valid=%d cached_h=%.1f\n",
                (void*)state, state ? state->layout_valid : -1,
                state ? state->cached_total_height : 0);
    }

    // Use cached layout if valid (avoids recomputing every frame)
    if (state && state->layout_valid && state->cached_total_height > 0) {
        if (getenv("KRYON_TRACE_LAYOUT")) {
            fprintf(stderr, "📊 TABLE: using cached layout (%.1fx%.1f)\n",
                    state->cached_total_width, state->cached_total_height);
        }
        // Mark children as valid when using cache (they already have positions from previous computation)
        for (uint32_t i = 0; i < table->child_count; i++) {
            IRComponent* section = table->children[i];
            if (!section) continue;
            if (section->layout_state) {
                section->layout_state->layout_valid = true;
            }

            for (uint32_t j = 0; j < section->child_count; j++) {
                IRComponent* row = section->children[j];
                if (!row) continue;
                if (row->layout_state) {
                    row->layout_state->layout_valid = true;
                }

                for (uint32_t k = 0; k < row->child_count; k++) {
                    IRComponent* cell = row->children[k];
                    if (!cell) continue;
                    if (cell->layout_state) {
                        cell->layout_state->layout_valid = true;
                    }
                }
            }
        }
        return;
    }

    IRStyle* table_style __attribute__((unused)) = table->style;

    uint32_t num_cols = table_count_columns(table);
    uint32_t num_rows = table_count_rows(table);

    if (num_cols == 0 || num_rows == 0) return;

    // Get table styling
    float cell_padding = state ? state->style.cell_padding : 8.0f;
    float border_width = state && state->style.show_borders ? state->style.border_width : 0.0f;

    // Allocate arrays for column widths and row heights
    float* col_widths = (float*)calloc(num_cols, sizeof(float));
    float* row_heights = (float*)calloc(num_rows, sizeof(float));
    float* min_col_widths = (float*)calloc(num_cols, sizeof(float));

    if (!col_widths || !row_heights || !min_col_widths) {
        free(col_widths);
        free(row_heights);
        free(min_col_widths);
        return;
    }

    // Initialize minimum widths from column definitions
    for (uint32_t c = 0; c < num_cols; c++) {
        if (state && c < state->column_count && state->columns) {
            IRTableColumnDef* col_def = &state->columns[c];
            if (col_def->width.type == IR_DIMENSION_PX) {
                col_widths[c] = col_def->width.value;
            }
            if (col_def->min_width.type == IR_DIMENSION_PX) {
                min_col_widths[c] = col_def->min_width.value;
            }
        }
    }

    // Phase 1: Calculate intrinsic column widths from cell content
    uint32_t row_idx = 0;
    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (section->type != IR_COMPONENT_TABLE_HEAD &&
            section->type != IR_COMPONENT_TABLE_BODY &&
            section->type != IR_COMPONENT_TABLE_FOOT) continue;

        for (uint32_t j = 0; j < section->child_count; j++) {
            IRComponent* row = section->children[j];
            if (row->type != IR_COMPONENT_TABLE_ROW) continue;

            uint32_t col_idx = 0;
            for (uint32_t k = 0; k < row->child_count; k++) {
                IRComponent* cell = row->children[k];
                IRTableCellData* data = get_cell_data(cell);
                uint32_t colspan = data ? data->colspan : 1;
                if (colspan == 0) colspan = 1;

                // Only measure cells without colspan for initial width
                if (colspan == 1 && col_idx < num_cols) {
                    float content_width = table_get_cell_content_width(cell) + cell_padding * 2;
                    if (content_width > col_widths[col_idx]) {
                        col_widths[col_idx] = content_width;
                    }
                }

                col_idx += colspan;
            }
            row_idx++;
        }
    }

    // Phase 2: Apply minimum widths
    for (uint32_t c = 0; c < num_cols; c++) {
        if (col_widths[c] < min_col_widths[c]) {
            col_widths[c] = min_col_widths[c];
        }
        if (col_widths[c] < 30.0f) {
            col_widths[c] = 30.0f;  // Absolute minimum
        }
    }

    // Phase 3: Distribute available width proportionally if we have excess space
    float total_width = 0;
    for (uint32_t c = 0; c < num_cols; c++) {
        total_width += col_widths[c];
    }
    total_width += border_width * (num_cols + 1);

    if (total_width < available_width && num_cols > 0) {
        float extra = available_width - total_width;
        float extra_per_col = extra / num_cols;
        for (uint32_t c = 0; c < num_cols; c++) {
            col_widths[c] += extra_per_col;
        }
        total_width = available_width;
    }

    // Phase 4: Calculate row heights from cell content
    row_idx = 0;
    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (section->type != IR_COMPONENT_TABLE_HEAD &&
            section->type != IR_COMPONENT_TABLE_BODY &&
            section->type != IR_COMPONENT_TABLE_FOOT) continue;

        for (uint32_t j = 0; j < section->child_count; j++) {
            IRComponent* row = section->children[j];
            if (row->type != IR_COMPONENT_TABLE_ROW) continue;

            float max_cell_height = 0;
            for (uint32_t k = 0; k < row->child_count; k++) {
                IRComponent* cell = row->children[k];
                IRTableCellData* data = get_cell_data(cell);
                uint32_t rowspan = data ? data->rowspan : 1;
                if (rowspan == 0) rowspan = 1;

                // Only measure cells without rowspan for initial height
                if (rowspan == 1) {
                    float content_height = table_get_cell_content_height(cell) + cell_padding * 2;
                    if (content_height > max_cell_height) {
                        max_cell_height = content_height;
                    }
                }
            }

            row_heights[row_idx] = max_cell_height > 0 ? max_cell_height : 30.0f;
            row_idx++;
        }
    }

    // Calculate total table height
    float total_height = 0;
    for (uint32_t r = 0; r < num_rows; r++) {
        total_height += row_heights[r];
    }
    total_height += border_width * (num_rows + 1);

    // Phase 5: Position all cells using layout_state->computed
    float y_offset = border_width;
    row_idx = 0;

    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (section->type != IR_COMPONENT_TABLE_HEAD &&
            section->type != IR_COMPONENT_TABLE_BODY &&
            section->type != IR_COMPONENT_TABLE_FOOT) continue;

        if (!section->layout_state) continue;

        // Position section to fill table width
        section->layout_state->computed.x = 0;
        section->layout_state->computed.y = y_offset;
        section->layout_state->computed.width = total_width;

        float section_height = 0;

        for (uint32_t j = 0; j < section->child_count; j++) {
            IRComponent* row = section->children[j];
            if (row->type != IR_COMPONENT_TABLE_ROW) continue;
            if (!row->layout_state) continue;

            // Position row
            row->layout_state->computed.x = 0;
            row->layout_state->computed.y = section_height;
            row->layout_state->computed.width = total_width;
            row->layout_state->computed.height = row_heights[row_idx];

            float x_offset = border_width;
            uint32_t col_idx = 0;

            for (uint32_t k = 0; k < row->child_count; k++) {
                IRComponent* cell = row->children[k];
                if (!cell->layout_state) continue;

                IRTableCellData* data = get_cell_data(cell);
                uint32_t colspan = data ? data->colspan : 1;
                uint32_t rowspan = data ? data->rowspan : 1;
                if (colspan == 0) colspan = 1;
                if (rowspan == 0) rowspan = 1;

                // Calculate cell width (sum of spanned columns)
                float cell_width = 0;
                for (uint32_t c = col_idx; c < col_idx + colspan && c < num_cols; c++) {
                    cell_width += col_widths[c];
                }
                cell_width += border_width * (colspan - 1);

                // Calculate cell height (sum of spanned rows)
                float cell_height = 0;
                for (uint32_t r = row_idx; r < row_idx + rowspan && r < num_rows; r++) {
                    cell_height += row_heights[r];
                }
                cell_height += border_width * (rowspan - 1);

                // Position cell (relative to row)
                cell->layout_state->computed.x = x_offset;
                cell->layout_state->computed.y = 0;  // Relative to row
                cell->layout_state->computed.width = cell_width;
                cell->layout_state->computed.height = cell_height;

                x_offset += cell_width + border_width;
                col_idx += colspan;
            }

            section_height += row_heights[row_idx] + border_width;
            row_idx++;
        }

        section->layout_state->computed.height = section_height;
        y_offset += section_height;
    }

    // Update table state with calculated widths if present
    if (state) {
        if (state->calculated_widths) free(state->calculated_widths);
        if (state->calculated_heights) free(state->calculated_heights);

        state->calculated_widths = col_widths;
        state->calculated_heights = row_heights;
        state->row_count = num_rows;
        state->layout_valid = true;
        state->cached_total_width = total_width;
        state->cached_total_height = total_height;

        // Mark all children as valid after computing layout
        // Also reset absolute_positions_computed since we just set RELATIVE positions
        for (uint32_t i = 0; i < table->child_count; i++) {
            IRComponent* section = table->children[i];
            if (!section) continue;
            if (section->layout_state) {
                section->layout_state->layout_valid = true;
            }

            for (uint32_t j = 0; j < section->child_count; j++) {
                IRComponent* row = section->children[j];
                if (!row) continue;
                if (row->layout_state) {
                    row->layout_state->layout_valid = true;
                }

                for (uint32_t k = 0; k < row->child_count; k++) {
                    IRComponent* cell = row->children[k];
                    if (!cell) continue;
                    if (cell->layout_state) {
                        cell->layout_state->layout_valid = true;
                    }
                }
            }
        }

        free(min_col_widths);
    } else {
        free(col_widths);
        free(row_heights);
        free(min_col_widths);
    }

    // Update table dimensions in layout_state->computed
    if (table->layout_state) {
        table->layout_state->computed.width = total_width;
        table->layout_state->computed.height = total_height;
        // X and Y should already be set by parent layout in constraint phase
    }

    // Mark layout as cached with current input dimensions
    if (state) {
        state->layout_valid = true;
        state->cached_available_width = available_width;
        state->cached_available_height = available_height;
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "📊 TABLE_LAYOUT: %u cols x %u rows, size=%.1fx%.1f\n",
            num_cols, num_rows, total_width, total_height);
    #endif
}


// ============================================================================
// Two-Pass Layout System - Clay-Inspired Architecture
// ============================================================================

// ============================================================================
// Phase 1: Bottom-Up Intrinsic Sizing (Content-Based)
// ============================================================================

/**
 * Compute intrinsic size for Text components
 * Uses backend text measurement callback if available, otherwise fallback heuristic
 */

// ============================================================================
// Single-Pass Recursive Layout System (NEW)
// ============================================================================

/**
 * Single-pass recursive layout algorithm
 *
 * Computes final dimensions and positions in a single bottom-up post-order traversal.
 * Each component fully resolves its layout before returning to parent, eliminating
 * timing issues where parents read stale child dimensions.
 *
 * Algorithm:
 * 1. Resolve own width/height (or mark AUTO)
 * 2. For each child:
 *    - Recursively call ir_layout_single_pass (child computes FINAL dimensions)
 *    - Position child using fresh child.computed.width/height
 *    - Advance position for next child
 * 3. If AUTO dimensions: finalize based on positioned children
 * 4. Convert to absolute coordinates
 * 5. Return (parent can now use our final dimensions)
 */
void ir_layout_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                           float parent_x, float parent_y) {
    if (!c) return;

    // Ensure layout state exists
    if (!c->layout_state) {
        c->layout_state = (IRLayoutState*)calloc(1, sizeof(IRLayoutState));
    }

    // If component is dirty or layout invalid, clear cached dimensions
    // This ensures fresh layout computation when needed
    if (c->layout_state->dirty || !c->layout_state->layout_valid) {
        c->layout_state->computed.width = 0;
        c->layout_state->computed.height = 0;
        c->layout_state->computed.valid = false;
        c->layout_state->dirty = false;  // Clear dirty flag as we're recomputing
    }

    // Modal components don't take space in normal layout flow
    // They are rendered as overlays with their own positioning
    if (c->type == IR_COMPONENT_MODAL) {
        c->layout_state->computed.x = parent_x;
        c->layout_state->computed.y = parent_y;
        c->layout_state->computed.width = 0;
        c->layout_state->computed.height = 0;
        c->layout_state->computed.valid = true;
        c->layout_state->layout_valid = true;

        // Get modal's actual dimensions from style for child layout
        float modal_width = 300.0f;  // Default
        float modal_height = 200.0f; // Default
        float padding = 24.0f;       // Default modal padding

        if (c->style) {
            if (c->style->width.type == IR_DIMENSION_PX && c->style->width.value > 0) {
                modal_width = c->style->width.value;
            }
            if (c->style->height.type == IR_DIMENSION_PX && c->style->height.value > 0) {
                modal_height = c->style->height.value;
            }
            if (c->style->padding.top > 0) {
                padding = c->style->padding.top;
            }
        }

        // Compute children layout relative to (0,0) with modal's content area size
        IRLayoutConstraints modal_constraints = {
            .min_width = 0,
            .max_width = modal_width - 2 * padding,
            .min_height = 0,
            .max_height = modal_height - 2 * padding
        };

        for (uint32_t i = 0; i < c->child_count; i++) {
            if (c->children[i]) {
                // Layout children relative to origin (0,0) - will be offset during rendering
                ir_layout_single_pass(c->children[i], modal_constraints, 0, 0);
            }
        }
        return;
    }

    // Try to dispatch to component-specific trait first
    ir_layout_dispatch(c, constraints, parent_x, parent_y);

    // If dispatch handled it (trait registered), we're done
    // Check if computed dimensions are set (trait completed layout)
    if (c->layout_state->computed.width > 0 || c->layout_state->computed.height > 0) {
        // Skip rendered_bounds sync for modal children - they are positioned
        // during overlay rendering with correct absolute coordinates.
        // Layout pass uses (0,0) as parent, but rendering repositions correctly.
        bool is_modal_descendant = false;
        IRComponent* ancestor = c->parent;
        while (ancestor) {
            if (ancestor->type == IR_COMPONENT_MODAL) {
                is_modal_descendant = true;
                break;
            }
            ancestor = ancestor->parent;
        }

        if (!is_modal_descendant) {
            // Sync computed layout to rendered_bounds for click detection
            c->rendered_bounds.x = c->layout_state->computed.x;
            c->rendered_bounds.y = c->layout_state->computed.y;
            c->rendered_bounds.width = c->layout_state->computed.width;
            c->rendered_bounds.height = c->layout_state->computed.height;
            c->rendered_bounds.valid = true;
        }
        return;
    }

    // Otherwise, fall back to generic container layout
    // This is a simple implementation - will be enhanced in Phase 4 (Axis-Parameterized Flexbox)

    if (getenv("KRYON_DEBUG_SINGLE_PASS")) {
        fprintf(stderr, "[SinglePass] Component type %d: using fallback generic layout\n", c->type);
    }

    // Special handling for Center component (IR_COMPONENT_CENTER = 8)
    if (c->type == IR_COMPONENT_CENTER) {
        // Get padding
        float pad_left = c->style ? c->style->padding.left : 0;
        float pad_top = c->style ? c->style->padding.top : 0;
        float pad_right = c->style ? c->style->padding.right : 0;
        float pad_bottom = c->style ? c->style->padding.bottom : 0;

        float available_width = constraints.max_width - pad_left - pad_right;
        float available_height = constraints.max_height - pad_top - pad_bottom;

        // Layout the child if it exists
        if (c->child_count > 0 && c->children[0]) {
            IRComponent* child = c->children[0];

            IRLayoutConstraints child_constraints = {
                .max_width = available_width,
                .max_height = available_height,
                .min_width = 0,
                .min_height = 0
            };

            // Layout child at temporary position
            ir_layout_single_pass(child, child_constraints, parent_x + pad_left, parent_y + pad_top);

            // Center the child (skip if absolutely positioned)
            if (child->layout_state) {
                // Skip absolutely positioned children - they already have their final position
                if (child->style && child->style->position_mode == IR_POSITION_ABSOLUTE) {
                    // Don't override absolute position
                } else {
                    float child_width = child->layout_state->computed.width;
                    float child_height = child->layout_state->computed.height;
                    float offset_x = (available_width - child_width) / 2.0f;
                    float offset_y = (available_height - child_height) / 2.0f;

                    child->layout_state->computed.x = parent_x + pad_left + offset_x;
                    child->layout_state->computed.y = parent_y + pad_top + offset_y;
                }
            }
        }

        // Set Center container's dimensions
        c->layout_state->computed.width = constraints.max_width;
        c->layout_state->computed.height = constraints.max_height;
        c->layout_state->computed.x = parent_x;
        c->layout_state->computed.y = parent_y;
        c->layout_state->layout_valid = true;
        c->layout_state->computed.valid = true;
        return;
    }

    // Resolve own dimensions
    float own_width = constraints.max_width;
    float own_height = constraints.max_height;

    // Apply explicit dimensions if set
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            own_width = c->style->width.value;
        } else if (c->style->width.type == IR_DIMENSION_PERCENT) {
            own_width = constraints.max_width * (c->style->width.value / 100.0f);
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            own_height = c->style->height.value;
        } else if (c->style->height.type == IR_DIMENSION_PERCENT) {
            own_height = constraints.max_height * (c->style->height.value / 100.0f);
        }
    }

    // Track whether dimensions are AUTO (need to compute from children)
    bool width_auto = (c->style && c->style->width.type == IR_DIMENSION_AUTO);
    bool height_auto = (c->style && c->style->height.type == IR_DIMENSION_AUTO);

    // For components with intrinsic sizing (Text, Button, etc), use intrinsic size if no explicit height
    bool has_intrinsic_size = (c->type == IR_COMPONENT_TEXT || c->type == IR_COMPONENT_BUTTON ||
                               c->type == IR_COMPONENT_INPUT || c->type == IR_COMPONENT_CHECKBOX);
    bool used_intrinsic_height = false;
    if (getenv("KRYON_DEBUG_TEXT") && has_intrinsic_size) {
        fprintf(stderr, "[INTRINSIC CHECK] type=%u has_intrinsic=%d height_type=%u\n",
               (uint32_t)c->type, has_intrinsic_size, c->style ? (uint32_t)c->style->height.type : 0xFFFFFFFFu);
    }
    if (has_intrinsic_size && (!c->style || c->style->height.type != IR_DIMENSION_PX)) {
        own_height = ir_get_component_intrinsic_height(c);
        used_intrinsic_height = true;  // Mark that we used intrinsic height
        if (getenv("KRYON_DEBUG_TEXT")) {
            fprintf(stderr, "[INTRINSIC HEIGHT] Got height=%.1f for type=%d\n", own_height, c->type);
        }
    }

    // Determine layout direction from flex.direction (0=column/vertical, 1=row/horizontal)
    IRLayout* layout = ir_get_layout(c);
    bool is_row = (layout && layout->flex.direction == 1);

    // Get padding values
    float pad_left = c->style ? c->style->padding.left : 0;
    float pad_top = c->style ? c->style->padding.top : 0;
    float pad_right = c->style ? c->style->padding.right : 0;
    float pad_bottom = c->style ? c->style->padding.bottom : 0;

    // Available space for children (subtract padding from container dimensions)
    float content_width = own_width - pad_left - pad_right;
    float content_height = own_height - pad_top - pad_bottom;

    // Layout children recursively
    float child_x = 0;
    float child_y = 0;
    float max_child_width = 0;
    float max_child_height = 0;
    float total_child_width = 0;
    float total_child_height = 0;

    if (getenv("KRYON_DEBUG_TAB_LAYOUT") &&
        (c->type == IR_COMPONENT_TAB_CONTENT || c->type == IR_COMPONENT_TAB_PANEL)) {
        fprintf(stderr, "[LAYOUT_CHILDREN] Component ID=%u type=%u has %u children\n",
               c->id, (uint32_t)c->type, c->child_count);
        for (uint32_t i = 0; i < c->child_count; i++) {
            fprintf(stderr, "[LAYOUT_CHILDREN]   Child %u: ID=%u type=%u\n",
                   i, c->children[i] ? c->children[i]->id : 0,
                   c->children[i] ? (uint32_t)c->children[i]->type : 0xFFFFFFFFu);
        }
    }

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child) continue;

        // Skip invisible children in layout computation
        if (child->style && !child->style->visible) continue;

        // Create constraints for child based on direction (use content dimensions)
        IRLayoutConstraints child_constraints = {
            .max_width = is_row ? (content_width - child_x) : content_width,
            .max_height = is_row ? content_height : (content_height - child_y),
            .min_width = 0,
            .min_height = 0
        };

        // RECURSIVELY layout child (child computes FINAL dimensions)
        // Offset by padding
        float child_final_x, child_final_y;
        if (is_row) {
            child_final_x = parent_x + pad_left + child_x;
            child_final_y = parent_y + pad_top;
            ir_layout_single_pass(child, child_constraints, child_final_x, child_final_y);
        } else {
            child_final_x = parent_x + pad_left;
            child_final_y = parent_y + pad_top + child_y;
            ir_layout_single_pass(child, child_constraints, child_final_x, child_final_y);
        }

        // Debug: Print Button positions to debug ForEach layout
        if (child->type == IR_COMPONENT_BUTTON) {
            const char* text = child->text_content ? child->text_content : "(null)";
            float w = child->layout_state ? child->layout_state->computed.width : 0;
            float h = child->layout_state ? child->layout_state->computed.height : 0;
            printf("[LAYOUT] Button ID=%u text='%s' pos=[%.1f, %.1f] size=[%.1fx%.1f] parent_type=%d parent_id=%d\n",
                   child->id, text, child_final_x, child_final_y, w, h,
                   c->type, c->id);
        }

        // Track child dimensions for AUTO dimension calculation
        if (child->layout_state) {
            max_child_width = fmaxf(max_child_width, child->layout_state->computed.width);
            max_child_height = fmaxf(max_child_height, child->layout_state->computed.height);
            total_child_width += child->layout_state->computed.width;
            total_child_height += child->layout_state->computed.height;

            // Advance position for next child based on direction
            if (is_row) {
                child_x += child->layout_state->computed.width;
                // Apply gap if set
                if (layout && layout->flex.gap > 0 && i < c->child_count - 1) {
                    child_x += layout->flex.gap;
                }
            } else {
                if (getenv("KRYON_DEBUG_TEXT")) {
                    fprintf(stderr, "[COLUMN LAYOUT] child %d: height=%.1f, advancing child_y from %.1f to %.1f, gap=%.1f\n",
                           i, child->layout_state->computed.height, child_y,
                           child_y + child->layout_state->computed.height,
                           layout ? layout->flex.gap : 0.0f);
                }
                child_y += child->layout_state->computed.height;
                // Apply gap if set
                if (layout && layout->flex.gap > 0 && i < c->child_count - 1) {
                    child_y += layout->flex.gap;
                }
            }
        }
    }

    // Apply flexbox alignment (justifyContent and alignItems)
    if (layout && c->child_count > 0) {
        // Calculate remaining space for main axis alignment (justifyContent)
        float total_gaps = layout->flex.gap * (c->child_count > 1 ? (c->child_count - 1) : 0);
        float remaining_main = is_row ?
            (content_width - total_child_width - total_gaps) :
            (content_height - total_child_height - total_gaps);

        // Apply main axis alignment (justifyContent)
        float main_offset = 0.0f;
        if (remaining_main > 0) {
            switch (layout->flex.justify_content) {
                case IR_ALIGNMENT_CENTER:
                    main_offset = remaining_main / 2.0f;
                    #ifdef __ANDROID__
                    __android_log_print(ANDROID_LOG_WARN, "KryonLayout",
                           "justifyContent CENTER: comp %u, remaining=%.1f, offset=%.1f, is_row=%d, children=%d",
                           c->id, remaining_main, main_offset, is_row, c->child_count);
                    #endif
                    break;
                case IR_ALIGNMENT_END:
                    main_offset = remaining_main;
                    break;
                default:
                    break;
            }
        }

        // Apply cross axis alignment (alignItems) and main axis offset to all children
        for (uint32_t i = 0; i < c->child_count; i++) {
            IRComponent* child = c->children[i];
            if (!child || !child->layout_state) continue;

            // Skip absolutely positioned children - they use their own coordinates
            if (child->style && child->style->position_mode == IR_POSITION_ABSOLUTE) {
                continue;
            }

            // Apply main axis offset
            if (is_row) {
                child->layout_state->computed.x += main_offset;
            } else {
                child->layout_state->computed.y += main_offset;
            }

            // Apply cross axis alignment (alignItems)
            float child_cross_size = is_row ? child->layout_state->computed.height : child->layout_state->computed.width;
            float available_cross = is_row ? content_height : content_width;
            float cross_offset = 0.0f;

            switch (layout->flex.cross_axis) {
                case IR_ALIGNMENT_CENTER:
                    cross_offset = (available_cross - child_cross_size) / 2.0f;
                    break;
                case IR_ALIGNMENT_END:
                    cross_offset = available_cross - child_cross_size;
                    break;
                default:
                    break;
            }

            if (is_row) {
                child->layout_state->computed.y += cross_offset;
            } else {
                child->layout_state->computed.x += cross_offset;
                if (child->type == IR_COMPONENT_CHECKBOX) {
                    printf("[ALIGNMENT] Checkbox ID=%u: cross_offset=%.1f, new x=%.1f\n",
                           child->id, cross_offset, child->layout_state->computed.x);
                }
            }

            // Debug text positioning
            #ifdef __ANDROID__
            if (child->type == IR_COMPONENT_TEXT) {
                __android_log_print(ANDROID_LOG_WARN, "KryonLayout",
                       "TEXT FINAL: id=%u pos=(%.1f, %.1f) main=%.1f cross=%.1f",
                       child->id, child->layout_state->computed.x, child->layout_state->computed.y,
                       main_offset, cross_offset);
            }
            #endif

            // Re-sync computed layout to rendered_bounds after alignment
            // Skip for modal descendants - they get correct bounds during rendering
            bool child_in_modal = false;
            IRComponent* anc = child->parent;
            while (anc) {
                if (anc->type == IR_COMPONENT_MODAL) { child_in_modal = true; break; }
                anc = anc->parent;
            }
            if (!child_in_modal) {
                child->rendered_bounds.x = child->layout_state->computed.x;
                child->rendered_bounds.y = child->layout_state->computed.y;
            }
            if (child->type == IR_COMPONENT_CHECKBOX) {
                printf("[RE-SYNC] Checkbox ID=%u: rendered_bounds updated to [%.1f, %.1f]\n",
                       child->id, child->rendered_bounds.x, child->rendered_bounds.y);
            }
        }
    }

    // Finalize AUTO dimensions based on direction
    // IMPORTANT: Don't override intrinsic sizing for leaf components!
    if (width_auto) {
        own_width = is_row ? total_child_width : max_child_width;
        // Add gaps for row layout
        if (is_row && layout && layout->flex.gap > 0 && c->child_count > 1) {
            own_width += layout->flex.gap * (c->child_count - 1);
        }
        // Add padding to auto width
        own_width += pad_left + pad_right;
    }
    if (height_auto && !used_intrinsic_height) {
        // Only calculate height from children if we didn't use intrinsic height
        own_height = is_row ? max_child_height : total_child_height;
        // Add gaps for column layout
        if (!is_row && layout && layout->flex.gap > 0 && c->child_count > 1) {
            own_height += layout->flex.gap * (c->child_count - 1);
        }
        // Add padding to auto height
        own_height += pad_top + pad_bottom;
    }

    // Set final computed dimensions
    c->layout_state->computed.width = own_width;
    c->layout_state->computed.height = own_height;

    // Use absolute position if specified, otherwise use parent position
    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        c->layout_state->computed.x = c->style->absolute_x;
        c->layout_state->computed.y = c->style->absolute_y;
    } else {
        c->layout_state->computed.x = parent_x;
        c->layout_state->computed.y = parent_y;
    }

    // DEBUG: Print layout positions for Text components
    if (c->type == IR_COMPONENT_TEXT && c->text_content && getenv("KRYON_DEBUG_TEXT")) {
        fprintf(stderr, "[TEXT LAYOUT] \"%s\" x=%.1f y=%.1f w=%.1f h=%.1f (own_height=%.1f computed=%.1f)\n",
               c->text_content, parent_x, parent_y, own_width, own_height, own_height,
               c->layout_state->computed.height);
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    c->layout_state->computed.valid = true;

    // Sync computed layout to rendered_bounds for click detection
    // Skip for modal descendants - they get correct bounds during overlay rendering
    bool in_modal = false;
    IRComponent* modal_check = c->parent;
    while (modal_check) {
        if (modal_check->type == IR_COMPONENT_MODAL) { in_modal = true; break; }
        modal_check = modal_check->parent;
    }
    if (!in_modal) {
        c->rendered_bounds.x = c->layout_state->computed.x;
        c->rendered_bounds.y = c->layout_state->computed.y;
        c->rendered_bounds.width = c->layout_state->computed.width;
        c->rendered_bounds.height = c->layout_state->computed.height;
        c->rendered_bounds.valid = true;
    }

    if (c->type == IR_COMPONENT_CHECKBOX) {
        printf("[SYNC_BOUNDS] Checkbox ID=%u: computed=[%.1f, %.1f, %.1f, %.1f] -> rendered_bounds\n",
               c->id, c->rendered_bounds.x, c->rendered_bounds.y,
               c->rendered_bounds.width, c->rendered_bounds.height);
    }

    // Debug: Print layout for tab-related components and their children
    if (getenv("KRYON_DEBUG_TAB_LAYOUT") &&
        (c->type == IR_COMPONENT_TAB_CONTENT || c->type == IR_COMPONENT_TAB_PANEL ||
         c->type == IR_COMPONENT_TEXT)) {
        const char* parent_info = "";
        if (c->parent) {
            static char parent_buf[64];
            snprintf(parent_buf, sizeof(parent_buf), " parent_id=%u parent_pos=(%.1f,%.1f)",
                    c->parent->id, c->parent->rendered_bounds.x, c->parent->rendered_bounds.y);
            parent_info = parent_buf;
        }
        fprintf(stderr, "[LAYOUT_COMPUTED] ID=%u type=%d pos=(%.1f, %.1f) size=(%.1f, %.1f)%s\n",
               c->id, c->type, c->rendered_bounds.x, c->rendered_bounds.y,
               c->rendered_bounds.width, c->rendered_bounds.height, parent_info);
    }
}

// ============================================================================
// Public API
// ============================================================================

/**
 * Main entry point for layout computation
 * Single-pass recursive layout system
 */
void ir_layout_compute_tree(IRComponent* root, float viewport_width, float viewport_height) {
    if (!root) return;

    // Track viewport changes and invalidate layout when viewport resizes
    static float last_width = 0, last_height = 0;
    if (viewport_width != last_width || viewport_height != last_height) {
        last_width = viewport_width;
        last_height = viewport_height;

        // CRITICAL: Invalidate entire layout tree when viewport changes
        // This forces recomputation with new constraints
        ir_layout_invalidate_subtree(root);
    }

    IRLayoutConstraints root_constraints = {
        .max_width = viewport_width,
        .max_height = viewport_height,
        .min_width = 0,
        .min_height = 0
    };
    ir_layout_single_pass(root, root_constraints, 0, 0);
}

/**
 * Get computed layout bounds for a component
 * Returns NULL if layout hasn't been computed
 */
IRComputedLayout* ir_layout_get_bounds(IRComponent* component) {
    if (!component || !component->layout_state || !component->layout_state->layout_valid) {
        return NULL;
    }
    return &component->layout_state->computed;
}
