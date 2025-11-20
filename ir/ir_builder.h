#ifndef IR_BUILDER_H
#define IR_BUILDER_H

#include "ir_core.h"
#include <stdlib.h>
#include <string.h>

// Global IR context
extern IRContext* g_ir_context;

// Context Management
IRContext* ir_create_context(void);
void ir_destroy_context(IRContext* context);
void ir_set_context(IRContext* context);

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

// Style Management
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
void ir_set_style(IRComponent* component, IRStyle* style);
IRStyle* ir_get_style(IRComponent* component);

// Style Property Helpers
void ir_set_width(IRStyle* style, IRDimensionType type, float value);
void ir_set_height(IRStyle* style, IRDimensionType type, float value);
void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left);
void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left);
void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);

// Layout Management
IRLayout* ir_create_layout(void);
void ir_destroy_layout(IRLayout* layout);
void ir_set_layout(IRComponent* component, IRLayout* layout);
IRLayout* ir_get_layout(IRComponent* component);

// Layout Property Helpers
void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis);
void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value);
void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value);
void ir_set_justify_content(IRLayout* layout, IRAlignment justify);
void ir_set_align_items(IRLayout* layout, IRAlignment align);
void ir_set_align_content(IRLayout* layout, IRAlignment align);

// Event Management
IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
void ir_destroy_event(IREvent* event);
void ir_add_event(IRComponent* component, IREvent* event);
void ir_remove_event(IRComponent* component, IREvent* event);
IREvent* ir_find_event(IRComponent* component, IREventType type);

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

// Convenience Functions for Common Components
IRComponent* ir_container(const char* tag);
IRComponent* ir_text(const char* content);
IRComponent* ir_button(const char* text);
IRComponent* ir_input(const char* placeholder);
IRComponent* ir_checkbox(const char* label);
IRComponent* ir_row(void);
IRComponent* ir_column(void);
IRComponent* ir_center(void);

// Dimension Helpers
IRDimension ir_dimension_px(float value);
IRDimension ir_dimension_percent(float value);
IRDimension ir_dimension_auto(void);
IRDimension ir_dimension_flex(float value);

// Color Helpers
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b);
IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
IRColor ir_color_transparent(void);

// Validation and Optimization
bool ir_validate_component(IRComponent* component);
void ir_optimize_component(IRComponent* component);

// Hit Testing
bool ir_is_point_in_component(IRComponent* component, float x, float y);
IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y);
void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height);

#endif // IR_BUILDER_H