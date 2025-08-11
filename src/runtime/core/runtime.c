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

#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/krb_format.h"
#include "internal/renderer_interface.h"
#include "internal/color_utils.h"
#include "internal/types.h"
#include "internal/elements.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef KRYON_RENDERER_RAYLIB
#include <raylib.h>
#endif
#include <time.h>
#include <math.h>
#include <assert.h>

// Forward declaration from element_manager.c
bool kryon_element_set_property_by_name(KryonElement *element, const char *name, const void *value);

// Element registration is now handled automatically by element registry
#include <stdarg.h>

// Global runtime reference for variable resolution during rendering
static KryonRuntime* g_current_runtime = NULL;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void update_element_tree(KryonRuntime *runtime, KryonElement *element, double delta_time);
static void process_event_queue(KryonRuntime *runtime);
static bool runtime_event_handler(const KryonEvent* event, void* userData);
static void runtime_error(KryonRuntime *runtime, const char *format, ...);
static const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name);
static const char* resolve_variable_reference(KryonRuntime* runtime, const char* value);
static const char* resolve_component_state_variable(KryonRuntime* runtime, KryonElement* element, const char* var_name);

// Event callback function for receiving input events from renderer (extern declaration in runtime.h)

// Element to render command conversion functions (Phase 5: added runtime parameter)
static void element_container_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_text_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_button_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_image_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_center_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_column_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_row_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);

// External function from krb_loader.c
extern bool kryon_runtime_load_krb_data(KryonRuntime *runtime, const uint8_t *data, size_t size);

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
            printf("✅ Created Lua VM for script execution\n");
        } else {
            printf("⚠️  Failed to create Lua VM, script execution disabled\n");
        }
    }
    
    // Initialize other fields
    runtime->next_element_id = 1;
    runtime->is_running = false;
    runtime->needs_update = true;
    
    // Initialize input state (Phase 5: replaces global variables)
    runtime->mouse_position.x = 0.0f;
    runtime->mouse_position.y = 0.0f;
    runtime->cursor_should_be_pointer = false;
    
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
    
    // Free styles (with null check)
    if (runtime->styles) {
        for (size_t i = 0; i < runtime->style_count; i++) {
            // TODO: Implement style destruction
        }
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
    // kryon_runtime_start called
    
    if (!runtime || runtime->is_running) {
        printf("❌ ERROR: runtime is NULL or already running\n");
        return false;
    }
    // runtime is valid and not running
    
    if (!runtime->root) {
        printf("❌ ERROR: No root element loaded\n");
        runtime_error(runtime, "No root element loaded");
        return false;
    }
    // root element is valid
    
    // Setting runtime state
    runtime->is_running = true;
    runtime->last_update_time = 0.0;
    runtime->stats.frame_count = 0;
    runtime->stats.total_time = 0.0;
    
    // Runtime start completed successfully
    // TODO: Initialize renderer
    
    return true;
}

void kryon_runtime_stop(KryonRuntime *runtime) {
    if (!runtime || !runtime->is_running) {
        return;
    }
    
    runtime->is_running = false;
    
    // TODO: Cleanup renderer
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
    
    if (!runtime) {
        printf("❌ ERROR: runtime is NULL\n");
        return false;
    }
    
    // Note: mouse_clicked_this_frame reset moved to end of frame to allow click processing
    
    // Reset cursor state at frame start - elements will vote for pointer cursor (Phase 1A)
    runtime->cursor_should_be_pointer = false;
    
    // Note: Input state is now handled via event-driven callbacks (Phase 4)
    // The renderer pushes events directly to runtime->event_system via runtime_receive_input_event
    
    if (!runtime->is_running) {
        printf("❌ ERROR: runtime is not running\n");
        return false;
    }
    
    if (!runtime->root) {
        printf("❌ ERROR: runtime->root is NULL\n");
        return false;
    }
    
    clock_t start_time = clock();
    
    // Generate render commands from element tree (regardless of renderer)
    if (runtime->root) {
        // Create render command array
        KryonRenderCommand commands[256]; // Stack allocation for performance
        size_t command_count = 0;
        
        // Generate render commands from element tree
        
        // Safety checks before calling render function
        if (!runtime->root->type_name) {
            printf("❌ ERROR: Root element has null type_name\n");
            return false;
        }
                
        // Set global runtime for variable resolution during rendering
        g_current_runtime = runtime;
        
        // Generate render commands from element tree
        kryon_element_tree_to_render_commands(runtime->root, commands, &command_count, 256);
        
        // Clear global runtime after rendering
        g_current_runtime = NULL;
        
        // Set cursor state before begin_frame (Phase 1B & 1C: cursor communication)
        // Apply cursor state based on hover detection from UI elements processed above
        #ifdef KRYON_RENDERER_RAYLIB
        if (runtime->cursor_should_be_pointer) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);  
        }
        #endif
        
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
            
            // Cursor state is now set before begin_frame (see line ~516)
            
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
    
    // Reset per-frame input state at end of frame (Phase 5: click detection fix)
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
            // TODO: Call onMount lifecycle hook
            element->state = KRYON_ELEMENT_STATE_MOUNTED;
            break;
            
        case KRYON_ELEMENT_STATE_MOUNTED:
            if (element->needs_layout || element->needs_render) {
                element->state = KRYON_ELEMENT_STATE_UPDATING;
                // TODO: Perform layout and render updates
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
static KryonProperty* find_element_property(KryonElement* element, const char* prop_name) {
    if (!element || !prop_name) return NULL;
    
    // Critical safety check: verify properties array exists when property_count > 0
    if (element->property_count > 0 && !element->properties) {
        printf("❌ ERROR: Element has property_count=%zu but properties array is NULL\n", element->property_count);
        return NULL;
    }
    
    // Bounds check: prevent excessive property counts
    if (element->property_count > 1000) {
        printf("❌ ERROR: Element has suspicious property_count=%zu, possible corruption\n", element->property_count);
        return NULL;
    }
    
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty* prop = element->properties[i];
        if (prop && prop->name && strcmp(prop->name, prop_name) == 0) {
            return prop;
        }
    }
    return NULL;
}

// Resolve variable references in string values
static const char* resolve_variable_reference(KryonRuntime* runtime, const char* value) {
    if (!runtime || !value || value[0] != '$') {
        return value; // Not a variable reference
    }
    
    // Extract variable name (skip the $ prefix)
    const char* var_name = value + 1;
    
    // Look up variable in runtime registry
    const char* resolved_value = kryon_runtime_get_variable(runtime, var_name);
    if (resolved_value) {
        // Resolved variable
        return resolved_value;
    }
    
    // Variable not found, using literal value
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
static const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return NULL;
    
    static int debug_call_count = 0;
    if (debug_call_count < 5) {
        printf("🔍 Found %s (type=%d)\n", prop_name, prop->type);
        debug_call_count++;
    }
    
    // Use rotating buffers for all string conversions to prevent overwriting
    static char string_buffers[4][1024];
    static int string_buffer_index = 0;
    char* buffer = string_buffers[string_buffer_index];
    string_buffer_index = (string_buffer_index + 1) % 4;

    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            // Check if property is bound to component state
            if (prop->is_bound && prop->binding_path && runtime) {
                const char* resolved_value = NULL;

                // Try to resolve component state variable first
                resolved_value = resolve_component_state_variable(runtime, element, prop->binding_path);
                
                // Fallback resolution logic
                if (!resolved_value) {
                    KryonElement* current = element;
                    while (current && current->parent) {
                        if (current->parent->type_name && strcmp(current->parent->type_name, "Column") == 0) {
                            KryonElement* column = current->parent;
                            for (size_t i = 0; i < column->child_count; i++) {
                                if (column->children[i] == current) {
                                    char pattern_buffer[128];
                                    snprintf(pattern_buffer, sizeof(pattern_buffer), "comp_%zu.%s", i, prop->binding_path);
                                    resolved_value = kryon_runtime_get_variable(runtime, pattern_buffer);
                                    if (resolved_value) break;
                                }
                            }
                            if (resolved_value) break;
                        }
                        current = current->parent;
                    }
                }

                if (!resolved_value) {
                    char pattern_buffer[128];
                    for (int comp_id = 0; comp_id < 10; comp_id++) {
                        snprintf(pattern_buffer, sizeof(pattern_buffer), "comp_%d.%s", comp_id, prop->binding_path);
                        resolved_value = kryon_runtime_get_variable(runtime, pattern_buffer);
                        if (resolved_value) break;
                    }
                }

                if (!resolved_value) {
                    resolved_value = kryon_runtime_get_variable(runtime, prop->binding_path);
                }

                if (resolved_value) {
                    strncpy(buffer, resolved_value, 1023);
                    buffer[1023] = '\0';
                    return buffer;
                }
                
                return prop->value.string_value;
            }
            
            // Legacy: Resolve variable references if runtime is available
            if (runtime) {
                const char* resolved = resolve_variable_reference(runtime, prop->value.string_value);
                strncpy(buffer, resolved, 1023);
                buffer[1023] = '\0';
                return buffer;
            }
            return prop->value.string_value;
            
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
        
        case KRYON_RUNTIME_PROP_REFERENCE:
            snprintf(buffer, 1024, "ref_%u", prop->value.ref_id);
            return buffer;
        
        case KRYON_RUNTIME_PROP_FUNCTION:
            return prop->value.string_value;
        
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

// Get property reference ID
static uint32_t get_element_property_ref(KryonElement* element, const char* prop_name, uint32_t default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return default_value;
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_REFERENCE:
            return prop->value.ref_id;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            return (uint32_t)prop->value.int_value;
            
        case KRYON_RUNTIME_PROP_STRING:
            if (prop->value.string_value) {
                return (uint32_t)atoi(prop->value.string_value);
            }
            return default_value;
            
        default:
            return default_value;
    }
}

// Get property expression (for reactive properties)
static void* get_element_property_expression(KryonElement* element, const char* prop_name) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return NULL;
    
    if (prop->type == KRYON_RUNTIME_PROP_EXPRESSION) {
        return prop->value.expression;
    }
    
    return NULL;
}

// Get property function pointer
static void* get_element_property_function(KryonElement* element, const char* prop_name) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return NULL;
    
    if (prop->type == KRYON_RUNTIME_PROP_FUNCTION) {
        return prop->value.function;
    }
    
    return NULL;
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

// Debug function to log all properties of an element
static void debug_log_element_properties(KryonElement* element, const char* element_name) {
    if (!element) return;
    
    static bool debug_logged = false;
    if (debug_logged) return;
    debug_logged = true;
    
    // Properties for element
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty* prop = element->properties[i];
        if (prop && prop->name) {
            printf("  - %s (type=%d): ", prop->name, prop->type);
            
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    printf("'%s'\n", prop->value.string_value ? prop->value.string_value : "NULL");
                    break;
                case KRYON_RUNTIME_PROP_INTEGER:
                    printf("%lld\n", (long long)prop->value.int_value);
                    break;
                case KRYON_RUNTIME_PROP_FLOAT:
                    printf("%.6f\n", prop->value.float_value);
                    break;
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    printf("%s\n", prop->value.bool_value ? "true" : "false");
                    break;
                case KRYON_RUNTIME_PROP_COLOR:
                    printf("#%08X\n", prop->value.color_value);
                    break;
                case KRYON_RUNTIME_PROP_REFERENCE:
                    printf("ref_%u\n", prop->value.ref_id);
                    break;
                default:
                    printf("(unknown type)\n");
                    break;
            }
        }
    }
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

// Convert Container element to render commands
static void element_container_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Debug: Log all properties for first container
    // debug_log_element_properties(element, "Container");
    
    // Get container properties with comprehensive type handling
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 100.0f);
    float height = get_element_property_float(element, "height", 100.0f);
    
    // Container position calculated
    
    // Get color properties directly as color values (not strings)
    uint32_t bg_color_val = get_element_property_color(element, "background", 0x00000000); // Transparent default
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF); // Black default
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0); // Default z-index 0
    
    // Convert RGBA values to KryonColor format
    KryonColor bg_color = {
        ((bg_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((bg_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((bg_color_val >> 8) & 0xFF) / 255.0f,   // B
        (bg_color_val & 0xFF) / 255.0f           // A
    };
    
    // Debug: Show color being rendered 
    if (bg_color_val != 0x00000000) {
        printf("🎨 Container background: 0x%08X -> RGBA(%.2f,%.2f,%.2f,%.2f) at (%.1f,%.1f) %dx%.0f\n", 
               bg_color_val, bg_color.r, bg_color.g, bg_color.b, bg_color.a, posX, posY, (int)width, height);
    }
    KryonColor border_color = {
        ((border_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((border_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((border_color_val >> 8) & 0xFF) / 255.0f,   // B
        (border_color_val & 0xFF) / 255.0f           // A
    };
    
    // Create container background command
    if (bg_color.a > 0.0f || border_width > 0.0f) {
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            (KryonVec2){posX, posY},
            (KryonVec2){width, height},
            bg_color,
            border_radius
        );
        
        // Set border properties
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        // Set z-index for proper layering
        cmd.z_index = z_index;
        
        commands[(*command_count)++] = cmd;
    }
}

// Convert element to render commands using proper button element command

static void element_button_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands) return;

    // --- 1. Get Visual Properties ---
    // Read all visual properties from the element with safe defaults.
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", -1.0f); // -1 triggers auto-sizing
    float height = get_element_property_float(element, "height", -1.0f);
    const char* text = get_element_property_string(element, "text");
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x3B82F6FF); // Blue-500
    uint32_t text_color_val = get_element_property_color(element, "color", 0xFFFFFFFF); // White
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x2563EBFF); // Blue-600
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 8.0f);
    bool enabled = get_element_property_bool(element, "enabled", true);

    // --- 2. Calculate Final Layout ---
    // Apply auto-sizing if width or height were not explicitly set.
    auto_size_element(element, &width, &height);

    // --- 3. Determine Visual State based on Interaction ---
    // This is purely for visual feedback (e.g., hover color, cursor change).
    bool is_hovered = false;
    if (enabled) {
        KryonVec2 mouse_pos = runtime->mouse_position;
        if (mouse_pos.x >= posX && mouse_pos.x <= posX + width &&
            mouse_pos.y >= posY && mouse_pos.y <= posY + height) {
            is_hovered = true;
            runtime->cursor_should_be_pointer = true;
        }
    }

    // --- 4. Prepare Colors and Final State ---
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor text_color = color_u32_to_f32(text_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Apply hover/disabled visual effects
    if (is_hovered) {
        bg_color = color_lighten(bg_color, 0.15f); // Make button brighter on hover
    }
    if (!enabled) {
        bg_color = color_desaturate(bg_color, 0.5f); // Make button grayish if disabled
        text_color.a *= 0.7f; // Make text slightly transparent
    }

    // --- 5. Generate a Unique and Correct ID for the Renderer ---
    const char* button_id_str = element->element_id;
    static char id_buffer[4][32];
    static int buffer_index = 0;

    if (!button_id_str || button_id_str[0] == '\0') {
        char* current_buffer = id_buffer[buffer_index];
        buffer_index = (buffer_index + 1) % 4; // Use a rotating buffer for safety
        snprintf(current_buffer, 32, "kryon-id-%u", element->id);
        button_id_str = current_buffer;
    }
    
    // --- 6. Create the Render Command ---
    // This is the final, pure "instruction" for the renderer.
    KryonRenderCommand cmd = kryon_cmd_draw_button(
        button_id_str,
        (KryonVec2){posX, posY},
        (KryonVec2){width, height},
        text ? text : "", // Use empty string as default, not "Button"
        bg_color,
        text_color
    );
    
    // Set the remaining visual properties on the command
    cmd.data.draw_button.border_color = border_color;
    cmd.data.draw_button.border_width = border_width;
    cmd.data.draw_button.border_radius = border_radius;
    cmd.data.draw_button.state = is_hovered ? KRYON_ELEMENT_STATE_HOVERED : KRYON_ELEMENT_STATE_NORMAL;
    cmd.data.draw_button.enabled = enabled;
    
    // Add the completed command to the render queue
    commands[(*command_count)++] = cmd;
}


// Convert Image element to render commands
static void element_image_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return; // Need space for image command
    
    // Get image properties with dynamic lookup
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 100.0f);
    float height = get_element_property_float(element, "height", 100.0f);
    
    const char* source = get_element_property_string(element, "source");
    const char* src = get_element_property_string(element, "src"); // Alternative property name
    float opacity = get_element_property_float(element, "opacity", 1.0f);
    bool maintain_aspect = get_element_property_bool(element, "keepAspectRatio", true);
    
    // Use 'src' if 'source' is not provided (common naming convention)
    if (!source && src) {
        source = src;
    }
    
    if (!source) {
        // No image source provided, skip rendering
        return;
    }
    
    // Create image render command
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_IMAGE;
    cmd.z_index = 0;
    cmd.data.draw_image = (KryonDrawImageData){
        .position = {posX, posY},
        .size = {width, height},
        .source = strdup(source),
        .opacity = opacity,
        .maintain_aspect = maintain_aspect
    };
    
    commands[(*command_count)++] = cmd;
}

// Convert Center element to render commands (handles child positioning)
static void element_center_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Center doesn't render itself but positions its children
    // Calculate center position based on parent or window size
    
    float center_x = 300.0f; // Default window center X
    float center_y = 200.0f; // Default window center Y
    
    // If parent exists, use parent dimensions
    if (element->parent) {
        float parent_width = get_element_property_float(element->parent, "windowWidth", 600.0f);
        float parent_height = get_element_property_float(element->parent, "windowHeight", 400.0f);
        center_x = parent_width / 2.0f;
        center_y = parent_height / 2.0f;
        
    }
    
    // Position children at the center
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            // Calculate child position to center it
            float child_width = get_element_property_float(child, "width", 100.0f);
            float child_height = get_element_property_float(child, "height", 50.0f);
            
            float child_x = center_x - (child_width / 2.0f);
            float child_y = center_y - (child_height / 2.0f);
            
            // Set position properties on child (create property if needed)
            // Find or create posX property
            KryonProperty* posX_prop = find_element_property(child, "posX");
            if (!posX_prop) {
                // Add new property - need to expand properties array
                if (child->property_count >= child->property_capacity) {
                    size_t new_capacity = child->property_capacity ? child->property_capacity * 2 : 4;
                    KryonProperty **new_props = kryon_realloc(child->properties, new_capacity * sizeof(KryonProperty*));
                    if (new_props) {
                        child->properties = new_props;
                        child->property_capacity = new_capacity;
                    }
                }
                
                if (child->property_count < child->property_capacity) {
                    posX_prop = kryon_alloc(sizeof(KryonProperty));
                    if (posX_prop) {
                        posX_prop->name = strdup("posX");
                        posX_prop->type = KRYON_RUNTIME_PROP_FLOAT;
                        child->properties[child->property_count++] = posX_prop;
                    }
                }
            }
            if (posX_prop) {
                posX_prop->value.float_value = child_x;
            }
            
            // Find or create posY property
            KryonProperty* posY_prop = find_element_property(child, "posY");
            if (!posY_prop) {
                if (child->property_count < child->property_capacity) {
                    posY_prop = kryon_alloc(sizeof(KryonProperty));
                    if (posY_prop) {
                        posY_prop->name = strdup("posY");
                        posY_prop->type = KRYON_RUNTIME_PROP_FLOAT;
                        child->properties[child->property_count++] = posY_prop;
                    }
                }
            }
            if (posY_prop) {
                posY_prop->value.float_value = child_y;
            }
        }
    }
}

// Helper function to set element position
static void set_element_position(KryonElement* element, float x, float y) {
    if (!element) return;
    
    // Set position properties on the element
    // printf("🔧 Setting position for element '%s' to (%.1f, %.1f)\n", 
    //        element->type_name ? element->type_name : "Unknown", x, y);
    
    // Find or create posX property (used by text rendering)
    KryonProperty* x_prop = find_element_property(element, "posX");
    if (!x_prop) {
        // Expand capacity if needed
        if (element->property_count >= element->property_capacity) {
            size_t new_capacity = element->property_capacity ? element->property_capacity * 2 : 4;
            KryonProperty **new_properties = kryon_realloc(element->properties,
                                                          new_capacity * sizeof(KryonProperty*));
            if (!new_properties) return;
            element->properties = new_properties;
            element->property_capacity = new_capacity;
        }
        
        // Add posX property
        x_prop = kryon_alloc(sizeof(KryonProperty));
        x_prop->name = kryon_strdup("posX");
        element->properties[element->property_count++] = x_prop;
    }
    x_prop->type = KRYON_RUNTIME_PROP_FLOAT;
    x_prop->value.float_value = x;
    // printf("🔧 Set posX to %.1f\n", x_prop->value.float_value);
    
    // Find or create posY property (used by text rendering)
    KryonProperty* y_prop = find_element_property(element, "posY");
    if (!y_prop) {
        // Expand capacity if needed
        if (element->property_count >= element->property_capacity) {
            size_t new_capacity = element->property_capacity ? element->property_capacity * 2 : 4;
            KryonProperty **new_properties = kryon_realloc(element->properties,
                                                          new_capacity * sizeof(KryonProperty*));
            if (!new_properties) return;
            element->properties = new_properties;
            element->property_capacity = new_capacity;
        }
        
        // Add posY property
        y_prop = kryon_alloc(sizeof(KryonProperty));
        y_prop->name = kryon_strdup("posY");
        element->properties[element->property_count++] = y_prop;
    }
    y_prop->type = KRYON_RUNTIME_PROP_FLOAT;
    y_prop->value.float_value = y;
    // printf("🔧 Set posY to %.1f\n", y_prop->value.float_value);
    
    // Position set successfully
}

// Convert Column element to render commands (arranges children vertically)
static void element_column_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Column arranges children vertically with optional spacing and alignment
    
    // Get layout properties
    const char* main_axis = get_element_property_string(element, "mainAxis");
    const char* cross_axis = get_element_property_string(element, "crossAxis"); 
    float gap = get_element_property_float(element, "gap", 0.0f);
    
    // Get container position and dimensions from parent
    float container_x = 0.0f;
    float container_y = 0.0f;
    float container_width = 600.0f;  // Default
    float container_height = 400.0f; // Default
    
    // Column layout inherits position and size from its parent container
    if (element->parent) {
        // Get parent's position (where this Column should start)
        container_x = get_element_property_float(element->parent, "posX", 0.0f);
        container_y = get_element_property_float(element->parent, "posY", 0.0f);
        
        // Get parent's dimensions (Column fills parent's content area)
        container_width = get_element_property_float(element->parent, "width", 600.0f);
        container_height = get_element_property_float(element->parent, "height", 400.0f);
        
        // Handle window-level parents
        if (strcmp(element->parent->type_name, "App") == 0) {
            container_width = get_element_property_float(element->parent, "windowWidth", 600.0f);
            container_height = get_element_property_float(element->parent, "windowHeight", 400.0f);
        }
        
        // Account for parent's padding
        float parent_padding = get_element_property_float(element->parent, "padding", 0.0f);
        container_x += parent_padding;
        container_y += parent_padding;
        container_width -= 2 * parent_padding;
        container_height -= 2 * parent_padding;
        
        // printf("🔧 Column inherits from parent '%s': pos=(%.1f,%.1f) size=%.1fx%.1f\n", 
        //        element->parent->type_name, container_x, container_y, container_width, container_height);
        
        // Set position properties on this Column so children can inherit from it
        set_element_position(element, container_x, container_y);
    }
    
    // Apply this Column's own padding to create content area for children
    float column_padding = get_element_property_float(element, "padding", 0.0f);
    container_x += column_padding;
    container_y += column_padding;
    container_width -= 2 * column_padding;
    container_height -= 2 * column_padding;
    
    // if (column_padding > 0) {
    //     printf("🔧 Column applying own padding %.1f, content area: pos=(%.1f,%.1f) size=%.1fx%.1f\n", 
    //            column_padding, container_x, container_y, container_width, container_height);
    // }
    
    // Calculate total height needed for all children
    float total_children_height = 0.0f;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            float child_height = calculate_element_height(child);
            total_children_height += child_height;
            if (i < element->child_count - 1) {
                total_children_height += gap; // Add gap between children
            }
        }
    }
    
    // Calculate starting Y position based on mainAxisAlignment
    float start_y = container_y;
    if (main_axis && strcmp(main_axis, "center") == 0) {
        start_y = container_y + (container_height - total_children_height) / 2.0f;
    } else if (main_axis && strcmp(main_axis, "end") == 0) {
        start_y = container_y + container_height - total_children_height;
    } else if (main_axis && strcmp(main_axis, "spaceEvenly") == 0) {
        // spaceEvenly: equal space before, between, and after all items
        if (element->child_count > 0) {
            float available_space = container_height - total_children_height + ((element->child_count - 1) * gap);
            gap = available_space / (element->child_count + 1);
            start_y = container_y + gap;
        }
    } else if (main_axis && strcmp(main_axis, "spaceAround") == 0) {
        // spaceAround: equal space around each item (half space at edges)
        if (element->child_count > 0) {
            float available_space = container_height - total_children_height + ((element->child_count - 1) * gap);
            gap = available_space / element->child_count;
            start_y = container_y + gap / 2.0f;
        }
    } else if (main_axis && strcmp(main_axis, "spaceBetween") == 0) {
        // spaceBetween: equal space between items (no space at edges)
        if (element->child_count > 1) {
            float available_space = container_height - total_children_height + ((element->child_count - 1) * gap);
            gap = available_space / (element->child_count - 1);
            start_y = container_y;
        }
    }
    // "start" or default uses container_y
    
    // Position children vertically
    float current_y = start_y;
    
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            float child_width = get_element_property_float(child, "width", 100.0f);
            float child_height = calculate_element_height(child);
            
            // Calculate X position based on crossAxisAlignment
            float child_x = container_x;
            if (cross_axis && strcmp(cross_axis, "center") == 0) {
                child_x = container_x + (container_width - child_width) / 2.0f;
            } else if (cross_axis && strcmp(cross_axis, "end") == 0) {
                child_x = container_x + container_width - child_width;
            }
            // "start" or default uses container_x
            
            // Set position properties on child
            set_element_position(child, child_x, current_y);
            
            current_y += child_height + gap;
        }
    }
}

// Convert Row element to render commands (arranges children horizontally)
static void element_row_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Row arranges children horizontally with optional spacing and alignment
    
    // Get layout properties
    const char* main_axis = get_element_property_string(element, "mainAxis");
    const char* cross_axis = get_element_property_string(element, "crossAxis");
    float gap = get_element_property_float(element, "gap", 0.0f);
    
    // Get container position and dimensions from parent
    float container_x = 0.0f;
    float container_y = 0.0f;
    float container_width = 600.0f;  // Default
    float container_height = 400.0f; // Default
    
    // Row layout inherits position and size from its parent container
    if (element->parent) {
        // Get this Row's position (set by parent's layout)
        container_x = get_element_property_float(element, "posX", 0.0f);
        container_y = get_element_property_float(element, "posY", 0.0f);
        
        // Find actual container dimensions by walking up the hierarchy
        // Look for the first parent that has explicit width/height properties
        KryonElement* size_parent = element->parent;
        while (size_parent) {
            // Check if this parent has explicit dimensions
            KryonProperty* width_prop = find_element_property(size_parent, "width");
            KryonProperty* height_prop = find_element_property(size_parent, "height");
            
            if (width_prop && height_prop) {
                // Found a parent with explicit dimensions
                container_width = width_prop->value.float_value;
                container_height = height_prop->value.float_value;
                
                // Account for this parent's padding
                float size_parent_padding = get_element_property_float(size_parent, "padding", 0.0f);
                if (size_parent_padding > 0) {
                    container_width -= 2 * size_parent_padding;
                    container_height -= 2 * size_parent_padding;
                    printf("🔍 Applied padding %.1f, usable size: %.1fx%.1f\n", 
                           size_parent_padding, container_width, container_height);
                }
                break;
            } else if (strcmp(size_parent->type_name, "App") == 0) {
                // Handle window-level parents
                container_width = get_element_property_float(size_parent, "windowWidth", 600.0f);
                container_height = get_element_property_float(size_parent, "windowHeight", 400.0f);
                break;
            }
            
            // Move up to the next parent
            size_parent = size_parent->parent;
        }
        
        // printf("🔧 Row inherits from parent '%s': pos=(%.1f,%.1f) size=%.1fx%.1f\n", 
        //        element->parent->type_name, container_x, container_y, container_width, container_height);
        
        // Set position properties on this Row so children can inherit from it
        set_element_position(element, container_x, container_y);
    }
    
    // Calculate total width needed for all children
    float total_children_width = 0.0f;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            float child_width = get_element_property_float(child, "width", 100.0f);
            total_children_width += child_width;
            if (i < element->child_count - 1) {
                total_children_width += gap; // Add gap between children
            }
        }
    }
    
    // Calculate starting X position based on mainAxisAlignment
    float start_x = container_x;
    if (main_axis && strcmp(main_axis, "center") == 0) {
        start_x = container_x + (container_width - total_children_width) / 2.0f;
    } else if (main_axis && strcmp(main_axis, "end") == 0) {
        start_x = container_x + container_width - total_children_width;
    } else if (main_axis && strcmp(main_axis, "spaceEvenly") == 0) {
        // spaceEvenly: equal space before, between, and after all items
        if (element->child_count > 0) {
            float available_space = container_width - total_children_width + ((element->child_count - 1) * gap);
            gap = available_space / (element->child_count + 1);
            start_x = container_x + gap;
        }
    } else if (main_axis && strcmp(main_axis, "spaceAround") == 0) {
        // spaceAround: equal space around each item (half space at edges)
        if (element->child_count > 0) {
            float available_space = container_width - total_children_width + ((element->child_count - 1) * gap);
            gap = available_space / element->child_count;
            start_x = container_x + gap / 2.0f;
        }
    } else if (main_axis && strcmp(main_axis, "spaceBetween") == 0) {
        // spaceBetween: equal space between items (no space at edges)
        if (element->child_count > 1) {
            float available_space = container_width - total_children_width + ((element->child_count - 1) * gap);
            gap = available_space / (element->child_count - 1);
            start_x = container_x;
        }
    }
    // "start" or default uses container_x
    
    // Position children horizontally
    float current_x = start_x;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            float child_width = get_element_property_float(child, "width", 100.0f);
            float child_height = get_element_property_float(child, "height", 50.0f);
            
            // Calculate Y position based on crossAxisAlignment
            float child_y = container_y;
            if (cross_axis && strcmp(cross_axis, "center") == 0) {
                child_y = container_y + (container_height - child_height) / 2.0f;
            } else if (cross_axis && strcmp(cross_axis, "end") == 0) {
                child_y = container_y + container_height - child_height;
            }
            // "start" or default uses container_y
            
            // Set position properties on child
            set_element_position(child, current_x, child_y);
            
            current_x += child_width + gap;
        }
    }
}

// Convert Text element to render commands  
static void element_text_to_commands(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Debug: Log all properties for first text element
    // debug_log_element_properties(element, "Text");
    
    // Get text properties with comprehensive type handling
    const char* text = get_element_property_string(element, "text");
    uint32_t text_color_val = get_element_property_color(element, "color", 0x000000FF); // Black default
    float font_size = get_element_property_float(element, "fontSize", 20.0f);
    const char* font_family = get_element_property_string(element, "fontFamily");
    const char* font_weight = get_element_property_string(element, "fontWeight");
    const char* text_align = get_element_property_string(element, "textAlignment");
    int z_index = get_element_property_int(element, "zIndex", 0); // Default z-index 0
    
    // Get text positioning
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float max_width = get_element_property_float(element, "maxWidth", 0.0f);
    
    if (!text) return;
    KryonColor text_color = {
        ((text_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((text_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((text_color_val >> 8) & 0xFF) / 255.0f,   // B
        (text_color_val & 0xFF) / 255.0f           // A
    };
    
    // Calculate proper positioning within parent container
    KryonVec2 position = {posX, posY};
    
    // If element has a parent and no explicit position properties, calculate centered position
    KryonProperty* posX_prop = find_element_property(element, "posX");
    KryonProperty* posY_prop = find_element_property(element, "posY");
    if (element->parent && !posX_prop && !posY_prop) {
        // Get parent properties
        float parent_x = get_element_property_float(element->parent, "posX", 0.0f);
        float parent_y = get_element_property_float(element->parent, "posY", 0.0f);
        float parent_width = get_element_property_float(element->parent, "width", 200.0f);
        float parent_height = get_element_property_float(element->parent, "height", 100.0f);
        float parent_padding = get_element_property_float(element->parent, "padding", 0.0f);
        const char* parent_alignment = get_element_property_string(element->parent, "contentAlignment");
        
        // Calculate available area inside parent (accounting for padding)
        float available_x = parent_x + parent_padding;
        float available_y = parent_y + parent_padding;
        float available_width = parent_width - (parent_padding * 2);
        float available_height = parent_height - (parent_padding * 2);
        
        // Center the text within the available area
        if (parent_alignment && strcmp(parent_alignment, "center") == 0) {
            position.x = available_x + (available_width / 2.0f);
            position.y = available_y + (available_height / 2.0f);
            // Centered in parent
        } else {
            // Default to top-left of available area
            position.x = available_x;
            position.y = available_y;
        }
    } else {
        // Element has explicit posX/posY properties, use them
        // (position is already set to {posX, posY} at the beginning of function)
    }
    
    // Store calculated position in element for hit testing
    element->x = position.x;
    element->y = position.y;
    
    // Parse text alignment - check parent's contentAlignment if no explicit textAlignment
    int text_align_code = 0; // 0=left, 1=center, 2=right
    if (text_align) {
        if (strcmp(text_align, "center") == 0) text_align_code = 1;
        else if (strcmp(text_align, "right") == 0) text_align_code = 2;
    } else if (element->parent) {
        // Use parent's contentAlignment if no explicit text alignment
        const char* parent_alignment = get_element_property_string(element->parent, "contentAlignment");
        if (parent_alignment && strcmp(parent_alignment, "center") == 0) {
            text_align_code = 1;
            // Using parent contentAlignment 'center' for text alignment
        }
    }
    
    // Parse font weight
    bool is_bold = false;
    if (font_weight && (strcmp(font_weight, "bold") == 0 || strcmp(font_weight, "700") == 0)) {
        is_bold = true;
    }
    
    // Create text render command with full properties
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.z_index = z_index;
    cmd.data.draw_text = (KryonDrawTextData){
        .position = position,
        .text = strdup(text),
        .font_size = font_size,
        .color = text_color,
        .font_family = font_family ? strdup(font_family) : strdup("Arial"),
        .bold = is_bold,
        .italic = false,
        .max_width = max_width,
        .text_align = text_align_code
    };
    
    commands[(*command_count)++] = cmd;
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
        {"Container", element_container_to_commands},
        {"Text", element_text_to_commands},
        {"Image", element_image_to_commands},
        // Add more element types here
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
        {"Center", element_center_to_commands},
        {"Column", element_column_to_commands},
        {"Row", element_row_to_commands},
        // Add more layout types here
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
    if (!element->type_name) {
        printf("⚠️  WARNING: Element has null type_name, skipping\n");
        return;
    }
    
    if (element->property_count > 1000 || element->child_count > 1000) {
        printf("❌ ERROR: Element has suspicious counts, possible corruption\n");
        return;
    }
    
    if (element->property_count > 0 && !element->properties) {
        printf("❌ ERROR: Element has properties but NULL array\n");
        return;
    }
    
    if (element->child_count > 0 && !element->children) {
        printf("❌ ERROR: Element has children but NULL array\n");
        return;
    }
    
    // Track recursion depth
    static int recursive_depth = 0;
    if (recursive_depth > 100) {
        printf("❌ ERROR: Recursive depth exceeded 100, possible infinite loop\n");
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
            element_to_commands_recursive(child, commands, command_count, max_commands);
        }
    }
    
    recursive_depth--;
}


// Main conversion function
void kryon_element_tree_to_render_commands(KryonElement* root, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!root || !commands || !command_count) return;
    
    *command_count = 0;
    
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