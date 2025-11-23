#define _GNU_SOURCE
#include "ir_builder.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Global IR context
IRContext* g_ir_context = NULL;

// Type system helper functions
const char* ir_component_type_to_string(IRComponentType type) {
    switch (type) {
        case IR_COMPONENT_CONTAINER: return "Container";
        case IR_COMPONENT_TEXT: return "Text";
        case IR_COMPONENT_BUTTON: return "Button";
        case IR_COMPONENT_INPUT: return "Input";
        case IR_COMPONENT_CHECKBOX: return "Checkbox";
        case IR_COMPONENT_ROW: return "Row";
        case IR_COMPONENT_COLUMN: return "Column";
        case IR_COMPONENT_CENTER: return "Center";
        case IR_COMPONENT_IMAGE: return "Image";
        case IR_COMPONENT_CANVAS: return "Canvas";
        case IR_COMPONENT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* ir_event_type_to_string(IREventType type) {
    switch (type) {
        case IR_EVENT_CLICK: return "Click";
        case IR_EVENT_HOVER: return "Hover";
        case IR_EVENT_FOCUS: return "Focus";
        case IR_EVENT_BLUR: return "Blur";
        case IR_EVENT_KEY: return "Key";
        case IR_EVENT_SCROLL: return "Scroll";
        case IR_EVENT_TIMER: return "Timer";
        case IR_EVENT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* ir_logic_type_to_string(LogicSourceType type) {
    switch (type) {
        case IR_LOGIC_NIM: return "Nim";
        case IR_LOGIC_C: return "C";
        case IR_LOGIC_LUA: return "Lua";
        case IR_LOGIC_WASM: return "WASM";
        case IR_LOGIC_NATIVE: return "Native";
        default: return "Unknown";
    }
}

// Context Management
IRContext* ir_create_context(void) {
    IRContext* context = malloc(sizeof(IRContext));
    if (!context) return NULL;

    context->root = NULL;
    context->logic_list = NULL;
    context->next_component_id = 1;
    context->next_logic_id = 1;

    return context;
}

void ir_destroy_context(IRContext* context) {
    if (!context) return;

    if (context->root) {
        ir_destroy_component(context->root);
    }

    // Destroy all logic objects
    IRLogic* logic = context->logic_list;
    while (logic) {
        IRLogic* next = logic->next;
        ir_destroy_logic(logic);
        logic = next;
    }

    free(context);
}

void ir_set_context(IRContext* context) {
    g_ir_context = context;
}

// Component Creation
IRComponent* ir_create_component(IRComponentType type) {
    return ir_create_component_with_id(type, 0);
}

IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id) {
    IRComponent* component = malloc(sizeof(IRComponent));
    if (!component) return NULL;

    memset(component, 0, sizeof(IRComponent));

    component->type = type;
    if (id == 0 && g_ir_context) {
        component->id = g_ir_context->next_component_id++;
    } else {
        component->id = id;
    }

    return component;
}

void ir_destroy_component(IRComponent* component) {
    if (!component) return;

    // Destroy style
    if (component->style) {
        ir_destroy_style(component->style);
    }

    // Destroy events
    IREvent* event = component->events;
    while (event) {
        IREvent* next = event->next;
        ir_destroy_event(event);
        event = next;
    }

    // Destroy logic
    IRLogic* logic = component->logic;
    while (logic) {
        IRLogic* next = logic->next;
        ir_destroy_logic(logic);
        logic = next;
    }

    // Destroy children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_destroy_component(component->children[i]);
    }

    // Destroy layout
    if (component->layout) {
        ir_destroy_layout(component->layout);
    }

    // Free strings
    if (component->tag) free(component->tag);
    if (component->text_content) free(component->text_content);
    if (component->custom_data) free(component->custom_data);

    // Free children array
    if (component->children) {
        free(component->children);
    }

    free(component);
}

// Tree Structure Management
void ir_add_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return;

    parent->children = realloc(parent->children,
                               sizeof(IRComponent*) * (parent->child_count + 1));
    parent->children[parent->child_count] = child;
    parent->child_count++;
    child->parent = parent;
}

void ir_remove_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child || parent->child_count == 0) return;

    // Find child
    uint32_t index = 0;
    bool found = false;
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            index = i;
            found = true;
            break;
        }
    }

    if (!found) return;

    // Shift remaining children
    for (uint32_t i = index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }

    parent->child_count--;
    child->parent = NULL;
}

void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index) {
    if (!parent || !child || index > parent->child_count) return;

    parent->children = realloc(parent->children,
                               sizeof(IRComponent*) * (parent->child_count + 1));

    // Shift children to make space
    for (uint32_t i = parent->child_count; i > index; i--) {
        parent->children[i] = parent->children[i - 1];
    }

    parent->children[index] = child;
    parent->child_count++;
    child->parent = parent;
}

IRComponent* ir_get_child(IRComponent* component, uint32_t index) {
    if (!component || index >= component->child_count) return NULL;
    return component->children[index];
}

IRComponent* ir_find_component_by_id(IRComponent* root, uint32_t id) {
    if (!root) return NULL;

    if (root->id == id) return root;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = ir_find_component_by_id(root->children[i], id);
        if (found) return found;
    }

    return NULL;
}

// Style Management
IRStyle* ir_create_style(void) {
    IRStyle* style = malloc(sizeof(IRStyle));
    if (!style) return NULL;

    memset(style, 0, sizeof(IRStyle));

    // Set sensible defaults
    style->visible = true;
    style->opacity = 1.0f;
    style->position_mode = IR_POSITION_RELATIVE;
    style->absolute_x = 0.0f;
    style->absolute_y = 0.0f;

    return style;
}

void ir_destroy_style(IRStyle* style) {
    if (!style) return;

    if (style->animations) {
        for (uint32_t i = 0; i < style->animation_count; i++) {
            if (style->animations[i].custom_data) {
                free(style->animations[i].custom_data);
            }
        }
        free(style->animations);
    }

    if (style->font.family) free(style->font.family);

    free(style);
}

void ir_set_style(IRComponent* component, IRStyle* style) {
    if (!component) return;

    if (component->style) {
        ir_destroy_style(component->style);
    }
    component->style = style;
}

IRStyle* ir_get_style(IRComponent* component) {
    if (!component) return NULL;

    if (!component->style) {
        component->style = ir_create_style();
    }

    return component->style;
}

// Style Property Helpers
void ir_set_width(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->width.type = type;
    style->width.value = value;
}

void ir_set_height(IRStyle* style, IRDimensionType type, float value) {
    if (!style) return;
    style->height.type = type;
    style->height.value = value;
}

void ir_set_background_color(IRStyle* style, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!style) return;
    style->background.type = IR_COLOR_SOLID;
    style->background.r = r;
    style->background.g = g;
    style->background.b = b;
    style->background.a = a;
}

void ir_set_border(IRStyle* style, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t radius) {
    if (!style) return;
    style->border.width = width;
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.r = r;
    style->border.color.g = g;
    style->border.color.b = b;
    style->border.color.a = a;
    style->border.radius = radius;
}

void ir_set_margin(IRStyle* style, float top, float right, float bottom, float left) {
    if (!style) return;
    style->margin.top = top;
    style->margin.right = right;
    style->margin.bottom = bottom;
    style->margin.left = left;
}

void ir_set_padding(IRStyle* style, float top, float right, float bottom, float left) {
    if (!style) return;
    style->padding.top = top;
    style->padding.right = right;
    style->padding.bottom = bottom;
    style->padding.left = left;
}

void ir_set_font(IRStyle* style, float size, const char* family, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool bold, bool italic) {
    if (!style) return;
    style->font.size = size;
    style->font.family = family ? strdup(family) : NULL;
    style->font.color.type = IR_COLOR_SOLID;
    style->font.color.r = r;
    style->font.color.g = g;
    style->font.color.b = b;
    style->font.color.a = a;
    style->font.bold = bold;
    style->font.italic = italic;
}

// Layout Management
IRLayout* ir_create_layout(void) {
    IRLayout* layout = malloc(sizeof(IRLayout));
    if (!layout) return NULL;

    memset(layout, 0, sizeof(IRLayout));

    // Set sensible defaults
    layout->flex.main_axis = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    layout->flex.justify_content = IR_ALIGNMENT_START;

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

void ir_set_flexbox(IRLayout* layout, bool wrap, uint32_t gap, IRAlignment main_axis, IRAlignment cross_axis) {
    if (!layout) return;
    layout->flex.wrap = wrap;
    layout->flex.gap = gap;
    layout->flex.main_axis = main_axis;
    layout->flex.cross_axis = cross_axis;
}

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

// Event Management
IREvent* ir_create_event(IREventType type, const char* logic_id, const char* handler_data) {
    IREvent* event = malloc(sizeof(IREvent));
    if (!event) return NULL;

    memset(event, 0, sizeof(IREvent));

    event->type = type;
    event->logic_id = logic_id ? strdup(logic_id) : NULL;
    event->handler_data = handler_data ? strdup(handler_data) : NULL;

    return event;
}

void ir_destroy_event(IREvent* event) {
    if (!event) return;

    if (event->logic_id) free(event->logic_id);
    if (event->handler_data) free(event->handler_data);

    free(event);
}

void ir_add_event(IRComponent* component, IREvent* event) {
    if (!component || !event) return;

    event->next = component->events;
    component->events = event;
}

void ir_remove_event(IRComponent* component, IREvent* event) {
    if (!component || !event || !component->events) return;

    if (component->events == event) {
        component->events = event->next;
        return;
    }

    IREvent* current = component->events;
    while (current->next) {
        if (current->next == event) {
            current->next = event->next;
            return;
        }
        current = current->next;
    }
}

IREvent* ir_find_event(IRComponent* component, IREventType type) {
    if (!component || !component->events) return NULL;

    IREvent* event = component->events;
    while (event) {
        if (event->type == type) return event;
        event = event->next;
    }

    return NULL;
}

// Logic Management
IRLogic* ir_create_logic(const char* id, LogicSourceType type, const char* source_code) {
    IRLogic* logic = malloc(sizeof(IRLogic));
    if (!logic) return NULL;

    memset(logic, 0, sizeof(IRLogic));

    logic->id = id ? strdup(id) : NULL;
    logic->type = type;
    logic->source_code = source_code ? strdup(source_code) : NULL;

    return logic;
}

void ir_destroy_logic(IRLogic* logic) {
    if (!logic) return;

    if (logic->id) free(logic->id);
    if (logic->source_code) free(logic->source_code);

    free(logic);
}

void ir_add_logic(IRComponent* component, IRLogic* logic) {
    if (!component || !logic) return;

    logic->next = component->logic;
    component->logic = logic;
}

void ir_remove_logic(IRComponent* component, IRLogic* logic) {
    if (!component || !logic || !component->logic) return;

    if (component->logic == logic) {
        component->logic = logic->next;
        return;
    }

    IRLogic* current = component->logic;
    while (current->next) {
        if (current->next == logic) {
            current->next = logic->next;
            return;
        }
        current = current->next;
    }
}

IRLogic* ir_find_logic_by_id(IRComponent* root, const char* id) {
    if (!root || !id) return NULL;

    // Check component's logic
    IRLogic* logic = root->logic;
    while (logic) {
        if (logic->id && strcmp(logic->id, id) == 0) return logic;
        logic = logic->next;
    }

    // Search children
    for (uint32_t i = 0; i < root->child_count; i++) {
        IRLogic* found = ir_find_logic_by_id(root->children[i], id);
        if (found) return found;
    }

    return NULL;
}

// Content Management
void ir_set_text_content(IRComponent* component, const char* text) {
    if (!component) return;

    if (component->text_content) {
        free(component->text_content);
    }
    component->text_content = text ? strdup(text) : NULL;
}

void ir_set_custom_data(IRComponent* component, const char* data) {
    if (!component) return;

    if (component->custom_data) {
        free(component->custom_data);
    }
    component->custom_data = data ? strdup(data) : NULL;
}

// Checkbox state helpers
bool ir_get_checkbox_state(IRComponent* component) {
    if (!component || !component->custom_data) return false;
    return strcmp(component->custom_data, "checked") == 0;
}

void ir_set_checkbox_state(IRComponent* component, bool checked) {
    if (!component) return;
    ir_set_custom_data(component, checked ? "checked" : "unchecked");
}

void ir_toggle_checkbox_state(IRComponent* component) {
    if (!component) return;
    bool current = ir_get_checkbox_state(component);
    ir_set_checkbox_state(component, !current);
}

// Dropdown state helpers
IRDropdownState* ir_get_dropdown_state(IRComponent* component) {
    if (!component || component->type != IR_COMPONENT_DROPDOWN || !component->custom_data) {
        return NULL;
    }
    return (IRDropdownState*)component->custom_data;
}

int32_t ir_get_dropdown_selected_index(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->selected_index : -1;
}

void ir_set_dropdown_selected_index(IRComponent* component, int32_t index) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Validate index
    if (index >= -1 && (uint32_t)index < state->option_count) {
        state->selected_index = index;
    }
}

void ir_set_dropdown_options(IRComponent* component, char** options, uint32_t count) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Free existing options
    if (state->options) {
        for (uint32_t i = 0; i < state->option_count; i++) {
            free(state->options[i]);
        }
        free(state->options);
    }

    // Copy new options
    state->option_count = count;
    if (count > 0 && options) {
        state->options = (char**)malloc(sizeof(char*) * count);
        for (uint32_t i = 0; i < count; i++) {
            state->options[i] = options[i] ? strdup(options[i]) : NULL;
        }
    } else {
        state->options = NULL;
    }

    // Reset selection if out of range
    if (state->selected_index >= (int32_t)count) {
        state->selected_index = -1;
    }
}

bool ir_get_dropdown_open_state(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->is_open : false;
}

void ir_set_dropdown_open_state(IRComponent* component, bool is_open) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (state) {
        state->is_open = is_open;
        if (!is_open) {
            state->hovered_index = -1;  // Reset hover when closing
        }
    }
}

void ir_toggle_dropdown_open_state(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (state) {
        state->is_open = !state->is_open;
        if (!state->is_open) {
            state->hovered_index = -1;  // Reset hover when closing
        }
    }
}

int32_t ir_get_dropdown_hovered_index(IRComponent* component) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    return state ? state->hovered_index : -1;
}

void ir_set_dropdown_hovered_index(IRComponent* component, int32_t index) {
    IRDropdownState* state = ir_get_dropdown_state(component);
    if (!state) return;

    // Validate index (-1 is valid for no hover)
    if (index >= -1 && (uint32_t)index < state->option_count) {
        state->hovered_index = index;
    }
}

void ir_set_tag(IRComponent* component, const char* tag) {
    if (!component) return;

    if (component->tag) {
        free(component->tag);
    }
    component->tag = tag ? strdup(tag) : NULL;
}

// Convenience Functions
IRComponent* ir_container(const char* tag) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CONTAINER);
    if (tag) ir_set_tag(component, tag);
    return component;
}

IRComponent* ir_text(const char* content) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TEXT);
    if (content) ir_set_text_content(component, content);
    return component;
}

IRComponent* ir_button(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_BUTTON);
    if (text) ir_set_text_content(component, text);
    return component;
}

IRComponent* ir_input(const char* placeholder) {
    IRComponent* component = ir_create_component(IR_COMPONENT_INPUT);
    if (placeholder) ir_set_custom_data(component, placeholder);
    return component;
}

IRComponent* ir_checkbox(const char* label) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CHECKBOX);
    if (label) ir_set_text_content(component, label);
    return component;
}

IRComponent* ir_dropdown(const char* placeholder, char** options, uint32_t option_count) {
    IRComponent* component = ir_create_component(IR_COMPONENT_DROPDOWN);

    // Create and initialize dropdown state
    IRDropdownState* state = (IRDropdownState*)malloc(sizeof(IRDropdownState));
    if (!state) return component;

    state->placeholder = placeholder ? strdup(placeholder) : NULL;
    state->option_count = option_count;
    state->selected_index = -1;  // No selection initially
    state->is_open = false;
    state->hovered_index = -1;

    // Copy options array
    if (option_count > 0 && options) {
        state->options = (char**)malloc(sizeof(char*) * option_count);
        for (uint32_t i = 0; i < option_count; i++) {
            state->options[i] = options[i] ? strdup(options[i]) : NULL;
        }
    } else {
        state->options = NULL;
    }

    // Store state pointer in custom_data
    component->custom_data = (char*)state;

    return component;
}

IRComponent* ir_row(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_ROW);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.main_axis = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    return component;
}

IRComponent* ir_column(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_COLUMN);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.main_axis = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    return component;
}

IRComponent* ir_center(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CENTER);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.main_axis = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    return component;
}

// Dimension Helpers
IRDimension ir_dimension_px(float value) {
    IRDimension dim = { IR_DIMENSION_PX, value };
    return dim;
}

IRDimension ir_dimension_percent(float value) {
    IRDimension dim = { IR_DIMENSION_PERCENT, value };
    return dim;
}

IRDimension ir_dimension_auto(void) {
    IRDimension dim = { IR_DIMENSION_AUTO, 0.0f };
    return dim;
}

IRDimension ir_dimension_flex(float value) {
    IRDimension dim = { IR_DIMENSION_FLEX, value };
    return dim;
}

// Color Helpers
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    IRColor color = { IR_COLOR_SOLID, r, g, b, 255 };
    return color;
}

IRColor ir_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    IRColor color = { IR_COLOR_SOLID, r, g, b, a };
    return color;
}

IRColor ir_color_transparent(void) {
    IRColor color = { IR_COLOR_TRANSPARENT, 0, 0, 0, 0 };
    return color;
}

// Validation and Optimization
bool ir_validate_component(IRComponent* component) {
    if (!component) return false;

    // Basic validation
    if (component->type == IR_COMPONENT_TEXT && !component->text_content) {
        return false;  // Text components need content
    }

    // Validate children recursively
    for (uint32_t i = 0; i < component->child_count; i++) {
        if (!ir_validate_component(component->children[i])) {
            return false;
        }
    }

    return true;
}

void ir_optimize_component(IRComponent* component) {
    if (!component) return;

    // Remove empty containers
    if (component->type == IR_COMPONENT_CONTAINER && component->child_count == 0) {
        // Mark for removal if parent exists
        return;
    }

    // Optimize children
    for (uint32_t i = 0; i < component->child_count; i++) {
        ir_optimize_component(component->children[i]);
    }
}

// ============================================================================
// Hit Testing - Find component at a given point
// ============================================================================

bool ir_is_point_in_component(IRComponent* component, float x, float y) {
    if (!component || !component->rendered_bounds.valid) {
        return false;
    }

    float comp_x = component->rendered_bounds.x;
    float comp_y = component->rendered_bounds.y;
    float comp_w = component->rendered_bounds.width;
    float comp_h = component->rendered_bounds.height;

    return (x >= comp_x && x < comp_x + comp_w &&
            y >= comp_y && y < comp_y + comp_h);
}

IRComponent* ir_find_component_at_point(IRComponent* root, float x, float y) {
    if (!root || !ir_is_point_in_component(root, x, y)) {
        return NULL;
    }

    // Find the child with highest z-index that contains the point
    IRComponent* best_target = NULL;
    uint32_t best_z_index = 0;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* child = root->children[i];
        if (!child) continue;

        // Check if point is in this child's bounds
        if (ir_is_point_in_component(child, x, y)) {
            // Recursively find target in this child's subtree
            IRComponent* child_target = ir_find_component_at_point(child, x, y);
            if (child_target != NULL) {
                // Get the effective z-index
                uint32_t effective_z = child_target->style ? child_target->style->z_index : 0;

                if (best_target == NULL || effective_z > best_z_index) {
                    best_z_index = effective_z;
                    best_target = child_target;
                }
            }
        }
    }

    if (best_target != NULL) {
        return best_target;
    }

    // No child handled the event, return this component
    return root;
}

void ir_set_rendered_bounds(IRComponent* component, float x, float y, float width, float height) {
    if (!component) return;

    component->rendered_bounds.x = x;
    component->rendered_bounds.y = y;
    component->rendered_bounds.width = width;
    component->rendered_bounds.height = height;
    component->rendered_bounds.valid = true;
}

// Layout alignment functions - proper implementation using flexbox
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
    layout->flex.main_axis = align;
}

