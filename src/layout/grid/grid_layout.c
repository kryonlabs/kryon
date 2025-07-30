/**
 * @file grid_layout.c
 * @brief Kryon CSS Grid Layout Engine Implementation
 */

#include "internal/layout.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// GRID LAYOUT STRUCTURES
// =============================================================================

typedef struct {
    float size;
    KryonGridTrackSizing sizing;
    bool is_flexible; // fr units
    float min_size;
    float max_size;
} KryonGridTrack;

typedef struct {
    KryonLayoutNode* node;
    uint32_t column_start;
    uint32_t column_end;
    uint32_t row_start;
    uint32_t row_end;
    KryonJustifySelf justify_self;
    KryonAlignSelf align_self;
    
    // Computed values
    float width;
    float height;
    float x;
    float y;
} KryonGridItem;

typedef struct {
    // Grid definition
    KryonGridTrack* column_tracks;
    KryonGridTrack* row_tracks;
    size_t column_count;
    size_t row_count;
    
    // Grid items
    KryonGridItem* items;
    size_t item_count;
    size_t item_capacity;
    
    // Gap
    float column_gap;
    float row_gap;
    
    // Alignment
    KryonJustifyContent justify_content;
    KryonAlignContent align_content;
    KryonJustifyItems justify_items;
    KryonAlignItems align_items;
    
    // Computed grid dimensions
    float total_width;
    float total_height;
    float* column_positions;
    float* row_positions;
    
} KryonGridContext;

// =============================================================================
// GRID TRACK SIZING
// =============================================================================

static void parse_grid_template(const char* template_str, KryonGridTrack** tracks, size_t* track_count) {
    if (!template_str || !tracks || !track_count) return;
    
    // Simple parser for grid template strings like "1fr 200px 1fr"
    // This is a simplified implementation
    
    size_t capacity = 4;
    *tracks = kryon_alloc(sizeof(KryonGridTrack) * capacity);
    if (!*tracks) return;
    
    *track_count = 0;
    
    char* template_copy = kryon_alloc(strlen(template_str) + 1);
    if (!template_copy) {
        kryon_free(*tracks);
        *tracks = NULL;
        return;
    }
    strcpy(template_copy, template_str);
    
    char* token = strtok(template_copy, " ");
    while (token != NULL) {
        if (*track_count >= capacity) {
            // Expand capacity
            capacity *= 2;
            KryonGridTrack* new_tracks = kryon_alloc(sizeof(KryonGridTrack) * capacity);
            if (!new_tracks) break;
            
            memcpy(new_tracks, *tracks, sizeof(KryonGridTrack) * (*track_count));
            kryon_free(*tracks);
            *tracks = new_tracks;
        }
        
        KryonGridTrack* track = &(*tracks)[*track_count];
        memset(track, 0, sizeof(KryonGridTrack));
        
        // Parse track size
        if (strstr(token, "fr")) {
            // Flexible track
            track->sizing = KRYON_GRID_TRACK_FR;
            track->size = atof(token);
            track->is_flexible = true;
        } else if (strstr(token, "px")) {
            // Fixed pixel size
            track->sizing = KRYON_GRID_TRACK_PX;
            track->size = atof(token);
            track->is_flexible = false;
        } else if (strstr(token, "%")) {
            // Percentage size
            track->sizing = KRYON_GRID_TRACK_PERCENT;
            track->size = atof(token) / 100.0f;
            track->is_flexible = false;
        } else if (strcmp(token, "auto") == 0) {
            // Auto size
            track->sizing = KRYON_GRID_TRACK_AUTO;
            track->size = 0.0f;
            track->is_flexible = false;
        } else {
            // Default to pixel size
            track->sizing = KRYON_GRID_TRACK_PX;
            track->size = atof(token);
            track->is_flexible = false;
        }
        
        (*track_count)++;
        token = strtok(NULL, " ");
    }
    
    kryon_free(template_copy);
}

static void resolve_track_sizes(KryonGridTrack* tracks, size_t track_count, 
                               float available_size, float gap_size) {
    if (!tracks || track_count == 0) return;
    
    float total_gap = gap_size * (track_count - 1);
    float content_size = available_size - total_gap;
    
    // First pass: resolve fixed and percentage sizes
    float used_size = 0.0f;
    float total_fr = 0.0f;
    size_t auto_tracks = 0;
    
    for (size_t i = 0; i < track_count; i++) {
        KryonGridTrack* track = &tracks[i];
        
        switch (track->sizing) {
            case KRYON_GRID_TRACK_PX:
                track->min_size = track->max_size = track->size;
                used_size += track->size;
                break;
                
            case KRYON_GRID_TRACK_PERCENT:
                track->min_size = track->max_size = content_size * track->size;
                used_size += track->min_size;
                break;
                
            case KRYON_GRID_TRACK_FR:
                total_fr += track->size;
                break;
                
            case KRYON_GRID_TRACK_AUTO:
                auto_tracks++;
                track->min_size = 0.0f;
                track->max_size = FLT_MAX;
                break;
                
            default:
                break;
        }
    }
    
    // Second pass: resolve flexible (fr) and auto tracks
    float remaining_size = content_size - used_size;
    
    if (total_fr > 0.0f && remaining_size > 0.0f) {
        float fr_unit_size = remaining_size / total_fr;
        
        for (size_t i = 0; i < track_count; i++) {
            KryonGridTrack* track = &tracks[i];
            
            if (track->sizing == KRYON_GRID_TRACK_FR) {
                track->min_size = track->max_size = track->size * fr_unit_size;
            }
        }
    } else if (auto_tracks > 0 && remaining_size > 0.0f) {
        float auto_track_size = remaining_size / auto_tracks;
        
        for (size_t i = 0; i < track_count; i++) {
            KryonGridTrack* track = &tracks[i];
            
            if (track->sizing == KRYON_GRID_TRACK_AUTO) {
                track->min_size = track->max_size = auto_track_size;
            }
        }
    }
    
    // Set final sizes
    for (size_t i = 0; i < track_count; i++) {
        tracks[i].size = tracks[i].max_size;
    }
}

// =============================================================================
// GRID ITEM PLACEMENT
// =============================================================================

static void place_grid_items(KryonGridContext* ctx, KryonLayoutNode* container) {
    if (!container->children || container->child_count == 0) return;
    
    // Allocate space for grid items
    ctx->item_capacity = container->child_count;
    ctx->items = kryon_alloc(sizeof(KryonGridItem) * ctx->item_capacity);
    if (!ctx->items) return;
    
    ctx->item_count = 0;
    
    for (size_t i = 0; i < container->child_count; i++) {
        KryonLayoutNode* child = container->children[i];
        if (child->style.position == KRYON_POSITION_ABSOLUTE) {
            continue; // Skip absolutely positioned items
        }
        
        KryonGridItem* item = &ctx->items[ctx->item_count];
        memset(item, 0, sizeof(KryonGridItem));
        
        item->node = child;
        
        // Parse grid area properties
        item->column_start = child->style.grid_column_start;
        item->column_end = child->style.grid_column_end;
        item->row_start = child->style.grid_row_start;
        item->row_end = child->style.grid_row_end;
        
        // Auto-placement if not specified
        if (item->column_start == 0) item->column_start = 1;
        if (item->column_end == 0) item->column_end = item->column_start + 1;
        if (item->row_start == 0) item->row_start = 1;
        if (item->row_end == 0) item->row_end = item->row_start + 1;
        
        // Clamp to grid bounds
        item->column_start = item->column_start > ctx->column_count ? ctx->column_count : item->column_start;
        item->column_end = item->column_end > ctx->column_count + 1 ? ctx->column_count + 1 : item->column_end;
        item->row_start = item->row_start > ctx->row_count ? ctx->row_count : item->row_start;
        item->row_end = item->row_end > ctx->row_count + 1 ? ctx->row_count + 1 : item->row_end;
        
        // Alignment
        item->justify_self = child->style.justify_self;
        item->align_self = child->style.align_self;
        
        if (item->justify_self == KRYON_JUSTIFY_SELF_AUTO) {
            item->justify_self = (KryonJustifySelf)ctx->justify_items;
        }
        if (item->align_self == KRYON_ALIGN_SELF_AUTO) {
            item->align_self = (KryonAlignSelf)ctx->align_items;
        }
        
        ctx->item_count++;
    }
}

// =============================================================================
// GRID POSITIONING
// =============================================================================

static void calculate_grid_positions(KryonGridContext* ctx) {
    // Calculate column positions
    ctx->column_positions = kryon_alloc(sizeof(float) * (ctx->column_count + 1));
    if (ctx->column_positions) {
        float position = 0.0f;
        ctx->column_positions[0] = position;
        
        for (size_t i = 0; i < ctx->column_count; i++) {
            position += ctx->column_tracks[i].size;
            if (i < ctx->column_count - 1) {
                position += ctx->column_gap;
            }
            ctx->column_positions[i + 1] = position;
        }
        
        ctx->total_width = position;
    }
    
    // Calculate row positions
    ctx->row_positions = kryon_alloc(sizeof(float) * (ctx->row_count + 1));
    if (ctx->row_positions) {
        float position = 0.0f;
        ctx->row_positions[0] = position;
        
        for (size_t i = 0; i < ctx->row_count; i++) {
            position += ctx->row_tracks[i].size;
            if (i < ctx->row_count - 1) {
                position += ctx->row_gap;
            }
            ctx->row_positions[i + 1] = position;
        }
        
        ctx->total_height = position;
    }
}

static void position_grid_items(KryonGridContext* ctx) {
    for (size_t i = 0; i < ctx->item_count; i++) {
        KryonGridItem* item = &ctx->items[i];
        
        // Calculate item area
        float area_x = ctx->column_positions[item->column_start - 1];
        float area_y = ctx->row_positions[item->row_start - 1];
        float area_width = ctx->column_positions[item->column_end - 1] - area_x;
        float area_height = ctx->row_positions[item->row_end - 1] - area_y;
        
        // Get item's intrinsic size
        float item_width = item->node->style.width;
        float item_height = item->node->style.height;
        
        if (item_width == KRYON_UNDEFINED) {
            item_width = area_width; // Fill area by default
        }
        if (item_height == KRYON_UNDEFINED) {
            item_height = area_height; // Fill area by default
        }
        
        // Apply justify-self
        float item_x = area_x;
        switch (item->justify_self) {
            case KRYON_JUSTIFY_SELF_START:
                item_x = area_x;
                break;
            case KRYON_JUSTIFY_SELF_END:
                item_x = area_x + area_width - item_width;
                break;
            case KRYON_JUSTIFY_SELF_CENTER:
                item_x = area_x + (area_width - item_width) / 2.0f;
                break;
            case KRYON_JUSTIFY_SELF_STRETCH:
                item_x = area_x;
                item_width = area_width;
                break;
            default:
                item_x = area_x;
                break;
        }
        
        // Apply align-self
        float item_y = area_y;
        switch (item->align_self) {
            case KRYON_ALIGN_SELF_FLEX_START:
                item_y = area_y;
                break;
            case KRYON_ALIGN_SELF_FLEX_END:
                item_y = area_y + area_height - item_height;
                break;
            case KRYON_ALIGN_SELF_CENTER:
                item_y = area_y + (area_height - item_height) / 2.0f;
                break;
            case KRYON_ALIGN_SELF_STRETCH:
                item_y = area_y;
                item_height = area_height;
                break;
            default:
                item_y = area_y;
                break;
        }
        
        // Set computed values
        item->x = item_x;
        item->y = item_y;
        item->width = item_width;
        item->height = item_height;
        
        // Update node layout
        item->node->computed.x = item_x;
        item->node->computed.y = item_y;
        item->node->computed.width = item_width;
        item->node->computed.height = item_height;
    }
}

// =============================================================================
// GRID CONTENT ALIGNMENT
// =============================================================================

static void align_grid_content(KryonGridContext* ctx, KryonLayoutNode* container) {
    float container_width = container->computed.width;
    float container_height = container->computed.height;
    
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    
    // Justify content (main axis alignment)
    if (ctx->total_width < container_width) {
        float free_space = container_width - ctx->total_width;
        
        switch (ctx->justify_content) {
            case KRYON_JUSTIFY_CONTENT_FLEX_START:
                offset_x = 0.0f;
                break;
            case KRYON_JUSTIFY_CONTENT_FLEX_END:
                offset_x = free_space;
                break;
            case KRYON_JUSTIFY_CONTENT_CENTER:
                offset_x = free_space / 2.0f;
                break;
            case KRYON_JUSTIFY_CONTENT_SPACE_BETWEEN:
                // TODO: Distribute space between tracks
                offset_x = 0.0f;
                break;
            case KRYON_JUSTIFY_CONTENT_SPACE_AROUND:
                // TODO: Distribute space around tracks
                offset_x = free_space / 2.0f;
                break;
            case KRYON_JUSTIFY_CONTENT_SPACE_EVENLY:
                // TODO: Distribute space evenly
                offset_x = free_space / 2.0f;
                break;
        }
    }
    
    // Align content (cross axis alignment)
    if (ctx->total_height < container_height) {
        float free_space = container_height - ctx->total_height;
        
        switch (ctx->align_content) {
            case KRYON_ALIGN_CONTENT_FLEX_START:
                offset_y = 0.0f;
                break;
            case KRYON_ALIGN_CONTENT_FLEX_END:
                offset_y = free_space;
                break;
            case KRYON_ALIGN_CONTENT_CENTER:
                offset_y = free_space / 2.0f;
                break;
            case KRYON_ALIGN_CONTENT_SPACE_BETWEEN:
                // TODO: Distribute space between tracks
                offset_y = 0.0f;
                break;
            case KRYON_ALIGN_CONTENT_SPACE_AROUND:
                // TODO: Distribute space around tracks
                offset_y = free_space / 2.0f;
                break;
            case KRYON_ALIGN_CONTENT_STRETCH:
                // TODO: Stretch tracks to fill container
                offset_y = 0.0f;
                break;
        }
    }
    
    // Apply offsets to all items
    if (offset_x != 0.0f || offset_y != 0.0f) {
        for (size_t i = 0; i < ctx->item_count; i++) {
            KryonGridItem* item = &ctx->items[i];
            item->node->computed.x += offset_x;
            item->node->computed.y += offset_y;
        }
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_layout_grid(KryonLayoutNode* container) {
    if (!container) return false;
    
    // Initialize grid context
    KryonGridContext ctx = {0};
    
    ctx.column_gap = container->style.column_gap;
    ctx.row_gap = container->style.row_gap;
    ctx.justify_content = container->style.justify_content;
    ctx.align_content = container->style.align_content;
    ctx.justify_items = container->style.justify_items;
    ctx.align_items = container->style.align_items;
    
    // Parse grid template columns and rows
    if (container->style.grid_template_columns) {
        parse_grid_template(container->style.grid_template_columns, 
                          &ctx.column_tracks, &ctx.column_count);
    }
    if (container->style.grid_template_rows) {
        parse_grid_template(container->style.grid_template_rows, 
                          &ctx.row_tracks, &ctx.row_count);
    }
    
    // Default to single column/row if not specified
    if (ctx.column_count == 0) {
        ctx.column_count = 1;
        ctx.column_tracks = kryon_alloc(sizeof(KryonGridTrack));
        if (ctx.column_tracks) {
            ctx.column_tracks[0].sizing = KRYON_GRID_TRACK_FR;
            ctx.column_tracks[0].size = 1.0f;
            ctx.column_tracks[0].is_flexible = true;
        }
    }
    if (ctx.row_count == 0) {
        ctx.row_count = 1;
        ctx.row_tracks = kryon_alloc(sizeof(KryonGridTrack));
        if (ctx.row_tracks) {
            ctx.row_tracks[0].sizing = KRYON_GRID_TRACK_AUTO;
            ctx.row_tracks[0].size = 0.0f;
            ctx.row_tracks[0].is_flexible = false;
        }
    }
    
    // Step 1: Resolve track sizes
    resolve_track_sizes(ctx.column_tracks, ctx.column_count, 
                       container->computed.width, ctx.column_gap);
    resolve_track_sizes(ctx.row_tracks, ctx.row_count, 
                       container->computed.height, ctx.row_gap);
    
    // Step 2: Place grid items
    place_grid_items(&ctx, container);
    
    // Step 3: Calculate grid positions
    calculate_grid_positions(&ctx);
    
    // Step 4: Position grid items
    position_grid_items(&ctx);
    
    // Step 5: Align grid content
    align_grid_content(&ctx, container);
    
    // Clean up
    kryon_free(ctx.column_tracks);
    kryon_free(ctx.row_tracks);
    kryon_free(ctx.items);
    kryon_free(ctx.column_positions);
    kryon_free(ctx.row_positions);
    
    return true;
}