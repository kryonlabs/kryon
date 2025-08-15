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
extern bool register_column_element(void);
extern bool register_row_element(void);
extern bool register_center_element(void);
extern bool register_app_element(void);
extern bool register_grid_element(void);

// Forward declarations for position calculation pipeline
static void calculate_element_position_recursive(struct KryonRuntime* runtime, struct KryonElement* element,
                                               float parent_x, float parent_y, float available_width, float available_height,
                                               struct KryonElement* layout_parent);
static void position_children_by_layout_type(struct KryonRuntime* runtime, struct KryonElement* parent);
static void position_row_children(struct KryonRuntime* runtime, struct KryonElement* row);
static void position_column_children(struct KryonRuntime* runtime, struct KryonElement* column);
static void position_container_children(struct KryonRuntime* runtime, struct KryonElement* container);
static void position_center_children(struct KryonRuntime* runtime, struct KryonElement* center);
static void position_app_children(struct KryonRuntime* runtime, struct KryonElement* app);
static void position_grid_children(struct KryonRuntime* runtime, struct KryonElement* grid); 

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
    
    if (!register_column_element()) {
        printf("ERROR: Failed to register Column element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_row_element()) {
        printf("ERROR: Failed to register Row element\n");
        element_registry_cleanup();
        return false;
    }
    
    if (!register_center_element()) {
        printf("ERROR: Failed to register Center element\n");
        element_registry_cleanup();
        return false;
    }
    
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
        fprintf(stderr, "SCRIPT ERROR in handler '%s': %s\n",
                handler_function_name, error ? error : "Unknown VM error");
        return false;
    }

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
 * @brief Single-pass position calculator - calculates final (x,y) for all elements
 * This replaces the complex VTable + recursive system with a simple pipeline
 */
void calculate_all_element_positions(struct KryonRuntime* runtime, struct KryonElement* root) {
    if (!root) return;
    
    // Simple debouncing: check if we've already calculated positions recently
    static struct KryonElement* last_calculated_root = NULL;
    static size_t last_calculation_frame = 0;
    static size_t current_frame = 0;
    current_frame++;
    
    if (root == last_calculated_root && (current_frame - last_calculation_frame) < 2) {
        return; // Skip if we calculated positions very recently for the same root
    }
    
    // Get window dimensions for root positioning
    float window_width = get_element_property_float(root, "windowWidth", 800.0f);
    float window_height = get_element_property_float(root, "windowHeight", 600.0f);
    
    // Start position calculation from root
    calculate_element_position_recursive(runtime, root, 0.0f, 0.0f, window_width, window_height, NULL);
    
    // Update render flags based on position changes
    update_render_flags_for_changed_positions(root);
    
    // Track this calculation
    last_calculated_root = root;
    last_calculation_frame = current_frame;
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
        // Use available space, but ensure layout containers get reasonable defaults
        if (element->type_name && (strcmp(element->type_name, "Column") == 0 || strcmp(element->type_name, "Row") == 0)) {
            element->height = available_height > 0.0f ? available_height : 300.0f;
        } else {
            element->height = available_height > 0.0f ? available_height : 50.0f;
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
static void position_children_by_layout_type(struct KryonRuntime* runtime, struct KryonElement* parent) {
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
    } else {
        // Default: position children at same location
        for (size_t i = 0; i < parent->child_count; i++) {
            calculate_element_position_recursive(runtime, parent->children[i], 
                                               parent->x, parent->y, parent->width, parent->height, parent);
        }
    }
}

/**
 * @brief Position children in a Row layout (horizontal arrangement)
 */
static void position_row_children(struct KryonRuntime* runtime, struct KryonElement* row) {
    if (!row || row->child_count == 0) return;
    
    // Get layout properties
    const char* main_axis = get_element_property_string(row, "mainAxisAlignment");
    if (!main_axis) main_axis = get_element_property_string(row, "mainAxis");
    if (!main_axis) main_axis = "start";
    
    const char* cross_axis = get_element_property_string(row, "crossAxisAlignment");
    if (!cross_axis) cross_axis = get_element_property_string(row, "crossAxis");
    if (!cross_axis) cross_axis = "start";
    
    float gap = get_element_property_float(row, "gap", 0.0f);
    float padding = get_element_property_float(row, "padding", 0.0f);
    
    // Calculate available space inside padding
    float content_x = row->x + padding;
    float content_y = row->y + padding;
    float content_width = row->width - (padding * 2.0f);
    float content_height = row->height - (padding * 2.0f);
    
    // Calculate total width needed by all children
    float total_child_width = 0.0f;
    for (size_t i = 0; i < row->child_count; i++) {
        float child_width = get_element_property_float(row->children[i], "width", 100.0f);
        total_child_width += child_width;
        if (i > 0) total_child_width += gap; // Add gap between children
    }
    
    
    // Calculate starting X position based on mainAxisAlignment
    float current_x = content_x;
    if (strcmp(main_axis, "center") == 0) {
        current_x = content_x + (content_width - total_child_width) / 2.0f;
    } else if (strcmp(main_axis, "end") == 0) {
        current_x = content_x + content_width - total_child_width;
    } else if (strcmp(main_axis, "spaceEvenly") == 0) {
        // SpaceEvenly: equal space before, between, and after children
        float remaining_space = content_width - total_child_width + (gap * (row->child_count - 1));
        float space = remaining_space / (row->child_count + 1);
        current_x = content_x + space;
        gap = space;
    } else if (strcmp(main_axis, "spaceAround") == 0) {
        // SpaceAround: half space before/after, full space between
        float remaining_space = content_width - total_child_width + (gap * (row->child_count - 1));
        float space = remaining_space / row->child_count;
        current_x = content_x + space / 2.0f;
        gap = space;
    } else if (strcmp(main_axis, "spaceBetween") == 0) {
        // SpaceBetween: no space before/after, equal space between
        if (row->child_count > 1) {
            float remaining_space = content_width - total_child_width + (gap * (row->child_count - 1));
            gap = remaining_space / (row->child_count - 1);
        }
        current_x = content_x;
    }
    // "start" uses current_x = content_x (already set)
    
    // Position each child
    for (size_t i = 0; i < row->child_count; i++) {
        struct KryonElement* child = row->children[i];
        float child_width = get_element_property_float(child, "width", 100.0f);
        float child_height = get_element_property_float(child, "height", 50.0f);
        
        // Calculate Y position based on crossAxisAlignment
        float child_y = content_y;
        if (strcmp(cross_axis, "center") == 0) {
            child_y = content_y + (content_height - child_height) / 2.0f;
        } else if (strcmp(cross_axis, "end") == 0) {
            child_y = content_y + content_height - child_height;
        }
        // "start" uses child_y = content_y (already set)
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, current_x, child_y, child_width, child_height, row);
        
        // Move to next position
        current_x += child_width + gap;
    }
}

/**
 * @brief Position children in a Column layout (vertical arrangement)
 */
static void position_column_children(struct KryonRuntime* runtime, struct KryonElement* column) {
    if (!column || column->child_count == 0) return;
    
    // Get layout properties
    const char* main_axis = get_element_property_string(column, "mainAxisAlignment");
    if (!main_axis) main_axis = get_element_property_string(column, "mainAxis");
    if (!main_axis) main_axis = "start";
    
    const char* cross_axis = get_element_property_string(column, "crossAxisAlignment");
    if (!cross_axis) cross_axis = get_element_property_string(column, "crossAxis");
    if (!cross_axis) cross_axis = "start";
    
    float gap = get_element_property_float(column, "gap", 0.0f);
    float padding = get_element_property_float(column, "padding", 0.0f);
    
    // Calculate available space inside padding
    float content_x = column->x + padding;
    float content_y = column->y + padding;
    float content_width = column->width - (padding * 2.0f);
    float content_height = column->height - (padding * 2.0f);
    
    // Calculate total height needed by all children
    float total_child_height = 0.0f;
    for (size_t i = 0; i < column->child_count; i++) {
        float child_height = get_element_property_float(column->children[i], "height", 50.0f);
        total_child_height += child_height;
        if (i > 0) total_child_height += gap; // Add gap between children
    }
    
    // Calculate starting Y position based on mainAxisAlignment
    float current_y = content_y;
    if (strcmp(main_axis, "center") == 0) {
        current_y = content_y + (content_height - total_child_height) / 2.0f;
    } else if (strcmp(main_axis, "end") == 0) {
        current_y = content_y + content_height - total_child_height;
    } else if (strcmp(main_axis, "spaceEvenly") == 0) {
        float space = (content_height - total_child_height + (gap * (column->child_count - 1))) / (column->child_count + 1);
        current_y = content_y + space;
        gap = space;
    } else if (strcmp(main_axis, "spaceAround") == 0) {
        float space = (content_height - total_child_height + (gap * (column->child_count - 1))) / column->child_count;
        current_y = content_y + space / 2.0f;
        gap = space;
    } else if (strcmp(main_axis, "spaceBetween") == 0) {
        if (column->child_count > 1) {
            gap = (content_height - total_child_height + (gap * (column->child_count - 1))) / (column->child_count - 1);
        }
        current_y = content_y;
    }
    // "start" uses current_y = content_y (already set)
    
    // Position each child
    for (size_t i = 0; i < column->child_count; i++) {
        struct KryonElement* child = column->children[i];
        
        // Get child dimensions - handle Text elements specially (they auto-size)
        float child_width = get_element_property_float(child, "width", 0.0f);
        float child_height = get_element_property_float(child, "height", 0.0f);
        
        // For Text elements without explicit size, use smaller defaults
        if (child_width == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_width = 200.0f; // Reasonable text width
            } else {
                child_width = 100.0f; // Default for other elements
            }
        }
        if (child_height == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_height = 20.0f; // Reasonable text height (was 50!)
            } else {
                child_height = 50.0f; // Default for other elements
            }
        }
        
        
        // Calculate X position based on crossAxisAlignment
        float child_x = content_x;
        if (strcmp(cross_axis, "center") == 0) {
            child_x = content_x + (content_width - child_width) / 2.0f;
        } else if (strcmp(cross_axis, "end") == 0) {
            child_x = content_x + content_width - child_width;
        } else if (strcmp(cross_axis, "stretch") == 0) {
            child_width = content_width; // Stretch to fill available width
            child_x = content_x;
        }
        // "start" uses child_x = content_x (already set)
        
        // For layout containers, pass the content area as available space
        float available_width_for_child = content_width;
        float available_height_for_child = child_height;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, current_y, available_width_for_child, available_height_for_child, column);
        
        // Move to next position
        current_y += child_height + gap;
    }
}

/**
 * @brief Position children in a Container (with contentAlignment)
 */
static void position_container_children(struct KryonRuntime* runtime, struct KryonElement* container) {
    if (!container || container->child_count == 0) return;
    
    const char* content_alignment = get_element_property_string(container, "contentAlignment");
    if (!content_alignment) content_alignment = "start";
    
    float padding = get_element_property_float(container, "padding", 0.0f);
    
    // Calculate available space inside padding
    float content_x = container->x + padding;
    float content_y = container->y + padding;
    float content_width = container->width - (padding * 2.0f);
    float content_height = container->height - (padding * 2.0f);
    
    // Position all children based on contentAlignment
    for (size_t i = 0; i < container->child_count; i++) {
        struct KryonElement* child = container->children[i];
        
        // Get child dimensions - handle Text elements specially (they auto-size)
        float child_width = get_element_property_float(child, "width", 0.0f);
        float child_height = get_element_property_float(child, "height", 0.0f);
        
        // For Text elements without explicit size, use smaller defaults
        if (child_width == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_width = 40.0f; // Smaller text width to allow centering
            } else {
                child_width = 100.0f; // Default for other elements
            }
        }
        if (child_height == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_height = 20.0f; // Reasonable text height
            } else {
                child_height = 50.0f; // Default for other elements
            }
        }
        
        float child_x = content_x;
        float child_y = content_y;
        
        if (strcmp(content_alignment, "center") == 0) {
            child_x = content_x + (content_width - child_width) / 2.0f;
            child_y = content_y + (content_height - child_height) / 2.0f;
        } else if (strcmp(content_alignment, "end") == 0) {
            child_x = content_x + content_width - child_width;
            child_y = content_y + content_height - child_height;
        }
        // "start" uses child_x = content_x, child_y = content_y (already set)
        
        // For layout containers, pass the content area as available space
        float available_width_for_child = content_width;
        float available_height_for_child = content_height;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, child_y, available_width_for_child, available_height_for_child, container);
    }
}

/**
 * @brief Position children in a Center layout (centers all children)
 */
static void position_center_children(struct KryonRuntime* runtime, struct KryonElement* center) {
    if (!center || center->child_count == 0) return;
    
    float padding = get_element_property_float(center, "padding", 0.0f);
    
    // Calculate available space inside padding
    float content_x = center->x + padding;
    float content_y = center->y + padding;
    float content_width = center->width - (padding * 2.0f);
    float content_height = center->height - (padding * 2.0f);
    
    // Center all children
    for (size_t i = 0; i < center->child_count; i++) {
        struct KryonElement* child = center->children[i];
        float child_width = get_element_property_float(child, "width", 100.0f);
        float child_height = get_element_property_float(child, "height", 50.0f);
        
        float child_x = content_x + (content_width - child_width) / 2.0f;
        float child_y = content_y + (content_height - child_height) / 2.0f;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, child_y, child_width, child_height, center);
    }
}

/**
 * @brief Position children in an App layout (with contentAlignment and padding)
 * App elements inherit Container-like layout behavior but use window dimensions as base
 */
static void position_app_children(struct KryonRuntime* runtime, struct KryonElement* app) {
    if (!app || app->child_count == 0) return;
    
    const char* content_alignment = get_element_property_string(app, "contentAlignment");
    if (!content_alignment) content_alignment = "start";
    
    float padding = get_element_property_float(app, "padding", 0.0f);
    
    // For App elements, use the full window dimensions as the base
    // (these should already be set by calculate_all_element_positions)
    float window_width = app->width;
    float window_height = app->height;
    
    // Calculate available space inside padding
    float content_x = app->x + padding;
    float content_y = app->y + padding;
    float content_width = window_width - (padding * 2.0f);
    float content_height = window_height - (padding * 2.0f);
    
    // Position all children based on contentAlignment
    for (size_t i = 0; i < app->child_count; i++) {
        struct KryonElement* child = app->children[i];
        
        // Get child dimensions - handle Text elements specially (they auto-size)
        float child_width = get_element_property_float(child, "width", 0.0f);
        float child_height = get_element_property_float(child, "height", 0.0f);
        
        // For Text elements without explicit size, use smaller defaults
        if (child_width == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_width = 80.0f; // Reasonable text width
            } else {
                child_width = 100.0f; // Default for other elements
            }
        }
        if (child_height == 0.0f) {
            if (child->type_name && strcmp(child->type_name, "Text") == 0) {
                child_height = 20.0f; // Reasonable text height
            } else {
                child_height = 50.0f; // Default for other elements
            }
        }
        
        float child_x = content_x;
        float child_y = content_y;
        
        if (strcmp(content_alignment, "center") == 0) {
            child_x = content_x + (content_width - child_width) / 2.0f;
            child_y = content_y + (content_height - child_height) / 2.0f;
        } else if (strcmp(content_alignment, "end") == 0) {
            child_x = content_x + content_width - child_width;
            child_y = content_y + content_height - child_height;
        }
        // "start" uses child_x = content_x, child_y = content_y (already set)
        
        // For layout containers, pass the content area as available space
        float available_width_for_child = content_width;
        float available_height_for_child = content_height;
        
        // Recursively position this child
        calculate_element_position_recursive(runtime, child, child_x, child_y, available_width_for_child, available_height_for_child, app);
    }
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

struct KryonElement* hit_test_find_element_at_point(struct KryonElement* root, float x, float y) {
    if (!root) return NULL;
    
    // First check for open dropdown popups (highest priority due to z-index)
    struct KryonElement* dropdown_hit = hit_test_find_dropdown_popup_at_point(root, x, y);
    if (dropdown_hit) {
        return dropdown_hit;
    }
    
    // Check children first (they render on top)
    for (size_t i = 0; i < root->child_count; i++) {
        struct KryonElement* hit_child = hit_test_find_element_at_point(root->children[i], x, y);
        if (hit_child) return hit_child;
    }
    
    // Check this element
    ElementBounds bounds = element_get_bounds(root);
    if (point_in_bounds(bounds, x, y)) {
        return root;
    }
    
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
                    
                    // Create the abstract click event
                    ElementEvent click_event = {
                        .timestamp = now,
                        .handled = false,
                        .type = is_double_click ? ELEMENT_EVENT_DOUBLE_CLICKED : ELEMENT_EVENT_CLICKED,
                        .data.mousePos = {x, y}
                    };
                    
                    element_dispatch_event(runtime, clicked, &click_event);
                    
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
            if (manager->focused_element) {
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
                element_dispatch_event(runtime, manager->focused_element, &text_event);
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

