/**
 * @file runtime_lifecycle.c
 * @brief Runtime Lifecycle Management
 *
 * Handles runtime initialization, shutdown, and configuration.
 * Extracted from runtime.c as part of refactoring to reduce monolithic file size.
 */

#include "lib9.h"
#include "runtime.h"
#include "memory.h"
#include "events.h"
#include "elements.h"
#include "component_state.h"
#include "language_plugins.h"
#include "../navigation/navigation.h"
#include "runtime_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External functions from other modules
extern struct HitTestManager* hit_test_manager_create(void);
extern void hit_test_manager_destroy(struct HitTestManager* manager);
extern bool element_registry_init(void);
extern void element_registry_cleanup(void);
extern bool runtime_event_handler(const KryonEvent* event, void* userData);

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Free a script function and its resources
 */
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

// =============================================================================
// PUBLIC API
// =============================================================================

KryonRuntimeConfig kryon_runtime_default_config(void) {
    KryonRuntimeConfig config;
    memset(&config, 0, sizeof(KryonRuntimeConfig));
    config.max_elements = 10000;
    config.max_update_fps = 60;
    config.mode = KRYON_MODE_PRODUCTION;
    config.enable_hot_reload = false;
    config.enable_profiling = false;
    config.enable_validation = false;
    return config;
}

bool kryon_runtime_lifecycle_validate_config(const KryonRuntimeConfig *config) {
    if (!config) {
        return false;
    }
    if (config->max_elements == 0) {
        fprintf(stderr, "Invalid config: max_elements must be > 0\n");
        return false;
    }
    if (config->max_update_fps == 0) {
        fprintf(stderr, "Invalid config: max_update_fps must be > 0\n");
        return false;
    }
    return true;
}

bool kryon_runtime_lifecycle_init(KryonRuntime *runtime, const KryonRuntimeConfig *config) {
    if (!runtime) {
        return false;
    }

    // Initialize structure
    memset(runtime, 0, sizeof(KryonRuntime));

    // Set configuration
    if (config) {
        if (!kryon_runtime_lifecycle_validate_config(config)) {
            return false;
        }
        runtime->config = *config;
    } else {
        runtime->config = kryon_runtime_default_config();
    }

    // Initialize element type registry
    if (!element_registry_init()) {
        return false;
    }

    // Element registry automatically registers all available elements during init

    // Initialize element storage
    runtime->element_capacity = 256;
    runtime->elements = kryon_alloc(runtime->element_capacity * sizeof(KryonElement*));
    if (!runtime->elements) {
        element_registry_cleanup();
        return false;
    }

    // Initialize event system
    runtime->event_system = kryon_event_system_create(256);
    if (!runtime->event_system) {
        kryon_free(runtime->elements);
        return false;
    }

    // Initialize hit testing manager
    runtime->hit_test_manager = hit_test_manager_create();
    if (!runtime->hit_test_manager) {
        kryon_event_system_destroy(runtime->event_system);
        kryon_free(runtime->elements);
        return false;
    }

    // Register runtime event handlers for all event types
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_MOUSE_BUTTON_DOWN, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_MOUSE_MOVED, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_TEXT_INPUT, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_KEY_DOWN, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_KEY_UP, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_WINDOW_FOCUS, runtime_event_handler, runtime);
    kryon_event_add_listener(runtime->event_system, KRYON_EVENT_WINDOW_RESIZE, runtime_event_handler, runtime);

    // Initialize language plugin system
    if (!kryon_language_init(runtime)) {
        fprintf(stderr, "⚠️ Warning: Language plugin system initialization failed\n");
    }

    // Initialize global state
    runtime->global_state = kryon_state_create("global", KRYON_STATE_OBJECT);
    if (!runtime->global_state) {
        kryon_event_system_destroy(runtime->event_system);
        kryon_free(runtime->elements);
        return false;
    }

    // Initialize navigation manager only if needed (when there are link elements)
    runtime->navigation_manager = NULL; // Will be created lazily when first link is encountered

    // Initialize other fields
    runtime->next_element_id = 1;
    runtime->is_running = false;
    runtime->needs_update = true;

    // Initialize input state
    runtime->mouse_position.x = 0.0f;
    runtime->mouse_position.y = 0.0f;

    // Initialize root variables with default viewport size
    kryon_runtime_set_variable(runtime, "root.width", "800");
    kryon_runtime_set_variable(runtime, "root.height", "600");

    return true;
}

void kryon_runtime_lifecycle_shutdown(KryonRuntime *runtime) {
    if (!runtime) {
        return;
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

    // Shutdown language plugin system
    kryon_language_shutdown(runtime);

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

    // Cleanup input state
    if (runtime->focused_input_id) {
        kryon_free(runtime->focused_input_id);
        runtime->focused_input_id = NULL;
    }
}
