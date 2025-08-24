/**
 * @file runtime.c
 * @brief Kryon Runtime System Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Core runtime for loading and executing KRB files with element management,
 * reactive state, and event handling.
 */

#include "runtime.h"
#include "memory.h"
#include "krb_format.h"
#include "renderer_interface.h"
#include "color_utils.h"
#include "types.h"
#include "elements.h"
#include "shared/kryon_mappings.h"
#include "validation.h"
#include "../navigation/navigation.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

// Forward declaration from element_manager.c
bool kryon_element_set_property_by_name(KryonElement *element, const char *name, const void *value);

// Element registration is now handled automatically by element registry
#include <stdarg.h>

// Global runtime reference for variable resolution during rendering
static KryonRuntime* g_current_runtime = NULL;

// =============================================================================
// CURSOR MANAGEMENT WITH AUTOMATIC STATE TRACKING
// =============================================================================

KryonRenderResult kryon_renderer_set_cursor(KryonRenderer* renderer, KryonCursorType cursor_type) {
    if (!renderer || !renderer->vtable || !renderer->vtable->set_cursor) {
        return KRYON_RENDER_ERROR_UNSUPPORTED_OPERATION;
    }
    
    KryonRenderResult result = renderer->vtable->set_cursor(cursor_type);
    
    // Track that cursor was set this frame (only for non-default cursors)
    if (result == KRYON_RENDER_SUCCESS && g_current_runtime && cursor_type != KRYON_CURSOR_DEFAULT) {
        g_current_runtime->cursor_set_this_frame = true;
    }
    
    return result;
}

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void update_element_tree(KryonRuntime *runtime, KryonElement *element, double delta_time);
static void process_event_queue(KryonRuntime *runtime);
static bool runtime_event_handler(const KryonEvent* event, void* userData);
static void runtime_error(KryonRuntime *runtime, const char *format, ...);
static void kryon_debug_inspector_toggle(KryonRuntime *runtime);
const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name);
static const char* resolve_variable_reference(KryonRuntime* runtime, const char* value);
static const char* resolve_component_state_variable(KryonRuntime* runtime, KryonElement* element, const char* var_name);

// @for template expansion functions
static void expand_for_template(KryonRuntime* runtime, KryonElement* for_element);
static void expand_for_iteration(KryonRuntime* runtime, KryonElement* for_element, const char* var_name, const char* var_value, size_t index);
static void clear_expanded_children(KryonRuntime* runtime, KryonElement* for_element);
static KryonElement* clone_element_with_substitution(KryonRuntime* runtime, KryonElement* template_element, const char* var_name, const char* var_value, size_t index);
static char* substitute_template_variable(const char* template_str, const char* var_name, const char* var_value);
static void add_child_element(KryonRuntime* runtime, KryonElement* parent, KryonElement* child);

// External function from krb_loader.c
extern bool kryon_runtime_load_krb_data(KryonRuntime *runtime, const uint8_t *data, size_t size);

// Helper function to resolve template properties with @for variable context
static char* resolve_for_template_property(KryonRuntime* runtime, KryonProperty* property, 
                                         const char* var_name, const char* var_value);

// =============================================================================
// CONFIGURATION
// =============================================================================

KryonRuntimeConfig kryon_runtime_default_config(void) {
    KryonRuntimeConfig config = {
        .mode = KRYON_MODE_PRODUCTION,
        .enable_hot_reload = false,
        .enable_profiling = false,
        .enable_validation = true,
        .max_elements = 10000,
        .max_update_fps = 60,
        .resource_path = NULL,
        .platform_context = NULL
    };
    return config;
}

KryonRuntimeConfig kryon_runtime_dev_config(void) {
    KryonRuntimeConfig config = kryon_runtime_default_config();
    config.mode = KRYON_MODE_DEVELOPMENT;
    config.enable_hot_reload = true;
    config.enable_profiling = true;
    config.enable_validation = true;
    return config;
}

KryonRuntimeConfig kryon_runtime_prod_config(void) {
    KryonRuntimeConfig config = kryon_runtime_default_config();
    config.mode = KRYON_MODE_PRODUCTION;
    config.enable_hot_reload = false;
    config.enable_profiling = false;
    config.enable_validation = false;
    return config;
}

// =============================================================================
// RUNTIME LIFECYCLE
// =============================================================================

KryonRuntime *kryon_runtime_create(const KryonRuntimeConfig *config) {
    KryonRuntime *runtime = kryon_alloc(sizeof(KryonRuntime));
    if (!runtime) {
        return NULL;
    }
    
    // Initialize structure
    memset(runtime, 0, sizeof(KryonRuntime));
    
    // Set configuration
    if (config) {
        runtime->config = *config;
    } else {
        runtime->config = kryon_runtime_default_config();
    }
    
    // Initialize element type registry
    if (!element_registry_init()) {
        kryon_free(runtime);
        return NULL;
    }
    
    // Element registry automatically registers all available elements during init
    
    // Initialize element storage
    runtime->element_capacity = 256;
    runtime->elements = kryon_alloc(runtime->element_capacity * sizeof(KryonElement*));
    if (!runtime->elements) {
        element_registry_cleanup();
        kryon_free(runtime);
        return NULL;
    }
    
    // Initialize event system
    runtime->event_system = kryon_event_system_create(256);
    if (!runtime->event_system) {
        kryon_free(runtime->elements);
        kryon_free(runtime);
        return NULL;
    }
    
    // Initialize hit testing manager
    runtime->hit_test_manager = hit_test_manager_create();
    if (!runtime->hit_test_manager) {
        kryon_event_system_destroy(runtime->event_system);
        kryon_free(runtime->elements);
        kryon_free(runtime);
        return NULL;
    }
    
    // Register runtime event handlers for all event types
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_MOUSE_BUTTON_DOWN, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_MOUSE_MOVED, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_TEXT_INPUT, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_KEY_DOWN, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_KEY_UP, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_WINDOW_FOCUS, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_WINDOW_RESIZE, runtime_event_handler, runtime);
    
    // Initialize global state
    runtime->global_state = kryon_state_create("global", KRYON_STATE_OBJECT);
    if (!runtime->global_state) {
        kryon_event_system_destroy(runtime->event_system);
        kryon_free(runtime->elements);
        kryon_free(runtime);
        return NULL;
    }
    
    // Initialize script VM (prefer Lua if available)
    runtime->script_vm = NULL;
    if (kryon_vm_is_available(KRYON_VM_LUA)) {
        runtime->script_vm = kryon_vm_create(KRYON_VM_LUA, NULL);
        if (runtime->script_vm) {
            runtime->script_vm->runtime = runtime; // Set runtime reference for variable access
        }
    }
    
    // Initialize navigation manager only if needed (when there are link elements)
    runtime->navigation_manager = NULL; // Will be created lazily when first link is encountered
    
    // Initialize other fields
    runtime->next_element_id = 1;
    runtime->is_running = false;
    runtime->needs_update = true;
    runtime->is_loading = false;
    
    // Initialize input state
    runtime->mouse_position.x = 0.0f;
    runtime->mouse_position.y = 0.0f;
    
    // Initialize root variables with default viewport size
    kryon_runtime_set_variable(runtime, "root.width", "800");
    kryon_runtime_set_variable(runtime, "root.height", "600");
    
    return runtime;
}

void kryon_runtime_destroy(KryonRuntime *runtime) {
    if (!runtime) {
        return;
    }
    
    // Stop runtime if running
    if (runtime->is_running) {
        kryon_runtime_stop(runtime);
    }
    
    // Destroy all elements (with null check for safety)
    if (runtime->root) {
        kryon_element_destroy(runtime, runtime->root);
        runtime->root = NULL;
    }
    
    // Free element storage (with null check)
    if (runtime->elements) {
        kryon_free(runtime->elements);
        runtime->elements = NULL;
    }
    
    // Cleanup element type registry
    element_registry_cleanup();
    
    // Destroy event system (with null check)
    if (runtime->event_system) {
        kryon_event_system_destroy(runtime->event_system);
        runtime->event_system = NULL;
    }
    
    // Destroy hit testing manager (with null check)
    if (runtime->hit_test_manager) {
        hit_test_manager_destroy(runtime->hit_test_manager);
        runtime->hit_test_manager = NULL;
    }
    
    // Destroy global state (with null check)
    if (runtime->global_state) {
        kryon_state_destroy(runtime->global_state);
        runtime->global_state = NULL;
    }
    
    // Destroy script VM (with null check)
    if (runtime->script_vm) {
        kryon_vm_destroy(runtime->script_vm);
        runtime->script_vm = NULL;
    }
    
    // Destroy navigation manager (with null check)
    if (runtime->navigation_manager) {
        kryon_navigation_destroy(runtime->navigation_manager);
        runtime->navigation_manager = NULL;
    }
    
    // Free styles (with null check)
    if (runtime->styles) {
        kryon_free(runtime->styles);
        runtime->styles = NULL;
    }
    
    // Free error messages (with null check)
    if (runtime->error_messages) {
        for (size_t i = 0; i < runtime->error_count; i++) {
            if (runtime->error_messages[i]) {
                kryon_free(runtime->error_messages[i]);
            }
        }
        kryon_free(runtime->error_messages);
        runtime->error_messages = NULL;
    }

    // Free string table
    if (runtime->string_table) {
        for (size_t i = 0; i < runtime->string_table_count; i++) {
            kryon_free(runtime->string_table[i]);
        }
        kryon_free(runtime->string_table);
        runtime->string_table = NULL;
        runtime->string_table_count = 0;
    }
    
    // Free component registry
    if (runtime->components) {
        for (size_t i = 0; i < runtime->component_count; i++) {
            KryonComponentDefinition* comp = runtime->components[i];
            if (comp) {
                // Free component name
                kryon_free(comp->name);
                
                // Free parameters
                for (size_t j = 0; j < comp->parameter_count; j++) {
                    kryon_free(comp->parameters[j]);
                    kryon_free(comp->param_defaults[j]);
                }
                kryon_free(comp->parameters);
                kryon_free(comp->param_defaults);
                
                // Free state variables
                for (size_t j = 0; j < comp->state_count; j++) {
                    kryon_free(comp->state_vars[j].name);
                    kryon_free(comp->state_vars[j].default_value);
                }
                kryon_free(comp->state_vars);
                
                // Free functions
                if (comp->functions) {
                    for (size_t j = 0; j < comp->function_count; j++) {
                        kryon_free(comp->functions[j].name);
                        kryon_free(comp->functions[j].language);
                        kryon_free(comp->functions[j].bytecode);
                    }
                    kryon_free(comp->functions);
                }
                
                // Free UI template (if any)
                if (comp->ui_template) {
                    kryon_element_destroy(runtime, comp->ui_template);
                }
                
                kryon_free(comp);
            }
        }
        kryon_free(runtime->components);
    }
    
    // Cleanup input state (Phase 5)
    if (runtime->focused_input_id) {
        kryon_free(runtime->focused_input_id);
        runtime->focused_input_id = NULL;
    }
        
    kryon_free(runtime);
}

// =============================================================================
// KRB LOADING
// =============================================================================

bool kryon_runtime_load_file(KryonRuntime *runtime, const char *filename) {
    if (!runtime || !filename) {
        return false;
    }
    
    // Open file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        runtime_error(runtime, "Failed to open file: %s", filename);
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        runtime_error(runtime, "Invalid file size: %s", filename);
        return false;
    }
    
    // Read file into memory
    uint8_t *data = kryon_alloc(file_size);
    if (!data) {
        fclose(file);
        runtime_error(runtime, "Failed to allocate memory for file");
        return false;
    }
    
    size_t bytes_read = fread(data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        kryon_free(data);
        runtime_error(runtime, "Failed to read complete file");
        return false;
    }
    
    // Load from binary
    bool success = kryon_runtime_load_binary(runtime, data, file_size);
    kryon_free(data);
    
    return success;
}

bool kryon_runtime_load_binary(KryonRuntime *runtime, const uint8_t *data, size_t size) {
    if (!runtime || !data || size == 0) {
        return false;
    }
    
    // Use simple KRB loader instead of full KRB reader
    return kryon_runtime_load_krb_data(runtime, data, size);
}


// =============================================================================
// RUNTIME EXECUTION
// =============================================================================

bool kryon_runtime_start(KryonRuntime *runtime) {
    if (!runtime || runtime->is_running) {
        return false;
    }
    
    if (!runtime->root) {
        runtime_error(runtime, "No root element loaded");
        return false;
    }
    
    runtime->is_running = true;
    runtime->last_update_time = 0.0;
    runtime->stats.frame_count = 0;
    runtime->stats.total_time = 0.0;
    
    return true;
}

void kryon_runtime_stop(KryonRuntime *runtime) {
    if (!runtime || !runtime->is_running) {
        return;
    }
    
    runtime->is_running = false;
}

bool kryon_runtime_update(KryonRuntime *runtime, double delta_time) {
    if (!runtime || !runtime->is_running) {
        return false;
    }
    
    clock_t start_time = clock();
    
    // Update statistics
    runtime->update_delta = delta_time;
    runtime->stats.total_time += delta_time;
    runtime->stats.frame_count++;
    
    // Set global runtime for event processing
    g_current_runtime = runtime;

    // Process event queue
    process_event_queue(runtime);

    // Clear global runtime after event processing
    g_current_runtime = NULL;
    
    // Update element tree
    if (runtime->needs_update && runtime->root) {
        update_element_tree(runtime, runtime->root, delta_time);
        runtime->needs_update = false;
    }
    
    // Update timing statistics
    clock_t end_time = clock();
    double update_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    runtime->stats.update_time += update_time;
    
    return true;
}

bool kryon_runtime_render(KryonRuntime *runtime) {
    if (!runtime || !runtime->is_running || !runtime->root) {
        return false;
    }
    
    clock_t start_time = clock();
    
    // Generate render commands from element tree (regardless of renderer)
    if (runtime->root) {
        // Create render command array
        KryonRenderCommand commands[256]; // Stack allocation for performance
        size_t command_count = 0;
        
        // Generate render commands from element tree
        
        // Safety check before calling render function
        if (!runtime->root->type_name) {
            return false;
        }
                
        // Reset cursor tracking flag at start of frame
        runtime->cursor_set_this_frame = false;
        
        // Set global runtime for variable resolution during rendering
        g_current_runtime = runtime;
        
        // Generate render commands from element tree
        kryon_element_tree_to_render_commands(runtime, runtime->root, commands, &command_count, 256);
        
        // Reset cursor to default if no interactive element set it this frame
        if (!runtime->cursor_set_this_frame && runtime->renderer) {
            kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_DEFAULT);
        }
        
        // Clear global runtime after rendering
        g_current_runtime = NULL;
        
        
        // Check if we have a renderer attached for actual rendering
        if (runtime->renderer) {
            // Cast renderer and use the minimal interface
            KryonRenderer* renderer = (KryonRenderer*)runtime->renderer;
            
            // Begin frame
            KryonRenderContext* context = NULL;
            KryonColor clear_color = {0.1f, 0.1f, 0.1f, 1.0f}; // Dark background
            
            if (renderer->vtable) {
                if (renderer->vtable->begin_frame) {
                    KryonRenderResult result = renderer->vtable->begin_frame(&context, clear_color);
                    if (result == KRYON_RENDER_ERROR_SURFACE_LOST) {
                        return false; // Graceful shutdown
                    } else if (result != KRYON_RENDER_SUCCESS) {
                        return false;
                    }
                }
            }
            
            // Execute render commands if we have any
            if (command_count > 0 && renderer->vtable && renderer->vtable->execute_commands) {
                KryonRenderResult result = renderer->vtable->execute_commands(context, commands, command_count);
                if (result != KRYON_RENDER_SUCCESS) {
                    return false;
                }
            }
                        
            // End frame
            if (renderer->vtable && renderer->vtable->end_frame) {
                KryonRenderResult result = renderer->vtable->end_frame(context);
                if (result != KRYON_RENDER_SUCCESS) {
                    return false;
                }
            }
        }
    }
    
    // Update timing statistics
    clock_t end_time = clock();
    double render_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    runtime->stats.render_time += render_time;
    runtime->stats.frame_count++;
    
    // Reset per-frame input state at end of frame
    runtime->mouse_clicked_this_frame = false;
    
    return true;
}

// =============================================================================
// ELEMENT MANAGEMENT
// =============================================================================

KryonElement *kryon_element_create(KryonRuntime *runtime, uint16_t type, KryonElement *parent) {
    if (!runtime) {
        return NULL;
    }
    
    KryonElement *element = kryon_alloc(sizeof(KryonElement));
    if (!element) {
        return NULL;
    }
    
    // Initialize element
    memset(element, 0, sizeof(KryonElement));
    element->id = runtime->next_element_id++;
    element->type = type;
    element->state = KRYON_ELEMENT_STATE_CREATED;
    element->visible = true;
    element->enabled = true;
    element->needs_layout = true;
    element->needs_render = true;
    
    // Add to parent
    if (parent) {
        element->parent = parent;
        
        // Expand parent's children array if needed
        if (parent->child_count >= parent->child_capacity) {
            size_t new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
            KryonElement **new_children = kryon_realloc(parent->children,
                                                       new_capacity * sizeof(KryonElement*));
            if (!new_children) {
                kryon_free(element);
                return NULL;
            }
            parent->children = new_children;
            parent->child_capacity = new_capacity;
        }
        
        parent->children[parent->child_count++] = element;
    }
    
    // Add to registry
    if (runtime->element_count >= runtime->element_capacity) {
        size_t new_capacity = runtime->element_capacity * 2;
        KryonElement **new_elements = kryon_realloc(runtime->elements,
                                                   new_capacity * sizeof(KryonElement*));
        if (!new_elements) {
            kryon_free(element);
            return NULL;
        }
        runtime->elements = new_elements;
        runtime->element_capacity = new_capacity;
    }
    
    runtime->elements[runtime->element_count++] = element;
    
    return element;
}

void kryon_element_destroy(KryonRuntime *runtime, KryonElement *element) {
    if (!runtime || !element) {
        return;
    }
    
    // Set state to unmounting
    element->state = KRYON_ELEMENT_STATE_UNMOUNTING;
    
    // Destroy children first
    while (element->child_count > 0) {
        kryon_element_destroy(runtime, element->children[element->child_count - 1]);
    }
    
    // Remove from parent
    if (element->parent) {
        KryonElement *parent = element->parent;
        for (size_t i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == element) {
                // Shift remaining children
                for (size_t j = i + 1; j < parent->child_count; j++) {
                    parent->children[j - 1] = parent->children[j];
                }
                parent->child_count--;
                break;
            }
        }
    }
    
    // Remove from registry
    for (size_t i = 0; i < runtime->element_count; i++) {
        if (runtime->elements[i] == element) {
            // Shift remaining elements
            for (size_t j = i + 1; j < runtime->element_count; j++) {
                runtime->elements[j - 1] = runtime->elements[j];
            }
            runtime->element_count--;
            break;
        }
    }
    
    // Free element data
    kryon_free(element->type_name);
    kryon_free(element->element_id);
    kryon_free(element->children);
    
    // Free properties
    if (element->properties) {
        for (size_t i = 0; i < element->property_count; i++) {
            KryonProperty *prop = element->properties[i];
            if (prop) {
                kryon_free(prop->name);
                if (prop->type == KRYON_RUNTIME_PROP_STRING && prop->value.string_value) {
                    kryon_free(prop->value.string_value);
                }
                kryon_free(prop);
            }
        }
        kryon_free(element->properties);
    }
    
    // Free event handlers
    if (element->handlers) {
        kryon_free(element->handlers);
    }
    
    // Free class names
    if (element->class_names) {
        for (size_t i = 0; i < element->class_count; i++) {
            kryon_free(element->class_names[i]);
        }
        kryon_free(element->class_names);
    }

    // Free @for directive template storage
    if (element->template_children) {
        // Note: template children are references to actual elements that will be freed
        // through normal element destruction, so we only free the array itself
        free(element->template_children);
        element->template_children = NULL;
        element->template_count = 0;
    }
    
    // Notify the script VM that this element is gone
    if (runtime->script_vm) {
        kryon_vm_notify_element_destroyed(runtime->script_vm, element);
    }
    
    // Set state to destroyed
    element->state = KRYON_ELEMENT_STATE_DESTROYED;
    
    kryon_free(element);
}

KryonElement *kryon_element_find_by_id(KryonRuntime *runtime, const char *element_id) {
    if (!runtime || !element_id) {
        return NULL;
    }
    
    for (size_t i = 0; i < runtime->element_count; i++) {
        KryonElement *element = runtime->elements[i];
        if (element && element->element_id && 
            strcmp(element->element_id, element_id) == 0) {
            return element;
        }
    }
    
    return NULL;
}

void kryon_element_invalidate_layout(KryonElement *element) {
    if (!element) {
        return;
    }
    
    element->needs_layout = true;
    
    // Propagate to parent
    if (element->parent) {
        kryon_element_invalidate_layout(element->parent);
    }
}

void kryon_element_invalidate_render(KryonElement *element) {
    if (!element) {
        return;
    }
    
    element->needs_render = true;
    
    // Propagate to parent
    if (element->parent) {
        kryon_element_invalidate_render(element->parent);
    }
}

void mark_elements_for_rerender(KryonElement *element) {
    if (!element) {
        return;
    }
    
    element->needs_render = true;
    
    // Recursively mark all children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            mark_elements_for_rerender(element->children[i]);
        }
    }
}

// =============================================================================
// STATE MANAGEMENT
// =============================================================================

KryonState *kryon_state_create(const char *key, KryonStateType type) {
    KryonState *state = kryon_alloc(sizeof(KryonState));
    if (!state) {
        return NULL;
    }
    
    memset(state, 0, sizeof(KryonState));
    state->key = kryon_strdup(key);
    state->type = type;
    
    // Initialize based on type
    switch (type) {
        case KRYON_STATE_ARRAY:
            state->value.array.capacity = 4;
            state->value.array.items = kryon_alloc(state->value.array.capacity * sizeof(KryonState*));
            break;
            
        case KRYON_STATE_OBJECT:
            state->value.object.capacity = 4;
            state->value.object.properties = kryon_alloc(state->value.object.capacity * sizeof(KryonState*));
            break;
            
        default:
            break;
    }
    
    return state;
}

void kryon_state_destroy(KryonState *state) {
    if (!state) {
        return;
    }
    
    kryon_free(state->key);
    
    // Free value based on type
    switch (state->type) {
        case KRYON_STATE_STRING:
            kryon_free(state->value.string_value);
            break;
            
        case KRYON_STATE_ARRAY:
            if (state->value.array.items) {
                for (size_t i = 0; i < state->value.array.count; i++) {
                    kryon_state_destroy(state->value.array.items[i]);
                }
                kryon_free(state->value.array.items);
            }
            break;
            
        case KRYON_STATE_OBJECT:
            if (state->value.object.properties) {
                for (size_t i = 0; i < state->value.object.count; i++) {
                    kryon_state_destroy(state->value.object.properties[i]);
                }
                kryon_free(state->value.object.properties);
            }
            break;
            
        default:
            break;
    }
    
    // Free observers
    kryon_free(state->observers);
    
    kryon_free(state);
}

// =============================================================================
// EVENT HANDLING
// =============================================================================

bool kryon_runtime_handle_event(KryonRuntime *runtime, const KryonEvent *event) {
    if (!runtime || !event || !runtime->event_system) {
        return false;
    }
    
    return kryon_event_push(runtime->event_system, event);
}

void runtime_receive_input_event(const KryonEvent* event, void* userData) {
    KryonRuntime* runtime = (KryonRuntime*)userData;
    if (!runtime || !runtime->event_system) {
        return;
    }
    
    // Push event directly to runtime's event system
    kryon_event_push(runtime->event_system, event);
}

static bool runtime_event_handler(const KryonEvent* event, void* userData) {
    KryonRuntime* runtime = (KryonRuntime*)userData;
    if (!runtime || !event) {
        return false;
    }


    if (runtime->hit_test_manager && runtime->root) {
        hit_test_process_input_event(runtime->hit_test_manager, runtime, runtime->root, event);
    }
    
    if (event->type == KRYON_EVENT_MOUSE_MOVED) {
        runtime->mouse_position.x = event->data.mouseMove.x;
        runtime->mouse_position.y = event->data.mouseMove.y;
    } else if (event->type == KRYON_EVENT_WINDOW_RESIZE) {
        // Update root variables when window resizes
        float new_width = (float)event->data.windowResize.width;
        float new_height = (float)event->data.windowResize.height;
        
        kryon_runtime_update_viewport_size(runtime, new_width, new_height);
    } else if (event->type == KRYON_EVENT_KEY_DOWN) {
        // Handle Ctrl+I for debug inspector
        if (event->data.key.ctrlPressed && event->data.key.keyCode == 73) { // 73 = 'I' key
            kryon_debug_inspector_toggle(runtime);
            return true; // Event handled
        }
    }

    return false;
}

static void process_event_queue(KryonRuntime *runtime) {
    if (!runtime || !runtime->event_system) {
        return;
    }
    
    kryon_event_process_all(runtime->event_system);
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

static void update_element_tree(KryonRuntime *runtime, KryonElement *element, double delta_time) {
    if (!element) {
        return;
    }
    
    // Update element state
    switch (element->state) {
        case KRYON_ELEMENT_STATE_CREATED:
            element->state = KRYON_ELEMENT_STATE_MOUNTING;
            element->state = KRYON_ELEMENT_STATE_MOUNTED;
            break;
            
        case KRYON_ELEMENT_STATE_MOUNTED:
            if (element->needs_layout || element->needs_render) {
                element->state = KRYON_ELEMENT_STATE_UPDATING;
                element->state = KRYON_ELEMENT_STATE_MOUNTED;
            }
            break;
            
        default:
            break;
    }
    
    // Update children
    for (size_t i = 0; i < element->child_count; i++) {
        update_element_tree(runtime, element->children[i], delta_time);
    }
}

static void runtime_error(KryonRuntime *runtime, const char *format, ...) {
    if (!runtime || !format) {
        return;
    }
    
    // Expand error array if needed
    if (runtime->error_count >= runtime->error_capacity) {
        size_t new_capacity = runtime->error_capacity ? runtime->error_capacity * 2 : 8;
        char **new_messages = kryon_realloc(runtime->error_messages,
                                           new_capacity * sizeof(char*));
        if (!new_messages) {
            return;
        }
        runtime->error_messages = new_messages;
        runtime->error_capacity = new_capacity;
    }
    
    // Format error message
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    runtime->error_messages[runtime->error_count++] = kryon_strdup(buffer);
}

// =============================================================================
// PUBLIC API IMPLEMENTATIONS
// =============================================================================

const char **kryon_runtime_get_errors(const KryonRuntime *runtime, size_t *out_count) {
    if (!runtime || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    *out_count = runtime->error_count;
    return (const char**)runtime->error_messages;
}

void kryon_runtime_clear_errors(KryonRuntime *runtime) {
    if (!runtime) {
        return;
    }
    
    for (size_t i = 0; i < runtime->error_count; i++) {
        kryon_free(runtime->error_messages[i]);
    }
    runtime->error_count = 0;
}

// =============================================================================
// AUTO-SIZING SYSTEM
// =============================================================================
// Note: auto_size_element function moved after property access functions

// =============================================================================
// ELEMENT TO RENDER COMMAND CONVERSION
// =============================================================================

// =============================================================================
// COMPREHENSIVE PROPERTY ACCESS SYSTEM
// =============================================================================

// Generic property finder
KryonProperty* find_element_property(KryonElement* element, const char* prop_name) {
    if (!element || !prop_name) return NULL;
    
    // Critical safety check: verify properties array exists when property_count > 0
    if (element->property_count > 0 && !element->properties) {
        return NULL;
    }
    
    // Bounds check: prevent excessive property counts
    if (element->property_count > 1000) {
        return NULL;
    }
    
    // Use kryon mappings to resolve aliases - get the canonical name from the alias
    uint16_t prop_hex = kryon_get_property_hex(prop_name);
    const char* canonical_name = NULL;
    if (prop_hex != 0) {
        canonical_name = kryon_get_property_name(prop_hex);
    }
    
    // Search for the property using both the original name and canonical name
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty* prop = element->properties[i];
        if (prop && prop->name) {
            // First try exact match with the requested name
            if (strcmp(prop->name, prop_name) == 0) {
                return prop;
            }
            // Then try canonical name if different
            if (canonical_name && strcmp(prop->name, canonical_name) == 0) {
                return prop;
            }
        }
    }
    return NULL;
}

// Resolve variable references in string values
static const char* resolve_variable_reference(KryonRuntime* runtime, const char* value) {
    if (!runtime || !value) {
        return value; // Invalid input
    }
    
    // With the compiler fix, we should now receive clean variable names directly
    // Just do a direct lookup - no string manipulation needed
    const char* resolved_value = kryon_runtime_get_variable(runtime, value);
    if (resolved_value) {
        // Add basic validation to ensure the pointer is reasonable
        if (resolved_value < (const char*)0x1000) {
            return value; // Return original if pointer is invalid
        }
        
        return resolved_value;
    }
    
    return value; // Return original if not found
}


// Resolve component state variable
static const char* resolve_component_state_variable(KryonRuntime* runtime, KryonElement* element, const char* var_name) {
    if (!runtime || !element || !var_name) {
        return NULL;
    }
    
    // First, check if the current element has a component instance
    KryonElement* current = element;
    while (current) {
        
        if (current->component_instance) {
            KryonComponentInstance* comp_inst = current->component_instance;
            
            // Check state variables
            if (comp_inst->definition && comp_inst->definition->state_vars) {
                for (size_t i = 0; i < comp_inst->definition->state_count; i++) {
                    if (comp_inst->definition->state_vars[i].name && 
                        strcmp(comp_inst->definition->state_vars[i].name, var_name) == 0) {
                        
                        // Found state variable, return current instance value
                        if (i < comp_inst->state_count && comp_inst->state_values[i]) {
                            return comp_inst->state_values[i];
                        } else if (comp_inst->definition->state_vars[i].default_value) {
                            return comp_inst->definition->state_vars[i].default_value;
                        }
                        return "0"; // Default fallback
                    }
                }
            }
            
            // Check component parameters
            if (comp_inst->definition && comp_inst->definition->parameters) {
                for (size_t i = 0; i < comp_inst->definition->parameter_count; i++) {
                    if (comp_inst->definition->parameters[i] && 
                        strcmp(comp_inst->definition->parameters[i], var_name) == 0) {
                        
                        // Found parameter, return current instance value
                        if (i < comp_inst->param_count && comp_inst->param_values[i]) {
                            return comp_inst->param_values[i];
                        } else if (comp_inst->definition->param_defaults[i]) {
                            return comp_inst->definition->param_defaults[i];
                        }
                        return ""; // Default fallback
                    }
                }
            }
        }
        
        // Move up the element tree to check parent components
        current = current->parent;
    }
    
    return NULL; // Not found
}

// Get property value as string with type conversion
const char* get_element_property_string(KryonElement* element, const char* prop_name) {
    return get_element_property_string_with_runtime(g_current_runtime, element, prop_name);
}

// Get property value as string with runtime for variable resolution
const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) {
        // Property not found - this is normal for optional properties
        return NULL;
    }
    
    // Use simple static buffer for string conversions
    static char buffer[1024];
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            // For literal strings, just return the value directly
            return prop->value.string_value ? prop->value.string_value : "";
            
        case KRYON_RUNTIME_PROP_REFERENCE:
            // For variable references, try to resolve them
            if (prop->is_bound && prop->binding_path && runtime) {
                printf("🔍 RESOLVING: Looking up variable '%s'\n", prop->binding_path);
                const char* resolved_value = resolve_variable_reference(runtime, prop->binding_path);
                if (resolved_value && resolved_value != prop->binding_path) {  // Check it actually resolved
                    printf("✅ RESOLVED: Variable '%s' = '%s'\n", prop->binding_path, resolved_value);
                    return resolved_value;
                } else {
                    printf("❌ FAILED: Variable '%s' not found, using fallback\n", prop->binding_path);
                }
            } else {
                printf("❌ BINDING: Property not properly bound (is_bound=%d, binding_path=%p, runtime=%p)\n", 
                       prop->is_bound, (void*)prop->binding_path, (void*)runtime);
            }
            return prop->value.string_value ? prop->value.string_value : "";
            
        case KRYON_RUNTIME_PROP_INTEGER:
            snprintf(buffer, 1024, "%lld", (long long)prop->value.int_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_FLOAT:
            snprintf(buffer, 1024, "%.6f", prop->value.float_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value ? "true" : "false";
            
        case KRYON_RUNTIME_PROP_COLOR:
            snprintf(buffer, 1024, "#%08X", prop->value.color_value);
            return buffer;
        
        case KRYON_RUNTIME_PROP_FUNCTION:
            return prop->value.string_value;
        
        case KRYON_RUNTIME_PROP_AST_EXPRESSION:
            // Evaluate the AST expression and convert to string
            if (prop->value.ast_expression && runtime) {
                KryonExpressionValue result = kryon_runtime_evaluate_expression(prop->value.ast_expression, runtime);
                char *result_str = kryon_expression_value_to_string(&result);
                if (result_str) {
                    snprintf(buffer, 1024, "%s", result_str);
                    free(result_str);
                    kryon_expression_value_free(&result);
                    return buffer;
                }
                kryon_expression_value_free(&result);
            }
            return "";
        
        default:
            return NULL;
    }
}


// Get property value as integer with type conversion
// Get array property helper function
const char** get_element_property_array(KryonElement* element, const char* prop_name, size_t* count) {
    if (!element || !prop_name || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty* prop = element->properties[i];
        if (prop && prop->name && strcmp(prop->name, prop_name) == 0) {
            if (prop->type == KRYON_RUNTIME_PROP_ARRAY) {
                *count = prop->value.array_value.count;
                return (const char**)prop->value.array_value.values;
            }
            break;
        }
    }
    
    *count = 0;
    return NULL;
}

int get_element_property_int(KryonElement* element, const char* prop_name, int default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return default_value;
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_INTEGER:
            return (int)prop->value.int_value;
            
        case KRYON_RUNTIME_PROP_FLOAT:
            return (int)prop->value.float_value;
            
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value ? 1 : 0;
            
        case KRYON_RUNTIME_PROP_STRING:
            if (prop->value.string_value) {
                return atoi(prop->value.string_value);
            }
            return default_value;
            
        case KRYON_RUNTIME_PROP_COLOR:
            return (int)prop->value.color_value;
        
        case KRYON_RUNTIME_PROP_AST_EXPRESSION:
            // Evaluate expression and convert to integer
            if (prop->value.ast_expression && g_current_runtime) {
                KryonExpressionValue result = kryon_runtime_evaluate_expression(prop->value.ast_expression, g_current_runtime);
                int int_result = (int)kryon_expression_value_to_number(&result);
                kryon_expression_value_free(&result);
                return int_result;
            }
            return default_value;
            
        default:
            return default_value;
    }
}

// Get property value as float with type conversion
float get_element_property_float(KryonElement* element, const char* prop_name, float default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) {
        // if (strcmp(prop_name, "posX") == 0 || strcmp(prop_name, "posY") == 0) {
        //     printf("🚨 Property '%s' not found! Using default %.1f\n", prop_name, default_value);
        // }
        return default_value;
    }
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_FLOAT:
            return (float)prop->value.float_value;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            return (float)prop->value.int_value;
            
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value ? 1.0f : 0.0f;
            
        case KRYON_RUNTIME_PROP_STRING:
            if (prop->value.string_value) {
                return atof(prop->value.string_value);
            }
            return default_value;
        
        case KRYON_RUNTIME_PROP_AST_EXPRESSION:
            // Evaluate expression and convert to float
            if (prop->value.ast_expression && g_current_runtime) {
                KryonExpressionValue result = kryon_runtime_evaluate_expression(prop->value.ast_expression, g_current_runtime);
                float float_result = (float)kryon_expression_value_to_number(&result);
                kryon_expression_value_free(&result);
                return float_result;
            }
            return default_value;
            
        default:
            return default_value;
    }
}

// Get property value as boolean with type conversion
bool get_element_property_bool(KryonElement* element, const char* prop_name, bool default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return default_value;
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            return prop->value.int_value != 0;
            
        case KRYON_RUNTIME_PROP_FLOAT:
            return prop->value.float_value != 0.0;
            
        case KRYON_RUNTIME_PROP_STRING:
            if (prop->value.string_value) {
                return strcmp(prop->value.string_value, "true") == 0 ||
                       strcmp(prop->value.string_value, "1") == 0 ||
                       strcmp(prop->value.string_value, "yes") == 0;
            }
            return default_value;
        
        case KRYON_RUNTIME_PROP_AST_EXPRESSION:
            // Evaluate expression and convert to boolean
            if (prop->value.ast_expression && g_current_runtime) {
                KryonExpressionValue result = kryon_runtime_evaluate_expression(prop->value.ast_expression, g_current_runtime);
                bool bool_result = kryon_expression_value_to_boolean(&result);
                kryon_expression_value_free(&result);
                return bool_result;
            }
            return default_value;
            
        default:
            return default_value;
    }
}

// Get property value as color (32-bit RGBA)
uint32_t get_element_property_color(KryonElement* element, const char* prop_name, uint32_t default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return default_value;
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_COLOR:
            return prop->value.color_value;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            return (uint32_t)prop->value.int_value;
            
        case KRYON_RUNTIME_PROP_STRING:
            if (prop->value.string_value) {
                // Parse hex color strings like "#FF0000" or "0xFF0000"
                const char* str = prop->value.string_value;
                if (str[0] == '#') str++;
                if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) str += 2;
                
                unsigned int color;
                if (sscanf(str, "%x", &color) == 1) {
                    return (uint32_t)color;
                }
            }
            return default_value;
            
        default:
            return default_value;
    }
}


/// Auto-size an element based on its content (universal sizing system)
void auto_size_element(KryonElement* element, float* width, float* height) {
    if (!element) return;
    
    // Get text and font size for sizing calculations
    const char* text = get_element_property_string(element, "text");
    float font_size = get_element_property_float(element, "fontSize", 16.0f); // Default base size
    
    // Access renderer through global runtime for text measurement
    KryonRenderer* renderer = g_current_runtime ? (KryonRenderer*)g_current_runtime->renderer : NULL;
    
    // Determine element type and calculate appropriate sizes
    if (element->type_name) {
        if (strcmp(element->type_name, "Button") == 0) {
            font_size = get_element_property_float(element, "fontSize", 20.0f); // Buttons default to larger text
            
            if (*width < 0 && text && renderer && renderer->vtable && renderer->vtable->measure_text_width) {
                float text_width = renderer->vtable->measure_text_width(text, font_size);
                *width = text_width + 24.0f; // Text width + padding (12px each side)
            } else if (*width < 0) {
                // Fallback sizing if no measurement available
                *width = text ? strlen(text) * font_size * 0.55f + 24.0f : 150.0f;
            }
            
            if (*height < 0) {
                *height = font_size + 20.0f; // Font height + padding (10px top/bottom)
            }
            
        } else if (strcmp(element->type_name, "Text") == 0) {
            font_size = get_element_property_float(element, "fontSize", 16.0f); // Text elements default size
            
            if (*width < 0 && text && renderer && renderer->vtable && renderer->vtable->measure_text_width) {
                *width = renderer->vtable->measure_text_width(text, font_size);
            } else if (*width < 0) {
                // Fallback sizing
                *width = text ? strlen(text) * font_size * 0.6f : 100.0f;
            }
            
            if (*height < 0) {
                *height = font_size * 1.2f; // Font height + small padding
            }
            
        } else if (strcmp(element->type_name, "Input") == 0) {
            font_size = get_element_property_float(element, "fontSize", 14.0f); // Input elements smaller text
            
            if (*width < 0) {
                *width = 200.0f; // Standard input width
            }
            if (*height < 0) {
                *height = font_size + 16.0f; // Font height + padding
            }
            
        } else if (strcmp(element->type_name, "Image") == 0) {
            // Images have different sizing logic - use intrinsic image dimensions if available
            if (*width < 0) *width = 100.0f;   // Default image size
            if (*height < 0) *height = 100.0f; 
            
        } else {
            // Default sizing for unknown elements
            if (*width < 0) *width = 100.0f;
            if (*height < 0) *height = 50.0f;
        }
    }
    
    // Apply minimum sizes to prevent elements from being too small
    if (*width > 0 && *width < 20.0f) *width = 20.0f;
    if (*height > 0 && *height < 16.0f) *height = 16.0f;
}


// Helper function to parse color string to KryonColor
static KryonColor parse_color_string(const char* color_str) {
    KryonColor color = {1.0f, 1.0f, 1.0f, 1.0f}; // Default white
    
    if (!color_str) return color;
    
    // Handle common color names
    if (strcmp(color_str, "red") == 0) {
        color = (KryonColor){1.0f, 0.0f, 0.0f, 1.0f};
    } else if (strcmp(color_str, "green") == 0) {
        color = (KryonColor){0.0f, 1.0f, 0.0f, 1.0f};
    } else if (strcmp(color_str, "blue") == 0) {
        color = (KryonColor){0.0f, 0.0f, 1.0f, 1.0f};
    } else if (strcmp(color_str, "yellow") == 0) {
        color = (KryonColor){1.0f, 1.0f, 0.0f, 1.0f};
    } else if (strcmp(color_str, "black") == 0) {
        color = (KryonColor){0.0f, 0.0f, 0.0f, 1.0f};
    } else if (strcmp(color_str, "white") == 0) {
        color = (KryonColor){1.0f, 1.0f, 1.0f, 1.0f};
    } else if (color_str[0] == '#' && strlen(color_str) >= 7) {
        // Parse hex color #RRGGBB or #RRGGBBAA
        unsigned int hex_color = 0;
        sscanf(color_str + 1, "%x", &hex_color);
        
        if (strlen(color_str) >= 9) { // #RRGGBBAA
            color.r = ((hex_color >> 24) & 0xFF) / 255.0f;
            color.g = ((hex_color >> 16) & 0xFF) / 255.0f;
            color.b = ((hex_color >> 8) & 0xFF) / 255.0f;
            color.a = (hex_color & 0xFF) / 255.0f;
        } else { // #RRGGBB
            color.r = ((hex_color >> 16) & 0xFF) / 255.0f;
            color.g = ((hex_color >> 8) & 0xFF) / 255.0f;
            color.b = (hex_color & 0xFF) / 255.0f;
            color.a = 1.0f;
        }
    }
    
    return color;
}

// Helper function to calculate element height based on its type and properties
static float calculate_element_height(KryonElement* element) {
    if (!element || !element->type_name) {
        return 50.0f; // Default fallback
    }
    
    // Check if element has explicit height property
    KryonProperty* height_prop = find_element_property(element, "height");
    if (height_prop) {
        return height_prop->value.float_value;
    }
    
    // Calculate height based on element type
    if (strcmp(element->type_name, "Text") == 0) {
        float font_size = get_element_property_float(element, "fontSize", 16.0f);
        float line_height = get_element_property_float(element, "lineHeight", 1.4f); // Default line-height ratio
        
        // If lineHeight is <= 2, treat it as a multiplier; otherwise treat as absolute pixels
        float calculated_height;
        if (line_height <= 2.0f) {
            calculated_height = font_size * line_height;
        } else {
            calculated_height = line_height;
        }
        
        // Use just the calculated line height without extra padding
        return calculated_height;
    }
    
    // For Row elements, calculate height based on tallest child
    if (strcmp(element->type_name, "Row") == 0) {
        float max_child_height = 0.0f;
        for (size_t i = 0; i < element->child_count; i++) {
            KryonElement* child = element->children[i];
            if (child) {
                float child_height = calculate_element_height(child);
                if (child_height > max_child_height) {
                    max_child_height = child_height;
                }
            }
        }
        return max_child_height > 0.0f ? max_child_height : 50.0f;
    }
    
    // For other elements, use explicit height or reasonable defaults
    if (strcmp(element->type_name, "Container") == 0) {
        return get_element_property_float(element, "height", 100.0f);
    } else if (strcmp(element->type_name, "Button") == 0) {
        return get_element_property_float(element, "height", 40.0f);
    } else if (strcmp(element->type_name, "Input") == 0) {
        return get_element_property_float(element, "height", 32.0f);
    }
    
    return 50.0f; // Default for unknown types
}

// Helper function to render a single element based on its type
static void render_element(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !element->type_name) return;
    
    KryonRuntime* runtime = g_current_runtime; // Use global runtime set during rendering
    
    // Try new registry system first
    if (element_render_via_registry(runtime, element, commands, command_count, max_commands)) {
        return; // Successfully rendered via registry
    }
    
    // Fallback to old dispatch system
    static const struct {
        const char* type_name;
        void (*render_func)(KryonRuntime*, KryonElement*, KryonRenderCommand*, size_t*, size_t);
    } element_renderers[] = {
        // All elements moved to new VTable system
    };
    
    // Find and call the appropriate render function
    for (size_t i = 0; i < sizeof(element_renderers) / sizeof(element_renderers[0]); i++) {
        if (strcmp(element->type_name, element_renderers[i].type_name) == 0) {
            element_renderers[i].render_func(runtime, element, commands, command_count, max_commands);
            break;
        }
    }
}

// Helper function to process layout elements
static void process_layout(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !element->type_name) return;
    
    KryonRuntime* runtime = g_current_runtime; // Use global runtime set during rendering
    
    // Try new registry system first (layout elements can also be registered)
    if (element_render_via_registry(runtime, element, commands, command_count, max_commands)) {
        return; // Successfully processed via registry
    }
    
    // Fallback to old layout dispatch system
    static const struct {
        const char* type_name;
        void (*layout_func)(KryonRuntime*, KryonElement*, KryonRenderCommand*, size_t*, size_t);
    } layout_processors[] = {
        // All layout elements moved to new VTable system
    };
    
    // Find and call the appropriate layout function
    for (size_t i = 0; i < sizeof(layout_processors) / sizeof(layout_processors[0]); i++) {
        if (strcmp(element->type_name, layout_processors[i].type_name) == 0) {
            layout_processors[i].layout_func(runtime, element, commands, command_count, max_commands);
            break;
        }
    }
}

// Recursive function to convert element tree to render commands
static void element_to_commands_recursive(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !commands || !command_count || *command_count >= max_commands) return;
    
    // Safety checks
    if (!element->type_name || 
        element->property_count > 1000 || element->child_count > 1000 ||
        (element->property_count > 0 && !element->properties) ||
        (element->child_count > 0 && !element->children)) {
        return;
    }
    
    // Track recursion depth
    static int recursive_depth = 0;
    if (recursive_depth > 100) {
        return;
    }
    
    
    recursive_depth++;
    
    // Step 1: Process layout commands (these position children but don't render)
    process_layout(element, commands, command_count, max_commands);
    
    // Step 2: Render the current element (parent renders first = background)
    render_element(element, commands, command_count, max_commands);
    
    // Step 3: Recursively render all children (children render on top of parent)
    for (size_t i = 0; i < element->child_count && *command_count < max_commands; i++) {
        KryonElement* child = element->children[i];
        if (child && child->type_name) {
            // Skip @for directive elements - they are templates, not renderable UI elements
            if (child->type == 0x8200) { // @for directive hex code
                    continue;
            }
            element_to_commands_recursive(child, commands, command_count, max_commands);
        }
    }
    
    recursive_depth--;
}


// Main conversion function
void kryon_element_tree_to_render_commands(KryonRuntime* runtime, KryonElement* root, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!runtime || !root || !commands || !command_count) return;
    
    *command_count = 0;
    
    // New position calculation pipeline - calculates final (x,y) for all elements
    calculate_all_element_positions(runtime, root);
    
    // Convert element tree to render commands
    element_to_commands_recursive(root, commands, command_count, max_commands);
    
    // Assign z-indices based on render order (first command = lowest z-index)
    // But preserve high z-indices set by elements like dropdown popups
    for (size_t i = 0; i < *command_count; i++) {
        if (commands[i].z_index < 1000) { // Don't overwrite high z-indices set by popups
            commands[i].z_index = (int)i;
        }
    }
}

// Variable management functions
bool kryon_runtime_set_variable(KryonRuntime *runtime, const char *name, const char *value) {
    if (!runtime || !name || !value) {
        return false;
    }
    
    // Initialize variable registry if needed
    if (!runtime->variable_names) {
        runtime->variable_names = kryon_alloc(16 * sizeof(char*));
        runtime->variable_values = kryon_alloc(16 * sizeof(char*));
        runtime->variable_capacity = 16;
        runtime->variable_count = 0;
        if (!runtime->variable_names || !runtime->variable_values) {
            return false;
        }
    }
    
    // Look for existing variable
    for (size_t i = 0; i < runtime->variable_count; i++) {
        if (runtime->variable_names[i] && strcmp(runtime->variable_names[i], name) == 0) {
            // Update existing variable
            kryon_free(runtime->variable_values[i]);
            runtime->variable_values[i] = kryon_alloc(strlen(value) + 1);
            if (runtime->variable_values[i]) {
                strcpy(runtime->variable_values[i], value);
                return true;
            }
            return false;
        }
    }
    
    // Add new variable if we have space
    if (runtime->variable_count < runtime->variable_capacity) {
        runtime->variable_names[runtime->variable_count] = kryon_alloc(strlen(name) + 1);
        runtime->variable_values[runtime->variable_count] = kryon_alloc(strlen(value) + 1);
        
        if (runtime->variable_names[runtime->variable_count] && runtime->variable_values[runtime->variable_count]) {
            strcpy(runtime->variable_names[runtime->variable_count], name);
            strcpy(runtime->variable_values[runtime->variable_count], value);
            runtime->variable_count++;
            return true;
        }
    }
    
    return false;
}

const char* kryon_runtime_get_variable(KryonRuntime *runtime, const char *name) {
    if (!runtime || !name || !runtime->variable_names) {
        return NULL;
    }
    
    // Look for variable
    for (size_t i = 0; i < runtime->variable_count; i++) {
        if (runtime->variable_names[i] && strcmp(runtime->variable_names[i], name) == 0) {
            return runtime->variable_values[i];
        }
    }
    
    return NULL;
}

KryonRuntime* kryon_runtime_get_current(void) {
    return g_current_runtime;
}

// Update root variables when viewport size changes
bool kryon_runtime_update_viewport_size(KryonRuntime* runtime, float width, float height) {
    if (!runtime) {
        return false;
    }
    
    // Convert dimensions to strings
    char width_str[32];
    char height_str[32];
    snprintf(width_str, sizeof(width_str), "%.0f", width);
    snprintf(height_str, sizeof(height_str), "%.0f", height);
    
    // Update root variables
    bool width_updated = kryon_runtime_set_variable(runtime, "root.width", width_str);
    bool height_updated = kryon_runtime_set_variable(runtime, "root.height", height_str);
    
    if (width_updated && height_updated) {
        // Mark elements for re-render since root variables changed
        if (runtime->root) {
            mark_elements_for_rerender(runtime->root);
        }
        runtime->needs_update = true;
        
        return true;
    } else {
        return false;
    }
}

// =============================================================================
// DEBUG INSPECTOR SYSTEM
// =============================================================================

static void kryon_debug_inspector_toggle(KryonRuntime *runtime) {
    if (!runtime) {
        return;
    }
    
    static bool inspector_running = false;
    static pid_t inspector_pid = -1;
    
    const char* inspector_kry_path = "examples/debug/inspector.kry";
    const char* inspector_krb_path = "/tmp/inspector.krb";
    
    if (inspector_running) {
        // Close inspector - kill the running process
        if (inspector_pid > 0) {
            kill(inspector_pid, SIGTERM);
            waitpid(inspector_pid, NULL, 0);
            inspector_pid = -1;
        }
        inspector_running = false;
        return;
    }
    
    // First, compile the inspector.kry to inspector.krb
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd), "./build/bin/kryon compile %s %s", 
             inspector_kry_path, inspector_krb_path);
    
    int compile_result = system(compile_cmd);
    if (compile_result != 0) {
        return;
    }
    
    // Then, launch the inspector as a separate process
    inspector_pid = fork();
    if (inspector_pid == 0) {
        // Child process - run the inspector
        char run_cmd[256];
        snprintf(run_cmd, sizeof(run_cmd), "./build/bin/kryon");
        execl(run_cmd, "kryon", "run", inspector_krb_path, NULL);
        exit(1); // If execl fails
    } else if (inspector_pid > 0) {
        // Parent process - inspector launched successfully
        inspector_running = true;
    }
}

// =============================================================================
// @FOR TEMPLATE EXPANSION
// =============================================================================

/**
 * @brief Process @for directives and expand them reactively based on array data
 * @param runtime Runtime context with variable state
 * @param element Root element to process (searches for @for templates)
 */
void process_for_directives(KryonRuntime* runtime, KryonElement* element) {
    // Simple entry check without printf to avoid stack corruption
    if (!runtime || !element) {
        return;
    }
    
    // Skip processing if KRB loading is in progress to avoid memory corruption
    if (runtime->is_loading) {
        return;
    }
    
    
    // Check if this element is a @for template
    if (element->type_name && strcmp(element->type_name, "for") == 0) {
        expand_for_template(runtime, element);
        // Don't process children of @for elements recursively; they are templates.
        return;
    }

    
    // Process children recursively (but skip children of @for templates)
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            process_for_directives(runtime, element->children[i]);
        }
    }
}

/**
 * @brief Expand a @for template based on current array variable state
 * @param runtime Runtime context
 * @param for_element The @for template element
 */
static void expand_for_template(KryonRuntime* runtime, KryonElement* for_element) {
    if (!runtime || !for_element) {
        return;
    }
    
    // Clean up previously generated elements before creating new ones
    if (for_element->parent) {
        printf("🧹 CLEANUP: Removing previously generated elements for @for directive\n");
        
        // Find all child elements in parent that were generated by this @for
        // We need to remove them to avoid accumulating duplicates
        KryonElement* parent = for_element->parent;
        size_t original_count = parent->child_count;
        
        // Find the @for element's position in parent's children
        size_t for_position = 0;
        for (size_t i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == for_element) {
                for_position = i;
                break;
            }
        }
        
        // Remove all elements after the @for element (these were generated by previous expansions)
        for (size_t i = for_position + 1; i < parent->child_count; ) {
            printf("🗑️ CLEANUP: Removing previously generated element at position %zu\n", i);
            KryonElement* to_remove = parent->children[i];
            
            // Remove from parent's children array by shifting remaining elements
            for (size_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            
            // Free the removed element (but don't increment i since we shifted the array)
            if (to_remove) {
                kryon_element_destroy(runtime, to_remove);
            }
        }
        
        printf("✅ CLEANUP: Removed %zu previously generated elements (parent had %zu, now has %zu)\n", 
               original_count - parent->child_count, original_count, parent->child_count);
    }
    
    // Extract template parameters from @for element properties
    const char* var_name = NULL;
    const char* array_name = NULL;
    
    // Find variable and array properties
    // printf("🔍 DEBUG expand_for_template: Processing %zu properties\n", for_element->property_count);
    for (size_t i = 0; i < for_element->property_count; i++) {
        KryonProperty* prop = for_element->properties[i];
        // printf("🔍 DEBUG: Property %zu: name='%s' type=%d\n", i, prop->name ? prop->name : "(null)", prop->type);
        
        if (prop->name && strcmp(prop->name, "variable") == 0) {
            var_name = prop->value.string_value;
            // printf("🔍 DEBUG: Found variable='%s'\n", var_name ? var_name : "(null)");
        } else if (prop->name && strcmp(prop->name, "array") == 0) {
            array_name = prop->value.string_value;
            // printf("🔍 DEBUG: Found array='%s'\n", array_name ? array_name : "(null)");
            // Debug: Check if array_name is valid
            if (!array_name || (uintptr_t)array_name < 0x1000) {
                write(STDERR_FILENO, "ERROR: Corrupted array_name pointer\n", 36);
                return;
            }
        }
    }
    
    if (!var_name || !array_name) {
        return;
    }
    
    // Get array data - check if it's already resolved JSON or needs variable lookup
    const char* array_data;
    if (array_name && array_name[0] == '[') {
        // Check if this looks like a JSON array
        array_data = array_name;
    } else {
        // Look up as variable name
        array_data = kryon_runtime_get_variable(runtime, array_name);
        if (!array_data) {
            return;
        }
    }
    
    // Parse array data - simple comma-separated strings
    // printf("🔍 DEBUG: About to strdup array_data: '%s'\n", array_data ? array_data : "(null)");
    // printf("🔍 DEBUG: array_data pointer: %p\n", array_data);
    char* array_copy = NULL;
    if (array_data) {
        array_copy = kryon_strdup(array_data);
        if (!array_copy) {
            return;
        }
    } else {
        return;
    }
    
    // Clear existing expanded children
    clear_expanded_children(runtime, for_element);
    
    // Split array and create elements
    char* token = strtok(array_copy, ",");
    size_t index = 0;
    
    while (token) {
        // Create a working copy for safe trimming
        size_t token_len = strlen(token);
        if (token_len == 0) {
            token = strtok(NULL, ",");
            continue;
        }
        
        // Find start position (skip leading whitespace, brackets, quotes)
        char* start = token;
        while (*start && (*start == ' ' || *start == '[' || *start == ']' || *start == '"')) {
            start++;
        }
        
        // Find end position (skip trailing whitespace, brackets, quotes)
        char* end = token + token_len - 1;
        while (end > start && (*end == ' ' || *end == '[' || *end == ']' || *end == '"')) {
            end--;
        }
        
        // Create trimmed string
        if (end >= start) {
            size_t trimmed_len = end - start + 1;
            char* trimmed = kryon_alloc(trimmed_len + 1);
            if (trimmed) {
                memcpy(trimmed, start, trimmed_len);
                trimmed[trimmed_len] = '\0';
                
                if (strlen(trimmed) > 0) {
                    // Create instance of template children for this array item
                    expand_for_iteration(runtime, for_element, var_name, trimmed, index);
                    index++;
                }
                
                kryon_free(trimmed);
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    kryon_free(array_copy);
}

/**
 * @brief Create element instances for one iteration of @for loop
 * @param runtime Runtime context
 * @param for_element The @for template element
 * @param var_name Loop variable name
 * @param var_value Current iteration value
 * @param index Current iteration index
 */
static void expand_for_iteration(KryonRuntime* runtime, KryonElement* for_element, 
                                const char* var_name, const char* var_value, size_t index) {
    if (!runtime || !for_element || !var_name || !var_value) {
        return;
    }
    
    // Clone template children and substitute variables
    for (size_t i = 0; i < for_element->child_count; i++) {
        KryonElement* template_child = for_element->children[i];
        if (!template_child) continue;
        
        // Use comprehensive validation system
        KryonValidationContext ctx;
        KryonValidationResult result = kryon_validate_element_deep(template_child, &ctx);
        
        if (result != KRYON_VALID) {
            printf("❌ VALIDATION ERROR: Template child %zu failed validation\n", i);
            kryon_report_validation_error(&ctx);
            continue;
        }
        
        printf("✅ Template child %zu validated successfully\n", i);
        
        printf("🔧 DEBUG: About to clone template child %zu with var_name='%s' var_value='%s'\n", 
               i, var_name, var_value);
        
        // Clone the template child
        KryonElement* instance = clone_element_with_substitution(runtime, template_child, 
                                                               var_name, var_value, index);
        if (instance) {
            printf("✅ Successfully cloned element, type='%s'\n", 
                   instance->type_name ? instance->type_name : "unknown");
            
            // Resolve template properties with @for variable context
            printf("🔧 Resolving template properties for @for variable '%s'='%s'\n", var_name, var_value);
            for (size_t prop_i = 0; prop_i < instance->property_count; prop_i++) {
                KryonProperty* prop = instance->properties[prop_i];
                if (prop && prop->type == KRYON_RUNTIME_PROP_TEMPLATE) {
                    printf("📝 Resolving template property '%s'\n", prop->name ? prop->name : "unnamed");
                    
                    char* resolved_text = resolve_for_template_property(runtime, prop, var_name, var_value);
                    if (resolved_text) {
                        printf("✅ Template resolved: '%s' (allocated at %p)\n", resolved_text, (void*)resolved_text);
                        
                        // Convert template to resolved string
                        // Free template segments first
                        if (prop->value.template_value.segments) {
                            for (size_t seg_i = 0; seg_i < prop->value.template_value.segment_count; seg_i++) {
                                KryonTemplateSegment* seg = &prop->value.template_value.segments[seg_i];
                                if (seg->type == KRYON_TEMPLATE_SEGMENT_LITERAL && seg->data.literal_text) {
                                    kryon_free(seg->data.literal_text);
                                } else if (seg->type == KRYON_TEMPLATE_SEGMENT_VARIABLE && seg->data.variable_name) {
                                    kryon_free(seg->data.variable_name);
                                }
                            }
                            kryon_free(prop->value.template_value.segments);
                        }
                        
                        // Convert to string property  
                        // IMPORTANT: Since value is a union, we CANNOT touch template_value fields after setting string_value
                        prop->type = KRYON_RUNTIME_PROP_STRING;
                        prop->value.string_value = resolved_text;
                        prop->is_bound = false; // No longer bound after resolution
                        // DO NOT set template_value.segments = NULL - it would overwrite string_value!
                      } else {
                        printf("❌ Failed to resolve template property '%s'\n", prop->name ? prop->name : "unnamed");
                    }
                }
            }
            
            // Add to parent (the element containing the @for)
            if (for_element->parent) {
                printf("🔧 Adding cloned element to parent '%s' (child_count before: %zu)\n",
                       for_element->parent->type_name ? for_element->parent->type_name : "unknown",
                       for_element->parent->child_count);
                add_child_element(runtime, for_element->parent, instance);
                printf("✅ Added child, parent now has %zu children\n", for_element->parent->child_count);
                
                // Immediately recalculate positions for the parent container to prevent flicker
                position_children_by_layout_type(runtime, for_element->parent);
            } else {
                printf("❌ WARNING: @for element has no parent, destroying clone\n");
                kryon_element_destroy(runtime, instance);
            }
        } else {
            printf("❌ WARNING: Failed to clone template child %zu\n", i);
        }
    }
}

/**
 * @brief Clear previously expanded children from @for template
 * @param runtime Runtime context  
 * @param for_element The @for template element
 */
static void clear_expanded_children(KryonRuntime* runtime, KryonElement* for_element) {
    // For now, this is a no-op since @for expansion happens during initial load
    // Future versions could track expanded children for reactive updates
    (void)runtime;
    (void)for_element;
}

/**
 * @brief Clone element and substitute template variables
 * @param runtime Runtime context
 * @param template_element Element to clone
 * @param var_name Variable name to substitute
 * @param var_value Variable value to substitute
 * @param index Iteration index
 * @return Cloned element with substituted variables
 */
static KryonElement* clone_element_with_substitution(KryonRuntime* runtime, 
                                                   KryonElement* template_element,
                                                   const char* var_name, 
                                                   const char* var_value,
                                                   size_t index) {
    // Comprehensive safety checks
    if (!runtime) {
        return NULL;
    }
    if (!template_element) {
        return NULL;
    }
    if (!var_name) {
        return NULL;
    }
    if (!var_value) {
        return NULL;
    }
    
    // Create new element of same type
    KryonElement* clone = kryon_element_create(runtime, template_element->type, NULL);
    if (!clone) {
        return NULL;
    }
    
    // Copy type_name from template element with comprehensive validation
    if (template_element->type_name) {
        KryonValidationContext ctx;
        clone->type_name = kryon_safe_strdup(template_element->type_name, &ctx);
        if (ctx.result != KRYON_VALID) {
            printf("❌ Failed to copy element type_name: %s\n", ctx.error_message);
        }
    }
    
    // Copy properties with variable substitution
    if (template_element->properties && template_element->property_count > 0) {
        // Allocate properties array for clone
        clone->property_capacity = template_element->property_count;
        clone->properties = kryon_alloc(clone->property_capacity * sizeof(KryonProperty*));
        if (!clone->properties) {
            kryon_element_destroy(runtime, clone);
            return NULL;
        }
        
        // Clone each property with comprehensive bounds checking
        for (size_t i = 0; i < template_element->property_count; i++) {
            
            // Validate property array bounds
            if (i >= template_element->property_count) {
                break;
            }
            
            KryonProperty* src_prop = template_element->properties[i];
            if (!src_prop) {
                continue;
            }
            
            // Validate clone property array capacity
            if (clone->property_count >= clone->property_capacity) {
                break;
            }
            
            // Allocate new property
            KryonProperty* dst_prop = kryon_alloc(sizeof(KryonProperty));
            if (!dst_prop) {
                break; // Stop on allocation failure
            }
            memset(dst_prop, 0, sizeof(KryonProperty));
            
            // Safely add to clone properties array
            size_t prop_index = clone->property_count;
            clone->properties[prop_index] = dst_prop;
            clone->property_count++;
            
            
            // Copy property name with comprehensive validation
            if (src_prop->name) {
                KryonValidationContext ctx;
                dst_prop->name = kryon_safe_strdup(src_prop->name, &ctx);
                if (ctx.result != KRYON_VALID) {
                    printf("❌ Failed to copy property name: %s\n", ctx.error_message);
                    dst_prop->name = kryon_safe_strdup("unknown", &ctx);
                }
            } else {
                dst_prop->name = NULL;
            }
            dst_prop->type = src_prop->type;
            
            // Handle property value based on type with safety checks
            if (src_prop->type == KRYON_RUNTIME_PROP_REFERENCE) {
                // Check if this REFERENCE property matches our substitution variable
                const char* ref_var = src_prop->value.string_value ? src_prop->value.string_value : src_prop->binding_path;
                
                if (ref_var && strcmp(ref_var, var_name) == 0) {
                    // Substitute: convert REFERENCE to STRING with the actual value
                    dst_prop->type = KRYON_RUNTIME_PROP_STRING;
                    if (var_value) {
                        dst_prop->value.string_value = kryon_strdup(var_value);
                        if (!dst_prop->value.string_value) {
                            dst_prop->value.string_value = kryon_strdup("");
                        }
                    } else {
                        dst_prop->value.string_value = kryon_strdup("");
                    }
                    dst_prop->is_bound = false;
                    dst_prop->binding_path = NULL;
                } else {
                    // Copy REFERENCE as-is (different variable) with safety check
                    if (ref_var && (uintptr_t)ref_var > 0x1000) {
                        dst_prop->value.string_value = kryon_strdup(ref_var);
                        if (!dst_prop->value.string_value) {
                            dst_prop->value.string_value = NULL;
                        }
                    } else {
                        dst_prop->value.string_value = NULL;
                    }
                    dst_prop->is_bound = src_prop->is_bound;
                    if (src_prop->binding_path) {
                        dst_prop->binding_path = kryon_strdup(src_prop->binding_path);
                        if (!dst_prop->binding_path) {
                            dst_prop->binding_path = NULL;
                        }
                    } else {
                        dst_prop->binding_path = NULL;
                    }
                }
            } else if (src_prop->type == KRYON_RUNTIME_PROP_TEMPLATE) {
                // Handle TEMPLATE property with variable substitution
                dst_prop->value.template_value.segment_count = src_prop->value.template_value.segment_count;
                dst_prop->value.template_value.segments = NULL;
                
                if (src_prop->value.template_value.segment_count > 0 && src_prop->value.template_value.segments) {
                    // Allocate segments array for clone
                    dst_prop->value.template_value.segments = kryon_alloc(
                        dst_prop->value.template_value.segment_count * sizeof(KryonTemplateSegment));
                    
                    if (dst_prop->value.template_value.segments) {
                        // Clone each segment with variable substitution
                        for (size_t seg_i = 0; seg_i < src_prop->value.template_value.segment_count; seg_i++) {
                            KryonTemplateSegment* src_seg = &src_prop->value.template_value.segments[seg_i];
                            KryonTemplateSegment* dst_seg = &dst_prop->value.template_value.segments[seg_i];
                            
                            dst_seg->type = src_seg->type;
                            
                            if (src_seg->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
                                // Copy literal text as-is
                                if (src_seg->data.literal_text) {
                                    dst_seg->data.literal_text = kryon_strdup(src_seg->data.literal_text);
                                } else {
                                    dst_seg->data.literal_text = NULL;
                                }
                            } else if (src_seg->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
                                // Check if this variable matches our substitution variable
                                if (src_seg->data.variable_name && strcmp(src_seg->data.variable_name, var_name) == 0) {
                                    // Convert VARIABLE segment to LITERAL segment with substituted value
                                    dst_seg->type = KRYON_TEMPLATE_SEGMENT_LITERAL;
                                    dst_seg->data.literal_text = kryon_strdup(var_value ? var_value : "");
                                } else {
                                    // Keep as VARIABLE (different variable name)
                                    if (src_seg->data.variable_name) {
                                        dst_seg->data.variable_name = kryon_strdup(src_seg->data.variable_name);
                                    } else {
                                        dst_seg->data.variable_name = NULL;
                                    }
                                }
                            }
                        }
                    } else {
                        // Allocation failed, reset segment count
                        dst_prop->value.template_value.segment_count = 0;
                    }
                }
                dst_prop->is_bound = false;
                dst_prop->binding_path = NULL;
            } else {
                // Copy literal properties (STRING, FLOAT, INT, etc.) as-is
                if (src_prop->type == KRYON_RUNTIME_PROP_STRING) {
                    if (src_prop->value.string_value) {
                        dst_prop->value.string_value = kryon_strdup(src_prop->value.string_value);
                        if (!dst_prop->value.string_value) {
                            dst_prop->value.string_value = kryon_strdup("");
                        }
                    } else {
                        dst_prop->value.string_value = NULL;
                    }
                } else {
                    // For numeric types, shallow copy
                    dst_prop->value = src_prop->value;
                }
                // For literal properties, force correct binding state
                if (src_prop->type == KRYON_RUNTIME_PROP_STRING || 
                    src_prop->type == KRYON_RUNTIME_PROP_FLOAT || 
                    src_prop->type == KRYON_RUNTIME_PROP_INTEGER) {
                    // Literal properties should never be bound
                    dst_prop->is_bound = false;
                    dst_prop->binding_path = NULL;
                } else {
                    // For other property types, copy binding state
                    dst_prop->is_bound = src_prop->is_bound;
                    if (src_prop->binding_path) {
                        dst_prop->binding_path = kryon_strdup(src_prop->binding_path);
                        if (!dst_prop->binding_path) {
                            dst_prop->binding_path = NULL;
                        }
                    } else {
                        dst_prop->binding_path = NULL;
                    }
                }
            }
        }
    }
    
    return clone;
}

/**
 * @brief Substitute template variables in string
 * @param template_str String with template variables like "${todo}"
 * @param var_name Variable name to replace
 * @param var_value Value to substitute
 * @return New string with variables substituted
 */
static char* substitute_template_variable(const char* template_str, 
                                        const char* var_name, 
                                        const char* var_value) {
    if (!template_str || !var_name || !var_value) {
        return kryon_strdup(template_str ? template_str : "");
    }
    
    // Create variable pattern like "${todo}"
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "${%s}", var_name);
    
    // Simple string replacement
    const char* pos = strstr(template_str, pattern);
    if (!pos) {
        // No substitution needed
        return kryon_strdup(template_str);
    }
    
    // Calculate result length
    size_t result_len = strlen(template_str) - strlen(pattern) + strlen(var_value) + 1;
    char* result = kryon_alloc(result_len);
    if (!result) {
        return kryon_strdup(template_str);
    }
    
    // Copy prefix
    size_t prefix_len = pos - template_str;
    strncpy(result, template_str, prefix_len);
    result[prefix_len] = '\0';
    
    // Add substituted value
    strcat(result, var_value);
    
    // Add suffix
    strcat(result, pos + strlen(pattern));
    
    return result;
}

/**
 * @brief Resolve template properties with @for loop variable context
 * @param runtime Runtime context
 * @param property Template property to resolve
 * @param var_name @for loop variable name (e.g. "todo")
 * @param var_value @for loop variable value (e.g. "Test todo")
 * @return Resolved string, or NULL on error (caller must free)
 */
static char* resolve_for_template_property(KryonRuntime* runtime, KryonProperty* property, 
                                         const char* var_name, const char* var_value) {
    if (!runtime || !property || !var_name || !var_value || property->type != KRYON_RUNTIME_PROP_TEMPLATE) {
        return NULL;
    }
    
    // Calculate total length needed for resolved string
    size_t total_len = 0;
    for (size_t i = 0; i < property->value.template_value.segment_count; i++) {
        KryonTemplateSegment *segment = &property->value.template_value.segments[i];
        
        if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
            if (segment->data.literal_text) {
                total_len += strlen(segment->data.literal_text);
            }
        } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
            // Check if this variable matches the @for variable
            const char *seg_var_name = segment->data.variable_name;
            if (seg_var_name && strcmp(seg_var_name, var_name) == 0) {
                // Use @for variable value
                total_len += strlen(var_value);
            } else {
                // Look up in runtime variables as fallback
                const char *fallback_value = NULL;
                for (size_t j = 0; j < runtime->variable_count; j++) {
                    if (runtime->variable_names[j] && seg_var_name &&
                        strcmp(runtime->variable_names[j], seg_var_name) == 0) {
                        fallback_value = runtime->variable_values[j];
                        break;
                    }
                }
                if (fallback_value) {
                    total_len += strlen(fallback_value);
                }
                // If no fallback found, variable remains as empty string (contributes 0 to length)
            }
        }
    }
    
    // Allocate result string
    char *result = kryon_alloc(total_len + 1);
    if (!result) {
        return NULL;
    }
    
    // Build resolved string by concatenating segments
    result[0] = '\0';
    for (size_t i = 0; i < property->value.template_value.segment_count; i++) {
        KryonTemplateSegment *segment = &property->value.template_value.segments[i];
        
        if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
            if (segment->data.literal_text) {
                strcat(result, segment->data.literal_text);
            }
        } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
            // Check if this variable matches the @for variable
            const char *seg_var_name = segment->data.variable_name;
            if (seg_var_name && strcmp(seg_var_name, var_name) == 0) {
                // Use @for variable value
                strcat(result, var_value);
            } else {
                // Look up in runtime variables as fallback
                const char *fallback_value = NULL;
                for (size_t j = 0; j < runtime->variable_count; j++) {
                    if (runtime->variable_names[j] && seg_var_name &&
                        strcmp(runtime->variable_names[j], seg_var_name) == 0) {
                        fallback_value = runtime->variable_values[j];
                        break;
                    }
                }
                if (fallback_value) {
                    strcat(result, fallback_value);
                }
                // If no fallback found, variable remains as empty string (adds nothing)
            }
        }
    }
    
    return result;
}

/**
 * @brief Add child element to parent
 * @param runtime Runtime context
 * @param parent Parent element
 * @param child Child element to add
 */
static void add_child_element(KryonRuntime* runtime, KryonElement* parent, KryonElement* child) {
    if (!runtime || !parent || !child) {
        return;
    }
    
    // Set parent relationship
    child->parent = parent;
    
    // Expand parent children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        KryonElement** new_children = kryon_realloc(parent->children, 
                                                  new_capacity * sizeof(KryonElement*));
        if (!new_children) {
            return;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    // Add child
    parent->children[parent->child_count++] = child;
    
    // Mark for re-render
    parent->needs_render = true;
}
