/**
 * @file flexbox_layout.c
 * @brief Kryon Flexbox Layout Engine Implementation
 */

#include "internal/layout.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

// =============================================================================
// FLEXBOX LAYOUT STRUCTURES
// =============================================================================

typedef struct {
    KryonLayoutNode* node;
    float main_size;
    float cross_size;
    float main_position;
    float cross_position;
    bool is_flexible;
    float flex_grow_factor;
    float flex_shrink_factor;
    float flex_basis;
    float hypothetical_main_size;
    float target_main_size;
    bool frozen;
} KryonFlexItem;

typedef struct {
    KryonFlexItem* items;
    size_t item_count;
    float main_size;
    float cross_size;
    float total_flex_grow;
    float total_flex_shrink;
    float total_weighted_flex_shrink;
    size_t num_auto_margins;
} KryonFlexLine;

typedef struct {
    KryonFlexDirection direction;
    KryonFlexWrap wrap;
    KryonJustifyContent justify_content;
    KryonAlignItems align_items;
    KryonAlignContent align_content;
    
    // Computed values
    bool is_row_direction;
    bool is_wrap_reverse;
    float main_axis_size;
    float cross_axis_size;
    
    // Lines
    KryonFlexLine* lines;
    size_t line_count;
    size_t line_capacity;
    
} KryonFlexboxContext;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static bool is_main_axis_horizontal(KryonFlexDirection direction) {
    return direction == KRYON_FLEX_DIRECTION_ROW || 
           direction == KRYON_FLEX_DIRECTION_ROW_REVERSE;
}

static bool is_reverse_direction(KryonFlexDirection direction) {
    return direction == KRYON_FLEX_DIRECTION_ROW_REVERSE ||
           direction == KRYON_FLEX_DIRECTION_COLUMN_REVERSE;
}

static float get_main_size(const KryonLayoutNode* node, bool is_horizontal) {
    return is_horizontal ? node->computed.width : node->computed.height;
}

static float get_cross_size(const KryonLayoutNode* node, bool is_horizontal) {
    return is_horizontal ? node->computed.height : node->computed.width;
}

static void set_main_size(KryonLayoutNode* node, float size, bool is_horizontal) {
    if (is_horizontal) {
        node->computed.width = size;
    } else {
        node->computed.height = size;
    }
}

static void set_cross_size(KryonLayoutNode* node, float size, bool is_horizontal) {
    if (is_horizontal) {
        node->computed.height = size;
    } else {
        node->computed.width = size;
    }
}

static void set_main_position(KryonLayoutNode* node, float position, bool is_horizontal) {
    if (is_horizontal) {
        node->computed.x = position;
    } else {
        node->computed.y = position;
    }
}

static void set_cross_position(KryonLayoutNode* node, float position, bool is_horizontal) {
    if (is_horizontal) {
        node->computed.y = position;
    } else {
        node->computed.x = position;
    }
}

static float resolve_flex_basis(const KryonLayoutNode* node, bool is_horizontal) {
    if (node->style.flex_basis != KRYON_UNDEFINED) {
        return node->style.flex_basis;
    }
    
    // Use width/height as flex-basis
    float size = is_horizontal ? node->style.width : node->style.height;
    return (size != KRYON_UNDEFINED) ? size : 0.0f;
}

static float get_margin_main_axis(const KryonLayoutNode* node, bool is_horizontal) {
    if (is_horizontal) {
        return node->style.margin_left + node->style.margin_right;
    } else {
        return node->style.margin_top + node->style.margin_bottom;
    }
}

static float get_margin_cross_axis(const KryonLayoutNode* node, bool is_horizontal) {
    if (is_horizontal) {
        return node->style.margin_top + node->style.margin_bottom;
    } else {
        return node->style.margin_left + node->style.margin_right;
    }
}

static float get_padding_main_axis(const KryonLayoutNode* node, bool is_horizontal) {
    if (is_horizontal) {
        return node->style.padding_left + node->style.padding_right;
    } else {
        return node->style.padding_top + node->style.padding_bottom;
    }
}

static float get_padding_cross_axis(const KryonLayoutNode* node, bool is_horizontal) {
    if (is_horizontal) {
        return node->style.padding_top + node->style.padding_bottom;
    } else {
        return node->style.padding_left + node->style.padding_right;
    }
}

// =============================================================================
// FLEX ITEM INITIALIZATION
// =============================================================================

static void init_flex_item(KryonFlexItem* item, KryonLayoutNode* node, bool is_horizontal) {
    item->node = node;
    item->flex_grow_factor = node->style.flex_grow;
    item->flex_shrink_factor = node->style.flex_shrink;
    item->flex_basis = resolve_flex_basis(node, is_horizontal);
    item->is_flexible = (item->flex_grow_factor > 0.0f || item->flex_shrink_factor > 0.0f);
    item->frozen = false;
    
    // Calculate hypothetical main size
    float content_size = item->flex_basis;
    float padding_margin = get_padding_main_axis(node, is_horizontal) + get_margin_main_axis(node, is_horizontal);
    
    item->hypothetical_main_size = content_size + padding_margin;
    item->target_main_size = item->hypothetical_main_size;
    
    // Calculate cross size
    float cross_content = is_horizontal ? node->style.height : node->style.width;
    if (cross_content == KRYON_UNDEFINED) {
        // Auto cross size - will be determined later
        cross_content = 0.0f;
    }
    item->cross_size = cross_content + get_padding_cross_axis(node, is_horizontal) + get_margin_cross_axis(node, is_horizontal);
}

// =============================================================================
// FLEX LINE MANAGEMENT
// =============================================================================

static KryonFlexLine* add_flex_line(KryonFlexboxContext* ctx) {
    if (ctx->line_count >= ctx->line_capacity) {
        size_t new_capacity = ctx->line_capacity == 0 ? 4 : ctx->line_capacity * 2;
        KryonFlexLine* new_lines = kryon_alloc(sizeof(KryonFlexLine) * new_capacity);
        if (!new_lines) return NULL;
        
        if (ctx->lines) {
            memcpy(new_lines, ctx->lines, sizeof(KryonFlexLine) * ctx->line_count);
            kryon_free(ctx->lines);
        }
        
        ctx->lines = new_lines;
        ctx->line_capacity = new_capacity;
    }
    
    KryonFlexLine* line = &ctx->lines[ctx->line_count++];
    memset(line, 0, sizeof(KryonFlexLine));
    
    return line;
}

static void collect_flex_items_into_lines(KryonFlexboxContext* ctx, KryonLayoutNode* container) {
    if (!container->children || container->child_count == 0) return;
    
    KryonFlexLine* current_line = add_flex_line(ctx);
    if (!current_line) return;
    
    // Allocate space for all items in the first line (we'll redistribute later if wrapping)
    current_line->items = kryon_alloc(sizeof(KryonFlexItem) * container->child_count);
    if (!current_line->items) return;
    
    float line_main_size = 0.0f;
    float available_main_size = ctx->main_axis_size;
    
    for (size_t i = 0; i < container->child_count; i++) {
        KryonLayoutNode* child = container->children[i];
        if (child->style.position == KRYON_POSITION_ABSOLUTE) {
            continue; // Skip absolutely positioned items
        }
        
        KryonFlexItem* item = &current_line->items[current_line->item_count];
        init_flex_item(item, child, ctx->is_row_direction);
        
        // Check if we need to wrap
        if (ctx->wrap != KRYON_FLEX_WRAP_NOWRAP && current_line->item_count > 0) {
            float item_size = item->hypothetical_main_size;
            if (line_main_size + item_size > available_main_size) {
                // Start a new line
                current_line = add_flex_line(ctx);
                if (!current_line) break;
                
                current_line->items = kryon_alloc(sizeof(KryonFlexItem) * (container->child_count - i));
                if (!current_line->items) break;
                
                line_main_size = 0.0f;
                item = &current_line->items[current_line->item_count];
                init_flex_item(item, child, ctx->is_row_direction);
            }
        }
        
        current_line->item_count++;
        line_main_size += item->hypothetical_main_size;
        
        // Update line totals
        current_line->total_flex_grow += item->flex_grow_factor;
        current_line->total_flex_shrink += item->flex_shrink_factor;
        current_line->total_weighted_flex_shrink += item->flex_shrink_factor * item->hypothetical_main_size;
    }
    
    // Calculate line main sizes
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonFlexLine* line = &ctx->lines[i];
        line->main_size = 0.0f;
        
        for (size_t j = 0; j < line->item_count; j++) {
            line->main_size += line->items[j].hypothetical_main_size;
        }
    }
}

// =============================================================================
// FLEX ITEM SIZING
// =============================================================================

static void resolve_flexible_lengths(KryonFlexLine* line, float available_main_size) {
    if (line->item_count == 0) return;
    
    float free_space = available_main_size - line->main_size;
    
    if (free_space > 0.0f && line->total_flex_grow > 0.0f) {
        // Distribute positive free space
        float remaining_free_space = free_space;
        
        for (size_t i = 0; i < line->item_count; i++) {
            KryonFlexItem* item = &line->items[i];
            if (item->flex_grow_factor > 0.0f && !item->frozen) {
                float ratio = item->flex_grow_factor / line->total_flex_grow;
                float extra_size = free_space * ratio;
                item->target_main_size = item->hypothetical_main_size + extra_size;
            }
        }
    } else if (free_space < 0.0f && line->total_weighted_flex_shrink > 0.0f) {
        // Distribute negative free space
        float scaled_shrink_factor_sum = line->total_weighted_flex_shrink;
        
        for (size_t i = 0; i < line->item_count; i++) {
            KryonFlexItem* item = &line->items[i];
            if (item->flex_shrink_factor > 0.0f && !item->frozen) {
                float scaled_shrink_factor = item->flex_shrink_factor * item->hypothetical_main_size;
                float ratio = scaled_shrink_factor / scaled_shrink_factor_sum;
                float size_reduction = (-free_space) * ratio;
                item->target_main_size = item->hypothetical_main_size - size_reduction;
                
                // Don't shrink below zero
                if (item->target_main_size < 0.0f) {
                    item->target_main_size = 0.0f;
                }
            }
        }
    }
    
    // Update item main sizes
    for (size_t i = 0; i < line->item_count; i++) {
        KryonFlexItem* item = &line->items[i];
        item->main_size = item->target_main_size;
        set_main_size(item->node, item->main_size, line == NULL ? true : true); // TODO: pass context
    }
}

// =============================================================================
// CROSS AXIS SIZING
// =============================================================================

static void determine_cross_axis_sizes(KryonFlexboxContext* ctx) {
    // Calculate cross size for each line
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonFlexLine* line = &ctx->lines[i];
        line->cross_size = 0.0f;
        
        for (size_t j = 0; j < line->item_count; j++) {
            KryonFlexItem* item = &line->items[j];
            
            // If cross size is auto, calculate it based on content
            if (item->cross_size == 0.0f) {
                // For now, use a simple heuristic
                item->cross_size = 20.0f; // Minimum cross size
            }
            
            line->cross_size = fmaxf(line->cross_size, item->cross_size);
        }
    }
    
    // Stretch items with align-self: stretch
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonFlexLine* line = &ctx->lines[i];
        
        for (size_t j = 0; j < line->item_count; j++) {
            KryonFlexItem* item = &line->items[j];
            KryonAlignSelf align_self = item->node->style.align_self;
            
            if (align_self == KRYON_ALIGN_SELF_AUTO) {
                align_self = (KryonAlignSelf)ctx->align_items;
            }
            
            if (align_self == KRYON_ALIGN_SELF_STRETCH) {
                item->cross_size = line->cross_size;
                set_cross_size(item->node, item->cross_size, ctx->is_row_direction);
            }
        }
    }
}

// =============================================================================
// MAIN AXIS ALIGNMENT
// =============================================================================

static void align_items_on_main_axis(KryonFlexboxContext* ctx, float container_main_size) {
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonFlexLine* line = &ctx->lines[i];
        
        float used_main_size = 0.0f;
        for (size_t j = 0; j < line->item_count; j++) {
            used_main_size += line->items[j].main_size;
        }
        
        float free_space = container_main_size - used_main_size;
        float current_main_position = 0.0f;
        
        switch (ctx->justify_content) {
            case KRYON_JUSTIFY_CONTENT_FLEX_START:
                // Items packed to start
                current_main_position = 0.0f;
                break;
                
            case KRYON_JUSTIFY_CONTENT_FLEX_END:
                // Items packed to end
                current_main_position = free_space;
                break;
                
            case KRYON_JUSTIFY_CONTENT_CENTER:
                // Items centered
                current_main_position = free_space / 2.0f;
                break;
                
            case KRYON_JUSTIFY_CONTENT_SPACE_BETWEEN:
                // Items distributed with space between
                current_main_position = 0.0f;
                break;
                
            case KRYON_JUSTIFY_CONTENT_SPACE_AROUND:
                // Items distributed with space around
                if (line->item_count > 0) {
                    float space_per_item = free_space / line->item_count;
                    current_main_position = space_per_item / 2.0f;
                }
                break;
                
            case KRYON_JUSTIFY_CONTENT_SPACE_EVENLY:
                // Items distributed with equal space
                if (line->item_count > 0) {
                    current_main_position = free_space / (line->item_count + 1);
                }
                break;
        }
        
        // Position items
        for (size_t j = 0; j < line->item_count; j++) {
            KryonFlexItem* item = &line->items[j];
            
            item->main_position = current_main_position;
            set_main_position(item->node, item->main_position, ctx->is_row_direction);
            
            // Calculate next position
            current_main_position += item->main_size;
            
            if (ctx->justify_content == KRYON_JUSTIFY_CONTENT_SPACE_BETWEEN && line->item_count > 1) {
                current_main_position += free_space / (line->item_count - 1);
            } else if (ctx->justify_content == KRYON_JUSTIFY_CONTENT_SPACE_AROUND && line->item_count > 0) {
                current_main_position += free_space / line->item_count;
            } else if (ctx->justify_content == KRYON_JUSTIFY_CONTENT_SPACE_EVENLY && line->item_count > 0) {
                current_main_position += free_space / (line->item_count + 1);
            }
        }
    }
}

// =============================================================================
// CROSS AXIS ALIGNMENT
// =============================================================================

static void align_items_on_cross_axis(KryonFlexboxContext* ctx, float container_cross_size) {
    // Calculate total cross size of all lines
    float total_lines_cross_size = 0.0f;
    for (size_t i = 0; i < ctx->line_count; i++) {
        total_lines_cross_size += ctx->lines[i].cross_size;
    }
    
    float free_cross_space = container_cross_size - total_lines_cross_size;
    float current_line_cross_position = 0.0f;
    
    // Align lines according to align-content
    switch (ctx->align_content) {
        case KRYON_ALIGN_CONTENT_FLEX_START:
            current_line_cross_position = 0.0f;
            break;
            
        case KRYON_ALIGN_CONTENT_FLEX_END:
            current_line_cross_position = free_cross_space;
            break;
            
        case KRYON_ALIGN_CONTENT_CENTER:
            current_line_cross_position = free_cross_space / 2.0f;
            break;
            
        case KRYON_ALIGN_CONTENT_SPACE_BETWEEN:
            current_line_cross_position = 0.0f;
            break;
            
        case KRYON_ALIGN_CONTENT_SPACE_AROUND:
            if (ctx->line_count > 0) {
                float space_per_line = free_cross_space / ctx->line_count;
                current_line_cross_position = space_per_line / 2.0f;
            }
            break;
            
        case KRYON_ALIGN_CONTENT_STRETCH:
            current_line_cross_position = 0.0f;
            // TODO: Stretch lines to fill container
            break;
    }
    
    // Position items within each line
    for (size_t i = 0; i < ctx->line_count; i++) {
        KryonFlexLine* line = &ctx->lines[i];
        
        for (size_t j = 0; j < line->item_count; j++) {
            KryonFlexItem* item = &line->items[j];
            KryonAlignSelf align_self = item->node->style.align_self;
            
            if (align_self == KRYON_ALIGN_SELF_AUTO) {
                align_self = (KryonAlignSelf)ctx->align_items;
            }
            
            float item_cross_position = current_line_cross_position;
            
            switch (align_self) {
                case KRYON_ALIGN_SELF_FLEX_START:
                    // Already at line start
                    break;
                    
                case KRYON_ALIGN_SELF_FLEX_END:
                    item_cross_position += line->cross_size - item->cross_size;
                    break;
                    
                case KRYON_ALIGN_SELF_CENTER:
                    item_cross_position += (line->cross_size - item->cross_size) / 2.0f;
                    break;
                    
                case KRYON_ALIGN_SELF_STRETCH:
                    // Cross size already set in determine_cross_axis_sizes
                    break;
                    
                default:
                    break;
            }
            
            item->cross_position = item_cross_position;
            set_cross_position(item->node, item->cross_position, ctx->is_row_direction);
        }
        
        // Move to next line
        current_line_cross_position += line->cross_size;
        
        if (ctx->align_content == KRYON_ALIGN_CONTENT_SPACE_BETWEEN && ctx->line_count > 1) {
            current_line_cross_position += free_cross_space / (ctx->line_count - 1);
        } else if (ctx->align_content == KRYON_ALIGN_CONTENT_SPACE_AROUND && ctx->line_count > 0) {
            current_line_cross_position += free_cross_space / ctx->line_count;
        }
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_layout_flexbox(KryonLayoutNode* container) {
    if (!container) return false;
    
    // Initialize flexbox context
    KryonFlexboxContext ctx = {0};
    ctx.direction = container->style.flex_direction;
    ctx.wrap = container->style.flex_wrap;
    ctx.justify_content = container->style.justify_content;
    ctx.align_items = container->style.align_items;
    ctx.align_content = container->style.align_content;
    
    ctx.is_row_direction = is_main_axis_horizontal(ctx.direction);
    ctx.is_wrap_reverse = (ctx.wrap == KRYON_FLEX_WRAP_WRAP_REVERSE);
    
    // Get container dimensions
    ctx.main_axis_size = get_main_size(container, ctx.is_row_direction);
    ctx.cross_axis_size = get_cross_size(container, ctx.is_row_direction);
    
    // Step 1: Collect flex items into flex lines
    collect_flex_items_into_lines(&ctx, container);
    
    // Step 2: Resolve flexible lengths
    for (size_t i = 0; i < ctx.line_count; i++) {
        resolve_flexible_lengths(&ctx.lines[i], ctx.main_axis_size);
    }
    
    // Step 3: Determine cross axis sizes
    determine_cross_axis_sizes(&ctx);
    
    // Step 4: Align items on main axis
    align_items_on_main_axis(&ctx, ctx.main_axis_size);
    
    // Step 5: Align items on cross axis
    align_items_on_cross_axis(&ctx, ctx.cross_axis_size);
    
    // Clean up
    for (size_t i = 0; i < ctx.line_count; i++) {
        kryon_free(ctx.lines[i].items);
    }
    kryon_free(ctx.lines);
    
    return true;
}