/**
 * @file ir_component_factory.c
 * @brief Component creation and tree manipulation functions implementation
 *
 * This module provides component lifecycle and tree structure functions.
 * Extracted from ir_builder.c to reduce file size and improve organization.
 */

#define _GNU_SOURCE
#include "ir_component_factory.h"
#include "ir_builder.h"
#include "ir_core.h"
#include "ir_memory.h"
#include "ir_hashmap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Global IR context (defined in ir_builder.c)
extern IRContext* g_ir_context;

// Thread-local IRContext getter (defined in ir_instance.c)
extern IRContext* ir_context_get_current(void);

// Get the active IRContext (thread-local first, then global)
static IRContext* get_active_context(void) {
    IRContext* ctx = ir_context_get_current();
    return ctx ? ctx : g_ir_context;
}

// ir_layout_mark_dirty is declared in ir_core.h
extern void ir_layout_mark_dirty(IRComponent* component);

// TabGroupState cleanup (defined in ir_builder.c)
extern void ir_tabgroup_destroy_state(struct TabGroupState* state);

// ============================================================================
// Component Creation and Destruction
// ============================================================================

IRComponent* ir_create_component(IRComponentType type) {
    return ir_create_component_with_id(type, 0);
}

IRComponent* ir_create_component_with_id(IRComponentType type, uint32_t id) {
    IRComponent* component = NULL;

    // Use pool allocator if context exists
    if (g_ir_context && g_ir_context->component_pool) {
        component = ir_pool_alloc_component(g_ir_context->component_pool);
        if (component) {
            // Zero-initialize to clear any stale data from previous use
            memset(component, 0, sizeof(IRComponent));
            component->layout_cache.dirty = true;
        }
    } else {
        // Fallback to malloc if no context/pool
        component = malloc(sizeof(IRComponent));
        if (component) {
            memset(component, 0, sizeof(IRComponent));
            component->layout_cache.dirty = true;
        }
    }

    if (!component) return NULL;

    component->type = type;

    // Get active context (thread-local first, then global)
    IRContext* active_ctx = get_active_context();

    if (id == 0 && active_ctx) {
        component->id = active_ctx->next_component_id++;
    } else {
        component->id = id;
    }

    // Add to hash map for fast lookups
    if (active_ctx && active_ctx->component_map) {
        ir_map_insert(active_ctx->component_map, component);
    }

    // Initialize layout state for two-pass layout system
    component->layout_state = calloc(1, sizeof(IRLayoutState));
    if (component->layout_state) {
        component->layout_state->dirty = true;  // Start dirty, needs initial layout
        component->layout_state->dirty_flags = IR_DIRTY_LAYOUT;  // Consolidated dirty tracking
    }

    // Set flex direction based on component type
    if (type == IR_COMPONENT_ROW) {
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.direction = 1;  // Row = horizontal
            layout->display_explicit = true;  // Enable flex CSS output
            layout->mode = IR_LAYOUT_MODE_FLEX;
        }
    } else if (type == IR_COMPONENT_COLUMN) {
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.direction = 0;  // Column = vertical
            layout->display_explicit = true;  // Enable flex CSS output
            layout->mode = IR_LAYOUT_MODE_FLEX;
        }
    }

    return component;
}

void ir_destroy_component(IRComponent* component) {
    if (!component) return;

    // Destroy style
    if (component->style) {
        ir_destroy_style(component->style);
        component->style = NULL;
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

    // Destroy layout state
    if (component->layout_state) {
        free(component->layout_state);
        component->layout_state = NULL;
    }

    // Clean up TabGroupState if this component is a TabGroup
    // IMPORTANT: custom_data might be a JSON string (e.g., '{"selectedIndex":0}') from serialization,
    // NOT a TabGroupState pointer. Only treat as TabGroupState if it doesn't start with '{'
    if (component->type == IR_COMPONENT_TAB_GROUP && component->custom_data) {
        char* data_str = (char*)component->custom_data;
        if (data_str[0] == '{') {
            // This is a JSON string, not a TabGroupState - free it as a string
            free(component->custom_data);
            component->custom_data = NULL;
        } else {
            struct TabGroupState* state = (struct TabGroupState*)component->custom_data;

            // Clear panel references to prevent dangling pointers
            for (uint32_t i = 0; i < state->panel_count; i++) {
                state->panels[i] = NULL;
            }
            state->panel_count = 0;

            // Clear tab references
            for (uint32_t i = 0; i < state->tab_count; i++) {
                state->tabs[i] = NULL;
            }
            state->tab_count = 0;

            // Clear other references
            state->tab_bar = NULL;
            state->tab_content = NULL;
            state->group = NULL;

            // Free the state itself
            free(state);
            component->custom_data = NULL;
        }
    }

    // Free strings
    if (component->tag) free(component->tag);
    if (component->text_content) free(component->text_content);
    if (component->css_class) free(component->css_class);
    if (component->text_expression) free(component->text_expression);
    if (component->component_ref) free(component->component_ref);
    if (component->component_props) free(component->component_props);
    if (component->module_ref) free(component->module_ref);
    if (component->export_name) free(component->export_name);
    if (component->scope) free(component->scope);
    if (component->source_module) free(component->source_module);
    if (component->each_source) free(component->each_source);
    if (component->each_item_name) free(component->each_item_name);
    if (component->each_index_name) free(component->each_index_name);

    // Free custom_data based on component type
    if (component->custom_data) {
        switch (component->type) {
            case IR_COMPONENT_LINK: {
                typedef struct { char* url; char* title; char* target; char* rel; } IRLinkData;
                IRLinkData* link_data = (IRLinkData*)component->custom_data;
                if (link_data->url) free(link_data->url);
                if (link_data->title) free(link_data->title);
                if (link_data->target) free(link_data->target);
                if (link_data->rel) free(link_data->rel);
                free(link_data);
                break;
            }
            case IR_COMPONENT_CODE_BLOCK: {
                typedef struct { char* language; char* code; size_t length; } IRCodeBlockData;
                IRCodeBlockData* code_data = (IRCodeBlockData*)component->custom_data;
                if (code_data->language) free(code_data->language);
                if (code_data->code) free(code_data->code);
                free(code_data);
                break;
            }
            default:
                free(component->custom_data);
                break;
        }
        component->custom_data = NULL;
    }
    if (component->visible_condition) free(component->visible_condition);

    // Free tab data
    if (component->tab_data) {
        if (component->tab_data->title) free(component->tab_data->title);
        free(component->tab_data);
    }

    // Free text layout
    if (component->text_layout) {
        ir_text_layout_destroy(component->text_layout);
        component->text_layout = NULL;
    }

    // Free children array
    if (component->children) {
        free(component->children);
    }

    // Remove from hash map
    if (g_ir_context && g_ir_context->component_map) {
        ir_map_remove(g_ir_context->component_map, component->id);
    }

    // Return to pool or free
    // Check if component was externally allocated (via malloc/calloc in JSON deserializer)
    // If so, free it directly instead of returning to the pool
    if (component->is_externally_allocated) {
        free(component);
    } else if (g_ir_context && g_ir_context->component_pool) {
        ir_pool_free_component(g_ir_context->component_pool, component);
    } else {
        free(component);
    }
}

// ============================================================================
// Tree Structure Management
// ============================================================================

void ir_add_child(IRComponent* parent, IRComponent* child) {
    if (!parent || !child) return;

    // Exponential growth strategy (like std::vector) - 90% fewer reallocs
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        IRComponent** new_children = realloc(parent->children,
                                            sizeof(IRComponent*) * new_capacity);
        if (!new_children) return;  // Failed to allocate

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count] = child;
    parent->child_count++;
    child->parent = parent;

    // Mark parent dirty - children changed
    ir_layout_mark_dirty(parent);
    if (parent->layout_state) {
        parent->layout_state->dirty_flags |= IR_DIRTY_CHILDREN | IR_DIRTY_LAYOUT;
    }
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

    // Mark parent dirty - children changed
    ir_layout_mark_dirty(parent);
    if (parent->layout_state) {
        parent->layout_state->dirty_flags |= IR_DIRTY_CHILDREN | IR_DIRTY_LAYOUT;
    }
}

void ir_insert_child(IRComponent* parent, IRComponent* child, uint32_t index) {
    if (!parent || !child || index > parent->child_count) return;

    // OPTIMIZATION: Exponential growth strategy (90% fewer reallocs, same as ir_add_child)
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        IRComponent** new_children = realloc(parent->children,
                                            sizeof(IRComponent*) * new_capacity);
        if (!new_children) return;  // Failed to allocate

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

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
    // Try hash map first for O(1) lookup
    if (g_ir_context && g_ir_context->component_map) {
        IRComponent* result = ir_map_lookup(g_ir_context->component_map, id);
        if (result) {
            return result;
        }
    }

    // Tree traversal fallback
    if (!root) return NULL;

    if (root->id == id) return root;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = ir_find_component_by_id(root->children[i], id);
        if (found) return found;
    }

    return NULL;
}

// ============================================================================
// Component Property Setters
// ============================================================================

void ir_set_text_content(IRComponent* component, const char* text) {
    if (!component) return;

    if (component->text_content) {
        free(component->text_content);
    }
    component->text_content = text ? strdup(text) : NULL;

    // Invalidate text layout when content changes
    if (component->text_layout) {
        ir_text_layout_destroy(component->text_layout);
        component->text_layout = NULL;
    }

    // Mark component dirty - content changed (two-pass layout system)
    ir_layout_mark_dirty(component);
    if (component->layout_state) {
        component->layout_state->dirty_flags |= IR_DIRTY_CONTENT | IR_DIRTY_LAYOUT;
    }
}

void ir_set_custom_data(IRComponent* component, const char* data) {
    if (!component) return;

    if (component->custom_data) {
        free(component->custom_data);
    }
    component->custom_data = data ? strdup(data) : NULL;
}

void ir_set_tag(IRComponent* component, const char* tag) {
    if (!component) return;

    if (component->tag) {
        free(component->tag);
    }
    component->tag = tag ? strdup(tag) : NULL;
}

void ir_set_each_source(IRComponent* component, const char* source) {
    if (!component) return;

    if (component->each_source) {
        free(component->each_source);
    }
    component->each_source = source ? strdup(source) : NULL;
}

// ============================================================================
// Component State Helpers (Checkbox, Dropdown, etc.)
// ============================================================================

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
    printf("[CHECKBOX_TOGGLE] Component ID %u: %s -> %s\n",
           component->id,
           current ? "checked" : "unchecked",
           !current ? "checked" : "unchecked");
    ir_set_checkbox_state(component, !current);
}

// Dropdown state helpers - IRDropdownState is defined in ir_core.h
// Note: ir_get_dropdown_state is declared in ir_builder.h
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
