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
#include "component_state.h"
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
static const char* resolve_variable_reference(KryonRuntime* runtime, KryonElement* element, const char* value);
static const char* resolve_component_state_variable(KryonRuntime* runtime, KryonElement* element, const char* var_name);

// @for template expansion functions
static void expand_for_template(KryonRuntime* runtime, KryonElement* for_element);
static void expand_for_iteration(KryonRuntime* runtime, KryonElement* for_element, const char* index_var_name, const char* var_name, const char* var_value, size_t index, size_t insert_position);
static void clear_expanded_children(KryonRuntime* runtime, KryonElement* for_element);
static KryonElement* clone_element_with_substitution(KryonRuntime* runtime, KryonElement* template_element, const char* var_name, const char* var_value, size_t index);
static char* substitute_template_variable(const char* template_str, const char* var_name, const char* var_value);
static const char* extract_json_property(const char* json_object, const char* property_name);
static void add_child_element(KryonRuntime* runtime, KryonElement* parent, KryonElement* child);
static void insert_child_element_at(KryonRuntime* runtime, KryonElement* parent, KryonElement* child, size_t position);
static char* extract_json_field(const char* json_value, const char* field_name);
static void set_loop_context_recursive(KryonElement* element, const char* index_var_name, const char* index_var_value, const char* var_name, const char* var_value);

// @if template expansion functions
static void mark_cloned_from_directive_recursive(KryonElement* element, KryonElement* directive);
static KryonElement* clone_element_deep(KryonRuntime* runtime, KryonElement* element);

// External function from krb_loader.c
extern bool kryon_runtime_load_krb_data(KryonRuntime *runtime, const uint8_t *data, size_t size);

// Helper function to resolve template properties with @for variable context
static char* resolve_for_template_property(KryonRuntime* runtime, KryonProperty* property, 
                                         const char* var_name, const char* var_value);

// Layout calculation functions  
static void clear_layout_flags_recursive(KryonElement* element);

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

static void free_script_function(KryonScriptFunction* fn) {
    if (!fn) {
        return;
    }

    kryon_free(fn->name);
    kryon_free(fn->language);
    kryon_free(fn->code);
    if (fn->parameters) {
        for (uint16_t i = 0; i < fn->param_count; i++) {
            kryon_free(fn->parameters[i]);
        }
        kryon_free(fn->parameters);
    }

    memset(fn, 0, sizeof(*fn));
}

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

void kryon_runtime_clear_all_content(KryonRuntime *runtime) {
    if (!runtime) {
        return;
    }
    
    printf("🧹 Clearing all runtime content for navigation\n");
    
    // Destroy all elements (with null check for safety)
    if (runtime->root) {
        kryon_element_destroy(runtime, runtime->root);
        runtime->root = NULL;
    }
    
    // Clear element counter
    runtime->element_count = 0;
    runtime->next_element_id = 1;
    
    // Clear string table
    if (runtime->string_table) {
        for (uint32_t i = 0; i < runtime->string_table_count; i++) {
            kryon_free(runtime->string_table[i]);
        }
        kryon_free(runtime->string_table);
        runtime->string_table = NULL;
        runtime->string_table_count = 0;
    }
    
    // Clear variables (keep global state but clear user variables)
    if (runtime->variable_names) {
        for (size_t i = 0; i < runtime->variable_count; i++) {
            kryon_free(runtime->variable_names[i]);
            kryon_free(runtime->variable_values[i]);
        }
        kryon_free(runtime->variable_names);
        kryon_free(runtime->variable_values);
        runtime->variable_names = NULL;
        runtime->variable_values = NULL;
        runtime->variable_count = 0;
    }
    
    // Clear functions
    if (runtime->script_functions) {
        for (size_t i = 0; i < runtime->function_count; i++) {
            free_script_function(&runtime->script_functions[i]);
        }
        kryon_free(runtime->script_functions);
        runtime->script_functions = NULL;
        runtime->function_count = 0;
        runtime->function_capacity = 0;
    }
    
    // Clear components
    if (runtime->components) {
        for (size_t i = 0; i < runtime->component_count; i++) {
            if (runtime->components[i]) {
                kryon_free(runtime->components[i]->name);
                // Component body cleanup would go here if needed
                kryon_free(runtime->components[i]);
            }
        }
        runtime->component_count = 0;
    }
    
    // Clear errors
    kryon_runtime_clear_errors(runtime);
    
    // Reset state flags
    runtime->needs_update = true;
    runtime->is_loading = false;
    
    printf("✅ Runtime content cleared successfully\n");
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

    if (runtime->script_functions) {
        for (size_t i = 0; i < runtime->function_count; i++) {
            free_script_function(&runtime->script_functions[i]);
        }
        kryon_free(runtime->script_functions);
        runtime->script_functions = NULL;
        runtime->function_count = 0;
        runtime->function_capacity = 0;
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
    
    // Destroy navigation manager (with null check)
    if (runtime->navigation_manager) {
        kryon_navigation_destroy(runtime->navigation_manager);
        runtime->navigation_manager = NULL;
    }
    
    // Free current file path (with null check)
    if (runtime->current_file_path) {
        kryon_free(runtime->current_file_path);
        runtime->current_file_path = NULL;
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
    
    // Set current file path for navigation
    if (success) {
        // Store the current file path
        if (runtime->current_file_path) {
            kryon_free(runtime->current_file_path);
        }
        runtime->current_file_path = kryon_strdup(filename);
        printf("🧭 Stored runtime file path: %s\n", filename);
        
        // Set up navigation if navigation manager exists
        if (runtime->navigation_manager) {
            kryon_navigation_set_current_path(runtime->navigation_manager, filename);
        }
    }
    
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
    if (element->component_scope_id) {
        kryon_free(element->component_scope_id);
    }

    // Free @for directive template storage
    if (element->template_children) {
        // Note: template children are references to actual elements that will be freed
        // through normal element destruction, so we only free the array itself
        free(element->template_children);
        element->template_children = NULL;
        element->template_count = 0;
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
static const char* resolve_variable_reference(KryonRuntime* runtime, KryonElement* element, const char* value) {
    if (!runtime || !value) {
        return value; // Invalid input
    }

    // First, check if this is a loop context variable on the element or its ancestors
    KryonElement* current = element;
    while (current) {
        // Check loop iteration variable
        if (current->loop_var_name && strcmp(current->loop_var_name, value) == 0) {
            return current->loop_var_value;
        }
        // Check loop index variable
        if (current->loop_index_var_name && strcmp(current->loop_index_var_name, value) == 0) {
            return current->loop_index_var_value;
        }
        current = current->parent;
    }

    // Fall back to global runtime variables
    const char* resolved_value = kryon_runtime_get_variable(runtime, value);
    if (resolved_value) {
        // Add basic validation to ensure the pointer is reasonable
        if (resolved_value < (const char*)0x1000) {
            return value; // Return original if pointer is invalid
        }

        // If the resolved value is itself a @ref, recursively resolve it
        if (strncmp(resolved_value, "@ref:", 5) == 0) {
            return resolve_variable_reference(runtime, element, resolved_value + 5);
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

    // First, try the new scoped state resolution system
    const char* scoped_result = kryon_resolve_scoped_state_variable(runtime, element, var_name);
    if (scoped_result) {
        return scoped_result;
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
        return NULL;
    }
    
    // Debug output for "to" property specifically
    if (strcmp(prop_name, "to") == 0) {
        printf("🐛 DEBUG: Found 'to' property, type=%d, element=%s\n", prop->type, element->type_name ? element->type_name : "unknown");
        if (prop->type == KRYON_RUNTIME_PROP_STRING) {
            printf("🐛 DEBUG: 'to' property STRING value: '%s'\n", prop->value.string_value ? prop->value.string_value : "NULL");
        } else if (prop->type == KRYON_RUNTIME_PROP_TEMPLATE) {
            printf("🐛 DEBUG: 'to' property is TEMPLATE type with %zu segments\n", prop->value.template_value.segment_count);
        }
    }
    
    // Use simple static buffer for string conversions
    static char buffer[1024];
    
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            // For literal strings, just return the value directly
            if (strcmp(prop_name, "to") == 0) {
                printf("🐛 PROP ACCESS: STRING property 'to' = '%s' (ptr=%p)\n", 
                       prop->value.string_value ? prop->value.string_value : "NULL", 
                       (void*)prop->value.string_value);
            }
            return prop->value.string_value ? prop->value.string_value : "";
            
        case KRYON_RUNTIME_PROP_REFERENCE:
            // For variable references, try to resolve them
            if (prop->is_bound && prop->binding_path && runtime) {
                const char* resolved_value = resolve_variable_reference(runtime, element, prop->binding_path);
                if (resolved_value && resolved_value != prop->binding_path) {  // Check it actually resolved
                    return resolved_value;
                } else {
                    // Variable not found - return NULL instead of fallback
                    return NULL;
                }
            } else {
                // Property not properly bound - return NULL instead of fallback
                return NULL;
            }
            
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
        
        case KRYON_RUNTIME_PROP_TEMPLATE:
            // Template properties should have been resolved during @for expansion,
            // but handle them gracefully by resolving on-demand
            if (strcmp(prop_name, "to") == 0) {
                printf("🐛 DEBUG: 'to' property is still TEMPLATE - this shouldn't happen after @for expansion!\n");
            }
            
            // Try to resolve template segments to build a string
            buffer[0] = '\0'; // Start with empty string
            for (size_t i = 0; i < prop->value.template_value.segment_count; i++) {
                KryonTemplateSegment* segment = &prop->value.template_value.segments[i];
                if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL && segment->data.literal_text) {
                    strncat(buffer, segment->data.literal_text, sizeof(buffer) - strlen(buffer) - 1);
                } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE && runtime) {
                    // Try to resolve variable from runtime
                    const char* var_name = segment->data.variable_name;
                    if (var_name) {
                        for (size_t j = 0; j < runtime->variable_count; j++) {
                            if (runtime->variable_names[j] && strcmp(runtime->variable_names[j], var_name) == 0) {
                                if (runtime->variable_values[j]) {
                                    strncat(buffer, runtime->variable_values[j], sizeof(buffer) - strlen(buffer) - 1);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            return buffer;
        
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
            
        case KRYON_RUNTIME_PROP_REFERENCE:
            // For variable references, try to resolve them
            if (prop->is_bound && prop->binding_path && g_current_runtime) {
                const char* resolved_value = resolve_variable_reference(g_current_runtime, element, prop->binding_path);
                if (resolved_value && resolved_value != prop->binding_path) {
                    // Parse the resolved color string
                    const char* str = resolved_value;
                    if (str[0] == '#') str++;
                    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) str += 2;

                    unsigned int color;
                    if (sscanf(str, "%x", &color) == 1) {
                        return (uint32_t)color;
                    }
                }
            }
            return default_value;
            
        default:
            return default_value;
    }
}

// Get property value as color with inheritance support
uint32_t get_element_property_color_inherited(KryonElement* element, const char* prop_name, uint32_t default_value) {
    if (!element || !prop_name) return default_value;

    // Check if property exists on this element
    KryonProperty* prop = find_element_property(element, prop_name);
    if (prop) {
        // Found on this element, use the existing logic
        return get_element_property_color(element, prop_name, default_value);
    }

    // Property not found - check if it's inheritable
    uint16_t prop_hex = kryon_get_property_hex(prop_name);
    if (prop_hex != 0 && kryon_is_property_inheritable(prop_hex)) {
        // Walk up the parent chain
        KryonElement* current = element->parent;
        while (current != NULL) {
            prop = find_element_property(current, prop_name);
            if (prop) {
                // Found in parent, return its value
                return get_element_property_color(current, prop_name, default_value);
            }
            current = current->parent;
        }
    }

    // Not found anywhere or not inheritable
    return default_value;
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
            }
            // No fallback - if width not set and no renderer, leave as-is for parent to handle
            
            if (*height < 0) {
                *height = font_size + 20.0f; // Font height + padding (10px top/bottom)
            }
            
        } else if (strcmp(element->type_name, "Text") == 0) {
            font_size = get_element_property_float(element, "fontSize", 16.0f); // Text elements default size
            
            if (*width < 0 && text && renderer && renderer->vtable && renderer->vtable->measure_text_width) {
                *width = renderer->vtable->measure_text_width(text, font_size);
            }
            // No fallback - if width not set and no renderer, leave as-is for parent to handle
            
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
    if (!element || !commands || !command_count || *command_count >= max_commands) {
        return;
    }

    // Skip rendering elements that haven't had their layout calculated yet
    if (element->needs_layout) {
        return;
    }

    // Enhanced safety checks for overlay injection corruption
    if (!element->type_name ||
        element->property_count > 1000 || element->child_count > 1000 ||
        (element->property_count > 0 && !element->properties) ||
        (element->child_count > 0 && !element->children)) {
        return;
    }
    
    // CRITICAL FIX: Validate all properties before rendering
    for (size_t i = 0; i < element->property_count; i++) {
        if (!element->properties[i] || !element->properties[i]->name) {
            return; // Skip corrupted elements safely
        }
        // Additional validation for string properties
        if (element->properties[i]->type == KRYON_RUNTIME_PROP_STRING) {
            if (!element->properties[i]->value.string_value) {
                return; // Skip elements with corrupted string properties
            }
        }
    }
    
    // Track recursion depth
    static int recursive_depth = 0;
    if (recursive_depth > 100) {
        return;
    }
    
    
    recursive_depth++;
    
    // Step 1: Process layout commands (these position children but don't render)
    process_layout(element, commands, command_count, max_commands);
    
    // Step 2: Handle component instances by expanding them
    if (element->component_instance && element->component_instance->definition && element->component_instance->definition->ui_template) {
        printf("🔄 Expanding component instance: %s\n", element->component_instance->definition->name);
        
        // Render the component's UI template instead of the component instance
        KryonElement* ui_template = element->component_instance->definition->ui_template;
        
        // Temporarily set the template's position and properties from the component instance
        ui_template->x = element->x;
        ui_template->y = element->y;
        ui_template->width = element->width;
        ui_template->height = element->height;
        ui_template->visible = element->visible;
        
        // Recursively render the UI template
        element_to_commands_recursive(ui_template, commands, command_count, max_commands);
        
        printf("✅ Component instance expanded and rendered\n");
    } else {
        // Step 2: Render the current element normally (parent renders first = background)
        render_element(element, commands, command_count, max_commands);
    }
    
    // Step 3: Recursively render all children (children render on top of parent)
    if (strcmp(element->type_name, "TabPanel") == 0) {
        for (size_t i = 0; i < element->child_count; i++) {
            if (element->children[i]) {
                // Show grandchildren if this is a Column
                if (strcmp(element->children[i]->type_name, "Column") == 0) {
                    for (size_t j = 0; j < element->children[i]->child_count; j++) {
                        KryonElement* grandchild = element->children[i]->children[j];
                        if (grandchild) {
                            printf("    Grandchild %zu: %s, visible=%d, type=0x%04X\n", j,
                                   grandchild->type_name, grandchild->visible, grandchild->type);
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < element->child_count && *command_count < max_commands; i++) {
        KryonElement* child = element->children[i];
        if (child && child->type_name) {

            if (child->visible) {
                // Skip directive elements - they are templates, not renderable UI elements
                if (child->type == 0x8200 || // @for directive hex code
                    child->type == 0x8300) { // @if directive hex code
                        continue;
                }
                element_to_commands_recursive(child, commands, command_count, max_commands);
            }
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
            const char* value = runtime->variable_values[i];

            // If value is a variable reference (@ref:variableName), resolve it recursively
            if (value && strncmp(value, "@ref:", 5) == 0) {
                const char* ref_var_name = value + 5;
                const char* resolved = kryon_runtime_get_variable(runtime, ref_var_name);
                return resolved ? resolved : value; // Return original if resolution fails
            }

            return value;
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
        printf("🔄 Found @for template element, calling expand_for_template\n");
        expand_for_template(runtime, element);
        // Don't process children of @for elements recursively; they are templates.
        return;
    }

    
    // Process children recursively (but skip children of @for templates)
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            if (element->children[i]->type_name) {
                printf("🔍 Processing child element: %s\n", element->children[i]->type_name);
            }
            process_for_directives(runtime, element->children[i]);
        }
    }
}

/**
 * @brief Recursively set loop context on an element and all its descendants
 * @param element Element to set context on
 * @param index_var_name Index variable name (e.g., "i"), or NULL if no index variable
 * @param index_var_value Index variable value (e.g., "0")
 * @param var_name Value variable name (e.g., "habit")
 * @param var_value Value variable value (e.g., "Meditation")
 */
static void set_loop_context_recursive(KryonElement* element,
                                       const char* index_var_name,
                                       const char* index_var_value,
                                       const char* var_name,
                                       const char* var_value) {
    if (!element) return;

    // Set loop context on this element
    if (index_var_name && index_var_value) {
        element->loop_index_var_name = kryon_strdup(index_var_name);
        element->loop_index_var_value = kryon_strdup(index_var_value);
    }
    if (var_name && var_value) {
        element->loop_var_name = kryon_strdup(var_name);
        element->loop_var_value = kryon_strdup(var_value);
    }

    // Recursively set on all children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            set_loop_context_recursive(element->children[i], index_var_name, index_var_value, var_name, var_value);
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
    // We track how many elements were generated last time to avoid removing static elements
    static size_t previous_generated_counts[100] = {0}; // Simple tracking by @for element ID
    size_t generated_count_idx = for_element->id % 100;
    size_t previous_count = previous_generated_counts[generated_count_idx];

    if (for_element->parent && previous_count > 0) {
        printf("🧹 CLEANUP: Removing %zu previously generated elements for @for directive\n", previous_count);

        KryonElement* parent = for_element->parent;

        // Find the @for element's position in parent's children
        size_t for_position = 0;
        for (size_t i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == for_element) {
                for_position = i;
                break;
            }
        }

        // Remove only the number of elements that were generated last time
        size_t removed = 0;
        for (size_t i = 0; i < previous_count && (for_position + 1 < parent->child_count); i++) {
            size_t remove_idx = for_position + 1; // Always remove the element right after @for
            printf("🗑️ CLEANUP: Removing element at position %zu (was generated by @for)\n", remove_idx);
            KryonElement* to_remove = parent->children[remove_idx];

            // Remove from parent's children array by shifting remaining elements
            for (size_t j = remove_idx; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            removed++;

            // Free the removed element
            if (to_remove) {
                kryon_element_destroy(runtime, to_remove);
            }
        }

        printf("✅ CLEANUP: Removed %zu previously generated elements\n", removed);
    }
    
    // Extract template parameters from @for element properties
    const char* index_var_name = NULL;
    const char* var_name = NULL;
    const char* array_name = NULL;

    // Find the @for metadata property (hex 0x9000, name "customProp_0x9000")
    // Format: "variable|array" (e.g., "habit|habits")
    // Or: "index,variable|array" (e.g., "i,habit|habits")
    for (size_t i = 0; i < for_element->property_count; i++) {
        KryonProperty* prop = for_element->properties[i];

        if (prop->name && strcmp(prop->name, "customProp_0x9000") == 0) {
            // Parse the combined string
            const char* combined = prop->value.string_value;
            if (combined) {
                // First, find the | separator between variables and array
                const char* pipe_sep = strchr(combined, '|');
                if (pipe_sep) {
                    // Extract array name (after |)
                    array_name = pipe_sep + 1;

                    // Now check if there's a comma in the variables part (index,var pattern)
                    const char* comma_sep = strchr(combined, ',');
                    if (comma_sep && comma_sep < pipe_sep) {
                        // We have "index,variable|array" pattern
                        // Extract index variable name (before comma)
                        size_t index_len = comma_sep - combined;
                        char* index_buf = malloc(index_len + 1);
                        if (index_buf) {
                            memcpy(index_buf, combined, index_len);
                            index_buf[index_len] = '\0';
                            index_var_name = index_buf;
                        }

                        // Extract value variable name (between comma and pipe)
                        size_t var_len = pipe_sep - (comma_sep + 1);
                        char* var_buf = malloc(var_len + 1);
                        if (var_buf) {
                            memcpy(var_buf, comma_sep + 1, var_len);
                            var_buf[var_len] = '\0';
                            var_name = var_buf;
                        }
                    } else {
                        // We have "variable|array" pattern (no index)
                        size_t var_len = pipe_sep - combined;
                        char* var_buf = malloc(var_len + 1);
                        if (var_buf) {
                            memcpy(var_buf, combined, var_len);
                            var_buf[var_len] = '\0';
                            var_name = var_buf;
                        }
                    }

                    printf("🔍 DEBUG: Parsed @for metadata: index='%s', variable='%s', array='%s'\n",
                           index_var_name ? index_var_name : "(none)",
                           var_name ? var_name : "(null)",
                           array_name ? array_name : "(null)");
                }
            }
            break;
        }

        // Legacy format support: check for old "variable" and "array" properties
        if (prop->name && strcmp(prop->name, "variable") == 0) {
            var_name = prop->value.string_value;
        } else if (prop->name && strcmp(prop->name, "array") == 0) {
            array_name = prop->value.string_value;
        }
    }

    if (!var_name || !array_name) {
        printf("❌ ERROR: @for directive missing variable or array name\n");
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
    
    // Parse JSON array data properly (handle nested JSON objects)
    if (!array_data) {
        return;
    }
    
    // Clear existing expanded children
    clear_expanded_children(runtime, for_element);

    // Find the @for element's position in parent for insertion
    size_t for_position = 0;
    if (for_element->parent) {
        for (size_t i = 0; i < for_element->parent->child_count; i++) {
            if (for_element->parent->children[i] == for_element) {
                for_position = i;
                break;
            }
        }
    }

    // Parse JSON array manually to handle nested objects correctly
    const char* ptr = array_data;
    size_t index = 0;

    // Skip leading whitespace and opening bracket
    while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
    if (*ptr == '[') ptr++;
    
    while (*ptr) {
        // Skip whitespace
        while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
        if (!*ptr || *ptr == ']') break;
        
        // Find the start of the next array item (object or string)
        if (*ptr == '{') {
            // Handle JSON objects like {"file":"button.kry","title":"Button"}
            const char* obj_start = ptr;
            int brace_count = 0;
            bool in_string = false;
            bool escaped = false;
            
            // Find the matching closing brace
            while (*ptr) {
                if (escaped) {
                    escaped = false;
                } else if (*ptr == '\\') {
                    escaped = true;
                } else if (*ptr == '"') {
                    in_string = !in_string;
                } else if (!in_string) {
                    if (*ptr == '{') {
                        brace_count++;
                    } else if (*ptr == '}') {
                        brace_count--;
                        if (brace_count == 0) {
                            ptr++; // Include the closing brace
                            break;
                        }
                    }
                }
                ptr++;
            }
            
            // Extract the JSON object
            size_t obj_len = ptr - obj_start;
            char* json_obj = kryon_alloc(obj_len + 1);
            if (json_obj) {
                memcpy(json_obj, obj_start, obj_len);
                json_obj[obj_len] = '\0';
                
                printf("🔧 Parsed JSON object %zu: '%s'\n", index, json_obj);

                // Create instance of template children for this array item
                // Insert at position: for_position + 1 (after @for) + index (already inserted items)
                expand_for_iteration(runtime, for_element, index_var_name, var_name, json_obj, index, for_position + 1 + index);
                index++;
                
                kryon_free(json_obj);
            }
        } else if (*ptr == '"') {
            // Handle simple strings like "Test todo", "Another item"
            const char* str_start = ptr;
            ptr++; // Skip opening quote
            bool escaped = false;
            
            // Find the closing quote
            while (*ptr) {
                if (escaped) {
                    escaped = false;
                } else if (*ptr == '\\') {
                    escaped = true;
                } else if (*ptr == '"') {
                    ptr++; // Include the closing quote
                    break;
                }
                ptr++;
            }
            
            // Extract the string value (without quotes)
            size_t str_len = ptr - str_start - 2; // Exclude quotes
            char* str_value = kryon_alloc(str_len + 1);
            if (str_value) {
                memcpy(str_value, str_start + 1, str_len); // Skip opening quote
                str_value[str_len] = '\0';
                
                printf("🔧 Parsed string %zu: '%s'\n", index, str_value);

                // Create instance of template children for this array item
                // Insert at position: for_position + 1 (after @for) + index (already inserted items)
                expand_for_iteration(runtime, for_element, index_var_name, var_name, str_value, index, for_position + 1 + index);
                index++;
                
                kryon_free(str_value);
            }
        }
        
        // Skip comma and whitespace
        while (*ptr && (*ptr == ',' || *ptr == ' ' || *ptr == '\t' || *ptr == '\n')) ptr++;
    }

    // Store the count of generated elements for next cleanup
    previous_generated_counts[generated_count_idx] = index;
    printf("📝 Stored generated count: %zu elements for @for element ID %u\n", index, for_element->id);
}

/**
 * @brief Create element instances for one iteration of @for loop
 * @param runtime Runtime context
 * @param for_element The @for template element
 * @param index_var_name Optional index variable name (can be NULL)
 * @param var_name Loop variable name (value variable)
 * @param var_value Current iteration value
 * @param index Current iteration index
 */
static void expand_for_iteration(KryonRuntime* runtime, KryonElement* for_element,
                                const char* index_var_name, const char* var_name, const char* var_value, size_t index, size_t insert_position) {
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
        
        // Clone the template child - first with index substitution if needed, then value substitution
        KryonElement* instance = template_child;

        // If we have an index variable, substitute it first
        if (index_var_name) {
            char index_str[32];
            snprintf(index_str, sizeof(index_str), "%zu", index);
            instance = clone_element_with_substitution(runtime, template_child, index_var_name, index_str, index);
            if (!instance) {
                printf("❌ Failed to clone with index substitution\n");
                continue;
            }
        }

        // Now substitute the value variable
        KryonElement* final_instance = clone_element_with_substitution(runtime, instance, var_name, var_value, index);

        // If we did index substitution first, we need to free the intermediate clone
        if (index_var_name && instance != template_child) {
            kryon_element_destroy(runtime, instance);
        }

        instance = final_instance;
        if (instance) {
            printf("✅ Successfully cloned element, type='%s'\n",
                   instance->type_name ? instance->type_name : "unknown");

            // Set loop context on the cloned element and all its descendants
            // so child @if directives can access the loop variables
            char index_str[32];
            const char* index_str_ptr = NULL;
            if (index_var_name) {
                snprintf(index_str, sizeof(index_str), "%zu", index);
                index_str_ptr = index_str;
            }
            set_loop_context_recursive(instance, index_var_name, index_str_ptr, var_name, var_value);
            printf("🔧 Set loop context recursively: %s=%s, %s=%s\n",
                   index_var_name ? index_var_name : "(none)",
                   index_str_ptr ? index_str_ptr : "(none)",
                   var_name,
                   var_value);

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
            
            // Add to parent at the specified insertion position (right after @for template)
            if (for_element->parent) {
                insert_child_element_at(runtime, for_element->parent, instance, insert_position);

                // Process @if directives in the cloned instance (component instances may have @if directives)
                printf("🔍 Calling process_if_directives on cloned instance [%p] type=%s\n",
                       (void*)instance, instance->type_name ? instance->type_name : "NULL");
                process_if_directives(runtime, instance);
                printf("✅ Finished process_if_directives on cloned instance\n");

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
                
                // Check for compound variable references like "example.file" or "example.title"
                if (ref_var) {
                    size_t var_name_len = strlen(var_name);
                    bool is_compound_match = (strncmp(ref_var, var_name, var_name_len) == 0 && ref_var[var_name_len] == '.');
                    bool is_exact_match = (strcmp(ref_var, var_name) == 0);
                    
                    if (is_exact_match || is_compound_match) {
                        // Substitute: convert REFERENCE to STRING with the actual value
                        dst_prop->type = KRYON_RUNTIME_PROP_STRING;
                        
                        if (is_compound_match) {
                            // Handle compound variables like "example.file" → extract "file" from JSON
                            const char* field_name = ref_var + var_name_len + 1; // Skip "example."
                            char* field_value = extract_json_field(var_value, field_name);
                            
                            printf("🔧 SUBSTITUTING compound variable: '%s' → field '%s' = '%s'\n", 
                                   ref_var, field_name, field_value ? field_value : "null");
                            
                            if (field_value) {
                                dst_prop->value.string_value = field_value; // Already allocated by extract_json_field
                            } else {
                                dst_prop->value.string_value = kryon_strdup("");
                            }
                        } else {
                            // Handle exact match: "example" → use value directly (string or JSON)
                            printf("🔧 SUBSTITUTING exact variable: '%s' = '%s'\n", ref_var, var_value);
                            dst_prop->value.string_value = kryon_strdup(var_value);
                            if (!dst_prop->value.string_value) {
                                dst_prop->value.string_value = kryon_strdup("");
                            }
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
                        // Check if the string contains the loop variable and substitute it
                        const char* original_str = src_prop->value.string_value;
                        char* substituted_str = substitute_template_variable(original_str, var_name, var_value);

                        if (substituted_str && substituted_str != original_str) {
                            // Variable was substituted
                            printf("🔧 STRING property substitution: '%s' → '%s'\n", original_str, substituted_str);
                            dst_prop->value.string_value = substituted_str;
                        } else {
                            // No substitution needed, just copy
                            dst_prop->value.string_value = kryon_strdup(original_str);
                            if (!dst_prop->value.string_value) {
                                dst_prop->value.string_value = kryon_strdup("");
                            }
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

    // Recursively clone children with variable substitution
    printf("🔧 Cloning %zu children for element '%s'\n", template_element->child_count,
           template_element->type_name ? template_element->type_name : "unknown");
    for (size_t i = 0; i < template_element->child_count; i++) {
        KryonElement* template_child = template_element->children[i];
        if (!template_child) continue;

        // Recursively clone child with same variable substitution
        KryonElement* child_clone = clone_element_with_substitution(runtime, template_child, var_name, var_value, index);
        if (child_clone) {
            // Expand children array if needed
            if (clone->child_count >= clone->child_capacity) {
                size_t new_cap = clone->child_capacity == 0 ? 4 : clone->child_capacity * 2;
                KryonElement** new_children = kryon_realloc(clone->children, new_cap * sizeof(KryonElement*));
                if (new_children) {
                    clone->children = new_children;
                    clone->child_capacity = new_cap;
                } else {
                    // Allocation failed, destroy child clone and skip
                    kryon_element_destroy(runtime, child_clone);
                    continue;
                }
            }

            // Add child to clone
            clone->children[clone->child_count++] = child_clone;
            child_clone->parent = clone;
        } else {
            printf("❌ Failed to clone child %zu\n", i);
        }
    }
    printf("🔧 Finished cloning element '%s', total children: %zu\n",
           clone->type_name ? clone->type_name : "unknown", clone->child_count);

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

    // First check for ${variable} pattern
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "${%s}", var_name);

    const char* pos = strstr(template_str, pattern);
    if (pos) {
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

    // Also check for bare variable name in expressions (e.g., "currentlyEditing == habit")
    // We need to be careful to only match whole words, not substrings
    size_t var_name_len = strlen(var_name);
    const char* search_pos = template_str;
    char* result = NULL;

    while ((search_pos = strstr(search_pos, var_name)) != NULL) {
        // Check if this is a whole word match (not part of a larger identifier)
        bool is_start_boundary = (search_pos == template_str ||
                                  !isalnum(search_pos[-1]) && search_pos[-1] != '_');
        bool is_end_boundary = (!isalnum(search_pos[var_name_len]) &&
                                search_pos[var_name_len] != '_');

        if (is_start_boundary && is_end_boundary) {
            // Found a whole word match - perform substitution
            size_t prefix_len = search_pos - template_str;
            size_t suffix_len = strlen(search_pos + var_name_len);
            size_t result_len = prefix_len + strlen(var_value) + suffix_len + 1;

            result = kryon_alloc(result_len);
            if (!result) {
                return kryon_strdup(template_str);
            }

            // Copy prefix
            strncpy(result, template_str, prefix_len);
            result[prefix_len] = '\0';

            // Add substituted value
            strcat(result, var_value);

            // Add suffix
            strcat(result, search_pos + var_name_len);

            return result;
        }

        search_pos++;
    }

    // No substitution needed
    return kryon_strdup(template_str);
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
            // Check if this variable matches the @for variable or is a property access
            const char *seg_var_name = segment->data.variable_name;
            if (seg_var_name && strcmp(seg_var_name, var_name) == 0) {
                // Direct variable match - use @for variable value
                total_len += strlen(var_value);
            } else if (seg_var_name && var_name) {
                // Check for property access (e.g., "example.title" where var_name is "example")
                size_t var_name_len = strlen(var_name);
                if (strncmp(seg_var_name, var_name, var_name_len) == 0 && seg_var_name[var_name_len] == '.') {
                    // This is a property access like "example.title"
                    const char* property_name = seg_var_name + var_name_len + 1; // Skip "example."
                    
                    // Calculate length of property value
                    const char* property_value = extract_json_property(var_value, property_name);
                    if (property_value) {
                        total_len += strlen(property_value);
                    }
                    // If property not found, contribute 0 to length
                } else {
                    // Look up in runtime variables (this is legitimate - not a fallback for failed @for vars)
                    const char *runtime_value = NULL;
                    for (size_t j = 0; j < runtime->variable_count; j++) {
                        if (runtime->variable_names[j] && seg_var_name &&
                            strcmp(runtime->variable_names[j], seg_var_name) == 0) {
                            runtime_value = runtime->variable_values[j];
                            break;
                        }
                    }
                    if (runtime_value) {
                        total_len += strlen(runtime_value);
                    }
                    // If not found in runtime variables, contribute 0 to length
                }
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
            // Check if this variable matches the @for variable or is a property access
            const char *seg_var_name = segment->data.variable_name;
            if (seg_var_name && strcmp(seg_var_name, var_name) == 0) {
                // Direct variable match - use @for variable value
                strcat(result, var_value);
            } else if (seg_var_name && var_name) {
                // Check for property access (e.g., "example.title" where var_name is "example")
                size_t var_name_len = strlen(var_name);
                if (strncmp(seg_var_name, var_name, var_name_len) == 0 && seg_var_name[var_name_len] == '.') {
                    // This is a property access like "example.title"
                    const char* property_name = seg_var_name + var_name_len + 1; // Skip "example."
                    
                    // Parse the @for variable value as JSON and extract the property
                    // For now, do a simple JSON property extraction
                    const char* property_value = extract_json_property(var_value, property_name);
                    if (property_value) {
                        strcat(result, property_value);
                    }
                    // If property not found, add nothing (no fallback)
                } else {
                    // Look up in runtime variables (this is legitimate - not a fallback for failed @for vars)
                    const char *runtime_value = NULL;
                    for (size_t j = 0; j < runtime->variable_count; j++) {
                        if (runtime->variable_names[j] && seg_var_name &&
                            strcmp(runtime->variable_names[j], seg_var_name) == 0) {
                            runtime_value = runtime->variable_values[j];
                            break;
                        }
                    }
                    if (runtime_value) {
                        strcat(result, runtime_value);
                    }
                    // If not found in runtime variables, add nothing
                }
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

/**
 * @brief Insert child element at specific position in parent's children array
 * @param runtime Runtime context
 * @param parent Parent element
 * @param child Child element to insert
 * @param position Position to insert at (children after this position are shifted right)
 */
static void insert_child_element_at(KryonRuntime* runtime, KryonElement* parent, KryonElement* child, size_t position) {
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

    // Clamp position to valid range
    if (position > parent->child_count) {
        position = parent->child_count;
    }

    // Shift existing children to make room
    for (size_t i = parent->child_count; i > position; i--) {
        parent->children[i] = parent->children[i - 1];
    }

    // Insert child at position
    parent->children[position] = child;
    parent->child_count++;

    // Mark for re-render
    parent->needs_render = true;
}

/**
 * @brief Extract a field value from a JSON object string
 * @param json_value JSON object string like '{"file":"button.kry","title":"Button"}'
 * @param field_name Field name to extract like "file" or "title"
 * @return Allocated string containing the field value, or NULL if not found
 */
static char* extract_json_field(const char* json_value, const char* field_name) {
    if (!json_value || !field_name) {
        return NULL;
    }
    
    // Create search pattern: "field_name":"
    size_t pattern_len = strlen(field_name) + 4; // for quotes, colon, and quotes
    char* pattern = kryon_alloc(pattern_len);
    if (!pattern) {
        return NULL;
    }
    snprintf(pattern, pattern_len, "\"%s\":", field_name);
    
    // Find the pattern in the JSON string
    const char* field_start = strstr(json_value, pattern);
    kryon_free(pattern);
    
    if (!field_start) {
        return NULL;
    }
    
    // Move past the pattern to find the value
    const char* value_start = field_start + strlen(field_name) + 3; // Skip "field_name":
    
    // Skip whitespace
    while (*value_start && (*value_start == ' ' || *value_start == '\t')) {
        value_start++;
    }
    
    // Value should start with a quote
    if (*value_start != '"') {
        return NULL;
    }
    value_start++; // Skip opening quote
    
    // Find the closing quote
    const char* value_end = value_start;
    while (*value_end && *value_end != '"') {
        if (*value_end == '\\') {
            // Skip escaped characters
            value_end++;
            if (*value_end) value_end++;
        } else {
            value_end++;
        }
    }
    
    if (*value_end != '"') {
        return NULL;
    }
    
    // Extract the value
    size_t value_len = value_end - value_start;
    char* result = kryon_alloc(value_len + 1);
    if (!result) {
        return NULL;
    }
    
    memcpy(result, value_start, value_len);
    result[value_len] = '\0';

    return result;
}

// =============================================================================
// @IF DIRECTIVE PROCESSING
// =============================================================================

// Forward declarations for @if helpers
static void expand_if_template(KryonRuntime* runtime, KryonElement* if_element);
static bool evaluate_runtime_condition(KryonRuntime* runtime, const char* condition_expr);

/**
 * @brief Process @if directives recursively in element tree
 * @param runtime Runtime context
 * @param element Current element to process
 */
void process_if_directives(KryonRuntime* runtime, KryonElement* element) {
    if (!runtime || !element) {
        return;
    }

    // Skip processing if KRB loading is in progress
    if (runtime->is_loading) {
        return;
    }

    // Check if this element is an @if template
    if (element->type_name && strcmp(element->type_name, "if") == 0) {
        printf("🔍 Found @if directive, expanding template\n");
        expand_if_template(runtime, element);
        // Don't process children of @if elements recursively; they are templates
        return;
    }

    // Skip processing children of @for elements - they are templates, not live elements
    if (element->type_name && strcmp(element->type_name, "for") == 0) {
        return;
    }

    // Process children recursively (but skip children of @if templates)
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            process_if_directives(runtime, element->children[i]);
        }
    }
}

/**
 * @brief Evaluate runtime condition expression
 * @param runtime Runtime context
 * @param condition_expr Condition string (e.g., "showMessage", "!isHidden", "count > 0")
 * @return true if condition evaluates to true, false otherwise
 */
static bool evaluate_runtime_condition(KryonRuntime* runtime, const char* condition_expr) {
    if (!runtime || !condition_expr) {
        return false;
    }

    printf("🔍 Evaluating condition: '%s'\n", condition_expr);

    // Handle simple variable references (most common case)
    // Remove leading/trailing whitespace
    const char* trimmed = condition_expr;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    // Handle negation (!variable)
    bool negate = false;
    if (*trimmed == '!') {
        negate = true;
        trimmed++;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
    }

    // Check for binary operators (==, !=, <, >, <=, >=)
    const char* op_pos = NULL;
    const char* op_str = NULL;
    int op_len = 0;

    // Search for == operator
    const char* eq_pos = strstr(trimmed, "==");
    if (eq_pos) {
        op_pos = eq_pos;
        op_str = "==";
        op_len = 2;
    }

    // Search for != operator
    const char* neq_pos = strstr(trimmed, "!=");
    if (neq_pos && (!op_pos || neq_pos < op_pos)) {
        op_pos = neq_pos;
        op_str = "!=";
        op_len = 2;
    }

    // If we found a binary operator, parse and evaluate
    if (op_pos) {
        printf("🔍 Found binary operator '%s'\n", op_str);

        // Extract left operand
        size_t left_len = op_pos - trimmed;
        char left_operand[256];
        strncpy(left_operand, trimmed, left_len);
        left_operand[left_len] = '\0';

        // Trim whitespace from left operand
        char* left_trim = left_operand;
        while (*left_trim == ' ' || *left_trim == '\t') left_trim++;
        size_t left_actual_len = strlen(left_trim);
        while (left_actual_len > 0 && (left_trim[left_actual_len-1] == ' ' || left_trim[left_actual_len-1] == '\t')) {
            left_trim[--left_actual_len] = '\0';
        }

        // Extract right operand
        const char* right_start = op_pos + op_len;
        while (*right_start == ' ' || *right_start == '\t') right_start++;
        char right_operand[256];
        strncpy(right_operand, right_start, sizeof(right_operand) - 1);
        right_operand[sizeof(right_operand) - 1] = '\0';

        // Trim whitespace from right operand
        size_t right_len = strlen(right_operand);
        while (right_len > 0 && (right_operand[right_len-1] == ' ' || right_operand[right_len-1] == '\t')) {
            right_operand[--right_len] = '\0';
        }

        printf("🔍 Left operand: '%s', Right operand: '%s'\n", left_trim, right_operand);

        // Resolve left operand (could be variable or literal)
        const char* left_value = kryon_runtime_get_variable(runtime, left_trim);
        if (!left_value) {
            // Not a variable, treat as literal
            left_value = left_trim;
        }

        // Resolve right operand (could be variable or literal)
        const char* right_value = kryon_runtime_get_variable(runtime, right_operand);
        if (!right_value) {
            // Not a variable, treat as literal
            right_value = right_operand;
        }

        printf("🔍 Resolved: '%s' %s '%s'\n", left_value, op_str, right_value);

        // Perform comparison
        bool result = false;
        if (strcmp(op_str, "==") == 0) {
            result = (strcmp(left_value, right_value) == 0);
        } else if (strcmp(op_str, "!=") == 0) {
            result = (strcmp(left_value, right_value) != 0);
        }

        printf("🔍 Comparison result: %s\n", result ? "true" : "false");

        if (negate) {
            result = !result;
            printf("🔍 After negation: %s\n", result ? "true" : "false");
        }

        return result;
    }

    // No binary operator - handle as simple variable reference
    const char* var_value = kryon_runtime_get_variable(runtime, trimmed);
    if (!var_value) {
        printf("⚠️  Variable '%s' not found, assuming false\n", trimmed);
        return negate ? true : false;
    }

    printf("🔍 Variable '%s' = '%s'\n", trimmed, var_value);

    // Evaluate boolean value
    bool result = false;
    if (strcmp(var_value, "true") == 0 || strcmp(var_value, "1") == 0) {
        result = true;
    } else if (strcmp(var_value, "false") == 0 || strcmp(var_value, "0") == 0) {
        result = false;
    } else {
        // Non-empty strings are truthy
        result = (var_value[0] != '\0');
    }

    printf("🔍 Condition '%s' evaluated to %s (before negation)\n",
           condition_expr, result ? "true" : "false");

    if (negate) {
        result = !result;
        printf("🔍 After negation: %s\n", result ? "true" : "false");
    }

    return result;
}

/**
 * @brief Expand @if template based on current condition state
 * @param runtime Runtime context
 * @param if_element The @if template element
 */
static void expand_if_template(KryonRuntime* runtime, KryonElement* if_element) {
    if (!runtime || !if_element) {
        return;
    }

    // Get condition property (customProp_0x9100)
    const char* condition_expr = NULL;
    size_t then_count = 0;

    for (size_t i = 0; i < if_element->property_count; i++) {
        KryonProperty* prop = if_element->properties[i];
        if (prop && prop->name) {
            if (strcmp(prop->name, "customProp_0x9100") == 0) {
                condition_expr = prop->value.string_value;
            } else if (strcmp(prop->name, "customProp_0x9101") == 0) {
                then_count = (size_t)prop->value.int_value;
            }
        }
    }

    if (!condition_expr) {
        printf("⚠️  @if element has no condition property (property_count=%zu)\n", if_element->property_count);
        return;
    }

    printf("🔍 @if condition: '%s', then_count: %zu\n", condition_expr, then_count);

    // Evaluate condition
    bool condition_result = evaluate_runtime_condition(runtime, condition_expr);

    printf("🔍 Condition result: %s\n", condition_result ? "true" : "false");

    // Get parent element
    KryonElement* parent = if_element->parent;
    if (!parent) {
        printf("⚠️  @if element has no parent\n");
        return;
    }

    // Find position of @if element in parent's children
    size_t if_position = 0;
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == if_element) {
            if_position = i;
            break;
        }
    }

    // Check if cloned children already exist
    // Clones could be anywhere in the children array (not necessarily right after @if)
    // We need to find ALL children that were cloned from ANY @if directive
    size_t* clone_indices = kryon_alloc(parent->child_count * sizeof(size_t));
    size_t clone_count = 0;

    if (clone_indices) {
        for (size_t i = if_position + 1; i < parent->child_count; i++) {
            KryonElement* child = parent->children[i];
            if (!child) continue;

            // Check if this child was cloned from an @if directive (type 0x8300)
            if (child->cloned_from_directive &&
                child->cloned_from_directive->type == 0x8300) {
                clone_indices[clone_count++] = i;
            }
        }

        // If cloned children exist, remove them (we'll recreate based on current branch)
        if (clone_count > 0) {
            // Remove clones in reverse order to avoid index shifting issues
            for (size_t i = clone_count; i > 0; i--) {
                size_t remove_idx = clone_indices[i - 1];
                KryonElement* child = parent->children[remove_idx];

                // Remove from parent array by shifting remaining elements
                for (size_t j = remove_idx; j < parent->child_count - 1; j++) {
                    parent->children[j] = parent->children[j + 1];
                }
                parent->child_count--;

                // Destroy the removed element
                if (child) {
                    kryon_element_destroy(runtime, child);
                }
            }
        }

        kryon_free(clone_indices);
    }

    // Determine which children to clone based on condition
    size_t start_idx = 0;
    size_t end_idx = 0;

    if (condition_result) {
        // Clone "then" children (indices 0 to then_count-1)
        start_idx = 0;
        end_idx = then_count;
        printf("✅ Condition is true, cloning then children [%zu..%zu)\n", start_idx, end_idx);
    } else {
        // Clone "else" children (indices then_count to end)
        start_idx = then_count;
        end_idx = if_element->child_count;
        printf("❌ Condition is false, cloning else children [%zu..%zu)\n", start_idx, end_idx);
    }

    // If no cloned children exist yet, create them
    if (true) {
        size_t clone_count = end_idx - start_idx;
        printf("🔧 Instantiating %zu children\n", clone_count);

        for (size_t i = start_idx; i < end_idx; i++) {
            KryonElement* template_child = if_element->children[i];
            if (!template_child) continue;

            printf("🔧 Cloning @if child %zu of type '%s'\n", i,
                   template_child->type_name ? template_child->type_name : "unknown");

            KryonElement* instance = NULL;

            // Check if @if is inside a @for loop (has loop context)
            if (if_element->loop_var_name && if_element->loop_var_value) {
                printf("🔧 @if has loop context: %s=%s, %s=%s\n",
                       if_element->loop_index_var_name ? if_element->loop_index_var_name : "(none)",
                       if_element->loop_index_var_value ? if_element->loop_index_var_value : "(none)",
                       if_element->loop_var_name,
                       if_element->loop_var_value);

                // Clone with variable substitution (like @for does)
                // First substitute index variable if present, then value variable
                KryonElement* temp_instance = template_child;

                if (if_element->loop_index_var_name && if_element->loop_index_var_value) {
                    temp_instance = clone_element_with_substitution(runtime, template_child,
                                                                    if_element->loop_index_var_name,
                                                                    if_element->loop_index_var_value,
                                                                    0);
                    if (!temp_instance) {
                        printf("❌ Failed to clone with index substitution\n");
                        continue;
                    }
                }

                // Now substitute the value variable
                instance = clone_element_with_substitution(runtime, temp_instance,
                                                          if_element->loop_var_name,
                                                          if_element->loop_var_value,
                                                          0);

                // Free intermediate clone if we did index substitution
                if (if_element->loop_index_var_name && temp_instance != template_child) {
                    kryon_element_destroy(runtime, temp_instance);
                }
            } else {
                // No loop context - use simple deep clone
                instance = clone_element_deep(runtime, template_child);
            }

            if (instance) {
                // Mark this element and all its descendants as cloned from this @if directive
                mark_cloned_from_directive_recursive(instance, if_element);

                // Add to parent element right after the @if template
                if (parent->child_count >= parent->child_capacity) {
                    size_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
                    KryonElement** new_children = kryon_realloc(parent->children,
                                                                new_capacity * sizeof(KryonElement*));
                    if (new_children) {
                        parent->children = new_children;
                        parent->child_capacity = new_capacity;
                    } else {
                        printf("❌ Failed to reallocate parent children array\n");
                        kryon_element_destroy(runtime, instance);
                        continue;
                    }
                }

                // Insert after the @if element
                parent->children[parent->child_count] = instance;
                parent->child_count++;
                instance->parent = parent;

                // Mark parent for re-layout and re-render
                parent->needs_layout = true;
                parent->needs_render = true;
            }
        }
    } else {
        printf("❌ Condition is false, not instantiating children\n");
    }
}

/**
 * @brief Recursively mark element and all descendants as cloned from a directive
 * @param element Element to mark
 * @param directive The @if or @for directive element that cloned this
 */
static void mark_cloned_from_directive_recursive(KryonElement* element, KryonElement* directive) {
    if (!element || !directive) {
        return;
    }

    element->cloned_from_directive = directive;

    // Recursively mark all children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            mark_cloned_from_directive_recursive(element->children[i], directive);
        }
    }
}

/**
 * @brief Deep clone element without variable substitution
 * @param runtime Runtime context
 * @param element Element to clone
 * @return Cloned element
 */
static KryonElement* clone_element_deep(KryonRuntime* runtime, KryonElement* element) {
    if (!runtime || !element) {
        return NULL;
    }

    // Create new element
    KryonElement* clone = kryon_element_create(runtime, element->type, NULL);
    if (!clone) {
        return NULL;
    }

    // Copy type_name
    if (element->type_name) {
        clone->type_name = kryon_strdup(element->type_name);
    }

    // Copy properties
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty* src_prop = element->properties[i];
        if (!src_prop) continue;

        KryonProperty* clone_prop = kryon_alloc(sizeof(KryonProperty));
        if (!clone_prop) continue;

        clone_prop->name = kryon_strdup(src_prop->name);
        clone_prop->type = src_prop->type;
        clone_prop->is_bound = src_prop->is_bound;
        clone_prop->binding_path = src_prop->binding_path ? kryon_strdup(src_prop->binding_path) : NULL;

        // Copy property value based on type
        switch (src_prop->type) {
            case KRYON_RUNTIME_PROP_STRING:
            case KRYON_RUNTIME_PROP_REFERENCE:
            case KRYON_RUNTIME_PROP_TEMPLATE:
                clone_prop->value.string_value = kryon_strdup(src_prop->value.string_value);
                break;
            default:
                clone_prop->value = src_prop->value;
                break;
        }

        // Add property to clone
        if (clone->property_count >= clone->property_capacity) {
            size_t new_cap = clone->property_capacity == 0 ? 4 : clone->property_capacity * 2;
            KryonProperty** new_props = kryon_realloc(clone->properties, new_cap * sizeof(KryonProperty*));
            if (new_props) {
                clone->properties = new_props;
                clone->property_capacity = new_cap;
            }
        }

        clone->properties[clone->property_count++] = clone_prop;
    }

    // Recursively clone children
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child_clone = clone_element_deep(runtime, element->children[i]);
        if (child_clone) {
            if (clone->child_count >= clone->child_capacity) {
                size_t new_cap = clone->child_capacity == 0 ? 4 : clone->child_capacity * 2;
                KryonElement** new_children = kryon_realloc(clone->children, new_cap * sizeof(KryonElement*));
                if (new_children) {
                    clone->children = new_children;
                    clone->child_capacity = new_cap;
                }
            }

            clone->children[clone->child_count++] = child_clone;
            child_clone->parent = clone;
        }
    }

    return clone;
}

// =============================================================================
// LAYOUT CALCULATION
// =============================================================================

/**
 * @brief Calculate layout positions for all elements in the runtime tree.
 * This ensures elements have proper x,y positions before rendering begins.
 * Should be called after loading KRB files but before first render pass.
 */
void kryon_runtime_calculate_layout(KryonRuntime* runtime) {
    if (!runtime || !runtime->root) {
        return;
    }
    
    printf("🧮 Calculating layout for entire element tree\n");
    
    // Start with root element - typically App element
    KryonElement* root = runtime->root;
    
    // Initialize root position (App element spans full window)
    root->x = 0.0f;
    root->y = 0.0f;
    
    // Get window size from App element properties or use defaults
    root->width = get_element_property_float(root, "windowWidth", 800.0f);
    root->height = get_element_property_float(root, "windowHeight", 600.0f);
    
    printf("🧮 Root element (%s) positioned at (%.1f, %.1f) size %.1fx%.1f\n", 
           root->type_name ? root->type_name : "unknown", 
           root->x, root->y, root->width, root->height);
    
    // Calculate positions for all child elements recursively
    position_children_by_layout_type(runtime, root);
    
    // Clear needs_layout flags throughout the tree
    clear_layout_flags_recursive(root);
    
    printf("✅ Layout calculation complete\n");
}

/**
 * @brief Clear needs_layout flags recursively after layout calculation
 */
static void clear_layout_flags_recursive(KryonElement* element) {
    if (!element) return;
    
    element->needs_layout = false;
    
    // Clear flags for all children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            clear_layout_flags_recursive(element->children[i]);
        }
    }
}

/**
 * @brief Extract a property from a JSON object for @for loop variable resolution
 * @param json_object JSON object string like '{"file":"button.kry","title":"Button"}'
 * @param property_name Property name to extract like "file" or "title"
 * @return Property value string, or NULL if not found (uses static buffer)
 */
static const char* extract_json_property(const char* json_object, const char* property_name) {
    if (!json_object || !property_name) {
        return NULL;
    }
    
    // Use the existing extract_json_field function which allocates memory
    char* allocated_result = extract_json_field(json_object, property_name);
    if (!allocated_result) {
        return NULL;
    }
    
    // Copy to static buffer to avoid memory management issues in template resolution
    static char property_buffer[1024];
    strncpy(property_buffer, allocated_result, sizeof(property_buffer) - 1);
    property_buffer[sizeof(property_buffer) - 1] = '\0';
    
    // Free the allocated memory
    kryon_free(allocated_result);
    
    return property_buffer;
}
