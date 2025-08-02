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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <stdarg.h>

// =============================================================================
// GLOBAL STATE FOR INPUT (simplified approach)
// =============================================================================

// Global mouse position updated by renderer for hover detection
KryonVec2 g_mouse_position = {0.0f, 0.0f};

// Global cursor state for simple cursor management
bool g_cursor_should_be_pointer = false;

// Global runtime reference for variable resolution during rendering
static KryonRuntime* g_current_runtime = NULL;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool load_krb_elements(KryonRuntime *runtime, KryonKrbFile *krb_file);
static KryonElement *create_element_from_krb(KryonRuntime *runtime, KryonKrbElement *krb_element, KryonElement *parent);
static bool process_element_properties(KryonElement *element, KryonKrbReader *reader);
static void update_element_tree(KryonRuntime *runtime, KryonElement *element, double delta_time);
static void process_event_queue(KryonRuntime *runtime);
static void find_and_execute_onclick_handlers(KryonRuntime *runtime, KryonElement *element, KryonVec2 click_pos, const KryonEvent *event);
static bool is_point_in_element_bounds(KryonElement *element, KryonVec2 point);
static void runtime_error(KryonRuntime *runtime, const char *format, ...);
static const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name);
static const char* resolve_variable_reference(KryonRuntime* runtime, const char* value);

// Element to render command conversion functions
static void element_container_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_text_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_button_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static void element_center_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);

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
    
    // Initialize element registry
    runtime->element_capacity = 256;
    runtime->elements = kryon_alloc(runtime->element_capacity * sizeof(KryonElement*));
    if (!runtime->elements) {
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
    
    // Initialize event queue (legacy)
    runtime->event_capacity = 256;
    runtime->event_queue = kryon_alloc(runtime->event_capacity * sizeof(KryonEvent));
    if (!runtime->event_queue) {
        kryon_event_system_destroy(runtime->event_system);
        kryon_free(runtime->elements);
        kryon_free(runtime);
        return NULL;
    }
    
    // Initialize global state
    runtime->global_state = kryon_state_create("global", KRYON_STATE_OBJECT);
    if (!runtime->global_state) {
        kryon_free(runtime->event_queue);
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
    
    // Destroy all elements
    if (runtime->root) {
        kryon_element_destroy(runtime, runtime->root);
    }
    
    // Free element registry
    kryon_free(runtime->elements);
    
    // Destroy event system
    if (runtime->event_system) {
        kryon_event_system_destroy(runtime->event_system);
    }
    
    // Free event queue (legacy)
    kryon_free(runtime->event_queue);
    
    // Destroy global state
    if (runtime->global_state) {
        kryon_state_destroy(runtime->global_state);
    }
    
    // Destroy script VM
    if (runtime->script_vm) {
        kryon_vm_destroy(runtime->script_vm);
    }
    
    // Free styles
    if (runtime->styles) {
        for (size_t i = 0; i < runtime->style_count; i++) {
            // TODO: Implement style destruction
        }
        kryon_free(runtime->styles);
    }
    
    // Free error messages
    if (runtime->error_messages) {
        for (size_t i = 0; i < runtime->error_count; i++) {
            kryon_free(runtime->error_messages[i]);
        }
        kryon_free(runtime->error_messages);
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

static bool load_krb_elements(KryonRuntime *runtime, KryonKrbFile *krb_file) {
    // For now, we'll use the raw binary data approach
    // In a full implementation, we would use the parsed KrbFile structure
    
    // This is a placeholder - the actual implementation is in krb_loader.c
    // which processes the raw binary data directly
    
    return true;
}

// =============================================================================
// RUNTIME EXECUTION
// =============================================================================

bool kryon_runtime_start(KryonRuntime *runtime) {
    printf("🔍 DEBUG: kryon_runtime_start called\n");
    
    if (!runtime || runtime->is_running) {
        printf("❌ ERROR: runtime is NULL or already running\n");
        return false;
    }
    printf("🔍 DEBUG: runtime is valid and not running\n");
    
    if (!runtime->root) {
        printf("❌ ERROR: No root element loaded\n");
        runtime_error(runtime, "No root element loaded");
        return false;
    }
    printf("🔍 DEBUG: root element is valid\n");
    
    printf("🔍 DEBUG: Setting runtime state\n");
    runtime->is_running = true;
    runtime->last_update_time = 0.0;
    runtime->stats.frame_count = 0;
    runtime->stats.total_time = 0.0;
    
    printf("🔍 DEBUG: Runtime start completed successfully\n");
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
    
    // Process event queue
    process_event_queue(runtime);
    
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
    
    // Update input state from renderer before generating commands
    if (runtime->renderer) {
        KryonRenderer* renderer = (KryonRenderer*)runtime->renderer;
        if (renderer->vtable && renderer->vtable->get_input_state) {
            KryonInputState input_state = {0};
            KryonRenderResult result = renderer->vtable->get_input_state(&input_state);
            if (result == KRYON_RENDER_SUCCESS) {
                // Update global mouse position for hover detection
                extern KryonVec2 g_mouse_position;
                g_mouse_position = input_state.mouse.position;
                
                // Generate mouse click events from input state
                if (input_state.mouse.left_pressed) {
                    KryonEvent event = {0};
                    event.type = KRYON_EVENT_MOUSE_BUTTON_DOWN;
                    event.data.mouseButton.button = 0; // Left button
                    event.data.mouseButton.x = input_state.mouse.position.x;
                    event.data.mouseButton.y = input_state.mouse.position.y;
                    kryon_runtime_handle_event(runtime, &event);
                    printf("🐛 DEBUG: Generated mouse click event at (%.1f, %.1f)\n", 
                           event.data.mouseButton.x, event.data.mouseButton.y);
                }
            }
        }
    }
    
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
    element->state = KRYON_ELEMENT_CREATED;
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
    element->state = KRYON_ELEMENT_UNMOUNTING;
    
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
    
    // Set state to destroyed
    element->state = KRYON_ELEMENT_DESTROYED;
    
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
    if (!runtime || !event) {
        return false;
    }
    
    // Add event to queue
    size_t next_write = (runtime->event_write_pos + 1) % runtime->event_capacity;
    if (next_write == runtime->event_read_pos) {
        // Queue is full, expand it
        size_t new_capacity = runtime->event_capacity * 2;
        KryonEvent *new_queue = kryon_realloc(runtime->event_queue,
                                             new_capacity * sizeof(KryonEvent));
        if (!new_queue) {
            return false;
        }
        
        // Reorganize queue to be contiguous
        if (runtime->event_write_pos < runtime->event_read_pos) {
            // Queue wraps around, need to move tail
            size_t tail_size = runtime->event_capacity - runtime->event_read_pos;
            memcpy(new_queue + runtime->event_capacity, 
                   new_queue + runtime->event_read_pos,
                   tail_size * sizeof(KryonEvent));
            runtime->event_read_pos += runtime->event_capacity;
        }
        
        runtime->event_queue = new_queue;
        runtime->event_capacity = new_capacity;
        next_write = (runtime->event_write_pos + 1) % runtime->event_capacity;
    }
    
    // Copy event to queue
    runtime->event_queue[runtime->event_write_pos] = *event;
    runtime->event_write_pos = next_write;
    runtime->event_count++;
    
    return true;
}

static void process_event_queue(KryonRuntime *runtime) {
    if (runtime->event_read_pos != runtime->event_write_pos) {
        printf("🐛 DEBUG: Processing event queue, %zu events pending\n", runtime->event_count);
    }
    
    while (runtime->event_read_pos != runtime->event_write_pos) {
        KryonEvent *event = &runtime->event_queue[runtime->event_read_pos];
        
        printf("🐛 DEBUG: Processing event type: %d\n", event->type);
        
        // Process mouse click events for onClick handlers
        if (event->type == KRYON_EVENT_MOUSE_BUTTON_DOWN && event->data.mouseButton.button == 0) {
            // Left mouse button clicked - check for onClick handlers
            KryonVec2 click_pos = {event->data.mouseButton.x, event->data.mouseButton.y};
            
            printf("🔥 DEBUG: Mouse click detected at (%.1f, %.1f)\n", click_pos.x, click_pos.y);
            
            // Find elements at click position with onClick handlers
            if (runtime->root) {
                find_and_execute_onclick_handlers(runtime, runtime->root, click_pos, event);
            }
        }
        
        runtime->event_read_pos = (runtime->event_read_pos + 1) % runtime->event_capacity;
        runtime->event_count--;
    }
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
        case KRYON_ELEMENT_CREATED:
            element->state = KRYON_ELEMENT_MOUNTING;
            // TODO: Call onMount lifecycle hook
            element->state = KRYON_ELEMENT_MOUNTED;
            break;
            
        case KRYON_ELEMENT_MOUNTED:
            if (element->needs_layout || element->needs_render) {
                element->state = KRYON_ELEMENT_UPDATING;
                // TODO: Perform layout and render updates
                element->state = KRYON_ELEMENT_MOUNTED;
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
        printf("🔄 DEBUG: Resolved variable '$%s' to '%s'\n", var_name, resolved_value);
        return resolved_value;
    }
    
    printf("⚠️  DEBUG: Variable '$%s' not found, using literal value\n", var_name);
    return value; // Return original if not found
}

// Get property value as string with type conversion
static const char* get_element_property_string(KryonElement* element, const char* prop_name) {
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
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            // Resolve variable references if runtime is available
            if (runtime) {
                return resolve_variable_reference(runtime, prop->value.string_value);
            }
            return prop->value.string_value;
            
        case KRYON_RUNTIME_PROP_INTEGER: {
            // Use rotating buffers to prevent overwriting in concurrent calls
            static char int_buffers[4][32];
            static int int_buffer_index = 0;
            char *buffer = int_buffers[int_buffer_index];
            int_buffer_index = (int_buffer_index + 1) % 4;
            snprintf(buffer, 32, "%lld", (long long)prop->value.int_value);
            return buffer;
        }
        
        case KRYON_RUNTIME_PROP_FLOAT: {
            // Use rotating buffers to prevent overwriting in concurrent calls
            static char float_buffers[4][32];
            static int float_buffer_index = 0;
            char *buffer = float_buffers[float_buffer_index];
            float_buffer_index = (float_buffer_index + 1) % 4;
            snprintf(buffer, 32, "%.6f", prop->value.float_value);
            return buffer;
        }
        
        case KRYON_RUNTIME_PROP_BOOLEAN:
            return prop->value.bool_value ? "true" : "false";
            
        case KRYON_RUNTIME_PROP_COLOR: {
            // Use rotating buffers to avoid overwriting when called multiple times
            static char color_buffers[4][16];
            static int buffer_index = 0;
            char *buffer = color_buffers[buffer_index];
            buffer_index = (buffer_index + 1) % 4;
            snprintf(buffer, 16, "#%08X", prop->value.color_value);
            return buffer;
        }
        
        case KRYON_RUNTIME_PROP_REFERENCE: {
            // Use rotating buffers to prevent overwriting in concurrent calls
            static char ref_buffers[4][32];
            static int ref_buffer_index = 0;
            char *buffer = ref_buffers[ref_buffer_index];
            ref_buffer_index = (ref_buffer_index + 1) % 4;
            snprintf(buffer, 32, "ref_%u", prop->value.ref_id);
            return buffer;
        }
        
        case KRYON_RUNTIME_PROP_FUNCTION:
            // Function properties are stored as string values (function names)
            return prop->value.string_value;
        
        default:
            return NULL;
    }
}

// Get property value as integer with type conversion
static int get_element_property_int(KryonElement* element, const char* prop_name, int default_value) {
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
static float get_element_property_float(KryonElement* element, const char* prop_name, float default_value) {
    KryonProperty* prop = find_element_property(element, prop_name);
    if (!prop) return default_value;
    
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
static bool get_element_property_bool(KryonElement* element, const char* prop_name, bool default_value) {
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
static uint32_t get_element_property_color(KryonElement* element, const char* prop_name, uint32_t default_value) {
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

// Debug function to log all properties of an element
static void debug_log_element_properties(KryonElement* element, const char* element_name) {
    if (!element) return;
    
    static bool debug_logged = false;
    if (debug_logged) return;
    debug_logged = true;
    
    printf("🔍 DEBUG: Properties for %s:\n", element_name);
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

// Convert Container element to render commands
static void element_container_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Debug: Log all properties for first container
    debug_log_element_properties(element, "Container");
    
    // Get container properties with comprehensive type handling
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 100.0f);
    float height = get_element_property_float(element, "height", 100.0f);
    
    // Check both "background" and "backgroundColor" since they map to the same property ID
    const char* bg_color_str = get_element_property_string(element, "background");
    if (!bg_color_str) {
        bg_color_str = get_element_property_string(element, "backgroundColor");
    }
    const char* border_color_str = get_element_property_string(element, "borderColor");
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    
    // Safety check for NULL color strings
    if (bg_color_str && strlen(bg_color_str) == 0) {
        bg_color_str = NULL;
    }
    if (border_color_str && strlen(border_color_str) == 0) {
        border_color_str = NULL;
    }
    
    // Debug: Log container properties
    static bool container_debug_logged = false;
    if (!container_debug_logged) {
        printf("🎨 DEBUG Container: pos=%.1f,%.1f size=%.1fx%.1f bg='%s' border='%s' width=%.1f radius=%.1f\n", 
               posX, posY, width, height, 
               bg_color_str ? bg_color_str : "NULL", 
               border_color_str ? border_color_str : "NULL", 
               border_width, border_radius);
        container_debug_logged = true;
    }
    
    // Use the proper color parsing utility instead of the basic one
    uint32_t bg_color_val = kryon_color_parse_string(bg_color_str ? bg_color_str : "transparent");
    uint32_t border_color_val = kryon_color_parse_string(border_color_str ? border_color_str : "transparent");
    
    // Convert to KryonColor format
    KryonColor bg_color = {
        ((bg_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((bg_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((bg_color_val >> 8) & 0xFF) / 255.0f,   // B
        (bg_color_val & 0xFF) / 255.0f           // A
    };
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
        
        commands[(*command_count)++] = cmd;
    }
}

// Convert Button element to render commands using proper button widget command
static void element_button_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return; // Need space for button command
    
    // Get button properties with dynamic lookup
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 150.0f);
    float height = get_element_property_float(element, "height", 50.0f);
    
    // Use canonical property name from mapping system
    const char* bg_color_str = get_element_property_string(element, "background");
    
    const char* border_color_str = get_element_property_string(element, "borderColor");
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 8.0f);
    
    const char* text = get_element_property_string(element, "text");
    
    const char* text_color_str = get_element_property_string(element, "color");
    
    // Style inheritance - traverse up the parent chain to find an element with a class,
    // then resolve the color from that style
    if (!text_color_str) {
        KryonElement* ancestor = element;
        while (ancestor) {
            // Check if this ancestor has any class names
            if (ancestor->class_count > 0 && ancestor->class_names) {
                for (size_t i = 0; i < ancestor->class_count; i++) {
                    if (ancestor->class_names[i]) {
                        // For now, hardcode the base style color since KRB styles aren't loaded yet
                        // TODO: Replace with proper style resolution from KRB style data
                        if (strcmp(ancestor->class_names[i], "base") == 0) {
                            text_color_str = "#FF9900FF";  // Orange color from base style
                            goto color_found;
                        }
                    }
                }
            }
            ancestor = ancestor->parent;
        }
    }
    color_found:
    
    if (!text_color_str) {
        text_color_str = "#FFFFFFFF";  // White fallback
    }
    const char* text_align = get_element_property_string(element, "textAlign");
    float font_size = get_element_property_float(element, "fontSize", 16.0f);
    
    // Removed verbose debug logging
    
    // Parse colors with proper fallbacks
    uint32_t bg_color_val = kryon_color_parse_string(bg_color_str ? bg_color_str : "#404080FF");
    uint32_t border_color_val = kryon_color_parse_string(border_color_str ? border_color_str : "#0099FFFF");
    uint32_t text_color_val = kryon_color_parse_string(text_color_str ? text_color_str : "#FFFFFFFF");
    
    // Convert to KryonColor format
    KryonColor bg_color = {
        ((bg_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((bg_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((bg_color_val >> 8) & 0xFF) / 255.0f,   // B
        (bg_color_val & 0xFF) / 255.0f           // A
    };
    KryonColor border_color = {
        ((border_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((border_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((border_color_val >> 8) & 0xFF) / 255.0f,   // B
        (border_color_val & 0xFF) / 255.0f           // A
    };
    KryonColor text_color = {
        ((text_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((text_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((text_color_val >> 8) & 0xFF) / 255.0f,   // B
        (text_color_val & 0xFF) / 255.0f           // A
    };
    
    // Check if mouse is hovering over button bounds
    // For now, get mouse position directly from runtime's renderer if available
    bool is_hovered = false;
    
    // Try to get input state from the renderer
    // This is a simplified approach - in a full implementation this would be passed as a parameter
    extern KryonVec2 g_mouse_position; // Global mouse position updated by renderer
    
    KryonVec2 mouse_pos = g_mouse_position;
    
    // Debug: Show real mouse position and button bounds occasionally
    // Mouse hover detection (debug output reduced)
    
    // Check if mouse is within button bounds
    if (mouse_pos.x >= posX && mouse_pos.x <= posX + width &&
        mouse_pos.y >= posY && mouse_pos.y <= posY + height) {
        is_hovered = true;
    }
    
    // Apply hover effects
    KryonColor final_bg_color = bg_color;
    KryonColor final_text_color = text_color;
    
    // Update global cursor state
    extern bool g_cursor_should_be_pointer;
    static bool was_hovered = false;
    if (is_hovered != was_hovered) {
        g_cursor_should_be_pointer = is_hovered;
        was_hovered = is_hovered;
    }
    
    if (is_hovered) {
        // Lighten background color for hover effect
        final_bg_color.r = fminf(1.0f, bg_color.r + 0.1f);
        final_bg_color.g = fminf(1.0f, bg_color.g + 0.1f);
        final_bg_color.b = fminf(1.0f, bg_color.b + 0.1f);
        
        // Hover effect active
    }
    
    // Use the proper button widget command instead of separate rect + text
    KryonRenderCommand cmd = kryon_cmd_draw_button(
        "button_1",  // TODO: Generate unique widget ID from element
        (KryonVec2){posX, posY},
        (KryonVec2){width, height},
        text ? text : "Button",
        final_bg_color,
        final_text_color
    );
    
    // Set the widget state
    cmd.data.draw_button.state = is_hovered ? KRYON_WIDGET_STATE_HOVERED : KRYON_WIDGET_STATE_NORMAL;
    cmd.data.draw_button.enabled = true;
    
    // Set onClick handler if present
    const char* onclick = get_element_property_string(element, "onClick");
    if (onclick) {
        cmd.data.draw_button.onclick_handler = strdup(onclick);
    }
    
    commands[(*command_count)++] = cmd;
}

// Convert Center element to render commands (handles child positioning)
static void element_center_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
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

// Convert Text element to render commands  
static void element_text_to_commands(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Debug: Log all properties for first text element
    debug_log_element_properties(element, "Text");
    
    // Get text properties with comprehensive type handling
    const char* text = get_element_property_string(element, "text");
    const char* color_str = get_element_property_string(element, "color");
    float font_size = get_element_property_float(element, "fontSize", 20.0f);
    const char* font_family = get_element_property_string(element, "fontFamily");
    const char* font_weight = get_element_property_string(element, "fontWeight");
    const char* text_align = get_element_property_string(element, "textAlignment");
    
    // Get text positioning
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float max_width = get_element_property_float(element, "maxWidth", 0.0f);
    
    // Debug: Log text properties
    static bool text_debug_logged = false;
    if (!text_debug_logged) {
        printf("🎨 DEBUG Text: text='%s' color='%s' size=%.1f pos=%.1f,%.1f font='%s' weight='%s' align='%s'\n", 
               text ? text : "NULL", 
               color_str ? color_str : "NULL",
               font_size, posX, posY,
               font_family ? font_family : "NULL",
               font_weight ? font_weight : "NULL",
               text_align ? text_align : "NULL");
        text_debug_logged = true;
    }
    
    if (!text) return;
    
    // Use the proper color parsing utility instead of the basic one
    uint32_t text_color_val = kryon_color_parse_string(color_str ? color_str : "black");
    KryonColor text_color = {
        ((text_color_val >> 24) & 0xFF) / 255.0f,  // R
        ((text_color_val >> 16) & 0xFF) / 255.0f,  // G
        ((text_color_val >> 8) & 0xFF) / 255.0f,   // B
        (text_color_val & 0xFF) / 255.0f           // A
    };
    
    // Calculate proper positioning within parent container
    KryonVec2 position = {posX, posY};
    
    // If element has a parent and no explicit position, calculate centered position
    if (element->parent && posX == 0.0f && posY == 0.0f) {
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
            printf("🎯 DEBUG: Centered in parent: pos=(%.1f,%.1f) parent=%.1f,%.1f+%.1fx%.1f padding=%.1f\n", 
                   position.x, position.y, parent_x, parent_y, parent_width, parent_height, parent_padding);
        } else {
            // Default to top-left of available area
            position.x = available_x;
            position.y = available_y;
            printf("🎯 DEBUG: Top-left in parent: pos=(%.1f,%.1f)\n", position.x, position.y);
        }
    } else {
        printf("🎯 DEBUG: Using explicit positioning: pos=(%.1f,%.1f)\n", position.x, position.y);
    }
    
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
            printf("🎯 DEBUG: Using parent contentAlignment 'center' for text alignment\n");
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
    cmd.z_index = 0;
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

// Recursive function to convert element tree to render commands
static void element_to_commands_recursive(KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !commands || !command_count || *command_count >= max_commands) return;
    
    // Additional safety checks
    if (!element->type_name) {
        printf("⚠️  WARNING: Element has null type_name, skipping\n");
        return;
    }
    
    // Critical bounds checking - prevent corruption or infinite loops
    if (element->property_count > 1000) {
        printf("❌ ERROR: Element has suspicious property_count=%zu, possible corruption\n", element->property_count);
        return;
    }
    
    if (element->child_count > 1000) {
        printf("❌ ERROR: Element has suspicious child_count=%zu, possible corruption\n", element->child_count);
        return;
    }
    
    // Check properties array validity (always check, not just in debug)
    if (element->property_count > 0 && !element->properties) {
        printf("❌ ERROR: Element has property_count=%zu but properties array is NULL\n", element->property_count);
        return;
    }
    
    // Check children array validity (always check, not just in debug)
    if (element->child_count > 0 && !element->children) {
        printf("❌ ERROR: Element has child_count=%zu but children array is NULL\n", element->child_count);
        return;
    }
    
    // Debug: Log each element being processed (reduced logging)
    static int elements_processed = 0;
    if (elements_processed++ < 5) { // Only log first 5 elements
        printf("🔍 DEBUG: Processing element %s with %zu properties, %zu children\n", 
               element->type_name, element->property_count, element->child_count);
    }
    
    // First run layout elements (Center) to position children
    if (strcmp(element->type_name, "Center") == 0) {
        element_center_to_commands(element, commands, command_count, max_commands);
    }
    
    // Then convert visible elements based on their type
    if (strcmp(element->type_name, "Container") == 0) {
        element_container_to_commands(element, commands, command_count, max_commands);
    } else if (strcmp(element->type_name, "Text") == 0) {
        element_text_to_commands(element, commands, command_count, max_commands);
    } else if (strcmp(element->type_name, "Button") == 0) {
        element_button_to_commands(element, commands, command_count, max_commands);
    }
    // App elements don't render directly, just process children
    
    // Process children with recursive depth protection
    static int recursive_depth = 0;
    if (recursive_depth > 100) {
        printf("❌ ERROR: Recursive depth exceeded 100, possible infinite loop\n");
        return;
    }
    
    recursive_depth++;
    for (size_t i = 0; i < element->child_count && *command_count < max_commands; i++) {
        KryonElement* child = element->children[i];
        if (child) {
            // Verify child pointer is valid before recursion
            if (child->type_name) { // Basic validity check
                element_to_commands_recursive(child, commands, command_count, max_commands);
            } else {
                printf("⚠️  WARNING: Child element %zu has null type_name, skipping\n", i);
            }
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
}

// =============================================================================
// ONCLICK EVENT HANDLING WITH VM INTEGRATION
// =============================================================================

static void find_and_execute_onclick_handlers(KryonRuntime *runtime, KryonElement *element, KryonVec2 click_pos, const KryonEvent *event) {
    if (!runtime || !element) {
        return;
    }
    
    printf("🐛 DEBUG: Checking element '%s' for onClick handlers\n", 
           element->type_name ? element->type_name : "Unknown");
    
    // Check if this element has onClick handler and if click is within bounds
    const char* onclick_handler = get_element_property_string(element, "onClick");
    printf("🐛 DEBUG: Element onClick handler: %s\n", onclick_handler ? onclick_handler : "(none)");
    
    if (onclick_handler) {
        bool in_bounds = is_point_in_element_bounds(element, click_pos);
        printf("🐛 DEBUG: Click (%.1f, %.1f) in bounds: %s\n", click_pos.x, click_pos.y, in_bounds ? "YES" : "NO");
        
        if (in_bounds) {
            printf("🔥 Click detected on element '%s' with handler: %s\n", 
                   element->type_name ? element->type_name : "Unknown", onclick_handler);
            
            // Execute the onClick handler using the script VM
            if (runtime->script_vm) {
                KryonVMResult result = kryon_vm_call_function(runtime->script_vm, onclick_handler, element, event);
                if (result == KRYON_VM_SUCCESS) {
                    printf("✅ Successfully executed onClick handler: %s\n", onclick_handler);
                } else {
                    const char* error = kryon_vm_get_error(runtime->script_vm);
                    printf("❌ Failed to execute onClick handler '%s': %s\n", 
                           onclick_handler, error ? error : "Unknown error");
                }
            } else {
                printf("⚠️  No script VM available to execute onClick handler: %s\n", onclick_handler);
            }
            
            // Don't propagate click events after handling (for now)
            return;
        }
    }
    
    // Recursively check children
    printf("🐛 DEBUG: Checking %zu children of element '%s'\n", 
           element->child_count, element->type_name ? element->type_name : "Unknown");
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            find_and_execute_onclick_handlers(runtime, element->children[i], click_pos, event);
        }
    }
}

static bool is_point_in_element_bounds(KryonElement *element, KryonVec2 point) {
    if (!element) {
        return false;
    }
    
    // Get element bounds from properties
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 100.0f);
    float height = get_element_property_float(element, "height", 50.0f);
    
    // Check if point is within bounds
    return (point.x >= posX && point.x <= posX + width &&
            point.y >= posY && point.y <= posY + height);
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