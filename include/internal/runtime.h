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
#include "internal/krb_format.h"
#include "internal/memory.h"
#include "internal/events.h"
#include "internal/elements.h"
#include "internal/script_vm.h"
#include "internal/renderer_interface.h"

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
    double last_update_time;
    double update_delta;
    bool is_running;
    bool needs_update;
    KryonRenderContext *render_context;
    void *renderer;
    KryonVM *script_vm;
    char **function_names;
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
    int input_text_scroll_offset;
    bool mouse_clicked_this_frame;
    bool mouse_pressed_last_frame; 
    bool cursor_should_be_pointer;
    
    // Dropdown state management
    struct {
        char* dropdown_id;           // Which dropdown is open (NULL = none)
        int selected_option_index;   // Currently selected option (-1 = none)
        bool is_dropdown_open;       // Is any dropdown open?
        KryonVec2 dropdown_position; // Position of open dropdown
        float dropdown_width;        // Width of open dropdown
        float dropdown_height;       // Height of open dropdown
        int hovered_option_index;    // Which option is hovered (-1 = none)
        char* focused_dropdown_id;   // Which dropdown has keyboard focus
        int keyboard_selected_index; // Index selected via keyboard navigation
        int option_count;            // Total number of options in current dropdown
        bool use_keyboard_selection; // Whether to use keyboard or mouse selection
        bool is_multi_select;        // Whether current dropdown supports multiple selection
        bool* selected_indices;      // Array of selected states for multi-select (size = option_count)
        int selected_count;          // Number of selected options in multi-select mode
        float max_dropdown_height;   // Maximum height for dropdown popup (enables scrolling)
        int scroll_offset;           // Current scroll offset for long lists
        int visible_options;         // Number of options visible at once
        bool is_searchable;          // Whether current dropdown supports search
        char search_text[256];       // Current search query text
        int* filtered_indices;       // Array of indices for options matching search (size = option_count)
        int filtered_count;          // Number of options that match current search
        bool needs_refresh;          // Whether dropdown needs to refresh its options/state
        int cached_option_count;     // Last known option count for change detection
        int cached_selected_index;   // Last known selected index for change detection
        char* validation_error;      // Current validation error message (NULL = no error)
        bool has_validation_error;   // Whether dropdown has validation errors
    } dropdown_state;
} KryonRuntime;

// =============================================================================
// API
// =============================================================================

KryonRuntime *kryon_runtime_create(const KryonRuntimeConfig *config);
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
void kryon_element_tree_to_render_commands(KryonElement* root, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
KryonRuntimeConfig kryon_runtime_default_config(void);
KryonRuntimeConfig kryon_runtime_dev_config(void);
KryonRuntimeConfig kryon_runtime_prod_config(void);
bool kryon_runtime_set_variable(KryonRuntime *runtime, const char *name, const char *value);
const char* kryon_runtime_get_variable(KryonRuntime *runtime, const char *name);
KryonRuntime* kryon_runtime_get_current(void);

// Event callback function for renderer integration (Phase 4)
void runtime_receive_input_event(const KryonEvent* event, void* userData);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_RUNTIME_H