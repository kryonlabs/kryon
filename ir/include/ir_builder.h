#ifndef IR_BUILDER_H
#define IR_BUILDER_H

#include "ir_core.h"
// Animation system moved to plugin - no longer included here
#include "ir_tabgroup.h"
#include "ir_style_builder.h"
#include "ir_layout_builder.h"
#include "ir_event_builder.h"
#include "ir_module_refs.h"
#include "ir_table.h"
#include "ir_markdown.h"
// Animation builder moved to plugin - no longer included here
#include "ir_gradient.h"
#include "ir_hit_test.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration for cJSON (used in module reference functions)
typedef struct cJSON cJSON;

// Global IR context
extern IRContext* g_ir_context;

// Context Management
IRContext* ir_create_context(void);
void ir_destroy_context(IRContext* context);
void ir_set_context(IRContext* context);
IRContext* ir_get_global_context(void);  // Get the global IR context
IRComponent* ir_get_root(void);  // Get the current root component
void ir_set_root(IRComponent* root);  // Set the root component

// Component Creation
IRComponent* ir_create_component(IRComponentType type);
IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id);
void ir_destroy_component(IRComponent* component);

// Tree Structure Management
void ir_add_child(IRComponent* parent, IRComponent* child);
void ir_remove_child(IRComponent* parent, IRComponent* child);
void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index);
IRComponent* ir_get_child(IRComponent* component, uint32_t index);
IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id);

// Style Management - see ir_style_builder.h
// Layout Management - see ir_layout_builder.h
// Event Management - see ir_event_builder.h
// Module Reference Management - see ir_module_refs.h

// Logic Management
IRLogic* ir_create_logic(const char* id, LogicSourceType type, const char* source_code);
void ir_destroy_logic(IRLogic* logic);
void ir_add_logic(IRComponent* component, IRLogic* logic);
void ir_remove_logic(IRComponent* component, IRLogic* logic);
IRLogic* ir_find_logic_by_id(IRComponent* root, const char* id);

// Content Management
void ir_set_text_content(IRComponent* component, const char* text);
void ir_set_custom_data(IRComponent* component, const char* data);
void ir_set_tag(IRComponent* component, const char* tag);
void ir_set_each_source(IRComponent* component, const char* source);  // ForEach data source

// Convenience Functions for Common Components
IRComponent* ir_container(const char* tag);
IRComponent* ir_text(const char* content);
IRComponent* ir_button(const char* text);
IRComponent* ir_input(const char* placeholder);
IRComponent* ir_textarea(const char* placeholder, uint32_t rows, uint32_t cols);
IRComponent* ir_checkbox(const char* label);
IRComponent* ir_row(void);
IRComponent* ir_column(void);
IRComponent* ir_center(void);

// Inline Semantic Components (for rich text)
IRComponent* ir_span(void);                  // Inline container <span>
IRComponent* ir_strong(const char* text);    // Bold/important <strong>
IRComponent* ir_em(const char* text);        // Emphasis/italic <em>
IRComponent* ir_code_inline(const char* text); // Inline code <code>
IRComponent* ir_small(const char* text);     // Small text <small>
IRComponent* ir_mark(const char* text);      // Highlighted <mark>

// Dimension Helpers
IRDimension ir_dimension_px(float value);
IRDimension ir_dimension_percent(float value);
IRDimension ir_dimension_auto(void);
IRDimension ir_dimension_flex(float value);

// Color Helpers
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b);
IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
IRColor ir_color_transparent(void);
IRColor ir_color_named(const char* name);

// Gradient Helpers
IRGradient* ir_gradient_create_linear(float angle);
IRGradient* ir_gradient_create_radial(float center_x, float center_y);
IRGradient* ir_gradient_create_conic(float center_x, float center_y);
void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_gradient_destroy(IRGradient* gradient);

// Validation and Optimization
bool ir_validate_component(IRComponent* component);
void ir_optimize_component(IRComponent* component);

// Tab Group Support - types and functions moved to ir_tabgroup.h
// (include ir_tabgroup.h for TabGroupState, TabVisualState, and all ir_tabgroup_* functions)

// Hit Testing
bool ir_is_point_in_component(IRComponent* component, float x, float y);
IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y);
void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height);

// Checkbox State Helpers
bool ir_get_checkbox_state(IRComponent* component);
void ir_set_checkbox_state(IRComponent* component, bool checked);
void ir_toggle_checkbox_state(IRComponent* component);

// Dropdown Convenience Function and State Helpers
IRComponent* ir_dropdown(const char* placeholder, char** options, uint32_t option_count);
int32_t ir_get_dropdown_selected_index(IRComponent* component);
void ir_set_dropdown_selected_index(IRComponent* component, int32_t index);
void ir_set_dropdown_options(IRComponent* component, char** options, uint32_t count);
bool ir_get_dropdown_open_state(IRComponent* component);
void ir_set_dropdown_open_state(IRComponent* component, bool is_open);
void ir_toggle_dropdown_open_state(IRComponent* component);
int32_t ir_get_dropdown_hovered_index(IRComponent* component);
void ir_set_dropdown_hovered_index(IRComponent* component, int32_t index);
IRDropdownState* ir_get_dropdown_state(IRComponent* component);

// Gradient Creation and Management
IRGradient* ir_gradient_create(IRGradientType type);
void ir_gradient_destroy(IRGradient* gradient);
void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_gradient_set_angle(IRGradient* gradient, float angle);
void ir_gradient_set_center(IRGradient* gradient, float x, float y);
IRColor ir_color_from_gradient(IRGradient* gradient);

// Style gradient setters
void ir_set_background_gradient(IRStyle* style, IRGradient* gradient);

// General component subtree finalization (post-construction propagation)
// Call this after adding children (especially from static loops) to ensure all
// post-construction propagation steps are performed
void ir_component_finalize_subtree(IRComponent* component);

// Grid Layout, Container Queries - see ir_layout_builder.h

// ============================================================================
// Table Components
// ============================================================================

// Table State Management
IRTableState* ir_table_create_state(void);
void ir_table_destroy_state(IRTableState* state);
IRTableState* ir_get_table_state(IRComponent* component);

// Table Column Definitions
void ir_table_add_column(IRTableState* state, IRTableColumnDef column);
IRTableColumnDef ir_table_column_auto(void);
IRTableColumnDef ir_table_column_px(float width);
IRTableColumnDef ir_table_column_percent(float percent);
IRTableColumnDef ir_table_column_with_alignment(IRTableColumnDef col, IRAlignment alignment);

// Table Styling
void ir_table_set_border_color(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_table_set_header_background(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_table_set_row_backgrounds(IRTableState* state,
                                   uint8_t even_r, uint8_t even_g, uint8_t even_b, uint8_t even_a,
                                   uint8_t odd_r, uint8_t odd_g, uint8_t odd_b, uint8_t odd_a);
void ir_table_set_border_width(IRTableState* state, float width);
void ir_table_set_cell_padding(IRTableState* state, float padding);
void ir_table_set_show_borders(IRTableState* state, bool show);
void ir_table_set_striped(IRTableState* state, bool striped);

// Table Cell Data
IRTableCellData* ir_table_cell_data_create(uint16_t colspan, uint16_t rowspan, IRAlignment alignment);
IRTableCellData* ir_get_table_cell_data(IRComponent* component);
void ir_table_cell_set_colspan(IRComponent* cell, uint16_t colspan);
void ir_table_cell_set_rowspan(IRComponent* cell, uint16_t rowspan);
void ir_table_cell_set_alignment(IRComponent* cell, IRAlignment alignment);

// Table Component Creation
IRComponent* ir_table(void);
IRComponent* ir_table_head(void);
IRComponent* ir_table_body(void);
IRComponent* ir_table_foot(void);
IRComponent* ir_table_row(void);
IRComponent* ir_table_cell(uint16_t colspan, uint16_t rowspan);
IRComponent* ir_table_header_cell(uint16_t colspan, uint16_t rowspan);

// Table Finalization (call after all children are added)
void ir_table_finalize(IRComponent* table);

// ============================================================================
// Markdown Components
// ============================================================================

// Markdown Component Creation
IRComponent* ir_heading(uint8_t level, const char* text);
IRComponent* ir_paragraph(void);
IRComponent* ir_blockquote(void);
IRComponent* ir_code_block(const char* language, const char* code);
IRComponent* ir_horizontal_rule(void);
IRComponent* ir_list(IRListType type);
IRComponent* ir_list_item(void);
IRComponent* ir_link(const char* url, const char* text);

// Markdown Component Setters
void ir_set_heading_level(IRComponent* comp, uint8_t level);
void ir_set_heading_id(IRComponent* comp, const char* id);
void ir_set_code_language(IRComponent* comp, const char* language);
void ir_set_code_content(IRComponent* comp, const char* code, size_t length);
void ir_set_code_show_line_numbers(IRComponent* comp, bool show);
void ir_set_list_type(IRComponent* comp, IRListType type);
void ir_set_list_start(IRComponent* comp, uint32_t start);
void ir_set_list_tight(IRComponent* comp, bool tight);
void ir_set_list_item_number(IRComponent* comp, uint32_t number);
void ir_set_list_item_marker(IRComponent* comp, const char* marker);
void ir_set_list_item_task(IRComponent* comp, bool is_task, bool checked);
void ir_set_link_url(IRComponent* comp, const char* url);
void ir_set_link_title(IRComponent* comp, const char* title);
void ir_set_link_target(IRComponent* comp, const char* target);
void ir_set_link_rel(IRComponent* comp, const char* rel);
void ir_set_blockquote_depth(IRComponent* comp, uint8_t depth);

// Window metadata
void ir_set_window_metadata(int width, int height, const char* title);

#endif // IR_BUILDER_H
