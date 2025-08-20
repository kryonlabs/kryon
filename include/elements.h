/**
 * @file elements.h
 * @brief Consolidated Element System - Clean Architecture
 * 
 * Perfect abstractions:
 * - Generic element registration and dispatch
 * - Abstract events with mouse position data
 * - Clean hit testing (no element-specific code)
 * - Element VTable interface
 * 
 * 0BSD License
 */

#ifndef KRYON_INTERNAL_ELEMENTS_H
#define KRYON_INTERNAL_ELEMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "events.h"
#include "types.h"
#include "renderer_interface.h"

// Forward declarations
struct KryonRuntime;
struct KryonElement;

// =============================================================================
// ELEMENT BASE STRUCTURE (Keep existing KryonElement)
// =============================================================================

/**
 * @brief Element structure (existing definition preserved)
 */
struct KryonElement {
    // Identification
    uint32_t id;                     // Unique element ID
    uint16_t type;                   // Element type (hex code)
    char *type_name;                 // Element type name
    char *element_id;                // User-defined ID (optional)
    
    // Hierarchy
    struct KryonElement *parent;            // Parent element
    struct KryonElement **children;         // Child elements
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
    float x, y;                      // Current position
    float width, height;             // Current size
    float last_x, last_y;            // Previous position for change detection
    float padding[4];                // Top, right, bottom, left
    float margin[4];                 // Top, right, bottom, left
    bool needs_layout;               // Layout dirty flag
    bool position_changed;           // Position changed since last frame
    
    // State
    KryonElementState state;         // Lifecycle state
    bool visible;                    // Visibility flag
    bool enabled;                    // Enabled flag
    void *user_data;                 // User-defined data
    
    // Events
    ElementEventHandler **handlers;  // Event handlers
    size_t handler_count;            // Number of handlers
    size_t handler_capacity;         // Handler array capacity
    
    // Rendering
    bool needs_render;               // Render dirty flag
    uint32_t render_order;           // Z-order for rendering
    void *render_data;               // Renderer-specific data
    
    // Component instance (if this element is a component)
    KryonComponentInstance *component_instance; // Component instance data

    // Scripting
    void *script_state;
    
    // @for directive template storage
    struct KryonElement **template_children;  // Template children for @for directive
    size_t template_count;                    // Number of template children
};

// =============================================================================
// ELEMENT EVENT TYPES
// =============================================================================

typedef enum {
    // Basic interaction events
    ELEMENT_EVENT_CLICKED,
    ELEMENT_EVENT_DOUBLE_CLICKED,
    ELEMENT_EVENT_HOVERED,
    ELEMENT_EVENT_UNHOVERED,
    ELEMENT_EVENT_FOCUSED,
    ELEMENT_EVENT_UNFOCUSED,
    
    // Mouse movement with position data
    ELEMENT_EVENT_MOUSE_MOVED,
    
    // Keyboard events  
    ELEMENT_EVENT_KEY_PRESSED,
    ELEMENT_EVENT_KEY_TYPED,
    
    // State change events
    ELEMENT_EVENT_SELECTION_CHANGED,
    ELEMENT_EVENT_VALUE_CHANGED,
    
    ELEMENT_EVENT_CUSTOM
} ElementEventType;

// =============================================================================
// EVENT DATA STRUCTURES
// =============================================================================

typedef struct {
    float x, y;
} ElementMousePos;

typedef struct {
    char character;
    bool shift, ctrl, alt;
} ElementKeyTyped;

typedef struct {
    int keyCode;
    bool shift, ctrl, alt;
} ElementKeyPressed;

typedef struct {
    int oldValue, newValue;
} ElementSelectionChanged;

typedef struct {
    const char* oldValue;
    const char* newValue;
} ElementValueChanged;

typedef struct {
    uint32_t eventId;
    void* data;
} ElementCustomData;

// =============================================================================
// ELEMENT EVENT STRUCTURE
// =============================================================================

typedef struct {
    ElementEventType type;
    uint64_t timestamp;
    bool handled;
    
    union {
        ElementMousePos mousePos;        // For MOUSE_MOVED, CLICKED, etc.
        ElementKeyTyped keyTyped;        // For KEY_TYPED
        ElementKeyPressed keyPressed;    // For KEY_PRESSED
        ElementSelectionChanged selectionChanged; // For SELECTION_CHANGED
        ElementValueChanged valueChanged;     // For VALUE_CHANGED
        ElementCustomData custom;        // For CUSTOM events
    } data;
} ElementEvent;

// =============================================================================
// ELEMENT BOUNDS
// =============================================================================

typedef struct {
    float x, y, width, height;
} ElementBounds;

// =============================================================================
// ELEMENT VTABLE INTERFACE
// =============================================================================

typedef struct {
    /**
     * Render element to command buffer
     */
    void (*render)(struct KryonRuntime* runtime, 
                  struct KryonElement* element,
                  KryonRenderCommand* commands,
                  size_t* command_count,
                  size_t max_commands);
    
    /**
     * Handle abstract element event
     */
    bool (*handle_event)(struct KryonRuntime* runtime,
                        struct KryonElement* element,
                        const ElementEvent* event);
    
    /**
     * Destroy element and cleanup state
     */
    void (*destroy)(struct KryonRuntime* runtime,
                   struct KryonElement* element);
} ElementVTable;

// =============================================================================
// HIT TESTING MANAGER
// =============================================================================

typedef struct HitTestManager {
    struct KryonElement* hovered_element;
    struct KryonElement* focused_element;
    struct KryonElement* clicked_element;
    
    uint64_t last_click_time;
    float last_click_x, last_click_y;
} HitTestManager;

// =============================================================================
// ELEMENT SYSTEM FUNCTIONS
// =============================================================================

/**
 * Initialize element registry
 */
bool element_registry_init(void);

/**
 * Cleanup element registry
 */
void element_registry_cleanup(void);

/**
 * Register element type with VTable
 */
bool element_register_type(const char* type_name, const ElementVTable* vtable);

/**
 * Get VTable for element type
 */
const ElementVTable* element_get_vtable(const char* type_name);

/**
 * Dispatch event to element via registry
 */
bool element_dispatch_event(struct KryonRuntime* runtime, 
                           struct KryonElement* element, 
                           const ElementEvent* event);

// =============================================================================
// HIT TESTING FUNCTIONS (Generic)
// =============================================================================

/**
 * Create hit testing manager
 */
HitTestManager* hit_test_manager_create(void);

/**
 * Destroy hit testing manager
 */
void hit_test_manager_destroy(HitTestManager* manager);

/**
 * Get element bounds from properties
 */
ElementBounds element_get_bounds(struct KryonElement* element);

/**
 * Test if point is within bounds
 */
bool point_in_bounds(ElementBounds bounds, float x, float y);

/**
 * Find topmost element at coordinates
 */
struct KryonElement* hit_test_find_element_at_point(struct KryonElement* root, float x, float y);

/**
 * Find dropdown popup at coordinates (highest priority)
 */
struct KryonElement* hit_test_find_dropdown_popup_at_point(struct KryonElement* root, float x, float y);

/**
 * Update hover states and send mouse position to elements
 */
void hit_test_update_hover_states(HitTestManager* manager, 
                                 struct KryonRuntime* runtime,
                                 struct KryonElement* root, 
                                 float mouse_x, float mouse_y);

/**
 * Process input event and convert to abstract element events
 */
void hit_test_process_input_event(HitTestManager* manager,
                                 struct KryonRuntime* runtime,
                                 struct KryonElement* root,
                                 const KryonEvent* input_event);

// =============================================================================
// ELEMENT RENDERING (Generic)
// =============================================================================

/**
 * Render element via registry lookup
 */
bool element_render_via_registry(struct KryonRuntime* runtime, 
                                struct KryonElement* element,
                                KryonRenderCommand* commands,
                                size_t* command_count,
                                size_t max_commands);

/**
 * Destroy element via registry lookup
 */
void element_destroy_via_registry(struct KryonRuntime* runtime, 
                                 struct KryonElement* element);


// =============================================================================
// REACTIVE LAYOUT SYSTEM
// =============================================================================

/**
 * Check if an element needs layout recalculation
 */
bool element_needs_layout(struct KryonElement* element);

/**
 * Single-pass position calculator - calculates final (x,y) for all elements
 */
void calculate_all_element_positions(struct KryonRuntime* runtime, struct KryonElement* root);

/**
 * Check if any elements changed position and mark them for re-rendering
 */
void update_render_flags_for_changed_positions(struct KryonElement* root);



/**
 * @brief A generic, reusable event handler that dispatches events to script functions.
 * Any element can use this in its VTable to support standard scriptable events
 * like "onClick", "onHover", etc.
 */
 bool generic_script_event_handler(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event);

// =============================================================================
// EXTERNAL UTILITY FUNCTIONS (defined elsewhere)
// =============================================================================

// Element property getters (defined in element_utils or runtime)
extern float get_element_property_float(struct KryonElement* element, const char* name, float default_val);
extern int get_element_property_int(struct KryonElement* element, const char* name, int default_val);
extern bool get_element_property_bool(struct KryonElement* element, const char* name, bool default_val);
extern const char* get_element_property_string(struct KryonElement* element, const char* name);
extern uint32_t get_element_property_color(struct KryonElement* element, const char* name, uint32_t default_val);
extern const char** get_element_property_array(struct KryonElement* element, const char* name, size_t* count);

// Element property setters (defined in runtime)
extern bool kryon_element_set_property_by_name(struct KryonElement* element, const char* name, const void* value);

// =============================================================================
// ELEMENT STATE MANAGEMENT API
// =============================================================================

// TabBar state management functions
extern int tabbar_get_selected_index(struct KryonElement* tabbar_element);
extern void tabbar_set_selected_index(struct KryonElement* tabbar_element, int index);
extern bool tabbar_is_tab_active(struct KryonElement* tabbar_element, int tab_index);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_ELEMENTS_H