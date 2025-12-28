#ifndef IR_BUILDER_H
#define IR_BUILDER_H

#include "ir_core.h"
#include "ir_animation.h"
#include <stdlib.h>
#include <string.h>

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

// Style Management
IRStyle* ir_create_style(void);
void ir_destroy_style(IRStyle* style);
void ir_set_style(IRComponent* component, IRStyle* style);
IRStyle* ir_get_style(IRComponent* component);

// Style Property Helpers
void ir_set_width(IRComponent* component, IRDimensionType type, float value);
void ir_set_height(IRComponent* component, IRDimensionType type, float value);
void ir_set_visible(IRStyle* style, bool visible);
void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius);
void ir_set_border_width(IRStyle* style, float width);
void ir_set_border_radius(IRStyle* style, uint8_t radius);
void ir_set_border_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_margin(IRComponent* component, float top, float right, float bottom, float left);
void ir_set_padding(IRComponent* component, float top, float right, float bottom, float left);
void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic);
void ir_set_font_size(IRStyle* style, float size);
void ir_set_font_family(IRStyle* style, const char* family);
void ir_set_font_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_font_style(IRStyle* style, bool bold, bool italic);
void ir_set_z_index(IRStyle* style, uint32_t z_index);

// Style Variable Reference Setters (for theme support)
void ir_set_background_color_var(IRStyle* style, IRStyleVarId var_id);
void ir_set_text_color_var(IRStyle* style, IRStyleVarId var_id);
void ir_set_border_color_var(IRStyle* style, IRStyleVarId var_id);

// Text Effect Helpers
void ir_set_text_overflow(IRStyle* style, IRTextOverflowType overflow);
void ir_set_text_fade(IRStyle* style, IRTextFadeType fade_type, float fade_length);
void ir_set_text_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_set_opacity(IRStyle* style, float opacity);

// Text Layout (Phase 1: Multi-line wrapping)
void ir_set_text_max_width(IRStyle* style, IRDimensionType type, float value);

// Extended Typography (Phase 3)
void ir_set_font_weight(IRStyle* style, uint16_t weight);  // 100-900 (400=normal, 700=bold)
void ir_set_line_height(IRStyle* style, float line_height);  // Line height multiplier
void ir_set_letter_spacing(IRStyle* style, float spacing);   // Letter spacing in pixels
void ir_set_word_spacing(IRStyle* style, float spacing);     // Word spacing in pixels
void ir_set_text_align(IRStyle* style, IRTextAlign align);   // Text alignment
void ir_set_text_decoration(IRStyle* style, uint8_t decoration);  // Decoration flags (bitfield)

// Box Shadow and Filters
void ir_set_box_shadow(IRStyle* style, float offset_x, float offset_y, float blur_radius,
                       float spread_radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool inset);
void ir_add_filter(IRStyle* style, IRFilterType type, float value);
void ir_clear_filters(IRStyle* style);

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
void ir_set_aspect_ratio(IRLayout* layout, float ratio);
void ir_set_justify_content(IRLayout* layout, IRAlignment justify);
void ir_set_align_items(IRLayout* layout, IRAlignment align);
void ir_set_align_content(IRLayout* layout, IRAlignment align);

// BiDi Direction Property Helpers
void ir_set_base_direction(IRComponent* component, IRDirection dir);
void ir_set_unicode_bidi(IRComponent* component, IRUnicodeBidi bidi);
IRDirection ir_parse_direction(const char* str);
IRUnicodeBidi ir_parse_unicode_bidi(const char* str);

// Event Management
IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data);
void ir_destroy_event(IREvent* event);
void ir_add_event(IRComponent* component, IREvent* event);
void ir_remove_event(IRComponent* component, IREvent* event);
IREvent* ir_find_event(IRComponent* component, IREventType type);

// Event Bytecode Support (IR v2.1)
void ir_event_set_bytecode_function_id(IREvent* event, uint32_t function_id);
uint32_t ir_event_get_bytecode_function_id(IREvent* event);

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

// Gradient Helpers
IRGradient* ir_gradient_create_linear(float angle);
IRGradient* ir_gradient_create_radial(float center_x, float center_y);
IRGradient* ir_gradient_create_conic(float center_x, float center_y);
void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_gradient_destroy(IRGradient* gradient);

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

// Maximum tabs per tab group (prevents realloc)
#define IR_MAX_TABS_PER_GROUP 32

// Tab Group Support (shared across frontends)
typedef struct TabGroupState {
    IRComponent* group;
    IRComponent* tab_bar;
    IRComponent* tab_content;
    IRComponent* tabs[IR_MAX_TABS_PER_GROUP];         // Fixed-size array
    IRComponent* panels[IR_MAX_TABS_PER_GROUP];       // Fixed-size array
    TabVisualState tab_visuals[IR_MAX_TABS_PER_GROUP]; // Fixed-size array
    TabClickCallback user_callbacks[IR_MAX_TABS_PER_GROUP]; // Fixed-size array
    void* user_callback_data[IR_MAX_TABS_PER_GROUP];  // Fixed-size array
    uint32_t tab_count;
    uint32_t panel_count;
    int selected_index;
    bool reorderable;
    bool dragging;
    int drag_index;
    float drag_x;
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

// Gradient Creation and Management
IRGradient* ir_gradient_create(IRGradientType type);
void ir_gradient_destroy(IRGradient* gradient);
void ir_gradient_add_stop(IRGradient* gradient, float position, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_gradient_set_angle(IRGradient* gradient, float angle);
void ir_gradient_set_center(IRGradient* gradient, float x, float y);
IRColor ir_color_from_gradient(IRGradient* gradient);

// Style gradient setters
void ir_set_background_gradient(IRStyle* style, IRGradient* gradient);

// Animation and Keyframe Creation
IRAnimation* ir_animation_create_keyframe(const char* name, float duration);
void ir_animation_destroy(IRAnimation* anim);
void ir_animation_set_iterations(IRAnimation* anim, int32_t count);  // -1 = infinite
void ir_animation_set_alternate(IRAnimation* anim, bool alternate);
void ir_animation_set_delay(IRAnimation* anim, float delay);
void ir_animation_set_default_easing(IRAnimation* anim, IREasingType easing);

// Keyframe management
IRKeyframe* ir_animation_add_keyframe(IRAnimation* anim, float offset);  // offset 0.0-1.0
void ir_keyframe_set_property(IRKeyframe* kf, IRAnimationProperty prop, float value);
void ir_keyframe_set_color_property(IRKeyframe* kf, IRAnimationProperty prop, IRColor color);
void ir_keyframe_set_easing(IRKeyframe* kf, IREasingType easing);

// Attach animation to component
void ir_component_add_animation(IRComponent* component, IRAnimation* anim);

// Re-propagate animation flags after tree construction
// Call this after all components are created and added to fix flag propagation
void ir_animation_propagate_flags(IRComponent* root);

// General component subtree finalization (post-construction propagation)
// Call this after adding children (especially from static loops) to ensure all
// post-construction propagation steps are performed
void ir_component_finalize_subtree(IRComponent* component);

// Transition creation
IRTransition* ir_transition_create(IRAnimationProperty property, float duration);
void ir_transition_destroy(IRTransition* transition);
void ir_transition_set_easing(IRTransition* transition, IREasingType easing);
void ir_transition_set_delay(IRTransition* transition, float delay);
void ir_transition_set_trigger(IRTransition* transition, uint32_t state_mask);

// Attach transition to component
void ir_component_add_transition(IRComponent* component, IRTransition* transition);

// Apply all animations to a component tree (call each frame from renderer)
void ir_animation_tree_update(IRComponent* root, float current_time);

// Grid Layout (Phase 5)
void ir_set_grid_template_rows(IRLayout* layout, IRGridTrack* tracks, uint8_t count);
void ir_set_grid_template_columns(IRLayout* layout, IRGridTrack* tracks, uint8_t count);
void ir_set_grid_gap(IRLayout* layout, float row_gap, float column_gap);
void ir_set_grid_auto_flow(IRLayout* layout, bool row_direction, bool dense);
void ir_set_grid_alignment(IRLayout* layout, IRAlignment justify_items, IRAlignment align_items,
                            IRAlignment justify_content, IRAlignment align_content);

// Grid Item Placement
void ir_set_grid_item_placement(IRStyle* style, int16_t row_start, int16_t row_end,
                                  int16_t column_start, int16_t column_end);
void ir_set_grid_item_alignment(IRStyle* style, IRAlignment justify_self, IRAlignment align_self);

// Grid Track Helpers
IRGridTrack ir_grid_track_px(float value);
IRGridTrack ir_grid_track_percent(float value);
IRGridTrack ir_grid_track_fr(float value);
IRGridTrack ir_grid_track_auto(void);
IRGridTrack ir_grid_track_min_content(void);
IRGridTrack ir_grid_track_max_content(void);

// Container Queries (Phase 6)
void ir_set_container_type(IRStyle* style, IRContainerType type);
void ir_set_container_name(IRStyle* style, const char* name);
void ir_add_breakpoint(IRStyle* style, IRQueryCondition* conditions, uint8_t condition_count);
void ir_breakpoint_set_width(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value);
void ir_breakpoint_set_height(IRStyle* style, uint8_t breakpoint_index, IRDimensionType type, float value);
void ir_breakpoint_set_visible(IRStyle* style, uint8_t breakpoint_index, bool visible);
void ir_breakpoint_set_opacity(IRStyle* style, uint8_t breakpoint_index, float opacity);
void ir_breakpoint_set_layout_mode(IRStyle* style, uint8_t breakpoint_index, IRLayoutMode mode);

// Query Condition Helpers
IRQueryCondition ir_query_min_width(float value);
IRQueryCondition ir_query_max_width(float value);
IRQueryCondition ir_query_min_height(float value);
IRQueryCondition ir_query_max_height(float value);

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
void ir_set_blockquote_depth(IRComponent* comp, uint8_t depth);

// Window metadata
void ir_set_window_metadata(int width, int height, const char* title);

#endif // IR_BUILDER_H
