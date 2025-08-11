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

#include "internal/elements.h"
#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/events.h"
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

static void calculate_element_layout_position(struct KryonRuntime* runtime, struct KryonElement* element) {
    if (!element) return;
    
    // Get element's current properties
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 0.0f);
    float height = get_element_property_float(element, "height", 0.0f);
    
    // Default sizes for different element types
    if (width == 0.0f) {
        if (element->type_name) {
            if (strcmp(element->type_name, "Dropdown") == 0) width = 200.0f;
            else if (strcmp(element->type_name, "Input") == 0) width = 200.0f;
            else if (strcmp(element->type_name, "Button") == 0) width = 150.0f;
            else if (strcmp(element->type_name, "Checkbox") == 0) width = 200.0f;
            else if (strcmp(element->type_name, "Slider") == 0) width = 200.0f;
            else width = 100.0f;
        }
    }
    if (height == 0.0f) {
        if (element->type_name) {
            if (strcmp(element->type_name, "Dropdown") == 0) height = 40.0f;
            else if (strcmp(element->type_name, "Input") == 0) height = 32.0f;
            else if (strcmp(element->type_name, "Button") == 0) height = 40.0f;
            else if (strcmp(element->type_name, "Checkbox") == 0) height = 24.0f;
            else if (strcmp(element->type_name, "Slider") == 0) height = 24.0f;
            else height = 30.0f;
        }
    }
    
    KryonVec2 position = {posX, posY};
    
    // If element has a parent and no explicit position properties, calculate layout position
    if (element->parent && !has_explicit_position_properties(element)) {
        // Get parent properties (parent should already have calculated position)
        float parent_x = element->parent->x != 0.0f ? element->parent->x : get_element_property_float(element->parent, "posX", 0.0f);
        float parent_y = element->parent->y != 0.0f ? element->parent->y : get_element_property_float(element->parent, "posY", 0.0f);
        float parent_width = element->parent->width != 0.0f ? element->parent->width : get_element_property_float(element->parent, "width", 400.0f);
        float parent_height = element->parent->height != 0.0f ? element->parent->height : get_element_property_float(element->parent, "height", 300.0f);
        float parent_padding = get_element_property_float(element->parent, "padding", 40.0f);
        const char* parent_alignment = get_element_property_string(element->parent, "mainAxisAlignment");
        const char* cross_alignment = get_element_property_string(element->parent, "crossAxisAlignment");
        
        // Calculate available area inside parent (accounting for padding)
        float available_x = parent_x + parent_padding;
        float available_y = parent_y + parent_padding;
        float available_width = parent_width - (parent_padding * 2);
        float available_height = parent_height - (parent_padding * 2);
        
        // Handle Column layout (main axis = vertical)
        if (parent_alignment && strcmp(parent_alignment, "center") == 0) {
            // Center vertically within parent
            position.y = available_y + (available_height / 2.0f);
        } else {
            // Default: start at top of available area
            position.y = available_y;
        }
        
        // Handle cross axis alignment (horizontal for Column)
        if (cross_alignment && strcmp(cross_alignment, "center") == 0) {
            // Center horizontally within parent  
            position.x = available_x + (available_width - width) / 2.0f;
        } else {
            // Default: start at left of available area
            position.x = available_x;
        }
    }
    
    // Store calculated position and size in element for hit testing
    element->x = position.x;
    element->y = position.y;
    element->width = width;
    element->height = height;
}

static void ensure_layout_positions_calculated(struct KryonRuntime* runtime, struct KryonElement* root) {
    if (!root) return;
    
    // Calculate layout for this element if needed
    if (root->x == 0.0f && root->y == 0.0f && root->width == 0.0f && root->height == 0.0f) {
        calculate_element_layout_position(runtime, root);
    }
    
    // Recursively ensure all children have layout calculated
    for (size_t i = 0; i < root->child_count; i++) {
        ensure_layout_positions_calculated(runtime, root->children[i]);
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
    
    ensure_layout_positions_calculated(runtime, root);
    
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
