// IR Layout Builder Module
// Layout property setters extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_layout_builder.h"
#include "ir_builder.h"
#include "ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper function to mark component dirty when layout changes
static void mark_style_dirty(IRComponent* component) {
    if (!component) return;
    ir_layout_mark_dirty(component);
    if (component->layout_state) {
        component->layout_state->dirty_flags |= IR_DIRTY_STYLE | IR_DIRTY_LAYOUT;
    }
}

// Case-insensitive string comparison
static int ir_str_ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

// ============================================================================
// Layout Management
// ============================================================================

IRLayout* ir_create_layout(void) {
    IRLayout* layout = malloc(sizeof(IRLayout));
    if (!layout) return NULL;

    memset(layout, 0, sizeof(IRLayout));

    // Set sensible defaults
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    layout->flex.shrink = 1;  // CSS default is 1, not 0

    return layout;
}

void ir_destroy_layout(IRLayout* layout) {
    free(layout);
}

void ir_set_layout(IRComponent* component, IRLayout* layout) {
    if (!component) return;

    if (component->layout) {
        ir_destroy_layout(component->layout);
    }
    component->layout = layout;
}

IRLayout* ir_get_layout(IRComponent* component) {
    if (!component) return NULL;

    if (!component->layout) {
        component->layout = ir_create_layout();
    }

    return component->layout;
}

// ============================================================================
// Flexbox Properties
// ============================================================================

void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis) {
    if (!layout) return;
    layout->flex.wrap = wrap;
    layout->flex.gap = gap;
    layout->flex.justify_content = main_axis;
    layout->flex.cross_axis = cross_axis;
}

void ir_set_flex_properties(IRLayout* layout, uint8_t grow, uint8_t shrink, uint8_t direction) {
    if (!layout) return;
    layout->flex.grow = grow;
    layout->flex.shrink = shrink;
    layout->flex.direction = direction;
}

// ============================================================================
// Size Constraints
// ============================================================================

void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->min_width.type = type;
    layout->min_width.value = value;
}

void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->min_height.type = type;
    layout->min_height.value = value;
}

void ir_set_max_width(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->max_width.type = type;
    layout->max_width.value = value;
}

void ir_set_max_height(IRLayout* layout, IRDimensionType type, float value) {
    if (!layout) return;
    layout->max_height.type = type;
    layout->max_height.value = value;
}

void ir_set_aspect_ratio(IRLayout* layout, float ratio) {
    if (!layout) return;
    layout->aspect_ratio = ratio;
}

// ============================================================================
// Alignment
// ============================================================================

void ir_set_justify_content(IRLayout* layout, IRAlignment justify) {
    if (!layout) return;
    // justify-content controls main axis alignment (horizontal for row, vertical for column)
    layout->flex.justify_content = justify;
}

void ir_set_align_items(IRLayout* layout, IRAlignment align) {
    if (!layout) return;
    // align-items controls cross axis alignment (vertical for row, horizontal for column)
    layout->flex.cross_axis = align;
}

void ir_set_align_content(IRLayout* layout, IRAlignment align) {
    if (!layout) return;
    // align-content controls how multiple rows/columns are aligned (when wrapping)
    // For now, use main_axis to control overall alignment
    layout->flex.justify_content = align;
}

// ============================================================================
// BiDi Direction Properties
// ============================================================================

void ir_set_base_direction(IRComponent* component, IRDirection dir) {
    if (!component || !component->layout) return;
    component->layout->flex.base_direction = (uint8_t)dir;
    mark_style_dirty(component);
}

void ir_set_unicode_bidi(IRComponent* component, IRUnicodeBidi bidi) {
    if (!component || !component->layout) return;
    component->layout->flex.unicode_bidi = (uint8_t)bidi;
    mark_style_dirty(component);
}

IRDirection ir_parse_direction(const char* str) {
    if (!str) return IR_DIRECTION_LTR;
    if (ir_str_ieq(str, "rtl")) return IR_DIRECTION_RTL;
    if (ir_str_ieq(str, "ltr")) return IR_DIRECTION_LTR;
    if (ir_str_ieq(str, "auto")) return IR_DIRECTION_AUTO;
    if (ir_str_ieq(str, "inherit")) return IR_DIRECTION_INHERIT;
    return IR_DIRECTION_LTR;  // Default to LTR
}

IRUnicodeBidi ir_parse_unicode_bidi(const char* str) {
    if (!str) return IR_UNICODE_BIDI_NORMAL;
    if (ir_str_ieq(str, "normal")) return IR_UNICODE_BIDI_NORMAL;
    if (ir_str_ieq(str, "embed")) return IR_UNICODE_BIDI_EMBED;
    if (ir_str_ieq(str, "isolate") || ir_str_ieq(str, "bidi-override")) return IR_UNICODE_BIDI_ISOLATE;
    if (ir_str_ieq(str, "plaintext")) return IR_UNICODE_BIDI_PLAINTEXT;
    return IR_UNICODE_BIDI_NORMAL;  // Default to normal
}

// ============================================================================
// Grid Layout
// ============================================================================

void ir_set_grid_template_rows(IRLayout* layout, IRGridTrack* tracks, uint8_t count) {
    if (!layout || !tracks) return;
    if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;

    layout->mode = IR_LAYOUT_MODE_GRID;
    for (uint8_t i = 0; i < count; i++) {
        layout->grid.rows[i] = tracks[i];
    }
    layout->grid.row_count = count;
}

void ir_set_grid_template_columns(IRLayout* layout, IRGridTrack* tracks, uint8_t count) {
    if (!layout || !tracks) return;
    if (count > IR_MAX_GRID_TRACKS) count = IR_MAX_GRID_TRACKS;

    layout->mode = IR_LAYOUT_MODE_GRID;
    for (uint8_t i = 0; i < count; i++) {
        layout->grid.columns[i] = tracks[i];
    }
    layout->grid.column_count = count;
}

void ir_set_grid_gap(IRLayout* layout, float row_gap, float column_gap) {
    if (!layout) return;
    layout->grid.row_gap = row_gap;
    layout->grid.column_gap = column_gap;
}

void ir_set_grid_auto_flow(IRLayout* layout, bool row_direction, bool dense) {
    if (!layout) return;
    layout->grid.auto_flow_row = row_direction;
    layout->grid.auto_flow_dense = dense;
}

void ir_set_grid_alignment(IRLayout* layout, IRAlignment justify_items, IRAlignment align_items,
                            IRAlignment justify_content, IRAlignment align_content) {
    if (!layout) return;
    layout->grid.justify_items = justify_items;
    layout->grid.align_items = align_items;
    layout->grid.justify_content = justify_content;
    layout->grid.align_content = align_content;
}

// ============================================================================
// Grid Item Placement
// ============================================================================

void ir_set_grid_item_placement(IRStyle* style, int16_t row_start, int16_t row_end,
                                  int16_t column_start, int16_t column_end) {
    if (!style) return;
    style->grid_item.row_start = row_start;
    style->grid_item.row_end = row_end;
    style->grid_item.column_start = column_start;
    style->grid_item.column_end = column_end;
}

void ir_set_grid_item_alignment(IRStyle* style, IRAlignment justify_self, IRAlignment align_self) {
    if (!style) return;
    style->grid_item.justify_self = justify_self;
    style->grid_item.align_self = align_self;
}

// ============================================================================
// Grid Track Helpers
// ============================================================================

IRGridTrack ir_grid_track_px(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_PX;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_percent(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_PERCENT;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_fr(float value) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_FR;
    track.value = value;
    return track;
}

IRGridTrack ir_grid_track_auto(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_AUTO;
    track.value = 0;
    return track;
}

IRGridTrack ir_grid_track_min_content(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_MIN_CONTENT;
    track.value = 0;
    return track;
}

IRGridTrack ir_grid_track_max_content(void) {
    IRGridTrack track;
    track.type = IR_GRID_TRACK_MAX_CONTENT;
    track.value = 0;
    return track;
}

// ============================================================================
// Container Queries
// ============================================================================

void ir_set_container_type(IRStyle* style, IRContainerType type) {
    if (!style) return;
    style->container_type = type;
}

void ir_set_container_name(IRStyle* style, const char* name) {
    if (!style) return;
    if (style->container_name) {
        free(style->container_name);
    }
    style->container_name = name ? strdup(name) : NULL;
}

void ir_add_breakpoint(IRStyle* style, IRQueryCondition* conditions, uint8_t condition_count) {
    if (!style || !conditions) return;
    if (style->breakpoint_count >= IR_MAX_BREAKPOINTS) return;
    if (condition_count > IR_MAX_QUERY_CONDITIONS) condition_count = IR_MAX_QUERY_CONDITIONS;

    IRBreakpoint* bp = &style->breakpoints[style->breakpoint_count];

    // Copy conditions
    for (uint8_t i = 0; i < condition_count; i++) {
        bp->conditions[i] = conditions[i];
    }
    bp->condition_count = condition_count;

    // Initialize with defaults
    bp->width.type = IR_DIMENSION_AUTO;
    bp->height.type = IR_DIMENSION_AUTO;
    bp->visible = true;
    bp->opacity = -1.0f;  // -1 means don't override
    bp->has_layout_mode = false;

    style->breakpoint_count++;
}

void ir_breakpoint_set_width(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->width.type = type;
    bp->width.value = value;
}

void ir_breakpoint_set_height(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->height.type = type;
    bp->height.value = value;
}

void ir_breakpoint_set_visible(IRStyle* style, uint8_t breakpoint_index, bool visible) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    style->breakpoints[breakpoint_index].visible = visible;
}

void ir_breakpoint_set_opacity(IRStyle* style, uint8_t breakpoint_index, float opacity) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    style->breakpoints[breakpoint_index].opacity = opacity;
}

void ir_breakpoint_set_layout_mode(IRStyle* style, uint8_t breakpoint_index, IRLayoutMode mode) {
    if (!style || breakpoint_index >= style->breakpoint_count) return;
    IRBreakpoint* bp = &style->breakpoints[breakpoint_index];
    bp->layout_mode = mode;
    bp->has_layout_mode = true;
}

// ============================================================================
// Query Condition Helpers
// ============================================================================

IRQueryCondition ir_query_min_width(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MIN_WIDTH;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_max_width(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MAX_WIDTH;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_min_height(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MIN_HEIGHT;
    cond.value = value;
    return cond;
}

IRQueryCondition ir_query_max_height(float value) {
    IRQueryCondition cond;
    cond.type = IR_QUERY_MAX_HEIGHT;
    cond.value = value;
    return cond;
}
