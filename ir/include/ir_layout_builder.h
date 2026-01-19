#ifndef IR_LAYOUT_BUILDER_H
#define IR_LAYOUT_BUILDER_H

#include <stdint.h>
#include <stdbool.h>

#include "ir_core.h"

// ============================================================================
// Layout Management
// ============================================================================

IRLayout* ir_create_layout(void);
void ir_destroy_layout(IRLayout* layout);
void ir_set_layout(IRComponent* component, IRLayout* layout);
IRLayout* ir_get_layout(IRComponent* component);

// ============================================================================
// Flexbox Properties
// ============================================================================

void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis);
void ir_set_flex_properties(IRLayout* layout, uint8_t grow, uint8_t shrink, uint8_t direction);

// ============================================================================
// Size Constraints
// ============================================================================

void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value);
void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value);
void ir_set_max_width(IRLayout* layout, IRDimensionType type, float value);
void ir_set_max_height(IRLayout* layout, IRDimensionType type, float value);
void ir_set_aspect_ratio(IRLayout* layout, float ratio);

// ============================================================================
// Alignment
// ============================================================================

void ir_set_justify_content(IRLayout* layout, IRAlignment justify);
void ir_set_align_items(IRLayout* layout, IRAlignment align);
void ir_set_align_content(IRLayout* layout, IRAlignment align);

// ============================================================================
// BiDi Direction Properties
// ============================================================================

void ir_set_base_direction(IRComponent* component, IRDirection dir);
void ir_set_unicode_bidi(IRComponent* component, IRUnicodeBidi bidi);
IRDirection ir_parse_direction(const char* str);
IRUnicodeBidi ir_parse_unicode_bidi(const char* str);

// ============================================================================
// Grid Layout
// ============================================================================

void ir_set_grid_template_rows(IRLayout* layout, IRGridTrack* tracks, uint8_t count);
void ir_set_grid_template_columns(IRLayout* layout, IRGridTrack* tracks, uint8_t count);
void ir_set_grid_gap(IRLayout* layout, float row_gap, float column_gap);
void ir_set_grid_auto_flow(IRLayout* layout, bool row_direction, bool dense);
void ir_set_grid_alignment(IRLayout* layout, IRAlignment justify_items, IRAlignment align_items,
                            IRAlignment justify_content, IRAlignment align_content);

// ============================================================================
// Grid Item Placement
// ============================================================================

void ir_set_grid_item_placement(IRStyle* style, int16_t row_start, int16_t row_end,
                                  int16_t column_start, int16_t column_end);
void ir_set_grid_item_alignment(IRStyle* style, IRAlignment justify_self, IRAlignment align_self);

// ============================================================================
// Grid Track Helpers
// ============================================================================

IRGridTrack ir_grid_track_px(float value);
IRGridTrack ir_grid_track_percent(float value);
IRGridTrack ir_grid_track_fr(float value);
IRGridTrack ir_grid_track_auto(void);
IRGridTrack ir_grid_track_min_content(void);
IRGridTrack ir_grid_track_max_content(void);

// ============================================================================
// Container Queries
// ============================================================================

void ir_set_container_type(IRStyle* style, IRContainerType type);
void ir_set_container_name(IRStyle* style, const char* name);
void ir_add_breakpoint(IRStyle* style, IRQueryCondition* conditions, uint8_t condition_count);
void ir_breakpoint_set_width(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value);
void ir_breakpoint_set_height(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value);
void ir_breakpoint_set_visible(IRStyle* style, uint8_t breakpoint_index, bool visible);
void ir_breakpoint_set_opacity(IRStyle* style, uint8_t breakpoint_index, float opacity);
void ir_breakpoint_set_layout_mode(IRStyle* style, uint8_t breakpoint_index, IRLayoutMode mode);

// ============================================================================
// Query Condition Helpers
// ============================================================================

IRQueryCondition ir_query_min_width(float value);
IRQueryCondition ir_query_max_width(float value);
IRQueryCondition ir_query_min_height(float value);
IRQueryCondition ir_query_max_height(float value);

#endif // IR_LAYOUT_BUILDER_H
