/**
 * @file elements.c
 * @brief Consolidated Element System - Registry, Events, Hit Testing
 * 
 * Clean architecture with perfect abstractions:
 * - Generic element registration and lifecycle
 * - Abstract event dispatch to elements
 * - Generic hit testing (no element-specific logic)
 * - Mouse position forwarding to elements
 * 
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "events.h"
#include "elements/element_mixins.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// =============================================================================
// TIMING UTILITIES
// =============================================================================

static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// =============================================================================
// ELEMENT REGISTRY SYSTEM
// =============================================================================

#define MAX_ELEMENT_TYPES 32

typedef struct {
    char* type_name;
    const ElementVTable* vtable;
} ElementTypeEntry;

static struct {
    ElementTypeEntry entries[MAX_ELEMENT_TYPES];
    size_t count;
    bool initialized;
} g_element_registry = {0};

// Forward declarations for element registration functions
extern bool register_dropdown_element(void);
extern bool register_input_element(void);
extern bool register_button_element(void);
extern bool register_checkbox_element(void);
extern bool register_slider_element(void); 
extern bool register_text_element(void);
extern bool register_container_element(void);
extern bool register_image_element(void);
// Layout elements (Column, Row, Center) now registered by register_container_element()
extern bool register_app_element(void);
extern bool register_grid_element(void);
extern bool register_tabbar_element(void);
extern bool register_tab_element(void);
extern bool register_tab_content_element(void);
extern bool register_tabpanel_element(void);
extern bool register_tabgroup_element(void);
extern bool register_link_element(void);

// Forward declarations for position calculation pipeline
static void calculate_element_position_recursive(struct KryonRuntime* runtime, struct KryonElement* element,
                                               float parent_x, float parent_y, float available_width, float available_height,
                                               struct KryonElement* layout_parent);
static void position_row_children(struct KryonRuntime* runtime, struct KryonElement* row);
static void position_column_children(struct KryonRuntime* runtime, struct KryonElement* column);
static void position_container_children(struct KryonRuntime* runtime, struct KryonElement* container);
static void position_center_children(struct KryonRuntime* runtime, struct KryonElement* center);
static void position_app_children(struct KryonRuntime* runtime, struct KryonElement* app);
static void position_grid_children(struct KryonRuntime* runtime, struct KryonElement* grid);
static void position_tabbar_children(struct KryonRuntime* runtime, struct KryonElement* tabbar);
static void position_tabgroup_children(struct KryonRuntime* runtime, struct KryonElement* tabgroup);
static void position_tab_children(struct KryonRuntime* runtime, struct KryonElement* tab);

// Generic contentAlignment positioning function
static void position_children_with_content_alignment(struct KryonRuntime* runtime, struct KryonElement* parent, 
                                                    const char* default_alignment, bool multi_child_stacking);

// Small efficiency helpers
static inline bool is_text_element(const struct KryonElement* element) {
    return element && element->type_name && (element->type_name[0] == 'T') && strcmp(element->type_name, "Text") == 0;
}

static inline bool is_button_element(const struct KryonElement* element) {
    return element && element->type_name && (element->type_name[0] == 'B') && strcmp(element->type_name, "Button") == 0;
}

// Efficient text measurement with reduced function calls
static void get_text_dimensions(struct KryonRuntime* runtime, struct KryonElement* element, 
                               float* width, float* height) {
    *width = 100.0f;  // Default
    *height = 50.0f;  // Default
    
    if (is_text_element(element)) {
        const char* text = get_element_property_string(element, "text");
        float font_size = get_element_property_float(element, "fontSize", 16.0f);
        
        // Calculate height first (cheaper operation)
        *height = font_size * 1.2f;
        
        if (!text) {
            *width = 100.0f;
            return;
        }
        
        // Try renderer measurement (more expensive)
        KryonRenderer* renderer = runtime ? (KryonRenderer*)runtime->renderer : NULL;
        if (renderer && renderer->vtable && renderer->vtable->measure_text_width) {
            *width = renderer->vtable->measure_text_width(text, font_size);
        } else {
            // Fallback calculation
            *width = strlen(text) * font_size * 0.6f;
        }
    } else if (is_button_element(element)) {
        // Handle Button elements - use EXACT same logic as button_render() does
        const char* text = get_element_property_string(element, "text");
        
        // Get explicit dimensions (use -1 for auto-sizing, matching button.c)
        *width = get_element_property_float(element, "width", -1.0f);
        *height = get_element_property_float(element, "height", -1.0f);
        
        // Call the exact same function that button_render() uses
        calculate_auto_size_with_text(element, width, height, text, 20.0f, 12.0f, 60.0f, 32.0f);
    }
    // For other elements, keep default 100x50 dimensions
}

static float get_default_element_height(struct KryonElement* element); 

bool element_registry_init_with_all_elements(void) {
    if (g_element_registry.initialized) {
        return true;
    }
    
    memset(&g_element_registry, 0, sizeof(g_element_registry));
    g_element_registry.initialized = true;
    
    // Auto-register all available elements
    if (!register_dropdown_element()) {
        printf("ERROR: Failed to register Dropdown element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_input_element()) {
        printf("ERROR: Failed to register Input element\n");
        element_registry_cleanup();
        return false;
    }

    if (!register_button_element()) {
        printf("ERROR: Failed to register Button element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_checkbox_element()) {
        printf("ERROR: Failed to register Checkbox element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_slider_element()) {
        printf("ERROR: Failed to register Slider element\n");
        element_registry_cleanup();
        return false;
    }


    if (!register_text_element()) {
        printf("ERROR: Failed to register Text element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_container_element()) {
        printf("ERROR: Failed to register Container element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_image_element()) {
        printf("ERROR: Failed to register Image element\n");
        element_registry_cleanup();
        return false;
    }
    
    // Layout elements (Column, Row, Center) are now registered by register_container_element()
    
    if (!register_app_element()) {
        printf("ERROR: Failed to register App element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_grid_element()) {
        printf("ERROR: Failed to register Grid element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_tabbar_element()) {
        printf("ERROR: Failed to register TabBar element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_tab_element()) {
        printf("ERROR: Failed to register Tab element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_tab_content_element()) {
        printf("ERROR: Failed to register TabContent element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_tabpanel_element()) {
        printf("ERROR: Failed to register TabPanel element\n");
        element_registry_cleanup();
        return false;
    }

    if (!register_tabgroup_element()) {
        printf("ERROR: Failed to register TabGroup element\n");
        element_registry_cleanup();
        return false;
    }

    if (!register_link_element()) {
        printf("ERROR: Failed to register Link element\n");
        element_registry_cleanup();
        return false;
    }
    
    return true;
}

bool element_registry_init(void) {
    return element_registry_init_with_all_elements();
}

void element_registry_cleanup(void) {
    if (!g_element_registry.initialized) {
        return;
    }
    
    for (size_t i = 0; i < g_element_registry.count; i++) {
        free(g_element_registry.entries[i].type_name);
        g_element_registry.entries[i].type_name = NULL;
        g_element_registry.entries[i].vtable = NULL;
    }
    
    g_element_registry.count = 0;
    g_element_registry.initialized = false;
}

bool element_register_type(const char* type_name, const ElementVTable* vtable) {
    if (!g_element_registry.initialized) {
        printf("ERROR: Element registry not initialized\n");
        return false;
    }
    
    if (!type_name || !vtable) {
        printf("ERROR: Invalid parameters for element registration\n");
        return false;
    }
    
    if (g_element_registry.count >= MAX_ELEMENT_TYPES) {
        printf("ERROR: Element registry full (max %d types)\n", MAX_ELEMENT_TYPES);
        return false;
    }
    
    // Check for duplicates
    for (size_t i = 0; i < g_element_registry.count; i++) {
        if (strcmp(g_element_registry.entries[i].type_name, type_name) == 0) {
            printf("WARNING: Element type '%s' already registered, updating\n", type_name);
            g_element_registry.entries[i].vtable = vtable;
            return true;
        }
    }
    
    // Add new entry
    size_t index = g_element_registry.count;
    g_element_registry.entries[index].type_name = strdup(type_name);
    g_element_registry.entries[index].vtable = vtable;
    
    if (!g_element_registry.entries[index].type_name) {
        printf("ERROR: Failed to allocate memory for type name '%s'\n", type_name);
        return false;
    }
    
    g_element_registry.count++;
    return true;
}

const ElementVTable* element_get_vtable(const char* type_name) {
    if (!type_name || !g_element_registry.initialized) {
        return NULL;
    }
    
    for (size_t i = 0; i < g_element_registry.count; i++) {
        if (strcmp(g_element_registry.entries[i].type_name, type_name) == 0) {
            return g_element_registry.entries[i].vtable;
        }
    }
    
    return NULL;
}

// =============================================================================
// TEXT INPUT FALLBACK
// =============================================================================

// Helper function to dispatch text events to all Input elements when no element is focused
static void dispatch_text_event_to_all_inputs(struct KryonRuntime* runtime, struct KryonElement* root, const ElementEvent* text_event) {
    if (!root || !text_event) return;
    
    // Check if this element is an Input
    if (root->type_name && strcmp(root->type_name, "Input") == 0) {
        element_dispatch_event(runtime, root, text_event);
        return; // Only dispatch to the first Input found
    }
    
    // Recursively check children
    for (size_t i = 0; i < root->child_count; i++) {
        dispatch_text_event_to_all_inputs(runtime, root->children[i], text_event);
    }
}

// =============================================================================
// ABSTRACT EVENT SYSTEM
// =============================================================================

bool element_dispatch_event(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event) {
        return false;
    }
    
    const ElementVTable* vtable = element_get_vtable(element->type_name);
    if (!vtable || !vtable->handle_event) {
        return false; // No event handler for this element type
    }
    
    
    return vtable->handle_event(runtime, element, event);
}


// =============================================================================
// GENERIC, DATA-DRIVEN EVENT DISPATCHER
// =============================================================================

// Maps abstract element events to their corresponding property names in KRY.
// This is the "router" that connects the C event system to the script world.
typedef struct {
    ElementEventType event_type;
    const char* property_name;
} EventPropertyMapping;

static const EventPropertyMapping event_property_map[] = {
    { ELEMENT_EVENT_CLICKED,           "onClick" },
    { ELEMENT_EVENT_DOUBLE_CLICKED,    "onDoubleClick" },
    { ELEMENT_EVENT_HOVERED,           "onHover" },
    { ELEMENT_EVENT_UNHOVERED,         "onUnhover" },
    { ELEMENT_EVENT_FOCUSED,           "onFocus" },
    { ELEMENT_EVENT_UNFOCUSED,         "onBlur" },
    { ELEMENT_EVENT_KEY_PRESSED,       "onKeyPress" },
    { ELEMENT_EVENT_KEY_TYPED,         "onKeyType" },
    { ELEMENT_EVENT_SELECTION_CHANGED, "onSelectionChange" },
    { ELEMENT_EVENT_VALUE_CHANGED,     "onChange" },
    { (ElementEventType)0, NULL } // End of list
};

// =============================================================================
// GENERIC SCRIPT EVENT HANDLER
// =============================================================================

/**
 * @brief A generic event handler for any element that uses script-based callbacks.
 *
 * This function is the core of the new data-driven event system. It receives
 * an abstract ElementEvent, finds the corresponding property name (e.g., "onClick"),
 * retrieves the function name from the element's properties, and executes it
 * using the script VM. Any interactive element (Button, Container, etc.) can
 * register this function in its VTable to become scriptable.
 */
bool generic_script_event_handler(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event || !runtime->script_vm) {
        printf("❌ SCRIPT HANDLER: Invalid parameters (runtime=%p, element=%p, event=%p, script_vm=%p)\n",
               (void*)runtime, (void*)element, (void*)event, 
               runtime ? (void*)runtime->script_vm : NULL);
        return false;
    }


    // Find the corresponding property name for the received event type
    const char* handler_prop_name = NULL;
    for (int i = 0; event_property_map[i].property_name != NULL; i++) {
        if (event_property_map[i].event_type == event->type) {
            handler_prop_name = event_property_map[i].property_name;
            break;
        }
    }

    if (!handler_prop_name) {
        return false; // This handler doesn't process this event type
    }

    // Check if the element has a handler defined for this event
    const char* handler_function_name = get_element_property_string(element, handler_prop_name);
    if (!handler_function_name || handler_function_name[0] == '\0') {
        // FALLBACK: If this is a double-click event with no onDoubleClick handler,
        // try to use the onClick handler instead (common UX expectation for buttons)
        if (event->type == ELEMENT_EVENT_DOUBLE_CLICKED) {
            handler_function_name = get_element_property_string(element, "onClick");
            if (!handler_function_name || handler_function_name[0] == '\0') {
                return false; // No fallback handler available
            }
        } else {
            return false; // No script function assigned to this event property
        }
    }

    // Execute the handler using the script VM
    KryonVMResult result = kryon_vm_call_function(runtime->script_vm, handler_function_name, element, (const void*)event);
    
    if (result != KRYON_VM_SUCCESS) {
        const char* error = kryon_vm_get_error(runtime->script_vm);
        printf("❌ SCRIPT ERROR in handler '%s': %s\n",
                handler_function_name, error ? error : "Unknown VM error");
        fprintf(stderr, "SCRIPT ERROR in handler '%s': %s\n",
                handler_function_name, error ? error : "Unknown VM error");
        return false;
    }

    printf("✅ SCRIPT HANDLER: Function '%s' executed successfully\n", handler_function_name);

    // The script successfully handled the event.
    ((ElementEvent*)event)->handled = true;
    
    // Invalidate the UI as the script might have changed state
    runtime->needs_update = true;
    mark_elements_for_rerender(runtime->root);

    return true;
}

// =============================================================================
// CENTRALIZED LAYOUT CALCULATION (Extracted from runtime.c)
// =============================================================================

static bool has_explicit_position_properties(struct KryonElement* element) {
    // Check if element has explicit posX/posY properties by looking for non-zero values
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    return (posX != 0.0f || posY != 0.0f);
}

// Legacy layout function removed - positioning now handled by calculate_all_element_positions pipeline

/**
 * @brief Clean, single-pass layout calculation
 * Replaces the complex ensure_layout_positions_calculated system
 */
void kryon_layout_calculate_once(struct KryonRuntime* runtime, struct KryonElement* root) {
    if (!root) return;
    
    // New pipeline approach: Calculate all positions in one pass
    calculate_all_element_positions(runtime, root);
}

/**
 * @brief Recursively clear needs_layout flags after layout calculation
 */
static void clear_needs_layout_recursive(struct KryonElement* element) {
    if (!element) return;

    element->needs_layout = false;

    // Clear flag for all children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            clear_needs_layout_recursive(element->children[i]);
        }
    }
}

/**
 * @brief Single-pass position calculator - calculates final (x,y) for all elements
 * This replaces the complex VTable + recursive system with a simple pipeline
 */
void calculate_all_element_positions(struct KryonRuntime* runtime, struct KryonElement* root) {
    if (!root) return;

    // Get window dimensions for root positioning
    float window_width = get_element_property_float(root, "windowWidth", 800.0f);
    float window_height = get_element_property_float(root, "windowHeight", 600.0f);

    // Start position calculation from root
    calculate_element_position_recursive(runtime, root, 0.0f, 0.0f, window_width, window_height, NULL);

    // Update render flags based on position changes
    update_render_flags_for_changed_positions(root);

    // Clear needs_layout flags now that layout is calculated
    clear_needs_layout_recursive(root);
}

/**
 * @brief Recursive position calculation - handles all layout rules in one place
 */
static void calculate_element_position_recursive(struct KryonRuntime* runtime, struct KryonElement* element,
                                               float parent_x, float parent_y, float available_width, float available_height,
                                               struct KryonElement* layout_parent) {
    if (!element) return;
    
    // Store previous position for change detection
    element->last_x = element->x;
    element->last_y = element->y;
    
    // 1. Handle explicit positioning first (posX, posY)
    float explicit_posX = get_element_property_float(element, "posX", 0.0f);
    float explicit_posY = get_element_property_float(element, "posY", 0.0f);
    
    if (explicit_posX != 0.0f || explicit_posY != 0.0f) {
        // Element has explicit positioning - use it
        element->x = explicit_posX != 0.0f ? explicit_posX : parent_x;
        element->y = explicit_posY != 0.0f ? explicit_posY : parent_y;
    } else {
        // Use parent-provided position (from layout parent)
        element->x = parent_x;
        element->y = parent_y;
    }
    
    // 2. Set element dimensions
    float explicit_width = get_element_property_float(element, "width", 0.0f);
    float explicit_height = get_element_property_float(element, "height", 0.0f);
    
    
    // Handle size inheritance properly
    if (explicit_width > 0.0f) {
        element->width = explicit_width;
    } else {
        // Use available space, but ensure layout containers get reasonable defaults
        if (element->type_name && (strcmp(element->type_name, "Column") == 0 || strcmp(element->type_name, "Row") == 0)) {
            element->width = available_width > 0.0f ? available_width : 400.0f;
        } else {
            element->width = available_width > 0.0f ? available_width : 100.0f;
        }
    }
    
    if (explicit_height > 0.0f) {
        element->height = explicit_height;
    } else {
        // Determine if element is a container/layout element that should fill space
        bool is_container = element->type_name && (
            strcmp(element->type_name, "Column") == 0 ||
            strcmp(element->type_name, "Row") == 0 ||
            strcmp(element->type_name, "Container") == 0 ||
            strcmp(element->type_name, "Center") == 0 ||
            strcmp(element->type_name, "Grid") == 0 ||
            strcmp(element->type_name, "App") == 0 ||
            strcmp(element->type_name, "TabGroup") == 0 ||
            strcmp(element->type_name, "TabPanel") == 0 ||
            strcmp(element->type_name, "TabContent") == 0 ||
            strcmp(element->type_name, "TabBar") == 0
        );

        if (is_container) {
            // Container elements should fill available space
            if (strcmp(element->type_name, "Row") == 0 && layout_parent && layout_parent->type_name &&
                strcmp(layout_parent->type_name, "Column") == 0) {
                // Row in Column: use smaller default height
                element->height = 40.0f;
            } else {
                element->height = available_height > 0.0f ? available_height : 300.0f;
            }
        } else {
            // Interactive/content elements use their intrinsic default height
            // This prevents Input/Button/Text from being forced to fill parent
            element->height = get_default_element_height(element);
        }
    }
    
    // 3. Check if position changed (avoid setting flag if positions are the same)
    bool pos_changed = (fabsf(element->x - element->last_x) > 0.1f || fabsf(element->y - element->last_y) > 0.1f);
    element->position_changed = pos_changed;
    
    // 4. Position children based on this element's layout type
    position_children_by_layout_type(runtime, element);
}

/**
 * @brief Position children based on parent's layout type (Row, Column, Container, etc.)
 */
void position_children_by_layout_type(struct KryonRuntime* runtime, struct KryonElement* parent) {
    if (!parent || parent->child_count == 0) return;
    
    if (!parent->type_name) {
        // No specific layout - position children at same location
        for (size_t i = 0; i < parent->child_count; i++) {
            calculate_element_position_recursive(runtime, parent->children[i], 
                                               parent->x, parent->y, parent->width, parent->height, parent);
        }
        return;
    }
    
    // Handle specific layout types
    if (strcmp(parent->type_name, "Row") == 0) {
        position_row_children(runtime, parent);
    } else if (strcmp(parent->type_name, "Column") == 0) {
        position_column_children(runtime, parent);
    } else if (strcmp(parent->type_name, "Container") == 0) {
        position_container_children(runtime, parent);
    } else if (strcmp(parent->type_name, "Center") == 0) {
        position_center_children(runtime, parent);
    } else if (strcmp(parent->type_name, "App") == 0) {
        position_app_children(runtime, parent);
    } else if (strcmp(parent->type_name, "Grid") == 0) {
        position_grid_children(runtime, parent);
    } else if (strcmp(parent->type_name, "TabBar") == 0) {
        position_tabbar_children(runtime, parent);
    } else if (strcmp(parent->type_name, "TabGroup") == 0) {
        position_tabgroup_children(runtime, parent);
    } else if (strcmp(parent->type_name, "Tab") == 0) {
        // Tab can contain children - center by default
        position_tab_children(runtime, parent);
    } else if (strcmp(parent->type_name, "TabPanel") == 0) {
        // TabPanel behaves like Container - uses contentAlignment
        printf("🔧 TABPANEL [%p]: Positioning %zu children (bounds: x=%.1f, y=%.1f, w=%.1f, h=%.1f)\n",
               (void*)parent, parent->child_count, parent->x, parent->y, parent->width, parent->height);
        position_container_children(runtime, parent);
    } else {
        // Default: position children at same location
        for (size_t i = 0; i < parent->child_count; i++) {
            calculate_element_position_recursive(runtime, parent->children[i],
                                               parent->x, parent->y, parent->width, parent->height, parent);
        }
    }
}

/**
 * @brief ENHANCED Row positioning with complete flexbox alignment support and flex wrapping.
 * Row elements automatically behave like Container with direction="row"
 */
static void position_row_children(struct KryonRuntime* runtime, struct KryonElement* row) {
    if (!row || row->child_count == 0) return;
    
    // Row elements use canonical property names only
    const char* main_axis = get_element_property_string(row, "mainAxis");
    if (!main_axis) main_axis = "start";
    
    const char* cross_axis = get_element_property_string(row, "crossAxis");
    if (!cross_axis) cross_axis = "start";
    
    float gap = get_element_property_float(row, "gap", 0.0f);
    float padding = get_element_property_float(row, "padding", 0.0f);
    
    
    // Check for flex wrap support
    const char* wrap = get_element_property_string(row, "wrap");
    if (!wrap) wrap = get_element_property_string(row, "flexWrap");
    bool should_wrap = (wrap && (strcmp(wrap, "wrap") == 0 || strcmp(wrap, "true") == 0));
    
    float content_x = row->x + padding;
    float content_y = row->y + padding;
    float content_width = row->width - (padding * 2.0f);
    float content_height = row->height - (padding * 2.0f);

    if (should_wrap) {
        // FLEX WRAP: Multi-line row layout
        float current_x = content_x;
        float current_y = content_y;
        float max_line_height = 0.0f;
        
        for (size_t i = 0; i < row->child_count; i++) {
            struct KryonElement* child = row->children[i];
            float child_width = get_element_property_float(child, "width", 100.0f);
            float child_height = get_element_property_float(child, "height", 50.0f);
            
            // Check if we need to wrap to next line
            if (current_x + child_width > content_x + content_width && i > 0) {
                // Move to next line
                current_y += max_line_height + gap;
                current_x = content_x;
                max_line_height = 0.0f;
            }
            
            // Track maximum height in current line for proper line spacing
            if (child_height > max_line_height) {
                max_line_height = child_height;
            }
            
            // Position child in current line
            float child_y = current_y;
            
            // Apply cross-axis alignment within the line
            if (strcmp(cross_axis, "center") == 0) {
                child_y = current_y; // Center alignment handled per-line
            } else if (strcmp(cross_axis, "stretch") == 0) {
                child_height = max_line_height;
            }
            
            calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
            current_x += child_width + gap;
        }
    } else {
        // SINGLE LINE: Enhanced row layout with complete alignment support
        
        // Pre-calculate all child dimensions for consistent positioning
        float* child_widths = malloc(sizeof(float) * row->child_count);
        float* child_heights = malloc(sizeof(float) * row->child_count);
        float total_child_width = 0.0f;
        
        for (size_t i = 0; i < row->child_count; i++) {
            child_widths[i] = get_element_property_float(row->children[i], "width", 100.0f);
            child_heights[i] = get_element_property_float(row->children[i], "height", 50.0f);
            total_child_width += child_widths[i];
        }
        total_child_width += (row->child_count > 1) ? (gap * (row->child_count - 1)) : 0;

        float current_x = content_x;
        float spacing = gap;

        // Enhanced main axis (horizontal) alignment
        if (strcmp(main_axis, "spaceBetween") == 0 || strcmp(main_axis, "space-between") == 0) {
            // Space Between: Equal spacing between items, no space at edges
            if (row->child_count > 1) {
                float child_widths_only = total_child_width - (gap * (row->child_count - 1));
                float remaining_space = content_width - child_widths_only;
                float space_between = remaining_space / (row->child_count - 1);
                
                for (size_t i = 0; i < row->child_count; i++) {
                    struct KryonElement* child = row->children[i];
                    float child_width = child_widths[i];
                    float child_height = child_heights[i];
                    float child_y = content_y;
                    
                    // Cross-axis alignment
                    if (strcmp(cross_axis, "center") == 0) {
                        child_y += (content_height - child_height) / 2.0f;
                    } else if (strcmp(cross_axis, "end") == 0 || strcmp(cross_axis, "flex-end") == 0) {
                        child_y += content_height - child_height;
                    } else if (strcmp(cross_axis, "stretch") == 0) {
                        child_height = content_height;
                    }
                    
                    calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
                    if (i < row->child_count - 1) {
                        current_x += child_width + space_between;
                    }
                }
            } else {
                // Single child - just center it
                struct KryonElement* child = row->children[0];
                float child_width = child_widths[0];
                float child_height = child_heights[0];
                float child_y = content_y;
                if (strcmp(cross_axis, "center") == 0) {
                    child_y += (content_height - child_height) / 2.0f;
                } else if (strcmp(cross_axis, "stretch") == 0) {
                    child_height = content_height;
                }
                calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
            }
        } else if (strcmp(main_axis, "spaceAround") == 0 || strcmp(main_axis, "space-around") == 0) {
            // Space Around: Equal space around each item
            if (row->child_count > 0) {
                float child_widths_only = total_child_width - (gap * (row->child_count - 1));
                float remaining_space = content_width - child_widths_only;
                float space_per_item = remaining_space / row->child_count;
                current_x += space_per_item / 2.0f;
                
                for (size_t i = 0; i < row->child_count; i++) {
                    struct KryonElement* child = row->children[i];
                    float child_width = child_widths[i];
                    float child_height = child_heights[i];
                    float child_y = content_y;
                    
                    // Cross-axis alignment
                    if (strcmp(cross_axis, "center") == 0) {
                        child_y += (content_height - child_height) / 2.0f;
                    } else if (strcmp(cross_axis, "end") == 0 || strcmp(cross_axis, "flex-end") == 0) {
                        child_y += content_height - child_height;
                    } else if (strcmp(cross_axis, "stretch") == 0) {
                        child_height = content_height;
                    }
                    
                    calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
                    current_x += child_width + space_per_item;
                }
            }
        } else if (strcmp(main_axis, "spaceEvenly") == 0 || strcmp(main_axis, "space-evenly") == 0) {
            // Space Evenly: Equal space before, between, and after items
            if (row->child_count > 0) {
                float child_widths_only = total_child_width - (gap * (row->child_count - 1));
                float remaining_space = content_width - child_widths_only;
                float space_per_gap = remaining_space / (row->child_count + 1);
                current_x += space_per_gap;
                
                for (size_t i = 0; i < row->child_count; i++) {
                    struct KryonElement* child = row->children[i];
                    float child_width = child_widths[i];
                    float child_height = child_heights[i];
                    float child_y = content_y;
                    
                    // Cross-axis alignment
                    if (strcmp(cross_axis, "center") == 0) {
                        child_y += (content_height - child_height) / 2.0f;
                    } else if (strcmp(cross_axis, "end") == 0 || strcmp(cross_axis, "flex-end") == 0) {
                        child_y += content_height - child_height;
                    } else if (strcmp(cross_axis, "stretch") == 0) {
                        child_height = content_height;
                    }
                    
                    calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
                    current_x += child_width + space_per_gap;
                }
            }
        } else {
            // Standard alignments: start, center, end
            if (strcmp(main_axis, "center") == 0) {
                current_x += (content_width - total_child_width) / 2.0f;
            } else if (strcmp(main_axis, "end") == 0 || strcmp(main_axis, "flex-end") == 0) {
                current_x += content_width - total_child_width;
            }
            // "start" and "flex-start" use default current_x = content_x
            
            for (size_t i = 0; i < row->child_count; i++) {
                struct KryonElement* child = row->children[i];
                float child_width = child_widths[i];
                float child_height = child_heights[i];
                float child_y = content_y;
                
                // Enhanced cross axis (vertical) alignment
                if (strcmp(cross_axis, "center") == 0) {
                    // Fix: Ensure proper centering within Row's content area
                    child_y = content_y + (content_height - child_height) / 2.0f;
                } else if (strcmp(cross_axis, "end") == 0 || strcmp(cross_axis, "flex-end") == 0) {
                    child_y += content_height - child_height;
                } else if (strcmp(cross_axis, "stretch") == 0) {
                    child_height = content_height;
                } else if (strcmp(cross_axis, "baseline") == 0) {
                    // For baseline alignment, use a simple font-based approach
                    if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                        float font_size = get_element_property_float(child, "fontSize", 16.0f);
                        child_y += font_size * 0.8f; // Approximate baseline offset
                    } else {
                        // Non-text elements align to their bottom edge for baseline
                        child_y += content_height - child_height;
                    }
                }
                // "start" and "flex-start" use default child_y = content_y
                
                calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
                current_x += child_width + gap; // Use original gap, not modified spacing
            }
        }
        
        // Clean up allocated memory
        free(child_widths);
        free(child_heights);
    }
}

/**
 * @brief ENHANCED Column positioning with complete flexbox alignment support and flex wrapping.
 * Column elements automatically behave like Container with direction="column"
 */
static void position_column_children(struct KryonRuntime* runtime, struct KryonElement* column) {
    if (!column || column->child_count == 0) return;
    
    // Column elements use canonical property names only
    const char* main_axis = get_element_property_string(column, "mainAxis");
    if (!main_axis) main_axis = "start";
    
    const char* cross_axis = get_element_property_string(column, "crossAxis");
    if (!cross_axis) cross_axis = "start";
    
    float gap = get_element_property_float(column, "gap", 0.0f);
    float padding = get_element_property_float(column, "padding", 0.0f);
    
    // Check for flex wrap support
    const char* wrap = get_element_property_string(column, "wrap");
    if (!wrap) wrap = get_element_property_string(column, "flexWrap");
    bool should_wrap = (wrap && (strcmp(wrap, "wrap") == 0 || strcmp(wrap, "true") == 0));
    
    float content_x = column->x + padding;
    float content_y = column->y + padding;
    float content_width = column->width - (padding * 2.0f);
    float content_height = column->height - (padding * 2.0f);

    if (should_wrap) {
        // FLEX WRAP: Multi-column layout (columns side by side)
        float current_x = content_x;
        float current_y = content_y;
        float max_column_width = 0.0f;
        
        for (size_t i = 0; i < column->child_count; i++) {
            struct KryonElement* child = column->children[i];
            float child_width = get_element_property_float(child, "width", 100.0f);
            float child_height = get_element_property_float(child, "height", 50.0f);
            
            // Check if we need to wrap to next column
            if (current_y + child_height > content_y + content_height && i > 0) {
                // Move to next column
                current_x += max_column_width + gap;
                current_y = content_y;
                max_column_width = 0.0f;
            }
            
            // Track maximum width in current column for proper column spacing
            if (child_width > max_column_width) {
                max_column_width = child_width;
            }
            
            // Position child in current column
            float child_x = current_x;
            
            // Apply cross-axis alignment within the column
            if (strcmp(cross_axis, "center") == 0) {
                child_x = current_x; // Center alignment handled per-column
            } else if (strcmp(cross_axis, "stretch") == 0) {
                child_width = max_column_width;
            }
            
            // Pass Column's content area as available space to children (like Container does) 
            calculate_element_position_recursive(runtime, child, child_x, current_y, content_width, content_height, column);
            current_y += child_height + gap;
        }
    } else {
        // SINGLE COLUMN: Enhanced column layout with complete alignment support
        float total_child_height = 0.0f;
        for (size_t i = 0; i < column->child_count; i++) {
            total_child_height += get_element_property_float(column->children[i], "height", get_default_element_height(column->children[i]));
        }
        total_child_height += (column->child_count > 1) ? (gap * (column->child_count - 1)) : 0;

        float current_y = content_y;
        float spacing = gap;
        

        // Enhanced main axis (vertical) alignment
        if (strcmp(main_axis, "center") == 0) {
            current_y += (content_height - total_child_height) / 2.0f;
        } else if (strcmp(main_axis, "end") == 0 || strcmp(main_axis, "flex-end") == 0) {
            current_y += content_height - total_child_height;
        } else if (strcmp(main_axis, "spaceBetween") == 0 || strcmp(main_axis, "space-between") == 0) {
            if (column->child_count > 1) {
                float child_heights_only = total_child_height - (gap * (column->child_count - 1));
                spacing = (content_height - child_heights_only) / (column->child_count - 1);
            }
        } else if (strcmp(main_axis, "spaceAround") == 0 || strcmp(main_axis, "space-around") == 0) {
            if (column->child_count > 0) {
                float child_heights_only = total_child_height - (gap * (column->child_count - 1));
                float remaining_space = content_height - child_heights_only;
                float space_per_item = remaining_space / column->child_count;
                current_y += space_per_item / 2.0f;
                spacing = gap + space_per_item;
            }
        } else if (strcmp(main_axis, "spaceEvenly") == 0 || strcmp(main_axis, "space-evenly") == 0) {
            if (column->child_count > 0) {
                float child_heights_only = total_child_height - (gap * (column->child_count - 1));
                float remaining_space = content_height - child_heights_only;
                float space_per_gap = remaining_space / (column->child_count + 1);
                current_y += space_per_gap;
                spacing = gap + space_per_gap;
            }
        }
        // "start" and "flex-start" use default current_y = content_y

        for (size_t i = 0; i < column->child_count; i++) {
            struct KryonElement* child = column->children[i];
            
            // Apply auto-sizing during layout phase to get correct dimensions
            float child_width = get_element_property_float(child, "width", -1.0f);
            float child_height = get_element_property_float(child, "height", -1.0f);
            
            // Auto-size if needed (especially for text elements)
            if (child_width < 0 || child_height < 0) {
                auto_size_element(child, &child_width, &child_height);
            }
            
            // Use defaults only if auto-sizing didn't provide values
            if (child_width < 0) child_width = 100.0f;
            if (child_height < 0) child_height = get_default_element_height(child);
            
            float child_x = content_x;
            
            // Enhanced cross axis (horizontal) alignment with bounds checking
            if (strcmp(cross_axis, "center") == 0) {
                float centered_x = content_x + (content_width - child_width) / 2.0f;
                child_x = fmaxf(centered_x, content_x); // Never go left of content area
            } else if (strcmp(cross_axis, "end") == 0 || strcmp(cross_axis, "flex-end") == 0) {
                float end_x = content_x + content_width - child_width;
                child_x = fmaxf(end_x, content_x); // Never go left of content area
            } else if (strcmp(cross_axis, "stretch") == 0) {
                child_width = content_width;
            } else if (strcmp(cross_axis, "baseline") == 0) {
                // For column layout, baseline alignment behaves like flex-start
                // (baseline is primarily meaningful for horizontal text alignment)
                child_x = content_x;
            }
            // "start" and "flex-start" use default child_x = content_x

            // Pass Column's content area as available space to children (like Container does)
            calculate_element_position_recursive(runtime, child, child_x, current_y, content_width, content_height, column);
            current_y += child_height + spacing;
        }
    }
}


/**
 * @brief Generic function to position children using contentAlignment
 * Used by Container, Center, App, and other elements that support contentAlignment
 */
static void position_children_with_content_alignment(struct KryonRuntime* runtime, struct KryonElement* parent, 
                                                    const char* default_alignment, bool multi_child_stacking) {
    if (!parent || parent->child_count == 0) return;
    
    const char* content_alignment = get_element_property_string(parent, "contentAlignment");
    if (!content_alignment) content_alignment = default_alignment;
    
    // Cache commonly used values to avoid repeated lookups
    const bool is_center_aligned = (strcmp(content_alignment, "center") == 0);
    
    float padding = get_element_property_float(parent, "padding", 0.0f);
    
    // Calculate available space inside padding
    float content_x = parent->x + padding;
    float content_y = parent->y + padding;
    float content_width = parent->width - (padding * 2.0f);
    float content_height = parent->height - (padding * 2.0f);
    
    // For multiple children with stacking, calculate total height first
    float total_height = 0.0f;
    float gap = 0.0f;
    if (multi_child_stacking && parent->child_count > 1) {
        gap = get_element_property_float(parent, "gap", 10.0f);
        
        // Calculate total height of all children plus gaps (optimized)
        for (size_t j = 0; j < parent->child_count; j++) {
            struct KryonElement* temp_child = parent->children[j];

            // Skip null or directive elements
            if (!temp_child || temp_child->type == 0x8200 || temp_child->type == 0x8300) {
                continue;
            }

            float temp_height = get_element_property_float(temp_child, "height", 0.0f);
            
            if (temp_height == 0.0f) {
                float temp_width;  // Unused but needed for function signature
                get_text_dimensions(runtime, temp_child, &temp_width, &temp_height);
            }
            
            total_height += temp_height;
            if (j < parent->child_count - 1) total_height += gap;
        }
    }
    
    // Position all children based on contentAlignment
    float stack_start_y = content_y;
    if (multi_child_stacking && parent->child_count > 1 && is_center_aligned) {
        stack_start_y = content_y + (content_height - total_height) / 2.0f;
    }
    
    float current_y = stack_start_y;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        struct KryonElement* child = parent->children[i];

        // Skip null or directive elements (they are templates, not UI elements)
        if (!child || child->type == 0x8200 || child->type == 0x8300) {
            continue;
        }

        // Get child dimensions - handle Text elements specially (they auto-size)
        float child_width = get_element_property_float(child, "width", 0.0f);
        float child_height = get_element_property_float(child, "height", 0.0f);
        
        // Get dimensions efficiently, avoiding redundant calculations
        if (child_width == 0.0f || child_height == 0.0f) {
            float measured_width, measured_height;
            get_text_dimensions(runtime, child, &measured_width, &measured_height);
            
            if (child_width == 0.0f) child_width = measured_width;
            if (child_height == 0.0f) child_height = measured_height;
        }
        
        float child_x = content_x;
        float child_y = content_y;
        
        // Apply contentAlignment positioning
        if (multi_child_stacking && parent->child_count > 1) {
            // Multiple children with stacking
            child_y = current_y;
            
            // Apply horizontal alignment for each child individually  
            if (is_center_aligned) {
                child_x = content_x + (content_width - child_width) / 2.0f;
            } else if (strcmp(content_alignment, "end") == 0) {
                child_x = content_x + content_width - child_width;
            }
            // "start" uses child_x = content_x (already set)
            
            current_y += child_height + gap;
        } else {
            // Single child or non-stacking elements
            if (is_center_aligned) {
                child_x = content_x + (content_width - child_width) / 2.0f;
                child_y = content_y + (content_height - child_height) / 2.0f;
            } else if (strcmp(content_alignment, "end") == 0) {
                child_x = content_x + content_width - child_width;
                child_y = content_y + content_height - child_height;
            } else if (strcmp(content_alignment, "centerX") == 0) {
                child_x = content_x + (content_width - child_width) / 2.0f;
                child_y = content_y;
            } else if (strcmp(content_alignment, "centerY") == 0) {
                child_x = content_x;
                child_y = content_y + (content_height - child_height) / 2.0f;
            }
            // "start" uses child_x = content_x, child_y = content_y (already set)
        }
        
        // For layout containers, pass the content area as available space
        float available_width_for_child = content_width;
        float available_height_for_child = content_height;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, child_y, 
                                           available_width_for_child, available_height_for_child, parent);
    }
}

/**
 * @brief Position children in a Container (with contentAlignment)
 */
static void position_container_children(struct KryonRuntime* runtime, struct KryonElement* container) {
    // Container uses no multi-child stacking by default
    printf("🔧 CONTAINER: Positioning %zu children for %s (x=%.1f, y=%.1f, w=%.1f, h=%.1f)\n",
           container->child_count, container->type_name, container->x, container->y, container->width, container->height);
    position_children_with_content_alignment(runtime, container, "start", false);

    // Debug: show first child's bounds after positioning
    if (container->child_count > 0 && container->children[0]) {
        printf("🔧 CONTAINER: First child '%s' positioned at (x=%.1f, y=%.1f, w=%.1f, h=%.1f), visible=%d\n",
               container->children[0]->type_name,
               container->children[0]->x, container->children[0]->y,
               container->children[0]->width, container->children[0]->height,
               container->children[0]->visible);
    }
}

/**
 * @brief Position children in a Tab layout (centered by default)
 */
static void position_tab_children(struct KryonRuntime* runtime, struct KryonElement* tab) {
    // Tab centers children by default
    position_children_with_content_alignment(runtime, tab, "center", false);
}

/**
 * @brief ENHANCED Center positioning with complete content alignment support.
 * Center elements automatically behave like Container with contentAlignment="center"
 */
static void position_center_children(struct KryonRuntime* runtime, struct KryonElement* center) {
    // Center uses center as default, no multi-child stacking
    position_children_with_content_alignment(runtime, center, "center", false);
}

/**
 * @brief Position children in an App layout (with contentAlignment and padding)
 * App elements inherit Container-like layout behavior but use window dimensions as base
 */
static void position_app_children(struct KryonRuntime* runtime, struct KryonElement* app) {
    // App uses multi-child stacking (vertical layout with gaps)
    position_children_with_content_alignment(runtime, app, "start", true);
}

/**
 * @brief Position children in a Grid layout
 * Grid elements arrange children in a 2D grid with configurable columns and spacing
 */
static void position_grid_children(struct KryonRuntime* runtime, struct KryonElement* grid) {
    if (!grid || grid->child_count == 0) return;
    
    // Get grid properties
    int columns = get_element_property_int(grid, "columns", 3);
    float gap = get_element_property_float(grid, "gap", 10.0f);
    float column_spacing = get_element_property_float(grid, "column_spacing", gap);
    float row_spacing = get_element_property_float(grid, "row_spacing", gap);
    float padding = get_element_property_float(grid, "padding", 0.0f);
    
    // Calculate grid structure
    int child_count = (int)grid->child_count;
    int rows = (int)ceil((double)child_count / (double)columns);
    
    // Calculate available space inside padding
    float content_x = grid->x + padding;
    float content_y = grid->y + padding;
    float content_width = grid->width - (padding * 2.0f);
    float content_height = grid->height - (padding * 2.0f);
    
    // Calculate cell dimensions
    float cell_width = (content_width - (column_spacing * (columns - 1))) / columns;
    float cell_height = (content_height - (row_spacing * (rows - 1))) / rows;
    
    // Position each child in the grid
    for (size_t i = 0; i < grid->child_count; i++) {
        struct KryonElement* child = grid->children[i];
        if (!child) continue;
        
        // Calculate grid position
        int row = (int)(i / columns);
        int col = (int)(i % columns);
        
        // Calculate child position
        float child_x = content_x + (col * (cell_width + column_spacing));
        float child_y = content_y + (row * (cell_height + row_spacing));
        
        // Calculate child dimensions - let child decide or use cell size
        float child_width = get_element_property_float(child, "width", cell_width);
        float child_height = get_element_property_float(child, "height", cell_height);
        
        // Don't exceed cell size
        if (child_width > cell_width) child_width = cell_width;
        if (child_height > cell_height) child_height = cell_height;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, child_y, child_width, child_height, grid);
    }
}

/**
 * @brief Check if any elements changed position and mark them for re-rendering
 */
void update_render_flags_for_changed_positions(struct KryonElement* root) {
    if (!root) return;
    
    // Check if this element's position changed
    if (root->position_changed) {
        root->needs_render = true;
    }
    
    // Recursively check children
    for (size_t i = 0; i < root->child_count; i++) {
        update_render_flags_for_changed_positions(root->children[i]);
    }
}




// =============================================================================
// ELEMENT BOUNDS AND HIT TESTING (Generic)
// =============================================================================

ElementBounds element_get_bounds(struct KryonElement* element) {
    ElementBounds bounds = {0, 0, 0, 0};
    if (!element) return bounds;
    
    // First priority: Use layout-calculated positions if available
    // The layout system calculates these during column/row layout
    if (element->x != 0.0f || element->y != 0.0f || element->width != 0.0f || element->height != 0.0f) {
        bounds.x = element->x;
        bounds.y = element->y;
        bounds.width = element->width;
        bounds.height = element->height;
        
        return bounds;
    }
    
    // Fallback: Use explicit properties (for manually positioned elements)
    bounds.x = get_element_property_float(element, "posX", 0.0f);
    bounds.y = get_element_property_float(element, "posY", 0.0f);  
    bounds.width = get_element_property_float(element, "width", 0.0f);
    bounds.height = get_element_property_float(element, "height", 0.0f);
    
    return bounds;
}

bool point_in_bounds(ElementBounds bounds, float x, float y) {
    return (x >= bounds.x && x <= bounds.x + bounds.width &&
            y >= bounds.y && y <= bounds.y + bounds.height);
}

bool point_in_bounds_with_tolerance(ElementBounds bounds, float x, float y, float tolerance) {
    return (x >= bounds.x - tolerance && x <= bounds.x + bounds.width + tolerance &&
            y >= bounds.y - tolerance && y <= bounds.y + bounds.height + tolerance);
}

// Helper function to get appropriate default height for element type
static float get_default_element_height(struct KryonElement* element) {
    if (!element || !element->type_name) return 50.0f;
    
    const char* type = element->type_name;
    
    // Text elements should calculate height based on font size
    if (strcmp(type, "Text") == 0) {
        float font_size = get_element_property_float(element, "fontSize", 16.0f);
        return font_size * 1.3f; // Font height + small padding
    }
    
    // Interactive elements have standard sizes
    if (strcmp(type, "Button") == 0) return 36.0f;
    if (strcmp(type, "Checkbox") == 0) return 24.0f;  
    if (strcmp(type, "Input") == 0) return 32.0f;
    if (strcmp(type, "Dropdown") == 0) return 32.0f;
    
    // Layout elements
    if (strcmp(type, "Column") == 0 || strcmp(type, "Row") == 0) return 200.0f;
    if (strcmp(type, "Container") == 0) return 100.0f;
    
    // Default fallback
    return 50.0f;
}

// Helper function to check if element type is interactive
static bool is_interactive_element(const char* type_name) {
    return (type_name && 
            (strcmp(type_name, "Checkbox") == 0 ||
             strcmp(type_name, "Button") == 0 ||
             strcmp(type_name, "Input") == 0 ||
             strcmp(type_name, "Dropdown") == 0 ||
             strcmp(type_name, "Tab") == 0 ||
             strcmp(type_name, "Link") == 0));
}

// Helper function to calculate text-based bounds for Link elements
static ElementBounds get_link_text_bounds(struct KryonElement* element) {
    ElementBounds bounds = {0, 0, 0, 0};
    if (!element || !element->type_name || strcmp(element->type_name, "Link") != 0) {
        return bounds;
    }
    
    // Get link text and font size
    const char* text = get_element_property_string(element, "text");
    if (!text) text = "Link";
    float font_size = get_element_property_float(element, "fontSize", 16.0f);
    const char* text_align = get_element_property_string(element, "textAlign");
    if (!text_align) text_align = "left";
    
    // Calculate approximate text dimensions (simple heuristic)
    // This is a basic estimation - in a real implementation you'd measure text with the renderer
    float text_width = strlen(text) * (font_size * 0.6f); // Rough character width estimate
    float text_height = font_size * 1.2f; // Line height estimate
    
    // Use element position as base
    bounds.x = element->x;
    bounds.y = element->y;
    bounds.width = text_width;
    bounds.height = text_height;
    
    // Handle text alignment within the element bounds
    if (strcmp(text_align, "center") == 0) {
        bounds.x += (element->width - text_width) / 2.0f;
    } else if (strcmp(text_align, "right") == 0) {
        bounds.x += element->width - text_width;
    }
    
    // Ensure bounds are within the element
    if (bounds.x < element->x) bounds.x = element->x;
    if (bounds.y < element->y) bounds.y = element->y;
    if (bounds.x + bounds.width > element->x + element->width) {
        bounds.width = element->x + element->width - bounds.x;
    }
    if (bounds.y + bounds.height > element->y + element->height) {
        bounds.height = element->y + element->height - bounds.y;
    }
    
    return bounds;
}

// Recursive function to find interactive elements only
static struct KryonElement* find_interactive_element_at_point(struct KryonElement* root, float x, float y) {
    if (!root) return NULL;
    
    // Check children first (render on top)
    for (size_t i = 0; i < root->child_count; i++) {
        struct KryonElement* hit_child = find_interactive_element_at_point(root->children[i], x, y);
        if (hit_child) return hit_child;
    }
    
    // Check this element if it's interactive
    if (is_interactive_element(root->type_name)) {
        ElementBounds bounds;
        
        // Use text-based bounds for Link elements to make only text clickable
        if (root->type_name && strcmp(root->type_name, "Link") == 0) {
            bounds = get_link_text_bounds(root);
        } else {
            bounds = element_get_bounds(root);
        }
        
        if (point_in_bounds_with_tolerance(bounds, x, y, 5.0f)) {
            // Debug output for Link elements
            if (root->type_name && strcmp(root->type_name, "Link") == 0) {
                const char* debug_text = get_element_property_string(root, "text");
                const char* debug_to = get_element_property_string(root, "to");
                printf("🎯 HIT TEST: Found Link element %p, text='%s', to='%s'\n", 
                       (void*)root, debug_text ? debug_text : "NULL", debug_to ? debug_to : "NULL");
            }
            return root;
        }
    }
    
    return NULL;
}

struct KryonElement* hit_test_find_element_at_point(struct KryonElement* root, float x, float y) {
    if (!root) return NULL;
    
    // First check for open dropdown popups (highest priority due to z-index)
    struct KryonElement* dropdown_hit = hit_test_find_dropdown_popup_at_point(root, x, y);
    if (dropdown_hit) {
        return dropdown_hit;
    }
    
    // Only check for interactive elements - no fallback to catch-all behavior
    struct KryonElement* interactive_hit = find_interactive_element_at_point(root, x, y);
    if (interactive_hit) {
        return interactive_hit;
    }
    
    // No fallback - only interactive elements should be clickable
    return NULL;
}

struct KryonElement* hit_test_find_dropdown_popup_at_point(struct KryonElement* root, float x, float y) {
    if (!root) return NULL;
    
    // Check if this element is an open dropdown
    if (root->type_name && strcmp(root->type_name, "Dropdown") == 0) {
        // Check if dropdown is open by inspecting user_data
        if (root->user_data) {
            typedef struct {
                int selected_option_index; 
                bool is_open;        
                int hovered_option_index;   
                int keyboard_selected_index; 
                int option_count; 
            } DropdownState;
            
            DropdownState* state = (DropdownState*)root->user_data;
            if (state->is_open) {
                ElementBounds bounds = element_get_bounds(root);
                float popup_y = bounds.y + bounds.height;
                float popup_height = fminf((float)state->option_count * 32.0f, 200.0f);
                
                // Check if point is in popup area
                if (x >= bounds.x && x <= bounds.x + bounds.width &&
                    y >= popup_y && y <= popup_y + popup_height) {
                    return root;
                }
            }
        }
    }
    
    // Recursively check children
    for (size_t i = 0; i < root->child_count; i++) {
        struct KryonElement* dropdown_hit = hit_test_find_dropdown_popup_at_point(root->children[i], x, y);
        if (dropdown_hit) return dropdown_hit;
    }
    
    return NULL;
}

// =============================================================================
// HIT TESTING MANAGER (Clean - No Element-Specific Logic)
// =============================================================================

HitTestManager* hit_test_manager_create(void) {
    HitTestManager* manager = kryon_calloc(1, sizeof(HitTestManager));
    if (!manager) return NULL;
    
    manager->hovered_element = NULL;
    manager->focused_element = NULL;
    manager->clicked_element = NULL;
    manager->last_click_time = 0;
    manager->last_click_x = 0;
    manager->last_click_y = 0;
    
    return manager;
}

void hit_test_manager_destroy(HitTestManager* manager) {
    if (!manager) return;
    kryon_free(manager);
}

void hit_test_update_hover_states(HitTestManager* manager, struct KryonRuntime* runtime, 
                                 struct KryonElement* root, float mouse_x, float mouse_y) {
    struct KryonElement* new_hovered = hit_test_find_element_at_point(root, mouse_x, mouse_y);
    
    // Handle hover state changes
    if (new_hovered != manager->hovered_element) {
        // Send unhover to old element
        if (manager->hovered_element) {
            ElementEvent unhover_event = {
                .type = ELEMENT_EVENT_UNHOVERED,
                .timestamp = 0,
                .handled = false
            };
            element_dispatch_event(runtime, manager->hovered_element, &unhover_event);
        }
        
        // Send hover to new element
        if (new_hovered) {
            ElementEvent hover_event = {
                .type = ELEMENT_EVENT_HOVERED,
                .timestamp = 0,
                .handled = false
            };
            element_dispatch_event(runtime, new_hovered, &hover_event);
        }
        
        manager->hovered_element = new_hovered;
    }
    
    // Send detailed mouse position to ANY hovered element (perfect abstraction)
    if (new_hovered) {
        ElementEvent mouse_move_event = {
            .type = ELEMENT_EVENT_MOUSE_MOVED,
            .timestamp = 0,
            .handled = false,
            .data = {.mousePos = {mouse_x, mouse_y}}
        };
        element_dispatch_event(runtime, new_hovered, &mouse_move_event);
    }
}

void hit_test_process_input_event(HitTestManager* manager, struct KryonRuntime* runtime,
                                 struct KryonElement* root, const KryonEvent* input_event) {
    if (!manager || !runtime || !input_event || !root) return;
    
    calculate_all_element_positions(runtime, root);
    
    switch (input_event->type) {
        case KRYON_EVENT_MOUSE_BUTTON_DOWN: {
            if (input_event->data.mouseButton.button == 0) { // Left click
                float x = input_event->data.mouseButton.x;
                float y = input_event->data.mouseButton.y;
                
                struct KryonElement* clicked = hit_test_find_element_at_point(root, x, y);

                // --- FOCUS MANAGEMENT ---
                if (clicked != manager->focused_element) {
                    // Unfocus the old element if it exists
                    if (manager->focused_element) {
                        ElementEvent unfocus_event = { .type = ELEMENT_EVENT_UNFOCUSED };
                        element_dispatch_event(runtime, manager->focused_element, &unfocus_event);
                    }
                    // Focus the new element if it exists
                    if (clicked) {
                        ElementEvent focus_event = { .type = ELEMENT_EVENT_FOCUSED };
                        element_dispatch_event(runtime, clicked, &focus_event);
                    }
                    // Update the manager's state
                    manager->focused_element = clicked;
                }

                if (clicked) {
                    manager->clicked_element = clicked;
                    
                    uint64_t now = get_current_time_ms(); // Use real system time instead of broken event timestamp
                    uint64_t time_diff = now - manager->last_click_time;
                    float pos_diff_x = fabsf(x - manager->last_click_x);
                    float pos_diff_y = fabsf(y - manager->last_click_y);
                    
                    bool is_double_click = (manager->last_click_time != 0) &&  // Ignore first click
                                         (time_diff < 500) &&
                                         (pos_diff_x < 5.0f) &&
                                         (pos_diff_y < 5.0f);

                    // Check if element has onDoubleClick handler
                    const char* double_click_handler = get_element_property_string(clicked, "onDoubleClick");
                    bool has_double_click_handler = (double_click_handler && double_click_handler[0] != '\0');

                    // If double-click detected AND element has onDoubleClick handler, send DOUBLE_CLICKED
                    // Otherwise, always send CLICKED (even for rapid clicks)
                    if (is_double_click && has_double_click_handler) {
                        ElementEvent double_click_event = {
                            .timestamp = now,
                            .handled = false,
                            .type = ELEMENT_EVENT_DOUBLE_CLICKED,
                            .data.mousePos = {x, y}
                        };
                        element_dispatch_event(runtime, clicked, &double_click_event);
                    } else {
                        ElementEvent click_event = {
                            .timestamp = now,
                            .handled = false,
                            .type = ELEMENT_EVENT_CLICKED,
                            .data.mousePos = {x, y}
                        };
                        element_dispatch_event(runtime, clicked, &click_event);
                    }
                    
                    // Update double-click detection state
                    manager->last_click_time = now;
                    manager->last_click_x = x;
                    manager->last_click_y = y;
                }
            }
            break;
        }
        
        case KRYON_EVENT_MOUSE_BUTTON_UP: {
            if (input_event->data.mouseButton.button == 0) { // Left click release
                // Reset clicked element state to allow subsequent clicks
                manager->clicked_element = NULL;
            }
            break;
        }
        
        case KRYON_EVENT_MOUSE_MOVED: {
            float x = input_event->data.mouseMove.x;
            float y = input_event->data.mouseMove.y;
            
            hit_test_update_hover_states(manager, runtime, root, x, y);
            break;
        }
        
        case KRYON_EVENT_KEY_DOWN: {
            if (manager->focused_element) {
                ElementEvent key_event = {
                    .timestamp = input_event->timestamp,
                    .handled = false,
                    .type = ELEMENT_EVENT_KEY_PRESSED,
                    .data.keyPressed = {
                        .keyCode = input_event->data.key.keyCode,
                        .shift = input_event->data.key.shiftPressed,
                        .ctrl = input_event->data.key.ctrlPressed,
                        .alt = input_event->data.key.altPressed
                    }
                };
                element_dispatch_event(runtime, manager->focused_element, &key_event);
            }
            break;
        }
        
        case KRYON_EVENT_TEXT_INPUT: {
            ElementEvent text_event = {
                .timestamp = input_event->timestamp,
                .handled = false,
                .type = ELEMENT_EVENT_KEY_TYPED,
                .data.keyTyped = {
                    // Assuming textInput.text is a single UTF-8 char for this event
                    .character = input_event->data.textInput.text[0], 
                    .shift = false, // Modifiers not typically available here
                    .ctrl = false,
                    .alt = false
                }
            };
            
            if (manager->focused_element) {
                element_dispatch_event(runtime, manager->focused_element, &text_event);
            } else {
                // FALLBACK: If no element is focused, try to find and dispatch to Input elements
                dispatch_text_event_to_all_inputs(runtime, root, &text_event);
            }
            break;
        }
        
        default:
            break;
    }
}

// =============================================================================
// ELEMENT RENDERING (Generic)
// =============================================================================

bool element_render_via_registry(struct KryonRuntime* runtime, struct KryonElement* element,
                                KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !element->type_name) {
        return false;
    }
    
    const ElementVTable* vtable = element_get_vtable(element->type_name);
    if (!vtable || !vtable->render) {
        return false;
    }
    
    vtable->render(runtime, element, commands, command_count, max_commands);
    return true;
}

void element_destroy_via_registry(struct KryonRuntime* runtime, struct KryonElement* element) {
    if (!element || !element->type_name) {
        return;
    }
    
    const ElementVTable* vtable = element_get_vtable(element->type_name);
    if (vtable && vtable->destroy) {
        vtable->destroy(runtime, element);
    }
}

// =============================================================================
// REACTIVE LAYOUT SYSTEM IMPLEMENTATION
// =============================================================================


/**
 * @brief Check if an element needs layout recalculation
 */
bool element_needs_layout(struct KryonElement* element) {
    return element && element->needs_layout;
}

/**
 * @brief Position children in a TabBar layout
 * TabBar divides space between tab headers and content area based on position
 */
static void position_tabbar_children(struct KryonRuntime* runtime, struct KryonElement* tabbar) {
    if (!tabbar || tabbar->child_count == 0) return;
    
    // Get TabBar layout properties
    const char* position = get_element_property_string(tabbar, "position");
    if (!position) position = "top";
    
    float tab_spacing = get_element_property_float(tabbar, "tabSpacing", 5.0f);
    float tab_area_height = 40.0f; // Default tab header height for top/bottom
    float tab_area_width = 120.0f; // Default tab header width for left/right
    
    // Get TabBar bounds
    float x = tabbar->x;
    float y = tabbar->y;
    float width = tabbar->width;
    float height = tabbar->height;
    
    // Calculate tab header area and content area based on position
    float tab_area_x, tab_area_y, tab_area_w, tab_area_h;
    float content_area_x, content_area_y, content_area_w, content_area_h;
    
    if (strcmp(position, "top") == 0) {
        tab_area_x = x; tab_area_y = y; tab_area_w = width; tab_area_h = tab_area_height;
        content_area_x = x; content_area_y = y + tab_area_height; 
        content_area_w = width; content_area_h = height - tab_area_height;
    } else if (strcmp(position, "bottom") == 0) {
        content_area_x = x; content_area_y = y; content_area_w = width; content_area_h = height - tab_area_height;
        tab_area_x = x; tab_area_y = y + height - tab_area_height; tab_area_w = width; tab_area_h = tab_area_height;
    } else if (strcmp(position, "left") == 0) {
        tab_area_x = x; tab_area_y = y; tab_area_w = tab_area_width; tab_area_h = height;
        content_area_x = x + tab_area_width; content_area_y = y; 
        content_area_w = width - tab_area_width; content_area_h = height;
    } else { // right
        content_area_x = x; content_area_y = y; content_area_w = width - tab_area_width; content_area_h = height;
        tab_area_x = x + width - tab_area_width; tab_area_y = y; tab_area_w = tab_area_width; tab_area_h = height;
    }
    
    // Count Tab children and calculate total custom widths
    int tab_count = 0;
    float total_custom_width = 0.0f;
    int custom_width_count = 0;

    for (size_t i = 0; i < tabbar->child_count; i++) {
        if (strcmp(tabbar->children[i]->type_name, "Tab") == 0) {
            tab_count++;
            // Check if this tab has a custom width
            int custom_w = get_element_property_int(tabbar->children[i], "width", -1);
            if (custom_w > 0) {
                total_custom_width += custom_w;
                custom_width_count++;
            }
        }
    }

    if (tab_count > 0) {
        // Calculate default tab size for auto-sized tabs
        float spacing_total = (tab_count - 1) * tab_spacing;
        float available_for_auto = tab_area_w - total_custom_width - spacing_total;
        int auto_tab_count = tab_count - custom_width_count;

        float default_tab_width = (auto_tab_count > 0)
            ? available_for_auto / auto_tab_count
            : tab_area_w;

        float default_tab_height = tab_area_h;

        // Position each tab
        float current_x = tab_area_x;
        float current_y = tab_area_y;

        for (size_t i = 0; i < tabbar->child_count; i++) {
            struct KryonElement* child = tabbar->children[i];
            if (!child || strcmp(child->type_name, "Tab") != 0) continue;

            // Get custom width if specified, otherwise use default
            int custom_width = get_element_property_int(child, "width", -1);
            float tab_width = (custom_width > 0) ? (float)custom_width : default_tab_width;
            float tab_height = default_tab_height;

            // Calculate tab position based on orientation
            float child_x, child_y;
            if (strcmp(position, "top") == 0 || strcmp(position, "bottom") == 0) {
                child_x = current_x;
                child_y = tab_area_y;
                current_x += tab_width + tab_spacing;
            } else {
                child_x = tab_area_x;
                child_y = current_y;
                current_y += tab_height + tab_spacing;
            }

            // Position this Tab child using the global positioning system
            calculate_element_position_recursive(runtime, child, child_x, child_y, tab_width, tab_height, tabbar);
        }
    }
    
    // Position TabContent children in content area
    // Only the selected TabContent should be visible, others should be hidden
    int selected_index = tabbar_get_selected_index(tabbar);
    int content_index = 0;
    
    for (size_t i = 0; i < tabbar->child_count; i++) {
        struct KryonElement* child = tabbar->children[i];
        if (!child || strcmp(child->type_name, "TabContent") != 0) continue;

        if (content_index == selected_index) {
            // Position active TabContent in content area
            calculate_element_position_recursive(runtime, child, content_area_x, content_area_y, 
                                               content_area_w, content_area_h, tabbar);
        } else {
            // Hide inactive TabContent by moving off-screen
            calculate_element_position_recursive(runtime, child, -9999, -9999, 0, 0, tabbar);
        }
        content_index++;
    }
}

/**
 * @brief Position children in a TabGroup layout
 * TabGroup manages TabBar and TabContent children with automatic state scoping
 * It uses a Column-like layout (stacks children vertically)
 */
static void position_tabgroup_children(struct KryonRuntime* runtime, struct KryonElement* tabgroup) {
    if (!tabgroup || tabgroup->child_count == 0) return;

    // TabGroup uses a simple vertical stacking layout (like Column)
    // This allows TabBar to be at the top and TabContent below
    position_column_children(runtime, tabgroup);
}

