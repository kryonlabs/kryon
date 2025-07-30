/**
 * @file runtime.h
 * @brief Kryon Runtime System - Core runtime for executing KRB files
 * 
 * Complete runtime system that loads KRB files, constructs element trees,
 * manages lifecycle, and coordinates rendering with reactive state updates.
 * 
 * @version 1.0.0
 * @author Kryon Labs
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

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonRuntime KryonRuntime;
typedef struct KryonRuntimeConfig KryonRuntimeConfig;
typedef struct KryonElement KryonElement;
typedef struct KryonProperty KryonProperty;
typedef struct KryonStyle KryonStyle;
typedef struct KryonState KryonState;
typedef struct KryonEventHandler KryonEventHandler;
typedef struct KryonRenderContext KryonRenderContext;

// =============================================================================
// RUNTIME CONFIGURATION
// =============================================================================

/**
 * @brief Runtime execution modes
 */
typedef enum {
    KRYON_MODE_PRODUCTION = 0,   // Optimized for production
    KRYON_MODE_DEVELOPMENT,      // Development mode with hot reload
    KRYON_MODE_DEBUG            // Debug mode with extensive logging
} KryonRuntimeMode;

/**
 * @brief Runtime configuration
 */
struct KryonRuntimeConfig {
    KryonRuntimeMode mode;           // Execution mode
    bool enable_hot_reload;          // Enable hot reload for development
    bool enable_profiling;           // Enable performance profiling
    bool enable_validation;          // Validate elements and properties
    size_t max_elements;             // Maximum number of elements
    size_t max_update_fps;           // Maximum updates per second
    const char *resource_path;       // Path to resource files
    void *platform_context;          // Platform-specific context
};

// =============================================================================
// ELEMENT SYSTEM
// =============================================================================

/**
 * @brief Element lifecycle states
 */
typedef enum {
    KRYON_ELEMENT_CREATED = 0,   // Element created but not mounted
    KRYON_ELEMENT_MOUNTING,      // Element is being mounted
    KRYON_ELEMENT_MOUNTED,       // Element is mounted and active
    KRYON_ELEMENT_UPDATING,      // Element is updating
    KRYON_ELEMENT_UNMOUNTING,    // Element is being unmounted
    KRYON_ELEMENT_UNMOUNTED,     // Element is unmounted
    KRYON_ELEMENT_DESTROYED      // Element is destroyed
} KryonElementState;

/**
 * @brief Element structure
 */
struct KryonElement {
    // Identification
    uint32_t id;                     // Unique element ID
    uint16_t type;                   // Element type (hex code)
    char *type_name;                 // Element type name
    char *element_id;                // User-defined ID (optional)
    
    // Hierarchy
    KryonElement *parent;            // Parent element
    KryonElement **children;         // Child elements
    size_t child_count;              // Number of children
    size_t child_capacity;           // Child array capacity
    
    // Properties
    KryonProperty **properties;      // Element properties
    size_t property_count;           // Number of properties
    size_t property_capacity;        // Property array capacity
    
    // Styling
    KryonStyle *computed_style;      // Computed style after cascade
    char **class_names;              // Applied CSS classes
    size_t class_count;              // Number of classes
    
    // Layout
    float x, y;                      // Position
    float width, height;             // Size
    float padding[4];                // Top, right, bottom, left
    float margin[4];                 // Top, right, bottom, left
    bool needs_layout;               // Layout dirty flag
    
    // State
    KryonElementState state;         // Lifecycle state
    bool visible;                    // Visibility flag
    bool enabled;                    // Enabled flag
    void *user_data;                 // User-defined data
    
    // Events
    KryonEventHandler **handlers;    // Event handlers
    size_t handler_count;            // Number of handlers
    size_t handler_capacity;         // Handler array capacity
    
    // Rendering
    bool needs_render;               // Render dirty flag
    uint32_t render_order;           // Z-order for rendering
    void *render_data;               // Renderer-specific data
};

/**
 * @brief Runtime property types (different from KRB property types)
 */
typedef enum {
    KRYON_RUNTIME_PROP_STRING = 0,
    KRYON_RUNTIME_PROP_INTEGER,
    KRYON_RUNTIME_PROP_FLOAT,
    KRYON_RUNTIME_PROP_BOOLEAN,
    KRYON_RUNTIME_PROP_COLOR,
    KRYON_RUNTIME_PROP_REFERENCE,        // Reference to another element
    KRYON_RUNTIME_PROP_EXPRESSION,        // Reactive expression
    KRYON_RUNTIME_PROP_FUNCTION          // Function reference
} KryonRuntimePropertyType;

/**
 * @brief Property structure
 */
struct KryonProperty {
    uint16_t id;                  // Property ID (hex code)
    char *name;                   // Property name
    KryonRuntimePropertyType type;       // Property type
    
    // Value storage
    union {
        char *string_value;
        int64_t int_value;
        double float_value;
        bool bool_value;
        uint32_t color_value;
        uint32_t ref_id;          // Element reference ID
        void *expression;         // Expression AST
        void *function;           // Function pointer
    } value;
    
    // Binding
    bool is_bound;                // Is property bound to state?
    char *binding_path;           // State binding path
    void *binding_context;        // Binding context
};

// =============================================================================
// STATE MANAGEMENT
// =============================================================================

/**
 * @brief State value types
 */
typedef enum {
    KRYON_STATE_NULL = 0,
    KRYON_STATE_BOOLEAN,
    KRYON_STATE_INTEGER,
    KRYON_STATE_FLOAT,
    KRYON_STATE_STRING,
    KRYON_STATE_OBJECT,
    KRYON_STATE_ARRAY
} KryonStateType;

/**
 * @brief State node structure
 */
struct KryonState {
    char *key;                    // State key
    KryonStateType type;          // Value type
    
    // Value storage
    union {
        bool bool_value;
        int64_t int_value;
        double float_value;
        char *string_value;
        struct {
            KryonState **items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            KryonState **properties;
            size_t count;
            size_t capacity;
        } object;
    } value;
    
    // Reactivity
    void **observers;             // Observer callbacks
    size_t observer_count;        // Number of observers
    size_t observer_capacity;     // Observer array capacity
    
    // Parent/child relationships
    KryonState *parent;           // Parent state node
};

// =============================================================================
// EVENT SYSTEM
// =============================================================================

/**
 * @brief Event types
 */
typedef enum {
    // Mouse events
    KRYON_EVENT_CLICK = 0,
    KRYON_EVENT_DOUBLE_CLICK,
    KRYON_EVENT_MOUSE_DOWN,
    KRYON_EVENT_MOUSE_UP,
    KRYON_EVENT_MOUSE_MOVE,
    KRYON_EVENT_MOUSE_ENTER,
    KRYON_EVENT_MOUSE_LEAVE,
    
    // Keyboard events
    KRYON_EVENT_KEY_DOWN,
    KRYON_EVENT_KEY_UP,
    KRYON_EVENT_KEY_PRESS,
    
    // Focus events
    KRYON_EVENT_FOCUS,
    KRYON_EVENT_BLUR,
    
    // Form events
    KRYON_EVENT_CHANGE,
    KRYON_EVENT_INPUT,
    KRYON_EVENT_SUBMIT,
    
    // Touch events
    KRYON_EVENT_TOUCH_START,
    KRYON_EVENT_TOUCH_MOVE,
    KRYON_EVENT_TOUCH_END,
    
    // Custom events
    KRYON_EVENT_CUSTOM
} KryonEventType;

/**
 * @brief Event structure
 */
typedef struct {
    KryonEventType type;          // Event type
    KryonElement *target;         // Target element
    KryonElement *current_target; // Current target (for bubbling)
    double timestamp;             // Event timestamp
    bool bubbles;                 // Does event bubble?
    bool cancelable;              // Can event be cancelled?
    bool default_prevented;       // Was default action prevented?
    bool propagation_stopped;     // Was propagation stopped?
    
    // Event-specific data
    union {
        struct {
            float x, y;           // Mouse position
            uint8_t button;       // Mouse button
            bool ctrl, alt, shift, meta; // Modifier keys
        } mouse;
        
        struct {
            uint32_t key_code;    // Key code
            char *key;            // Key string
            bool ctrl, alt, shift, meta; // Modifier keys
        } keyboard;
        
        struct {
            float x, y;           // Touch position
            uint32_t identifier;  // Touch identifier
        } touch;
        
        void *custom_data;        // Custom event data
    } data;
} KryonEvent;

/**
 * @brief Event handler function type
 */
typedef void (*KryonEventHandlerFn)(const KryonEvent *event, void *user_data);

/**
 * @brief Event handler structure
 */
struct KryonEventHandler {
    KryonEventType type;          // Event type to handle
    KryonEventHandlerFn handler;  // Handler function
    void *user_data;              // User data for handler
    bool capture;                 // Use capture phase?
};

// =============================================================================
// RUNTIME STRUCTURE
// =============================================================================

/**
 * @brief Main runtime structure
 */
struct KryonRuntime {
    // Configuration
    KryonRuntimeConfig config;    // Runtime configuration
    
    // Element tree
    KryonElement *root;           // Root element
    uint32_t next_element_id;     // Next element ID
    
    // Element registry
    KryonElement **elements;      // All elements by ID
    size_t element_count;         // Number of elements
    size_t element_capacity;      // Element array capacity
    
    // State management
    KryonState *global_state;     // Global state tree
    
    // Style system
    KryonStyle **styles;          // Registered styles
    size_t style_count;           // Number of styles
    size_t style_capacity;        // Style array capacity
    
    // Event system
    KryonEvent *event_queue;      // Event queue
    size_t event_count;           // Events in queue
    size_t event_capacity;        // Queue capacity
    size_t event_read_pos;        // Queue read position
    size_t event_write_pos;       // Queue write position
    
    // Update cycle
    double last_update_time;      // Last update timestamp
    double update_delta;          // Time since last update
    bool is_running;              // Is runtime running?
    bool needs_update;            // Update required flag
    
    // Rendering
    KryonRenderContext *render_context; // Rendering context
    void *renderer;               // Platform renderer
    
    // Resource management
    KryonMemoryManager *memory;   // Memory manager
    
    // Performance
    struct {
        uint64_t frame_count;     // Total frames rendered
        double total_time;        // Total runtime
        double update_time;       // Time in updates
        double render_time;       // Time in rendering
        double idle_time;         // Time idle
    } stats;
    
    // Error handling
    char **error_messages;        // Error message buffer
    size_t error_count;           // Number of errors
    size_t error_capacity;        // Error buffer capacity
};

// =============================================================================
// RUNTIME API
// =============================================================================

/**
 * @brief Create runtime instance
 * @param config Runtime configuration (NULL for defaults)
 * @return Runtime instance or NULL on failure
 */
KryonRuntime *kryon_runtime_create(const KryonRuntimeConfig *config);

/**
 * @brief Destroy runtime and free all resources
 * @param runtime Runtime instance
 */
void kryon_runtime_destroy(KryonRuntime *runtime);

/**
 * @brief Load and execute KRB file
 * @param runtime Runtime instance
 * @param filename KRB file path
 * @return true on success, false on failure
 */
bool kryon_runtime_load_file(KryonRuntime *runtime, const char *filename);

/**
 * @brief Load and execute KRB from memory
 * @param runtime Runtime instance
 * @param data KRB binary data
 * @param size Size of binary data
 * @return true on success, false on failure
 */
bool kryon_runtime_load_binary(KryonRuntime *runtime, const uint8_t *data, size_t size);

/**
 * @brief Start runtime execution
 * @param runtime Runtime instance
 * @return true on success, false on failure
 */
bool kryon_runtime_start(KryonRuntime *runtime);

/**
 * @brief Stop runtime execution
 * @param runtime Runtime instance
 */
void kryon_runtime_stop(KryonRuntime *runtime);

/**
 * @brief Update runtime (call each frame)
 * @param runtime Runtime instance
 * @param delta_time Time since last update (seconds)
 * @return true if update occurred, false if idle
 */
bool kryon_runtime_update(KryonRuntime *runtime, double delta_time);

/**
 * @brief Render current frame
 * @param runtime Runtime instance
 * @return true on success, false on failure
 */
bool kryon_runtime_render(KryonRuntime *runtime);

/**
 * @brief Process single event
 * @param runtime Runtime instance
 * @param event Event to process
 * @return true if event was handled
 */
bool kryon_runtime_handle_event(KryonRuntime *runtime, const KryonEvent *event);

// =============================================================================
// ELEMENT API
// =============================================================================

/**
 * @brief Create element
 * @param runtime Runtime instance
 * @param type Element type hex code
 * @param parent Parent element (NULL for root)
 * @return New element or NULL on failure
 */
KryonElement *kryon_element_create(KryonRuntime *runtime, uint16_t type, KryonElement *parent);

/**
 * @brief Destroy element and its children
 * @param runtime Runtime instance
 * @param element Element to destroy
 */
void kryon_element_destroy(KryonRuntime *runtime, KryonElement *element);

/**
 * @brief Find element by ID
 * @param runtime Runtime instance
 * @param element_id Element ID string
 * @return Element or NULL if not found
 */
KryonElement *kryon_element_find_by_id(KryonRuntime *runtime, const char *element_id);

/**
 * @brief Set element property
 * @param element Element
 * @param property_id Property hex code
 * @param value Property value
 * @return true on success
 */
bool kryon_element_set_property(KryonElement *element, uint16_t property_id, const void *value);

/**
 * @brief Get element property
 * @param element Element
 * @param property_id Property hex code
 * @return Property value or NULL
 */
const void *kryon_element_get_property(const KryonElement *element, uint16_t property_id);

/**
 * @brief Add event handler to element
 * @param element Element
 * @param type Event type
 * @param handler Handler function
 * @param user_data User data
 * @param capture Use capture phase
 * @return true on success
 */
bool kryon_element_add_handler(KryonElement *element, KryonEventType type,
                              KryonEventHandlerFn handler, void *user_data, bool capture);

/**
 * @brief Remove event handler from element
 * @param element Element
 * @param type Event type
 * @param handler Handler function
 * @return true if handler was removed
 */
bool kryon_element_remove_handler(KryonElement *element, KryonEventType type,
                                 KryonEventHandlerFn handler);

/**
 * @brief Mark element for layout update
 * @param element Element
 */
void kryon_element_invalidate_layout(KryonElement *element);

/**
 * @brief Mark element for render update
 * @param element Element
 */
void kryon_element_invalidate_render(KryonElement *element);

// =============================================================================
// STATE API
// =============================================================================

/**
 * @brief Create state node
 * @param key State key
 * @param type Value type
 * @return New state node or NULL
 */
KryonState *kryon_state_create(const char *key, KryonStateType type);

/**
 * @brief Destroy state node
 * @param state State node
 */
void kryon_state_destroy(KryonState *state);

/**
 * @brief Set state value
 * @param state State node
 * @param value New value
 * @return true on success
 */
bool kryon_state_set_value(KryonState *state, const void *value);

/**
 * @brief Get state value
 * @param state State node
 * @return Value pointer or NULL
 */
const void *kryon_state_get_value(const KryonState *state);

/**
 * @brief Add state observer
 * @param state State node
 * @param observer Observer callback
 * @param user_data User data
 * @return true on success
 */
bool kryon_state_add_observer(KryonState *state, void (*observer)(KryonState*, void*), void *user_data);

/**
 * @brief Remove state observer
 * @param state State node
 * @param observer Observer callback
 * @return true if observer was removed
 */
bool kryon_state_remove_observer(KryonState *state, void (*observer)(KryonState*, void*));

/**
 * @brief Get state by path
 * @param runtime Runtime instance
 * @param path State path (e.g., "user.profile.name")
 * @return State node or NULL
 */
KryonState *kryon_state_get_by_path(KryonRuntime *runtime, const char *path);

// =============================================================================
// LIFECYCLE HOOKS
// =============================================================================

/**
 * @brief Lifecycle hook function type
 */
typedef void (*KryonLifecycleHook)(KryonElement *element, void *user_data);

/**
 * @brief Register element lifecycle hook
 * @param runtime Runtime instance
 * @param element_type Element type (0 for all)
 * @param hook Hook function
 * @param user_data User data
 * @return true on success
 */
bool kryon_runtime_add_lifecycle_hook(KryonRuntime *runtime, uint16_t element_type,
                                     KryonLifecycleHook hook, void *user_data);

// =============================================================================
// ERROR HANDLING
// =============================================================================

/**
 * @brief Get runtime errors
 * @param runtime Runtime instance
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_runtime_get_errors(const KryonRuntime *runtime, size_t *out_count);

/**
 * @brief Clear runtime errors
 * @param runtime Runtime instance
 */
void kryon_runtime_clear_errors(KryonRuntime *runtime);

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Get default runtime configuration
 * @return Default configuration
 */
KryonRuntimeConfig kryon_runtime_default_config(void);

/**
 * @brief Get development runtime configuration
 * @return Development configuration with hot reload
 */
KryonRuntimeConfig kryon_runtime_dev_config(void);

/**
 * @brief Get production runtime configuration
 * @return Optimized production configuration
 */
KryonRuntimeConfig kryon_runtime_prod_config(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_RUNTIME_H