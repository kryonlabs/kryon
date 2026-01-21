#define _GNU_SOURCE
#include "../include/ir_builder.h"
#include "utils/ir_color_utils.h"
#include "../include/ir_component_factory.h"
#include "utils/ir_memory.h"
#include "utils/ir_hashmap.h"
#include "../include/ir_component_handler.h"
#include "components/registry.h"
#include "cJSON.h"
#include "utils/ir_json_helpers.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

// Global IR context
IRContext* g_ir_context = NULL;

// Thread-local IRContext getter/setter (defined in ir_instance.c)
// Returns the current IRContext for this thread, or NULL if using global
extern IRContext* ir_context_get_current(void);

// Get the active IRContext (thread-local first, then global)
__attribute__((unused)) static IRContext* get_active_context(void) {
    IRContext* ctx = ir_context_get_current();
    return ctx ? ctx : g_ir_context;
}

// Callback for cleanup when components are removed
// This allows the reactive system to clean up when tab panels change
extern void luaOnComponentRemoved(IRComponent* component) __attribute__((weak));

// Callback to clean up button/canvas/input handlers when components are removed
extern void luaCleanupHandlersForComponent(IRComponent* component) __attribute__((weak));

// Callback when components are added to the tree
// This resets cleanup tracking so panels can be cleaned up again when removed
extern void luaOnComponentAdded(IRComponent* component) __attribute__((weak));

// Forward declarations
void ir_destroy_handler_source(IRHandlerSource* source);

const char* ir_logic_type_to_string(LogicSourceType type) {
    switch (type) {
        case IR_LOGIC_C: return "C";
        case IR_LOGIC_LUA: return "Lua";
        case IR_LOGIC_WASM: return "WASM";
        case IR_LOGIC_NATIVE: return "Native";
        default: return "Unknown";
    }
}

// Case-insensitive string comparison (used by ir_parse_direction and ir_parse_unicode_bidi)
__attribute__((unused)) static int ir_str_ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

// ir_color_named moved to ir_color_utils.c
// TabGroup functions moved to ir_tabgroup.c

// mark_style_dirty has been consolidated into ir_component_mark_style_dirty() in ir_layout.c

// Context Management
IRContext* ir_create_context(void) {
    IRContext* context = malloc(sizeof(IRContext));
    if (!context) return NULL;

    context->root = NULL;
    context->logic_list = NULL;
    context->next_component_id = 1;
    context->next_logic_id = 1;
    context->metadata = NULL;
    context->source_metadata = NULL;
    context->source_structures = NULL;
    context->reactive_manifest = NULL;
    context->stylesheet = NULL;  // CRITICAL: Initialize to NULL to prevent garbage pointer access

    // Create component pool
    context->component_pool = ir_pool_create();
    if (!context->component_pool) {
        free(context);
        return NULL;
    }

    // Create component hash map
    context->component_map = ir_map_create(256);
    if (!context->component_map) {
        ir_pool_destroy(context->component_pool);
        free(context);
        return NULL;
    }

    // Initialize component layout traits (once per context)
    static bool traits_initialized = false;
    if (!traits_initialized) {
        ir_layout_init_traits();
        ir_component_handler_registry_init();
        traits_initialized = true;
    }

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

    // Destroy component hash map
    if (context->component_map) {
        ir_map_destroy(context->component_map);
    }

    // Destroy component pool
    if (context->component_pool) {
        ir_pool_destroy(context->component_pool);
    }

    free(context);
}

void ir_set_context(IRContext* context) {
    g_ir_context = context;
}

IRContext* ir_get_global_context(void) {
    return g_ir_context;
}

IRComponent* ir_get_root(void) {
    return g_ir_context ? g_ir_context->root : NULL;
}

void ir_set_root(IRComponent* root) {
    if (g_ir_context) {
        g_ir_context->root = root;
    }
}

// Component Creation functions moved to ir_component_factory.c
// Tree Structure Management functions moved to ir_component_factory.c
// Style Management functions moved to ir_style_builder.c

// Layout Management functions moved to ir_layout_builder.c
// Event Management functions moved to ir_event_builder.c

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

// Content Management functions moved to ir_component_factory.c
// ir_set_text_content
// ir_set_custom_data
// ir_set_tag
// ir_set_each_source

// Module Reference Management functions moved to ir_module_refs.c

// Checkbox state helpers moved to ir_component_factory.c
// Dropdown state helpers moved to ir_component_factory.c
// ir_set_tag moved to ir_component_factory.c

// Convenience Functions
IRComponent* ir_container(const char* tag) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CONTAINER);
    // Container is a generic block-level element like HTML <div>
    // It stacks children vertically (block flow) without flex layout
    IRLayout* layout = ir_get_layout(component);
    layout->flex.direction = 0xFF;  // Not a flex container - uses block layout
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
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

IRComponent* ir_textarea(const char* placeholder, uint32_t rows, uint32_t cols) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TEXTAREA);
    if (placeholder) ir_set_custom_data(component, placeholder);
    // Store rows and cols in custom data as "rows:cols" format or use style
    // For now, we'll store them in the component's tag for simplicity
    char tag_buf[64];
    snprintf(tag_buf, sizeof(tag_buf), "textarea:%u:%u", rows, cols);
    if (placeholder) {
        ir_set_custom_data(component, placeholder);
    }
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
    layout->flex.direction = 1;  // Row = horizontal (1)
    layout->flex.justify_content = IR_ALIGNMENT_START;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    return component;
}

IRComponent* ir_column(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_COLUMN);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.direction = 0;  // Column = vertical (0)
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_START;
    return component;
}

IRComponent* ir_center(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CENTER);
    IRLayout* layout = ir_get_layout(component);
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
    layout->flex.justify_content = IR_ALIGNMENT_CENTER;
    return component;
}

// Inline Semantic Components (for rich text)
IRComponent* ir_span(void) {
    return ir_create_component(IR_COMPONENT_SPAN);
}

IRComponent* ir_strong(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_STRONG);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply bold font weight
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.weight = 700;
    }
    return component;
}

IRComponent* ir_em(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_EM);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply italic style
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.italic = true;
    }
    return component;
}

IRComponent* ir_code_inline(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_CODE_INLINE);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply monospace font
    IRStyle* style = ir_get_style(component);
    if (style) {
        if (style->font.family) free(style->font.family);
        style->font.family = strdup("monospace");
    }
    return component;
}

IRComponent* ir_small(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_SMALL);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply smaller font size
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->font.size = 0.8f * 16.0f;  // 80% of default 16px
    }
    return component;
}

IRComponent* ir_mark(const char* text) {
    IRComponent* component = ir_create_component(IR_COMPONENT_MARK);
    if (text) {
        component->text_content = strdup(text);
    }
    // Apply yellow highlight background
    IRStyle* style = ir_get_style(component);
    if (style) {
        style->background = IR_COLOR_RGBA(255, 255, 0, 255);
    }
    return component;
}

// Dimension Helpers
IRDimension ir_dimension_flex(float value) {
    IRDimension dim = { IR_DIMENSION_FLEX, value };
    return dim;
}

// Color Helpers moved to ir_color_utils.c
// ir_color_rgb, ir_color_transparent, ir_color_named

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

// Hit Testing functions moved to ir_hit_test.c
// Gradient Helpers moved to ir_gradient.c
// Animation Builder Functions moved to ir_animation_builder.c
// Grid Layout functions moved to ir_layout_builder.c
// Container Queries functions moved to ir_layout_builder.c

// Table Components moved to ir_table.c

// Markdown Components moved to ir_markdown.c


// Helper functions (Lua FFI)
IRComponentType ir_get_component_type(IRComponent* component) {
    if (!component) return IR_COMPONENT_CONTAINER;  // Default fallback
    return component->type;
}

// Get component ID (helper for Lua FFI)
uint32_t ir_get_component_id(IRComponent* component) {
    if (!component) return 0;
    return component->id;
}

// Get child count (helper for Lua FFI)
uint32_t ir_get_child_count(IRComponent* component) {
    if (!component) return 0;
    return component->child_count;
}

// Get child at index (helper for Lua FFI)
IRComponent* ir_get_child_at(IRComponent* component, uint32_t index) {
    if (!component || index >= component->child_count) return NULL;
    return component->children[index];
}

// Set window metadata on IR context
void ir_set_window_metadata(int width, int height, const char* title) {
    if (!g_ir_context) {
        g_ir_context = ir_create_context();
        if (!g_ir_context) return;
    }

    // Create metadata if it doesn't exist
    if (!g_ir_context->metadata) {
        g_ir_context->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
        if (!g_ir_context->metadata) return;
    }

    // Set window dimensions
    g_ir_context->metadata->window_width = width;
    g_ir_context->metadata->window_height = height;

    // Set window title
    if (title) {
        if (g_ir_context->metadata->window_title) {
            free(g_ir_context->metadata->window_title);
        }
        g_ir_context->metadata->window_title = strdup(title);
    }
}
