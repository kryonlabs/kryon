// IR Layout Engine - Unified flexbox layout system integrated with IR
// Moved from /core/layout.c and adapted for IR architecture

#define KRYON_TRACE_LAYOUT 1  // DEBUG: Remove after fixing flowchart issue

#include "ir_core.h"
#include "ir_builder.h"
#include "ir_text_shaping.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declarations
void ir_layout_compute_flowchart(IRComponent* flowchart, float available_width, float available_height);

// ============================================================================
// Text Measurement Callback (Backend-Specific)
// ============================================================================

static IRTextMeasureCallback g_text_measure_callback = NULL;

void ir_layout_set_text_measure_callback(IRTextMeasureCallback callback) {
    g_text_measure_callback = callback;
}

// ============================================================================
// Layout Cache & Dirty Tracking
// ============================================================================

void ir_layout_mark_dirty(IRComponent* component) {
    if (!component || !component->layout_state) return;

    // Mark this component dirty (invalidate both passes)
    component->layout_state->dirty = true;
    component->layout_state->intrinsic_valid = false;
    component->layout_state->layout_valid = false;

    // Keep old dirty_flags for compatibility during transition
    component->dirty_flags |= IR_DIRTY_LAYOUT;

    // Propagate dirty flag upward to parents
    // Parent intrinsic sizes may change if child size changes
    IRComponent* parent = component->parent;
    while (parent) {
        if (!parent->layout_state) break;

        // Mark parent layout invalid (but intrinsic needs recomputation too)
        parent->layout_state->dirty = true;
        parent->layout_state->layout_valid = false;
        // Note: We DON'T invalidate parent's intrinsic here yet
        // because we'll recompute it in the bottom-up pass anyway

        // Keep old dirty_flags for compatibility
        parent->dirty_flags |= IR_DIRTY_SUBTREE;
        parent->layout_cache.dirty = true;
        parent->layout_cache.cached_intrinsic_width = -1.0f;
        parent->layout_cache.cached_intrinsic_height = -1.0f;

        parent = parent->parent;
    }
}

void ir_layout_mark_render_dirty(IRComponent* component) {
    if (!component) return;

    component->dirty_flags |= IR_DIRTY_RENDER;

    // Do NOT propagate to parents - visual-only changes don't affect parent layout
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

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "🔄 INVALIDATE_CACHE for component %u (type=%d)\n", component->id, component->type);
    #endif

    component->layout_cache.dirty = true;
    component->layout_cache.cache_generation++;
    // OPTIMIZATION: Use -1.0f to distinguish "not cached" from "cached as 0"
    component->layout_cache.cached_intrinsic_width = -1.0f;
    component->layout_cache.cached_intrinsic_height = -1.0f;
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

        case IR_COMPONENT_FLOWCHART: {
            IRFlowchartState* fc_state = ir_get_flowchart_state(component);
            if (fc_state) {
                // If layout already computed, use natural width
                if (fc_state->layout_computed && fc_state->natural_width > 0) {
                    return fc_state->natural_width;
                }
                // Pre-compute layout to get natural dimensions
                ir_layout_compute_flowchart(component, 0, 0);
                if (fc_state->natural_width > 0) {
                    return fc_state->natural_width;
                }
            }
            return 400.0f;  // Reasonable default for flowcharts
        }

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
    fprintf(stderr, "🔹 _impl called: type=%d, style=%p\n",
        component ? component->type : -1, component ? (void*)component->style : NULL);
    if (!component || !component->style) {
        fprintf(stderr, "🔹 _impl early return (no style): returning 50.0f\n");
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

            // Fallback to single-line height
            return font_size + 4.0f;
        }

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

        case IR_COMPONENT_FLOWCHART: {
            IRFlowchartState* fc_state = ir_get_flowchart_state(component);
            fprintf(stderr, "      🔷 FLOWCHART intrinsic height: state=%p\n", (void*)fc_state);
            if (fc_state) {
                fprintf(stderr, "      🔷 node_count=%u, layout_computed=%d, natural_h=%.1f\n",
                    fc_state->node_count, fc_state->layout_computed, fc_state->natural_height);
            }
            if (fc_state) {
                // If layout already computed, use natural height
                if (fc_state->layout_computed && fc_state->natural_height > 0) {
                    return fc_state->natural_height;
                }
                // Pre-compute layout to get natural dimensions
                // Use 0,0 to compute without scaling constraints
                ir_layout_compute_flowchart(component, 0, 0);
                #ifdef KRYON_TRACE_LAYOUT
                fprintf(stderr, "      🔷 After compute: natural_h=%.1f\n", fc_state->natural_height);
                #endif
                if (fc_state->natural_height > 0) {
                    return fc_state->natural_height;
                }
            }
            return 200.0f;  // Reasonable default for flowcharts
        }

        default:
            return 50.0f;
    }
}

float ir_get_component_intrinsic_height(IRComponent* component) {
    if (!component) return 0.0f;

    fprintf(stderr, "🔶 ir_get_component_intrinsic_height called: type=%d\n", component->type);

    // Check cache (use >= 0 since -1.0f means not cached)
    if (!component->layout_cache.dirty && component->layout_cache.cached_intrinsic_height >= 0.0f) {
        #ifdef KRYON_TRACE_LAYOUT
        fprintf(stderr, "📋 CACHE HIT for component %u (type=%d): returning cached height %.1f\n",
            component->id, component->type, component->layout_cache.cached_intrinsic_height);
        #endif
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

    // Layout children based on layout mode
    if (root->child_count > 0) {
        // Special handling for table components
        if (root->type == IR_COMPONENT_TABLE) {
            ir_layout_compute_table(root, inner_width, inner_height);
        } else if (root->type == IR_COMPONENT_FLOWCHART) {
            ir_layout_compute_flowchart(root, inner_width, inner_height);
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
    // SKIP flowchart children - their layout is handled by ir_layout_compute_flowchart()
    // These use RELATIVE coordinates that would be corrupted by flex layout
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        if (child->style && child->style->visible) {
            // Skip table children - their layout is managed by table layout algorithm
            bool is_table_child = (child->type == IR_COMPONENT_TABLE_HEAD ||
                                   child->type == IR_COMPONENT_TABLE_BODY ||
                                   child->type == IR_COMPONENT_TABLE_FOOT ||
                                   child->type == IR_COMPONENT_TABLE_ROW ||
                                   child->type == IR_COMPONENT_TABLE_CELL ||
                                   child->type == IR_COMPONENT_TABLE_HEADER_CELL);
            // Skip flowchart children - their layout is managed by flowchart layout algorithm
            bool is_flowchart_child = (child->type == IR_COMPONENT_FLOWCHART_NODE ||
                                       child->type == IR_COMPONENT_FLOWCHART_EDGE ||
                                       child->type == IR_COMPONENT_FLOWCHART_SUBGRAPH ||
                                       child->type == IR_COMPONENT_FLOWCHART_LABEL);
            if (!is_table_child && !is_flowchart_child) {
                ir_layout_compute(child, child->rendered_bounds.width, child->rendered_bounds.height);
            }
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

    // Apply main-axis alignment (justify_content) for row layout
    if (remaining_width > 0 && total_flex_grow == 0) {
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

        current_x += child_width + child->style->margin.left + child->style->margin.right + layout->flex.gap + extra_gap;
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

    // Apply main-axis alignment (justify_content) for column layout
    if (remaining_height > 0 && total_flex_grow == 0) {
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

        #ifdef KRYON_TRACE_LAYOUT
        fprintf(stderr, "║ Child %u: bounds=[%.1f, %.1f, %.1f, %.1f] intrinsic_h=%.1f\n",
            i, child->rendered_bounds.x, child->rendered_bounds.y,
            child->rendered_bounds.width, child->rendered_bounds.height,
            ir_get_component_intrinsic_height(child));
        #endif

        current_y += child_height + child->style->margin.top + child->style->margin.bottom + layout->flex.gap + extra_gap;
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

    // First pass: calculate fixed and percentage track sizes
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
            case IR_GRID_TRACK_AUTO:
            case IR_GRID_TRACK_MIN_CONTENT:
            case IR_GRID_TRACK_MAX_CONTENT:
                // TODO: Implement content-based sizing
                // For now, treat as FR
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
            case IR_GRID_TRACK_AUTO:
            case IR_GRID_TRACK_MIN_CONTENT:
            case IR_GRID_TRACK_MAX_CONTENT:
                // TODO: Implement content-based sizing
                // For now, treat as FR
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
    uint8_t auto_row = 0, auto_col = 0;

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
static void intrinsic_size_table(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out || c->type != IR_COMPONENT_TABLE) return;

    IRTableState* state = (IRTableState*)c->custom_data;
    if (!state) {
        // No state - return default size
        memset(out, 0, sizeof(IRIntrinsicSize));
        return;
    }

    // Use cached dimensions if valid
    if (state->layout_valid && state->cached_total_width > 0 && state->cached_total_height > 0) {
        out->min_content_width = state->cached_total_width;
        out->max_content_width = state->cached_total_width;
        out->min_content_height = state->cached_total_height;
        out->max_content_height = state->cached_total_height;
        return;
    }

    // Calculate intrinsic size by running table layout algorithm
    // We need to compute column widths and row heights

    uint32_t num_cols = table_count_columns(c);
    uint32_t num_rows = table_count_rows(c);

    if (num_cols == 0 || num_rows == 0) {
        memset(out, 0, sizeof(IRIntrinsicSize));
        return;
    }

    // Get table styling
    float cell_padding = state->style.cell_padding;
    float border_width = state->style.show_borders ? state->style.border_width : 0.0f;

    // Allocate arrays for column widths and row heights
    float* col_widths = (float*)calloc(num_cols, sizeof(float));
    float* row_heights = (float*)calloc(num_rows, sizeof(float));
    float* min_col_widths = (float*)calloc(num_cols, sizeof(float));

    if (!col_widths || !row_heights || !min_col_widths) {
        free(col_widths);
        free(row_heights);
        free(min_col_widths);
        memset(out, 0, sizeof(IRIntrinsicSize));
        return;
    }

    // Initialize minimum widths from column definitions
    for (uint32_t col = 0; col < num_cols; col++) {
        if (col < state->column_count && state->columns) {
            IRTableColumnDef* col_def = &state->columns[col];
            if (col_def->width.type == IR_DIMENSION_PX) {
                col_widths[col] = col_def->width.value;
            }
            if (col_def->min_width.type == IR_DIMENSION_PX) {
                min_col_widths[col] = col_def->min_width.value;
            }
        }
    }

    // Phase 1: Calculate intrinsic column widths from cell content
    uint32_t row_idx = 0;
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* section = c->children[i];
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
    for (uint32_t col = 0; col < num_cols; col++) {
        if (col_widths[col] < min_col_widths[col]) {
            col_widths[col] = min_col_widths[col];
        }
        if (col_widths[col] < 30.0f) {
            col_widths[col] = 30.0f;  // Absolute minimum
        }
    }

    // Phase 3: Calculate total width (no expansion - intrinsic size)
    float total_width = 0;
    for (uint32_t col = 0; col < num_cols; col++) {
        total_width += col_widths[col];
    }
    total_width += border_width * (num_cols + 1);

    // Phase 4: Calculate row heights from cell content
    row_idx = 0;
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* section = c->children[i];
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
    for (uint32_t row = 0; row < num_rows; row++) {
        total_height += row_heights[row];
    }
    total_height += border_width * (num_rows + 1);

    // Cache the dimensions (but don't set layout_valid yet - that's done in Phase 2 after cell positions are set)
    state->cached_total_width = total_width;
    state->cached_total_height = total_height;
    // NOTE: We don't set layout_valid = true here because Phase 2 needs to run to set cell positions

    // Store in output
    out->min_content_width = total_width;
    out->max_content_width = total_width;
    out->min_content_height = total_height;
    out->max_content_height = total_height;

    // Clean up temporary arrays
    free(col_widths);
    free(row_heights);
    free(min_col_widths);
}

// Main table layout function
void ir_layout_compute_table(IRComponent* table, float available_width, float available_height) {
    if (!table || table->type != IR_COMPONENT_TABLE) {
        return;
    }

    IRTableState* state = (IRTableState*)table->custom_data;

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

    IRStyle* table_style = table->style;

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
                section->layout_state->absolute_positions_computed = false;
            }

            for (uint32_t j = 0; j < section->child_count; j++) {
                IRComponent* row = section->children[j];
                if (!row) continue;
                if (row->layout_state) {
                    row->layout_state->layout_valid = true;
                    row->layout_state->absolute_positions_computed = false;
                }

                for (uint32_t k = 0; k < row->child_count; k++) {
                    IRComponent* cell = row->children[k];
                    if (!cell) continue;
                    if (cell->layout_state) {
                        cell->layout_state->layout_valid = true;
                        cell->layout_state->absolute_positions_computed = false;
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
// FLOWCHART LAYOUT
// ============================================================================

// Default layout parameters
#define FLOWCHART_NODE_MIN_WIDTH 40.0f
#define FLOWCHART_NODE_MIN_HEIGHT 24.0f
#define FLOWCHART_NODE_PADDING 15.0f
#define FLOWCHART_NODE_SPACING 20.0f
#define FLOWCHART_RANK_SPACING 40.0f
#define FLOWCHART_SUBGRAPH_PADDING 40.0f
#define FLOWCHART_SUBGRAPH_TITLE_HEIGHT 30.0f

// Compute bounding boxes for subgraphs based on their contained nodes
static void compute_subgraph_bounds(IRFlowchartState* fc_state) {
    if (!fc_state) return;

    // Iterate over all subgraphs
    for (uint32_t i = 0; i < fc_state->subgraph_count; i++) {
        IRFlowchartSubgraphData* sg_data = fc_state->subgraphs[i];
        if (!sg_data || !sg_data->subgraph_id) continue;

        // Find all nodes that belong to this subgraph
        float min_x = INFINITY, min_y = INFINITY;
        float max_x = -INFINITY, max_y = -INFINITY;
        bool has_nodes = false;

        for (uint32_t j = 0; j < fc_state->node_count; j++) {
            IRFlowchartNodeData* node_data = fc_state->nodes[j];

            // Check if this node belongs to the current subgraph
            if (node_data && node_data->subgraph_id &&
                strcmp(node_data->subgraph_id, sg_data->subgraph_id) == 0) {

                has_nodes = true;

                // Update bounding box
                float node_left = node_data->x;
                float node_top = node_data->y;
                float node_right = node_data->x + node_data->width;
                float node_bottom = node_data->y + node_data->height;

                if (node_left < min_x) min_x = node_left;
                if (node_top < min_y) min_y = node_top;
                if (node_right > max_x) max_x = node_right;
                if (node_bottom > max_y) max_y = node_bottom;
            }
        }

        if (has_nodes) {
            // Add padding around nodes for subgraph box
            sg_data->x = min_x - FLOWCHART_SUBGRAPH_PADDING;
            sg_data->y = min_y - FLOWCHART_SUBGRAPH_PADDING - FLOWCHART_SUBGRAPH_TITLE_HEIGHT;
            sg_data->width = (max_x - min_x) + (FLOWCHART_SUBGRAPH_PADDING * 2);
            sg_data->height = (max_y - min_y) + (FLOWCHART_SUBGRAPH_PADDING * 2) + FLOWCHART_SUBGRAPH_TITLE_HEIGHT;

            #ifdef KRYON_TRACE_LAYOUT
            fprintf(stderr, "  📦 Subgraph '%s' bounds: x=%.1f y=%.1f w=%.1f h=%.1f\n",
                   sg_data->subgraph_id, sg_data->x, sg_data->y,
                   sg_data->width, sg_data->height);
            #endif
        }
    }
}

// Helper: Transform coordinates recursively for nested subgraphs
static void transform_subgraph_coordinates(IRComponent* subgraph, float offset_x, float offset_y, IRFlowchartState* state) {
    if (!subgraph || !state) return;

    IRFlowchartSubgraphData* sg_data = ir_get_flowchart_subgraph_data(subgraph);
    if (!sg_data || !sg_data->subgraph_id) return;

    // Transform all nodes in this subgraph
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node && node->subgraph_id && strcmp(node->subgraph_id, sg_data->subgraph_id) == 0) {
            node->x += offset_x;
            node->y += offset_y;
        }
    }

    // Transform edges (if they belong to this subgraph)
    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (edge && edge->path_points) {
            // Check if edge connects nodes in this subgraph
            bool from_in_subgraph = false;
            bool to_in_subgraph = false;

            for (uint32_t j = 0; j < state->node_count; j++) {
                IRFlowchartNodeData* node = state->nodes[j];
                if (node && node->subgraph_id && strcmp(node->subgraph_id, sg_data->subgraph_id) == 0) {
                    if (edge->from_id && node->node_id && strcmp(node->node_id, edge->from_id) == 0) {
                        from_in_subgraph = true;
                    }
                    if (edge->to_id && node->node_id && strcmp(node->node_id, edge->to_id) == 0) {
                        to_in_subgraph = true;
                    }
                }
            }

            // Transform edge if both endpoints are in this subgraph
            if (from_in_subgraph && to_in_subgraph) {
                for (uint32_t p = 0; p < edge->path_point_count; p++) {
                    edge->path_points[p * 2] += offset_x;
                    edge->path_points[p * 2 + 1] += offset_y;
                }
            }
        }
    }

    // Update subgraph bounds
    sg_data->x += offset_x;
    sg_data->y += offset_y;

    // Recursively transform nested subgraphs
    for (uint32_t i = 0; i < subgraph->child_count; i++) {
        IRComponent* child = subgraph->children[i];
        if (child && child->type == IR_COMPONENT_FLOWCHART_SUBGRAPH) {
            transform_subgraph_coordinates(child, offset_x, offset_y, state);
        }
    }
}

// Helper: Compute node sizes based on labels and shapes
static void compute_flowchart_node_sizes(IRFlowchartState* state, float font_size) {
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (!node) continue;

        float label_width = 50.0f;
        float label_height = font_size * 1.2f;

        if (node->label && node->label[0] != '\0') {
            if (g_ir_font_metrics && g_ir_font_metrics->get_text_width) {
                size_t len = strlen(node->label);
                label_width = g_ir_font_metrics->get_text_width(node->label, (uint32_t)len, font_size, NULL);
                label_height = g_ir_font_metrics->get_font_height(font_size, NULL);
            } else {
                // No font metrics - use estimate
                label_width = strlen(node->label) * font_size * 0.6f;
            }
        }

        // Add padding around text
        float h_padding = 32.0f;  // 16px on each side
        float v_padding = 20.0f;  // 10px on each side

        // Extra padding for non-rectangular shapes (text area is smaller)
        if (node->shape == IR_FLOWCHART_SHAPE_DIAMOND) {
            h_padding *= 2.0f;  // Diamond needs ~2x because text area is diagonal
            v_padding *= 2.0f;
        } else if (node->shape == IR_FLOWCHART_SHAPE_CIRCLE ||
                   node->shape == IR_FLOWCHART_SHAPE_HEXAGON) {
            h_padding *= 1.5f;
            v_padding *= 1.5f;
        }

        node->width = fmaxf(FLOWCHART_NODE_MIN_WIDTH, label_width + h_padding);
        node->height = fmaxf(FLOWCHART_NODE_MIN_HEIGHT, label_height + v_padding);

        // Make circles and diamonds square
        if (node->shape == IR_FLOWCHART_SHAPE_CIRCLE ||
            node->shape == IR_FLOWCHART_SHAPE_DIAMOND) {
            float size = fmaxf(node->width, node->height);
            node->width = size;
            node->height = size;
        }
    }
}

// Helper: Layout nodes belonging to a specific subgraph (or top-level if subgraph_id is NULL)
// Returns the computed size of the subgraph in local coordinates
static void layout_subgraph_nodes(IRFlowchartState* state, const char* subgraph_id,
                                   IRFlowchartDirection direction, float node_spacing,
                                   float rank_spacing, float* out_width, float* out_height) {
    if (!state) return;

    // Filter nodes that belong to this subgraph
    uint32_t* subgraph_node_indices = (uint32_t*)malloc(state->node_count * sizeof(uint32_t));
    uint32_t subgraph_node_count = 0;

    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (!node) continue;

        // Check if node belongs to this subgraph
        bool belongs = false;
        if (subgraph_id == NULL) {
            // Top-level: nodes with no subgraph_id
            belongs = (node->subgraph_id == NULL);
        } else {
            // Specific subgraph: nodes with matching subgraph_id
            belongs = (node->subgraph_id && strcmp(node->subgraph_id, subgraph_id) == 0);
        }

        if (belongs) {
            subgraph_node_indices[subgraph_node_count++] = i;
        }
    }

    if (subgraph_node_count == 0) {
        free(subgraph_node_indices);
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    // TODO: Implement layering and positioning for subgraph nodes
    // For now, just free and return
    free(subgraph_node_indices);
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
}

// Simple layered layout for flowcharts
// Uses topological sort to assign layers, then positions nodes within layers
void ir_layout_compute_flowchart(IRComponent* flowchart, float available_width, float available_height) {
    if (!flowchart || flowchart->type != IR_COMPONENT_FLOWCHART) return;

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state) return;

    // Check if layout is already computed and dimensions match
    if (state->layout_computed &&
        state->computed_width == available_width &&
        state->computed_height == available_height) {
        return;
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "🔀 FLOWCHART_LAYOUT: %u nodes, %u edges, dir=%s\n",
            state->node_count, state->edge_count,
            ir_flowchart_direction_to_string(state->direction));
    #endif

    if (state->node_count == 0) {
        state->layout_computed = true;
        state->computed_width = available_width;
        state->computed_height = available_height;
        return;
    }

    // Use layout parameters from state or defaults
    float node_spacing = state->node_spacing > 0 ? state->node_spacing : FLOWCHART_NODE_SPACING;
    float rank_spacing = state->rank_spacing > 0 ? state->rank_spacing : FLOWCHART_RANK_SPACING;

    // Phase 1: Compute node sizes based on labels
    // Use flowchart's font size if specified, otherwise default to 14
    float font_size = (flowchart->style && flowchart->style->font.size > 0)
                      ? flowchart->style->font.size : 14.0f;
    compute_flowchart_node_sizes(state, font_size);

    // Check if any subgraphs have different directions than parent
    bool has_directional_subgraphs = false;
    for (uint32_t i = 0; i < state->subgraph_count; i++) {
        IRFlowchartSubgraphData* sg = state->subgraphs[i];
        if (sg && sg->direction != state->direction) {
            has_directional_subgraphs = true;
            #ifdef KRYON_TRACE_LAYOUT
            fprintf(stderr, "  📐 Subgraph '%s' has direction %s (parent: %s)\n",
                    sg->subgraph_id ? sg->subgraph_id : "?",
                    ir_flowchart_direction_to_string(sg->direction),
                    ir_flowchart_direction_to_string(state->direction));
            #endif
            break;
        }
    }

    #ifdef KRYON_TRACE_LAYOUT
    if (has_directional_subgraphs) {
        fprintf(stderr, "  🔀 Detected subgraphs with independent directions\n");
    }
    #endif

    // Phase 2: Assign nodes to layers using longest-path algorithm
    // This properly handles cycles by only considering forward edges

    // Create layer assignment (-1 = unassigned)
    int* node_layer = calloc(state->node_count, sizeof(int));
    bool* processed = calloc(state->node_count, sizeof(bool));
    if (!node_layer || !processed) {
        free(node_layer);
        free(processed);
        return;
    }

    // Initialize all layers to -1
    for (uint32_t i = 0; i < state->node_count; i++) {
        node_layer[i] = -1;
    }

    // Find nodes with no incoming edges (start nodes)
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (!node || !node->node_id) continue;

        bool has_incoming = false;
        for (uint32_t e = 0; e < state->edge_count; e++) {
            IRFlowchartEdgeData* edge = state->edges[e];
            if (edge && edge->to_id && strcmp(edge->to_id, node->node_id) == 0) {
                has_incoming = true;
                break;
            }
        }

        if (!has_incoming) {
            node_layer[i] = 0;
            processed[i] = true;
        }
    }

    // If no start nodes found, pick the first node
    bool any_assigned = false;
    for (uint32_t i = 0; i < state->node_count; i++) {
        if (node_layer[i] >= 0) {
            any_assigned = true;
            break;
        }
    }
    if (!any_assigned && state->node_count > 0) {
        node_layer[0] = 0;
        processed[0] = true;
    }

    // Iterate until all nodes are assigned
    // Each node's layer = 1 + max(layer of predecessors that are already assigned)
    int max_iterations = state->node_count * 2;  // Prevent infinite loops
    for (int iter = 0; iter < max_iterations; iter++) {
        bool made_progress = false;

        for (uint32_t i = 0; i < state->node_count; i++) {
            if (processed[i]) continue;

            IRFlowchartNodeData* node = state->nodes[i];
            if (!node || !node->node_id) continue;

            // Find max layer of all predecessors that have been assigned
            int max_pred_layer = -1;
            bool has_assigned_pred = false;

            for (uint32_t e = 0; e < state->edge_count; e++) {
                IRFlowchartEdgeData* edge = state->edges[e];
                if (!edge || !edge->to_id || strcmp(edge->to_id, node->node_id) != 0) continue;

                // Find the source node
                for (uint32_t j = 0; j < state->node_count; j++) {
                    if (state->nodes[j] && state->nodes[j]->node_id &&
                        edge->from_id && strcmp(state->nodes[j]->node_id, edge->from_id) == 0) {
                        if (node_layer[j] >= 0) {
                            has_assigned_pred = true;
                            if (node_layer[j] > max_pred_layer) {
                                max_pred_layer = node_layer[j];
                            }
                        }
                        break;
                    }
                }
            }

            // If we have at least one assigned predecessor, assign this node
            if (has_assigned_pred) {
                node_layer[i] = max_pred_layer + 1;
                processed[i] = true;
                made_progress = true;
            }
        }

        // If no progress and still have unassigned nodes, pick one
        if (!made_progress) {
            for (uint32_t i = 0; i < state->node_count; i++) {
                if (!processed[i]) {
                    node_layer[i] = 0;  // Assign to layer 0
                    processed[i] = true;
                    made_progress = true;
                    break;
                }
            }
        }

        // Check if all nodes are assigned
        bool all_done = true;
        for (uint32_t i = 0; i < state->node_count; i++) {
            if (!processed[i]) {
                all_done = false;
                break;
            }
        }
        if (all_done) break;
    }

    // Find max layer
    int max_layer = 0;
    for (uint32_t i = 0; i < state->node_count; i++) {
        if (node_layer[i] > max_layer) {
            max_layer = node_layer[i];
        }
    }

    // Phase 3: Count nodes per layer and find max
    int* nodes_per_layer = calloc(max_layer + 1, sizeof(int));
    if (!nodes_per_layer) {
        free(node_layer);
        free(processed);
        return;
    }

    for (uint32_t i = 0; i < state->node_count; i++) {
        nodes_per_layer[node_layer[i]]++;
    }

    int max_nodes_in_layer = 0;
    for (int l = 0; l <= max_layer; l++) {
        if (nodes_per_layer[l] > max_nodes_in_layer) {
            max_nodes_in_layer = nodes_per_layer[l];
        }
    }

    // Phase 4: Position nodes
    // Track position within each layer
    int* layer_position = calloc(max_layer + 1, sizeof(int));
    if (!layer_position) {
        free(node_layer);
        free(processed);
        free(nodes_per_layer);
        return;
    }

    // Find max node dimensions for spacing
    float max_node_width = FLOWCHART_NODE_MIN_WIDTH;
    float max_node_height = FLOWCHART_NODE_MIN_HEIGHT;
    for (uint32_t i = 0; i < state->node_count; i++) {
        if (state->nodes[i]) {
            max_node_width = fmaxf(max_node_width, state->nodes[i]->width);
            max_node_height = fmaxf(max_node_height, state->nodes[i]->height);
        }
    }

    // Assign positions based on direction
    bool horizontal = (state->direction == IR_FLOWCHART_DIR_LR ||
                       state->direction == IR_FLOWCHART_DIR_RL);
    bool reversed = (state->direction == IR_FLOWCHART_DIR_BT ||
                     state->direction == IR_FLOWCHART_DIR_RL);

    float total_primary_size = 0;
    float total_secondary_size = 0;

    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (!node) continue;

        int layer = node_layer[i];

        // For nodes in subgraphs with different directions, track position separately
        bool node_in_directional_subgraph = false;
        if (has_directional_subgraphs && node->subgraph_id) {
            for (uint32_t sg_idx = 0; sg_idx < state->subgraph_count; sg_idx++) {
                IRFlowchartSubgraphData* sg = state->subgraphs[sg_idx];
                if (sg && sg->subgraph_id && strcmp(node->subgraph_id, sg->subgraph_id) == 0) {
                    if (sg->direction != state->direction) {
                        node_in_directional_subgraph = true;
                        break;
                    }
                }
            }
        }

        int pos;
        if (node_in_directional_subgraph) {
            // For directional subgraphs, calculate position within subgraph context
            pos = 0;  // Count nodes in same (layer, subgraph) before this one
            for (uint32_t j = 0; j < i; j++) {
                if (node_layer[j] == layer) {
                    IRFlowchartNodeData* other = state->nodes[j];
                    if (other && other->subgraph_id && node->subgraph_id &&
                        strcmp(other->subgraph_id, node->subgraph_id) == 0) {
                        pos++;
                    }
                }
            }
        } else {
            // Global position for top-level nodes
            pos = layer_position[layer]++;
        }

        // Calculate center position within layer
        // Count only nodes in the same subgraph (or top-level) for proper centering
        int nodes_in_this_layer = 0;
        for (uint32_t j = 0; j < state->node_count; j++) {
            if (node_layer[j] == layer) {
                IRFlowchartNodeData* other = state->nodes[j];
                if (!other) continue;

                // Check if both nodes are in the same subgraph
                bool same_subgraph = false;
                if (node->subgraph_id == NULL && other->subgraph_id == NULL) {
                    same_subgraph = true;  // Both top-level
                } else if (node->subgraph_id && other->subgraph_id &&
                           strcmp(node->subgraph_id, other->subgraph_id) == 0) {
                    same_subgraph = true;  // Both in same subgraph
                }

                if (same_subgraph) {
                    nodes_in_this_layer++;
                }
            }
        }

        #ifdef KRYON_TRACE_LAYOUT
        if (has_directional_subgraphs && node->subgraph_id) {
            fprintf(stderr, "    [DEBUG] Node '%s' L%d P%d: nodes_in_this_layer=%d (subgraph: %s)\n",
                    node->node_id ? node->node_id : "?", layer, pos,
                    nodes_in_this_layer, node->subgraph_id);
        }
        #endif

        // Check if node belongs to a subgraph with different direction
        IRFlowchartDirection node_direction = state->direction;
        bool node_horizontal = horizontal;
        bool node_reversed = reversed;

        if (has_directional_subgraphs && node->subgraph_id) {
            for (uint32_t sg_idx = 0; sg_idx < state->subgraph_count; sg_idx++) {
                IRFlowchartSubgraphData* sg = state->subgraphs[sg_idx];
                if (sg && sg->subgraph_id && strcmp(node->subgraph_id, sg->subgraph_id) == 0) {
                    if (sg->direction != state->direction) {  // Use subgraph's direction if different from parent
                        node_direction = sg->direction;
                        node_horizontal = (node_direction == IR_FLOWCHART_DIR_LR ||
                                          node_direction == IR_FLOWCHART_DIR_RL);
                        node_reversed = (node_direction == IR_FLOWCHART_DIR_BT ||
                                        node_direction == IR_FLOWCHART_DIR_RL);
                        #ifdef KRYON_TRACE_LAYOUT
                        fprintf(stderr, "    → Node '%s' in subgraph '%s' using direction: %s\n",
                                node->node_id ? node->node_id : "?", sg->subgraph_id,
                                ir_flowchart_direction_to_string(node_direction));
                        #endif
                    }
                    break;
                }
            }
        }

        // Calculate centering offset for nodes within this layer
        float layer_start;
        if (node_horizontal) {
            // LR/RL: nodes in layer arranged vertically (use height for spacing)
            layer_start = (max_nodes_in_layer - nodes_in_this_layer) * (max_node_height + node_spacing) / 2.0f;
        } else {
            // TB/BT: nodes in layer arranged horizontally (use width for spacing)
            layer_start = (max_nodes_in_layer - nodes_in_this_layer) * (max_node_width + node_spacing) / 2.0f;
        }

        float primary_coord, secondary_coord;

        if (node_horizontal) {
            // LR/RL: layers are columns, positions are rows
            primary_coord = layer * (max_node_width + rank_spacing);
            secondary_coord = layer_start + pos * (max_node_height + node_spacing);

            if (node_reversed) {
                primary_coord = (max_layer - layer) * (max_node_width + rank_spacing);
            }

            node->x = primary_coord + (max_node_width - node->width) / 2.0f;
            node->y = secondary_coord + (max_node_height - node->height) / 2.0f;
        } else {
            // TB/BT: layers are rows, positions are columns
            primary_coord = layer * (max_node_height + rank_spacing);
            secondary_coord = layer_start + pos * (max_node_width + node_spacing);

            if (node_reversed) {
                primary_coord = (max_layer - layer) * (max_node_height + rank_spacing);
            }

            node->x = secondary_coord + (max_node_width - node->width) / 2.0f;
            node->y = primary_coord + (max_node_height - node->height) / 2.0f;
        }

        // Track total size
        total_primary_size = fmaxf(total_primary_size,
            horizontal ? (node->x + node->width) : (node->y + node->height));
        total_secondary_size = fmaxf(total_secondary_size,
            horizontal ? (node->y + node->height) : (node->x + node->width));

        #ifdef KRYON_TRACE_LAYOUT
        fprintf(stderr, "  Node '%s' L%d P%d: (%.1f, %.1f) %.1fx%.1f\n",
                node->node_id ? node->node_id : "?", layer, pos,
                node->x, node->y, node->width, node->height);
        #endif
    }

    // Phase 5: Route edges (simple straight-line for now)
    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (!edge) continue;

        // Find source and target nodes
        IRFlowchartNodeData* from_node = NULL;
        IRFlowchartNodeData* to_node = NULL;

        for (uint32_t j = 0; j < state->node_count; j++) {
            if (state->nodes[j] && state->nodes[j]->node_id) {
                if (edge->from_id && strcmp(state->nodes[j]->node_id, edge->from_id) == 0) {
                    from_node = state->nodes[j];
                }
                if (edge->to_id && strcmp(state->nodes[j]->node_id, edge->to_id) == 0) {
                    to_node = state->nodes[j];
                }
            }
        }

        if (from_node && to_node) {
            // Free existing path points
            free(edge->path_points);

            // Create simple 2-point path (straight line from center to center)
            edge->path_point_count = 2;
            edge->path_points = malloc(4 * sizeof(float));  // 2 points x 2 coords
            if (edge->path_points) {
                // From center
                edge->path_points[0] = from_node->x + from_node->width / 2;
                edge->path_points[1] = from_node->y + from_node->height / 2;
                // To center
                edge->path_points[2] = to_node->x + to_node->width / 2;
                edge->path_points[3] = to_node->y + to_node->height / 2;

                #ifdef KRYON_TRACE_LAYOUT
                fprintf(stderr, "  Edge '%s'->'%s': (%.1f,%.1f) -> (%.1f,%.1f)\n",
                        edge->from_id ? edge->from_id : "?",
                        edge->to_id ? edge->to_id : "?",
                        edge->path_points[0], edge->path_points[1],
                        edge->path_points[2], edge->path_points[3]);
                #endif
            }
        }
    }

    // Cleanup
    free(node_layer);
    free(processed);
    free(nodes_per_layer);
    free(layer_position);

    // Calculate natural size
    float natural_width = horizontal ? total_primary_size : total_secondary_size;
    float natural_height = horizontal ? total_secondary_size : total_primary_size;

    // Add padding
    float padding = 20.0f;
    natural_width += padding * 2;
    natural_height += padding * 2;

    // Store natural dimensions (before scaling) for intrinsic sizing
    state->natural_width = natural_width;
    state->natural_height = natural_height;

    // Scale to fit within available space if needed
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (natural_width > available_width && available_width > 0) {
        scale_x = (available_width - padding * 2) / (natural_width - padding * 2);
    }
    if (natural_height > available_height && available_height > 0) {
        scale_y = (available_height - padding * 2) / (natural_height - padding * 2);
    }

    // Use uniform scaling to preserve aspect ratio
    float scale = fminf(scale_x, scale_y);

    // Limit minimum scale to keep nodes readable (at least 0.6)
    // Below this, text becomes unreadable
    float min_scale = 0.6f;
    if (scale < min_scale) {
        scale = min_scale;
    }

    // Apply scaling to node POSITIONS only (not dimensions!)
    // Node dimensions must stay the same size as the text they contain
    // Only positions scale to fit within available space
    if (scale < 1.0f) {
        for (uint32_t i = 0; i < state->node_count; i++) {
            IRFlowchartNodeData* node = state->nodes[i];
            if (!node) continue;

            // Scale position only, NOT width/height
            // Text rendering uses node dimensions directly
            node->x = padding + (node->x) * scale;
            node->y = padding + (node->y) * scale;
            // DO NOT scale: node->width *= scale;
            // DO NOT scale: node->height *= scale;
        }

        // Also scale edge path points
        for (uint32_t i = 0; i < state->edge_count; i++) {
            IRFlowchartEdgeData* edge = state->edges[i];
            if (!edge || !edge->path_points) continue;

            for (uint32_t p = 0; p < edge->path_point_count; p++) {
                edge->path_points[p * 2] = padding + edge->path_points[p * 2] * scale;
                edge->path_points[p * 2 + 1] = padding + edge->path_points[p * 2 + 1] * scale;
            }
        }

        // Update computed size
        natural_width = (natural_width - padding * 2) * scale + padding * 2;
        natural_height = (natural_height - padding * 2) * scale + padding * 2;
    } else {
        // Just add padding offset
        for (uint32_t i = 0; i < state->node_count; i++) {
            IRFlowchartNodeData* node = state->nodes[i];
            if (!node) continue;
            node->x += padding;
            node->y += padding;
        }

        for (uint32_t i = 0; i < state->edge_count; i++) {
            IRFlowchartEdgeData* edge = state->edges[i];
            if (!edge || !edge->path_points) continue;

            for (uint32_t p = 0; p < edge->path_point_count; p++) {
                edge->path_points[p * 2] += padding;
                edge->path_points[p * 2 + 1] += padding;
            }
        }
    }

    // Compute subgraph bounding boxes after node positions are finalized
    compute_subgraph_bounds(state);

    // NOTE: Do NOT overwrite flowchart->rendered_bounds here!
    // The parent container (Column/Row) sets the flowchart's bounds based on
    // its width/height style properties. The flowchart layout just positions
    // nodes within those bounds.

    // Mark layout as computed
    state->layout_computed = true;
    state->computed_width = available_width;
    state->computed_height = available_height;

    // Use natural dimensions (already computed in Phase 4)
    // These include layer spacing, not just node bounding box
    // NOTE: SVG generator will add its own padding, so we remove the padding here
    // to avoid double-padding
    if (state->node_count > 0) {
        const float PADDING = 20.0f;
        state->content_width = state->natural_width - (PADDING * 2);
        state->content_height = state->natural_height - (PADDING * 2);
        state->content_offset_x = 0.0f;
        state->content_offset_y = 0.0f;
    } else {
        // Empty flowchart - use minimum dimensions
        state->content_width = 100.0f;
        state->content_height = 100.0f;
        state->content_offset_x = 0.0f;
        state->content_offset_y = 0.0f;
    }

    #ifdef KRYON_TRACE_LAYOUT
    fprintf(stderr, "🔀 FLOWCHART_LAYOUT done: size=%.1fx%.1f\n",
            flowchart->rendered_bounds.width, flowchart->rendered_bounds.height);
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
static void intrinsic_size_text(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    float font_size = (c->style && c->style->font.size > 0) ? c->style->font.size : 16.0f;
    const char* text = c->text_content ? c->text_content : "";

    if (g_text_measure_callback) {
        // Use backend measurement (SDL3 TTF, terminal, etc.)
        float width = 0, height = 0;
        g_text_measure_callback(text, font_size, 0, &width, &height);
        out->max_content_width = width;
        out->max_content_height = height;
        out->min_content_width = font_size * 0.5f;  // At least 1 character wide
        out->min_content_height = height;
    } else {
        // Heuristic fallback (no backend available)
        float char_width = font_size * 0.5f;  // Rough approximation
        size_t len = strlen(text);
        out->max_content_width = len * char_width;
        out->max_content_height = font_size * 1.2f;  // Line height
        out->min_content_width = char_width;
        out->min_content_height = font_size * 1.2f;
    }
}

/**
 * Compute intrinsic size for Column components
 * Sums children heights, takes max width
 */
static void intrinsic_size_column(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    float max_width = 0;
    float sum_height = 0;
    float gap = (c->layout && c->layout->flex.gap > 0) ? (float)c->layout->flex.gap : 0.0f;

    // Sum children dimensions
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        IRIntrinsicSize* child_intrinsic = &child->layout_state->intrinsic;

        // Max width across all children
        if (child_intrinsic->max_content_width > max_width) {
            max_width = child_intrinsic->max_content_width;
        }

        // Sum heights
        sum_height += child_intrinsic->max_content_height;
        if (i > 0) sum_height += gap;  // Gap between children
    }

    // Add padding
    IRSpacing pad = {0};
    if (c->style) pad = c->style->padding;

    out->min_content_width = max_width + pad.left + pad.right;
    out->max_content_width = max_width + pad.left + pad.right;
    out->min_content_height = sum_height + pad.top + pad.bottom;
    out->max_content_height = sum_height + pad.top + pad.bottom;
}

/**
 * Compute intrinsic size for Row components
 * Sums children widths, takes max height
 */
static void intrinsic_size_row(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    float sum_width = 0;
    float max_height = 0;
    float gap = (c->layout && c->layout->flex.gap > 0) ? (float)c->layout->flex.gap : 0.0f;

    // Sum children dimensions
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        IRIntrinsicSize* child_intrinsic = &child->layout_state->intrinsic;

        // Sum widths
        sum_width += child_intrinsic->max_content_width;
        if (i > 0) sum_width += gap;  // Gap between children

        // Max height across all children
        if (child_intrinsic->max_content_height > max_height) {
            max_height = child_intrinsic->max_content_height;
        }
    }

    // Add padding
    IRSpacing pad = {0};
    if (c->style) pad = c->style->padding;

    out->min_content_width = sum_width + pad.left + pad.right;
    out->max_content_width = sum_width + pad.left + pad.right;
    out->min_content_height = max_height + pad.top + pad.bottom;
    out->max_content_height = max_height + pad.top + pad.bottom;
}

/**
 * Compute intrinsic size for Center components
 * Auto-sizes to single child
 */
static void intrinsic_size_center(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    // Center auto-sizes to its single child (or 0 if no children)
    if (c->child_count == 1 && c->children[0] && c->children[0]->layout_state) {
        *out = c->children[0]->layout_state->intrinsic;
    } else {
        // No children or multiple children - default to 0
        memset(out, 0, sizeof(IRIntrinsicSize));
    }

    // Add padding if present
    if (c->style) {
        IRSpacing pad = c->style->padding;
        out->min_content_width += pad.left + pad.right;
        out->max_content_width += pad.left + pad.right;
        out->min_content_height += pad.top + pad.bottom;
        out->max_content_height += pad.top + pad.bottom;
    }
}

/**
 * Compute intrinsic size for Button components
 * Measures text content plus padding
 */
static void intrinsic_size_button(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    // Buttons are like text with extra padding
    intrinsic_size_text(c, out);

    // Add button-specific padding (standard button padding)
    const float BUTTON_PADDING_H = 16.0f;
    const float BUTTON_PADDING_V = 8.0f;

    out->min_content_width += BUTTON_PADDING_H * 2;
    out->max_content_width += BUTTON_PADDING_H * 2;
    out->min_content_height += BUTTON_PADDING_V * 2;
    out->max_content_height += BUTTON_PADDING_V * 2;

    // Add explicit padding if present
    if (c->style) {
        IRSpacing pad = c->style->padding;
        out->min_content_width += pad.left + pad.right;
        out->max_content_width += pad.left + pad.right;
        out->min_content_height += pad.top + pad.bottom;
        out->max_content_height += pad.top + pad.bottom;
    }
}

/**
 * Compute intrinsic size for Container components
 * Similar to Column, but supports both directions
 */
static void intrinsic_size_container(IRComponent* c, IRIntrinsicSize* out) {
    if (!c || !out) return;

    // Check layout direction (default to column)
    bool is_row = false;
    if (c->layout && c->layout->flex.direction == 1) {
        is_row = true;
    }

    if (is_row) {
        intrinsic_size_row(c, out);
    } else {
        intrinsic_size_column(c, out);
    }

    // If container has explicit dimensions, override intrinsic size with those
    if (c->style) {
        if (c->style->width.type == IR_DIMENSION_PX) {
            out->min_content_width = c->style->width.value;
            out->max_content_width = c->style->width.value;
        }
        if (c->style->height.type == IR_DIMENSION_PX) {
            out->min_content_height = c->style->height.value;
            out->max_content_height = c->style->height.value;
        }
    }
}

/**
 * Main intrinsic measurement function (Pass 1: Bottom-Up)
 * Traverses tree from leaves to root, computing content-based sizes
 */
void ir_layout_compute_intrinsic(IRComponent* component) {
    if (!component || !component->layout_state) return;

    // Skip if already valid and not dirty
    if (component->layout_state->intrinsic_valid && !component->layout_state->dirty) {
        return;
    }

    // Recurse to children FIRST (bottom-up traversal)
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_layout_compute_intrinsic(component->children[i]);
    }

    // Now measure THIS component based on its type
    IRIntrinsicSize* out = &component->layout_state->intrinsic;

    switch (component->type) {
        case IR_COMPONENT_TEXT:
            intrinsic_size_text(component, out);
            break;

        case IR_COMPONENT_COLUMN:
            intrinsic_size_column(component, out);
            break;

        case IR_COMPONENT_ROW:
            intrinsic_size_row(component, out);
            break;

        case IR_COMPONENT_CENTER:
            intrinsic_size_center(component, out);
            break;

        case IR_COMPONENT_BUTTON:
            intrinsic_size_button(component, out);
            break;

        case IR_COMPONENT_CONTAINER:
            intrinsic_size_container(component, out);
            break;

        case IR_COMPONENT_CHECKBOX:
            // Checkboxes are like buttons with text
            intrinsic_size_button(component, out);
            break;

        case IR_COMPONENT_INPUT:
            // Inputs need a default size
            out->min_content_width = 100.0f;
            out->max_content_width = 200.0f;
            out->min_content_height = 24.0f;
            out->max_content_height = 24.0f;
            if (component->style) {
                IRSpacing pad = component->style->padding;
                out->min_content_width += pad.left + pad.right;
                out->max_content_width += pad.left + pad.right;
                out->min_content_height += pad.top + pad.bottom;
                out->max_content_height += pad.top + pad.bottom;
            }
            break;

        case IR_COMPONENT_TAB_GROUP:
        case IR_COMPONENT_TAB_BAR:
        case IR_COMPONENT_TAB_CONTENT:
            // These are containers, treat like columns
            intrinsic_size_column(component, out);
            break;

        case IR_COMPONENT_TAB:
        case IR_COMPONENT_TAB_PANEL:
            // Tabs and panels can have mixed content
            intrinsic_size_container(component, out);
            break;

        case IR_COMPONENT_TABLE:
            // Tables need special intrinsic size calculation
            intrinsic_size_table(component, out);
            break;

        default:
            // Unknown type - assume container-like behavior
            if (component->child_count > 0) {
                intrinsic_size_container(component, out);
            } else {
                // Leaf component with no handler - default to 0
                memset(out, 0, sizeof(IRIntrinsicSize));
            }
            break;
    }

    // Mark intrinsic sizing as valid
    component->layout_state->intrinsic_valid = true;
}

// ============================================================================
// Phase 2: Top-Down Constraint Application
// ============================================================================

/**
 * Resolve dimension based on style and constraints
 * Handles FIXED (px), PERCENT (%), and AUTO sizing
 */
static float layout_resolve_dimension(IRDimension dim, float constraint, float intrinsic_size) {
    switch (dim.type) {
        case IR_DIMENSION_PX:
            // Fixed size in pixels
            return dim.value;

        case IR_DIMENSION_PERCENT:
            // Percentage of parent constraint
            return constraint * (dim.value / 100.0f);

        case IR_DIMENSION_AUTO:
        default:
            // Auto: use intrinsic size, clamped to constraint
            if (intrinsic_size > constraint) {
                return constraint;
            }
            return intrinsic_size;
    }
}

/**
 * Apply main-axis alignment (justify-content for flex containers)
 */
static void apply_main_axis_alignment(IRComponent* c, float available_space, bool is_column) {
    if (!c || !c->layout || c->child_count == 0) return;

    IRAlignment align = c->layout->flex.justify_content;
    if (align == IR_ALIGNMENT_START) return;  // Already positioned at start

    if (getenv("KRYON_TRACE_ALIGNMENT")) {
        fprintf(stderr, "[ALIGN] Component %u, align=%d, available=%.1f, is_column=%d\n",
                c->id, align, available_space, is_column);
    }

    float gap = c->layout->flex.gap;
    float total_content = 0;

    // Calculate total content size
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        if (is_column) {
            total_content += child->layout_state->computed.height;
        } else {
            total_content += child->layout_state->computed.width;
        }
        if (i > 0) total_content += gap;
    }

    float remaining = available_space - total_content;
    if (getenv("KRYON_TRACE_ALIGNMENT")) {
        fprintf(stderr, "[ALIGN]   total_content=%.1f, remaining=%.1f\n", total_content, remaining);
    }
    if (remaining <= 0) return;  // No space to distribute

    float offset = 0;
    if (align == IR_ALIGNMENT_CENTER) {
        offset = remaining / 2.0f;
    } else if (align == IR_ALIGNMENT_END) {
        offset = remaining;
    } else if (align == IR_ALIGNMENT_SPACE_BETWEEN) {
        // First child stays at 0, distribute remaining space between children
        if (c->child_count > 1) {
            float spacing = remaining / (c->child_count - 1);
            for (uint32_t i = 1; i < c->child_count; i++) {
                IRComponent* child = c->children[i];
                if (!child || !child->layout_state) continue;
                if (is_column) {
                    child->layout_state->computed.y += spacing * i;
                } else {
                    child->layout_state->computed.x += spacing * i;
                }
            }
        }
        return;
    } else if (align == IR_ALIGNMENT_SPACE_EVENLY) {
        // Equal space before, between, and after all children
        float spacing = remaining / (c->child_count + 1);
        for (uint32_t i = 0; i < c->child_count; i++) {
            IRComponent* child = c->children[i];
            if (!child || !child->layout_state) continue;
            if (is_column) {
                child->layout_state->computed.y += spacing * (i + 1);
            } else {
                child->layout_state->computed.x += spacing * (i + 1);
            }
        }
        return;
    } else if (align == IR_ALIGNMENT_SPACE_AROUND) {
        // Half space before first, full space between, half space after last
        float spacing = remaining / c->child_count;
        for (uint32_t i = 0; i < c->child_count; i++) {
            IRComponent* child = c->children[i];
            if (!child || !child->layout_state) continue;
            if (is_column) {
                child->layout_state->computed.y += spacing * (i + 0.5f);
            } else {
                child->layout_state->computed.x += spacing * (i + 0.5f);
            }
        }
        return;
    }

    // Apply offset to all children (for CENTER and END)
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;
        if (getenv("KRYON_TRACE_ALIGNMENT")) {
            fprintf(stderr, "[ALIGN]   Child %u: before offset: %s=%.1f\n",
                    child->id, is_column ? "y" : "x",
                    is_column ? child->layout_state->computed.y : child->layout_state->computed.x);
        }
        if (is_column) {
            child->layout_state->computed.y += offset;
        } else {
            child->layout_state->computed.x += offset;
        }
        if (getenv("KRYON_TRACE_ALIGNMENT")) {
            fprintf(stderr, "[ALIGN]   Child %u: after offset: %s=%.1f (offset=%.1f)\n",
                    child->id, is_column ? "y" : "x",
                    is_column ? child->layout_state->computed.y : child->layout_state->computed.x,
                    offset);
        }
    }
}

/**
 * Apply cross-axis alignment (align-items for flex containers)
 */
static void apply_cross_axis_alignment(IRComponent* c, float available_space, bool is_column) {
    if (!c || !c->layout || c->child_count == 0) return;

    IRAlignment align = c->layout->flex.cross_axis;
    if (align == IR_ALIGNMENT_START) return;  // Already positioned at start

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        float child_size = is_column ? child->layout_state->computed.width
                                     : child->layout_state->computed.height;
        float remaining = available_space - child_size;
        if (remaining <= 0) continue;

        float offset = 0;
        if (align == IR_ALIGNMENT_CENTER) {
            offset = remaining / 2.0f;
        } else if (align == IR_ALIGNMENT_END) {
            offset = remaining;
        }

        if (is_column) {
            // Cross-axis is X for columns
            child->layout_state->computed.x += offset;
        } else {
            // Cross-axis is Y for rows
            child->layout_state->computed.y += offset;
        }
    }
}

/**
 * Layout children in a column with flex_grow support
 */
static void layout_column_children(IRComponent* c, IRLayoutConstraints constraints) {
    if (!c || !c->layout_state) return;

    IRSpacing pad = {0};
    if (c->style) pad = c->style->padding;

    // Use the component's COMPUTED dimensions, not the parent's constraints
    float available_height = c->layout_state->computed.height - (pad.top + pad.bottom);
    float available_width = c->layout_state->computed.width - (pad.left + pad.right);
    float gap = c->layout ? (float)c->layout->flex.gap : 0.0f;

    // Phase 1: Measure fixed-size children and count flex_grow
    float total_flex_grow = 0;
    float fixed_height = 0;

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        } else {
            // Fixed-size child: use intrinsic or explicit height
            fixed_height += child->layout_state->intrinsic.max_content_height;
        }
        if (i > 0) fixed_height += gap;
    }

    // Phase 2: Distribute remaining space to flex_grow children
    float remaining_height = available_height - fixed_height;
    float current_y = pad.top;

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        IRLayoutConstraints child_constraints = {
            .max_width = available_width,
            .max_height = available_height,  // Default to available height for percentage calculations
            .min_width = 0,
            .min_height = 0
        };

        // Determine child height
        if (child->layout && child->layout->flex.grow > 0 && total_flex_grow > 0) {
            // Flex-grow child: distribute remaining space proportionally
            float share = remaining_height * (child->layout->flex.grow / total_flex_grow);
            child_constraints.max_height = share > 0 ? share : 0;
        } else if (child->type == IR_COMPONENT_CENTER) {
            // CENTER components fill available height to center their children
            child_constraints.max_height = available_height;
        }
        // For all other children (fixed size, percentage, AUTO):
        // They get available_height as max constraint (already set above)

        // Recursively layout child
        ir_layout_compute_constraints(child, child_constraints);

        // Position child
        child->layout_state->computed.x = pad.left;
        child->layout_state->computed.y = current_y;

        current_y += child->layout_state->computed.height + gap;
    }

    // Apply cross-axis alignment (horizontal alignment in columns)
    apply_cross_axis_alignment(c, available_width, true);

    // Apply main-axis alignment (vertical alignment in columns)
    apply_main_axis_alignment(c, available_height, true);
}

/**
 * Layout children in a row with flex_grow support
 */
static void layout_row_children(IRComponent* c, IRLayoutConstraints constraints) {
    if (!c || !c->layout_state) return;

    IRSpacing pad = {0};
    if (c->style) pad = c->style->padding;

    // Use the component's COMPUTED dimensions, not the parent's constraints
    float available_width = c->layout_state->computed.width - (pad.left + pad.right);
    float available_height = c->layout_state->computed.height - (pad.top + pad.bottom);
    float gap = c->layout ? (float)c->layout->flex.gap : 0.0f;

    // Phase 1: Measure fixed-size children and count flex_grow
    float total_flex_grow = 0;
    float fixed_width = 0;

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        if (child->layout && child->layout->flex.grow > 0) {
            total_flex_grow += child->layout->flex.grow;
        } else {
            // Fixed-size child: use intrinsic or explicit width
            fixed_width += child->layout_state->intrinsic.max_content_width;
        }
        if (i > 0) fixed_width += gap;
    }

    // Phase 2: Distribute remaining space to flex_grow children
    float remaining_width = available_width - fixed_width;
    float current_x = pad.left;

    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (!child || !child->layout_state) continue;

        IRLayoutConstraints child_constraints = {
            .max_width = available_width,  // Default to available width for percentage calculations
            .max_height = available_height,
            .min_width = 0,
            .min_height = 0
        };

        // Determine child width
        if (child->layout && child->layout->flex.grow > 0 && total_flex_grow > 0) {
            // Flex-grow child: distribute remaining space proportionally
            float share = remaining_width * (child->layout->flex.grow / total_flex_grow);
            child_constraints.max_width = share > 0 ? share : 0;
        } else if (child->type == IR_COMPONENT_CENTER) {
            // CENTER components fill available width to center their children
            child_constraints.max_width = available_width;
        }
        // For all other children (fixed size, percentage, AUTO):
        // They get available_width as max constraint (already set above)

        // Recursively layout child
        ir_layout_compute_constraints(child, child_constraints);

        // Position child
        child->layout_state->computed.x = current_x;
        child->layout_state->computed.y = pad.top;

        current_x += child->layout_state->computed.width + gap;
    }

    // Apply cross-axis alignment (vertical alignment in rows)
    apply_cross_axis_alignment(c, available_height, false);

    // Apply main-axis alignment (horizontal alignment in rows)
    apply_main_axis_alignment(c, available_width, false);
}

/**
 * Main constraint application function (Pass 2: Top-Down)
 * Applies parent constraints and computes final layout
 */
void ir_layout_compute_constraints(IRComponent* c, IRLayoutConstraints constraints) {
    if (!c || !c->layout_state) return;

    // Skip if already valid and not dirty
    if (c->layout_state->layout_valid && !c->layout_state->dirty) {
        return;
    }

    // Reset absolute positions flag since we're recomputing layout
    // Positions will be relative after this pass and need Phase 3 conversion
    c->layout_state->absolute_positions_computed = false;

    IRComputedLayout* layout = &c->layout_state->computed;
    IRIntrinsicSize* intrinsic = &c->layout_state->intrinsic;
    IRStyle* style = c->style;

    // Resolve width
    if (style && style->width.type != IR_DIMENSION_AUTO) {
        layout->width = layout_resolve_dimension(style->width, constraints.max_width,
                                                intrinsic->max_content_width);
    } else {
        // AUTO: use intrinsic size, clamped to constraint
        // SPECIAL CASE: CENTER components fill their parent
        if (c->type == IR_COMPONENT_CENTER) {
            layout->width = constraints.max_width;
        }
        // SPECIAL CASE: Rows with justifyContent alignment need to FILL parent
        // to have space for alignment to work
        else if (c->type == IR_COMPONENT_ROW && c->layout) {
            IRAlignment justify = c->layout->flex.justify_content;
            // Only START can shrink-to-fit; all others need full width for alignment
            if (justify != IR_ALIGNMENT_START) {
                layout->width = constraints.max_width;
            } else {
                layout->width = intrinsic->max_content_width;
                if (layout->width > constraints.max_width) {
                    layout->width = constraints.max_width;
                }
            }
        } else {
            layout->width = intrinsic->max_content_width;
            if (layout->width > constraints.max_width) {
                layout->width = constraints.max_width;
            }
        }
    }

    // Resolve height
    if (style && style->height.type != IR_DIMENSION_AUTO) {
        layout->height = layout_resolve_dimension(style->height, constraints.max_height,
                                                 intrinsic->max_content_height);
    } else {
        // AUTO: use intrinsic size, clamped to constraint
        // SPECIAL CASE: CENTER components fill their parent
        if (c->type == IR_COMPONENT_CENTER) {
            layout->height = constraints.max_height;
        }
        // SPECIAL CASE: Columns with justifyContent alignment need to FILL parent
        // to have space for alignment to work
        else if (c->type == IR_COMPONENT_COLUMN && c->layout) {
            IRAlignment justify = c->layout->flex.justify_content;
            // Only START can shrink-to-fit; all others need full height for alignment
            if (justify != IR_ALIGNMENT_START) {
                layout->height = constraints.max_height;
            } else {
                layout->height = intrinsic->max_content_height;
                if (layout->height > constraints.max_height) {
                    layout->height = constraints.max_height;
                }
            }
        } else {
            layout->height = intrinsic->max_content_height;
            if (layout->height > constraints.max_height) {
                layout->height = constraints.max_height;
            }
        }
    }

    // Apply min/max constraints (if we add them later)
    // TODO: Add min_width, max_width, min_height, max_height support

    // Apply aspect ratio (if present)
    // TODO: Add aspect_ratio support

    // Build child constraints
    IRSpacing pad = {0};
    if (style) pad = style->padding;

    IRLayoutConstraints child_constraints = {
        .max_width = layout->width - (pad.left + pad.right),
        .max_height = layout->height - (pad.top + pad.bottom),
        .min_width = 0,
        .min_height = 0
    };

    // Layout children based on component type
    switch (c->type) {
        case IR_COMPONENT_COLUMN:
        case IR_COMPONENT_CONTAINER:
            // Check if container is column or row
            if (c->layout && c->layout->flex.direction == 1) {
                layout_row_children(c, constraints);
            } else {
                layout_column_children(c, constraints);
            }
            break;

        case IR_COMPONENT_ROW:
            layout_row_children(c, constraints);
            break;

        case IR_COMPONENT_CENTER:
            // Center positions its single child in the middle
            if (c->child_count == 1) {
                IRComponent* child = c->children[0];
                ir_layout_compute_constraints(child, child_constraints);

                // Center the child
                child->layout_state->computed.x =
                    (layout->width - child->layout_state->computed.width) / 2.0f;
                child->layout_state->computed.y =
                    (layout->height - child->layout_state->computed.height) / 2.0f;
            }
            break;

        case IR_COMPONENT_TAB_GROUP:
        case IR_COMPONENT_TAB_BAR:
        case IR_COMPONENT_TAB_CONTENT:
            // Tab components behave like columns
            layout_column_children(c, constraints);
            break;

        case IR_COMPONENT_TABLE:
            // Tables use specialized layout algorithm
            // Table dimensions are already resolved above
            // Now layout table contents (sections, rows, cells)
            ir_layout_compute_table(c, layout->width, layout->height);
            break;

        default:
            // Default: layout each child independently with same constraints
            for (uint32_t i = 0; i < c->child_count; i++) {
                ir_layout_compute_constraints(c->children[i], child_constraints);
            }
            break;
    }

    // Mark layout as valid
    c->layout_state->layout_valid = true;
    layout->valid = true;
}

// ============================================================================
// Phase 3: Absolute Positioning
// ============================================================================

/**
 * Convert relative positions to absolute screen coordinates
 * Traverses top-down: root → leaves
 */
void ir_layout_compute_absolute_positions(IRComponent* c, float parent_x, float parent_y) {
    if (!c || !c->layout_state) return;

    // Skip if absolute positions already computed (prevents accumulation on each frame)
    if (c->layout_state->absolute_positions_computed) {
        // Positions are already absolute, recurse to children that may not have been converted yet
        for (uint32_t i = 0; i < c->child_count; i++) {
            IRComponent* child = c->children[i];
            if (child && child->layout_state && !child->layout_state->absolute_positions_computed) {
                ir_layout_compute_absolute_positions(child,
                    c->layout_state->computed.x,
                    c->layout_state->computed.y);
            }
        }
        return;
    }

    // Check for absolute positioning
    if (c->style && c->style->position_mode == IR_POSITION_ABSOLUTE) {
        // Use explicit left/top values (positioned relative to viewport, not parent)
        c->layout_state->computed.x = c->style->absolute_x;
        c->layout_state->computed.y = c->style->absolute_y;
        if (getenv("KRYON_TRACE_ABSOLUTE")) {
            fprintf(stderr, "[ABSOLUTE] Component %u: position=absolute, left=%.1f, top=%.1f\n",
                    c->id, c->style->absolute_x, c->style->absolute_y);
        }
    } else {
        // Convert relative to absolute
        c->layout_state->computed.x += parent_x;
        c->layout_state->computed.y += parent_y;
    }

    // Mark as converted
    c->layout_state->absolute_positions_computed = true;

    // Recurse to children
    for (uint32_t i = 0; i < c->child_count; i++) {
        IRComponent* child = c->children[i];
        if (child && child->layout_state) {
            ir_layout_compute_absolute_positions(child,
                c->layout_state->computed.x,
                c->layout_state->computed.y);
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

/**
 * Main entry point for two-pass layout computation
 * Call this before rendering to compute all layout
 */
void ir_layout_compute_tree(IRComponent* root, float viewport_width, float viewport_height) {
    if (!root) return;

    // Phase 1: Bottom-up intrinsic sizing
    ir_layout_compute_intrinsic(root);

    // Phase 2: Top-down constraint application
    IRLayoutConstraints root_constraints = {
        .max_width = viewport_width,
        .max_height = viewport_height,
        .min_width = 0,
        .min_height = 0
    };
    ir_layout_compute_constraints(root, root_constraints);

    // Phase 3: Convert to absolute coordinates
    ir_layout_compute_absolute_positions(root, 0, 0);
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
