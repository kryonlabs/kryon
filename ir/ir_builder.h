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

// Style Management
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
void ir_set_style(IRComponent* component, IRStyle* style);
IRStyle* ir_get_style(IRComponent* component);

// Style Property Helpers
void ir_set_width(IRStyle* style, IRDimensionType type, float value);
void ir_set_height(IRStyle* style, IRDimensionType type, float value);
void ir_set_visible(IRStyle* style, bool visible);
void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left);
void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left);
void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);
void ir_set_z_index(IRStyle* style, uint32_t z_index);

// Layout Management
IRLayout* ir_create_layout(void);
void ir_destroy_layout(IRLayout* layout);
void ir_set_layout(IRComponent* component, IRLayout* layout);
IRLayout* ir_get_layout(IRComponent* component);

// Layout Property Helpers
void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis);
void ir_set_flex_properties(IRLayout* layout, uint8_t grow, uint8_t shrink, uint8_t direction);
void ir_set_min_width(IRLayout* layout, IRDimensionType type, float value);
void ir_set_min_height(IRLayout* layout, IRDimensionType type, float value);
void ir_set_max_width(IRLayout* layout, IRDimensionType type, float value);
void ir_set_max_height(IRLayout* layout, IRDimensionType type, float value);
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
IRColor ir_color_named(const char* name);

// Validation and Optimization
bool ir_validate_component(IRComponent* component);
void ir_optimize_component(IRComponent* component);

// Tab Visual State (colors for active/inactive tabs)
typedef struct TabVisualState {
    uint32_t background_color;        // RGBA packed: 0xRRGGBBAA
    uint32_t active_background_color;
    uint32_t text_color;
    uint32_t active_text_color;
} TabVisualState;

// Tab click callback type - called BEFORE tab selection
// tab_index: which tab was clicked
// user_data: user-provided context (e.g., component ID for frontend bridge)
typedef void (*TabClickCallback)(uint32_t tab_index, void* user_data);

// Tab Group Support (shared across frontends)
typedef struct TabGroupState {
    IRComponent* group;
    IRComponent* tab_bar;
    IRComponent* tab_content;
    IRComponent** tabs;
    IRComponent** panels;
    TabVisualState* tab_visuals;  // Array of visual states per tab
    uint32_t tab_count;
    uint32_t panel_count;
    int selected_index;
    bool reorderable;
    bool dragging;
    int drag_index;
    float drag_x;
    // User callback storage - called before tab selection
    TabClickCallback* user_callbacks;  // Array of callbacks (one per tab)
    void** user_callback_data;         // Array of user data pointers
} TabGroupState;

TabGroupState* ir_tabgroup_create_state(IRComponent* group,
                                        IRComponent* tab_bar,
                                        IRComponent* tab_content,
                                        int selected_index,
                                        bool reorderable);
void ir_tabgroup_register_bar(TabGroupState* state, IRComponent* tab_bar);
void ir_tabgroup_register_content(TabGroupState* state, IRComponent* tab_content);
void ir_tabgroup_register_tab(TabGroupState* state, IRComponent* tab);
void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel);
void ir_tabgroup_finalize(TabGroupState* state);
void ir_tabgroup_select(TabGroupState* state, int index);
void ir_tabgroup_reorder(TabGroupState* state, int from_index, int to_index);
void ir_tabgroup_handle_drag(TabGroupState* state, float x, float y, bool is_down, bool is_up);
void ir_tabgroup_set_reorderable(TabGroupState* state, bool reorderable);
void ir_tabgroup_set_tab_visual(TabGroupState* state, int index, TabVisualState visual);
void ir_tabgroup_apply_visuals(TabGroupState* state);  // Apply active/inactive colors based on selected_index

// Tab Group Query Functions
uint32_t ir_tabgroup_get_tab_count(TabGroupState* state);
uint32_t ir_tabgroup_get_panel_count(TabGroupState* state);
int ir_tabgroup_get_selected(TabGroupState* state);
IRComponent* ir_tabgroup_get_tab(TabGroupState* state, uint32_t index);
IRComponent* ir_tabgroup_get_panel(TabGroupState* state, uint32_t index);

// Tab User Callback Registration - callback called BEFORE tab selection
void ir_tabgroup_set_tab_callback(TabGroupState* state, uint32_t index,
                                   TabClickCallback callback, void* user_data);

// Tab Click Handling - combines user callback + selection logic
// Call this from renderer when a tab is clicked
void ir_tabgroup_handle_tab_click(TabGroupState* state, uint32_t tab_index);

// Cleanup
void ir_tabgroup_destroy_state(TabGroupState* state);

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

#endif // IR_BUILDER_H
