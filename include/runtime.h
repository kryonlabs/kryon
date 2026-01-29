/**
 * @file runtime.h
 * @brief Kryon Runtime System - Core runtime for executing KRB files
 */

#ifndef KRYON_INTERNAL_RUNTIME_H
#define KRYON_INTERNAL_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "krb_format.h"
#include "memory.h"
#include "events.h"
#include "elements.h"
#include "renderer_interface.h"

// Forward declarations
struct HitTestManager;
struct KryonNavigationManager;

// =============================================================================
// RUNTIME CONFIGURATION
// =============================================================================

typedef enum {
    KRYON_MODE_PRODUCTION = 0,
    KRYON_MODE_DEVELOPMENT,
    KRYON_MODE_DEBUG
} KryonRuntimeMode;

typedef struct {
    KryonRuntimeMode mode;
    bool enable_hot_reload;
    bool enable_profiling;
    bool enable_validation;
    size_t max_elements;
    size_t max_update_fps;
    const char *resource_path;
    void *platform_context;
} KryonRuntimeConfig;

// =============================================================================
// RUNTIME STRUCTURE
// =============================================================================

typedef struct KryonScriptFunction {
    char *name;
    char *language;
    char *code;
    uint16_t param_count;
    char **parameters;
} KryonScriptFunction;

typedef struct KryonRuntime {
    KryonRuntimeConfig config;
    KryonElement *root;
    uint32_t next_element_id;
    KryonElement **elements;
    size_t element_count;
    size_t element_capacity;
    KryonState *global_state;
    KryonStyle **styles;
    size_t style_count;
    size_t style_capacity;
    KryonEventSystem* event_system;
    struct HitTestManager* hit_test_manager;
    double last_update_time;
    double update_delta;
    bool is_running;
    bool needs_update;
    bool is_loading; // Flag to prevent @for processing during KRB loading
    KryonRenderContext *render_context;
    void *renderer;
    KryonScriptFunction *script_functions;
    size_t function_count;
    size_t function_capacity;
    char **variable_names;
    char **variable_values;
    size_t variable_count;
    size_t variable_capacity;
    KryonComponentDefinition **components;
    size_t component_count;
    size_t component_capacity;
    char **string_table;
    size_t string_table_count;
    size_t string_table_offset;
    KryonMemoryManager *memory;
    struct {
        uint64_t frame_count;
        double total_time;
        double update_time;
        double render_time;
        double idle_time;
    } stats;
    char **error_messages;
    size_t error_count;
    size_t error_capacity;
    
    // Input state (Phase 5: replaces global variables) 
    KryonVec2 mouse_position;
    char* focused_input_id;
    char input_text_buffer[1024];
    size_t input_text_length;
    
    // Cursor management to prevent jittery behavior
    bool cursor_set_this_frame;
    int input_text_scroll_offset;
    bool mouse_clicked_this_frame;
    bool mouse_pressed_last_frame;
    
    // Navigation system
    struct KryonNavigationManager* navigation_manager;
    char* current_file_path; // Path of currently loaded file
    
    // Note: Dropdown state now managed by individual dropdown elements
} KryonRuntime;

// =============================================================================
// API
// =============================================================================

KryonRuntime *kryon_runtime_create(const KryonRuntimeConfig *config);
void kryon_runtime_clear_all_content(KryonRuntime *runtime);
void kryon_runtime_destroy(KryonRuntime *runtime);
bool kryon_runtime_load_file(KryonRuntime *runtime, const char *filename);
bool kryon_runtime_load_binary(KryonRuntime *runtime, const uint8_t *data, size_t size);
bool kryon_runtime_start(KryonRuntime *runtime);
void kryon_runtime_stop(KryonRuntime *runtime);
bool kryon_runtime_update(KryonRuntime *runtime, double delta_time);
bool kryon_runtime_render(KryonRuntime *runtime);
bool kryon_runtime_handle_event(KryonRuntime *runtime, const KryonEvent *event);
KryonElement *kryon_element_create(KryonRuntime *runtime, uint16_t type, KryonElement *parent);
void kryon_element_destroy(KryonRuntime *runtime, KryonElement *element);
KryonElement *kryon_element_find_by_id(KryonRuntime *runtime, const char *element_id);
bool kryon_element_set_property(KryonElement *element, uint16_t property_id, const void *value);
const void *kryon_element_get_property(const KryonElement *element, uint16_t property_id);
bool kryon_element_add_handler(KryonElement *element, KryonEventType type, KryonEventHandler handler, void *user_data, bool capture);
bool kryon_element_remove_handler(KryonElement *element, KryonEventType type, KryonEventHandler handler);
void kryon_element_invalidate_layout(KryonElement *element);
void kryon_element_invalidate_render(KryonElement *element);
void mark_elements_for_rerender(KryonElement *element);
KryonState *kryon_state_create(const char *key, KryonStateType type);
void kryon_state_destroy(KryonState *state);
bool kryon_state_set_value(KryonState *state, const void *value);
const void *kryon_state_get_value(const KryonState *state);
bool kryon_state_add_observer(KryonState *state, void (*observer)(KryonState*, void*), void *user_data);
bool kryon_state_remove_observer(KryonState *state, void (*observer)(KryonState*, void*));
KryonState *kryon_state_get_by_path(KryonRuntime *runtime, const char *path);
typedef void (*KryonLifecycleHook)(KryonElement *element, void *user_data);
bool kryon_runtime_add_lifecycle_hook(KryonRuntime *runtime, uint16_t element_type, KryonLifecycleHook hook, void *user_data);
const char **kryon_runtime_get_errors(const KryonRuntime *runtime, size_t *out_count);
void kryon_runtime_clear_errors(KryonRuntime *runtime);
void kryon_element_tree_to_render_commands(KryonRuntime* runtime, KryonElement* root, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
KryonRuntimeConfig kryon_runtime_default_config(void);
KryonRuntimeConfig kryon_runtime_dev_config(void);
KryonRuntimeConfig kryon_runtime_prod_config(void);
bool kryon_runtime_set_variable(KryonRuntime *runtime, const char *name, const char *value);
const char* kryon_runtime_get_variable(KryonRuntime *runtime, const char *name);
KryonRuntime* kryon_runtime_get_current(void);

// @for directive processing
void process_for_directives(KryonRuntime* runtime, KryonElement* element);

// @if directive processing
void process_if_directives(KryonRuntime* runtime, KryonElement* element);

// Update root variables when viewport/window size changes
bool kryon_runtime_update_viewport_size(KryonRuntime* runtime, float width, float height);

// Event callback function for renderer integration (Phase 4)
void runtime_receive_input_event(const KryonEvent* event, void* userData);


/**
* @brief Finds a property on an element by its string name.
* @return A pointer to the KryonProperty, or NULL if not found.
*/
KryonProperty* find_element_property(KryonElement* element, const char* prop_name);

/**
* @brief Get property value as string with runtime context for reactive variable resolution
* @param runtime Runtime context for variable resolution
* @param element Element to get property from  
* @param prop_name Property name
* @return String value or NULL if not found
*/
const char* get_element_property_string_with_runtime(KryonRuntime* runtime, KryonElement* element, const char* prop_name);

/**
 * @brief Calculates the width and height of an element based on its content.
 * This is a utility for elements that support automatic sizing.
 * If width or height are >= 0, they are not modified. If they are < 0,
 * they are replaced with a calculated size.
 */
 void auto_size_element(KryonElement* element, float* width, float* height);

// =============================================================================
// EXPRESSION EVALUATION API
// =============================================================================

/**
 * @brief Evaluate an expression AST node to get its value
 * @param expression The expression node to evaluate
 * @param runtime Runtime context for variable resolution
 * @return Evaluated value
 */
KryonExpressionValue kryon_runtime_evaluate_expression(const KryonExpressionNode *expression, KryonRuntime *runtime);

/**
 * @brief Convert expression value to string
 * @param value Expression value to convert
 * @return Allocated string (caller must free)
 */
char *kryon_expression_value_to_string(const KryonExpressionValue *value);

/**
 * @brief Convert expression value to number
 * @param value Expression value to convert
 * @return Numeric value
 */
double kryon_expression_value_to_number(const KryonExpressionValue *value);

/**
 * @brief Convert expression value to boolean
 * @param value Expression value to convert
 * @return Boolean value
 */
bool kryon_expression_value_to_boolean(const KryonExpressionValue *value);

/**
 * @brief Free expression value resources
 * @param value Expression value to free
 */
void kryon_expression_value_free(KryonExpressionValue *value);

/**
 * @brief Free expression AST node
 * @param node Expression node to free
 */
void kryon_expression_node_free(KryonExpressionNode *node);

/**
 * @brief Evaluate runtime condition expression (supports ==, !=, &&, ||, !)
 * @param runtime Runtime instance
 * @param condition_expr Condition expression to evaluate
 * @return True if condition is true, false otherwise
 */
bool kryon_evaluate_runtime_condition(KryonRuntime *runtime, const char *condition_expr);

/**
 * @brief Calculate layout positions for all elements in the runtime tree.
 * This ensures elements have proper x,y positions before rendering begins.
 * Should be called after loading KRB files but before first render pass.
 * @param runtime Runtime context with loaded elements
 */
void kryon_runtime_calculate_layout(KryonRuntime* runtime);
 
#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_RUNTIME_H
