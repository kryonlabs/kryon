/**
 * @file clickable_behavior.c
 * @brief Clickable Behavior Implementation
 * 
 * Handles click events, hover states, and cursor management for interactive elements.
 * Provides visual feedback for hover and click states.
 * 
 * 0BSD License
 */

#include "element_behaviors.h"
#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// CLICKABLE BEHAVIOR IMPLEMENTATION
// =============================================================================

static bool clickable_init(struct KryonElement* element) {
    // Ensure the element is enabled by default for clickable elements
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    // Initialize clickable state
    state->hovered = false;
    state->clicked = false;
    state->pressed = false;
    
    return true;
}

static void clickable_render(struct KryonRuntime* runtime, 
                            struct KryonElement* element,
                            KryonRenderCommand* commands,
                            size_t* command_count,
                            size_t max_commands) {
    
    // Clickable behavior handles visual feedback for interaction states
    // This runs after Renderable behavior, so we can modify colors based on state
    
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return;
    
    // Check if element is enabled
    bool enabled = get_element_property_bool(element, "enabled", true);
    if (!enabled) return;
    
    // Update hover state based on mouse position
    KryonVec2 mouse_pos = runtime->mouse_position;
    bool mouse_over = (mouse_pos.x >= element->x && mouse_pos.x <= element->x + element->width &&
                       mouse_pos.y >= element->y && mouse_pos.y <= element->y + element->height);
    
    if (mouse_over != state->hovered) {
        element_set_hovered(element, mouse_over);
    }
    
    // Set cursor to pointer when hovering over clickable elements
    if (state->hovered && runtime->renderer) {
        kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
    }
    
    // Visual feedback could be handled here by modifying the last render command
    // For now, individual elements can check hover state in their custom render functions
}

static bool clickable_handle_event(struct KryonRuntime* runtime,
                                  struct KryonElement* element,
                                  const ElementEvent* event) {
    
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    // Check if element is enabled
    bool enabled = get_element_property_bool(element, "enabled", true);
    if (!enabled) return false;
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED:
            element_set_clicked(element, true);
            
            // Trigger script-based onClick handler if present
            return generic_script_event_handler(runtime, element, event);
            
        case ELEMENT_EVENT_HOVERED:
            element_set_hovered(element, true);
            break;
            
        case ELEMENT_EVENT_UNHOVERED:
            element_set_hovered(element, false);
            break;
            
        case ELEMENT_EVENT_MOUSE_MOVED:
            // Update hover state based on mouse position
            {
                float mouse_x = event->data.mousePos.x;
                float mouse_y = event->data.mousePos.y;
                
                bool mouse_over = (mouse_x >= element->x && mouse_x <= element->x + element->width &&
                                  mouse_y >= element->y && mouse_y <= element->y + element->height);
                
                if (mouse_over != state->hovered) {
                    element_set_hovered(element, mouse_over);
                    
                    // Generate hover/unhover events
                    ElementEvent hover_event = {0};
                    hover_event.type = mouse_over ? ELEMENT_EVENT_HOVERED : ELEMENT_EVENT_UNHOVERED;
                    hover_event.timestamp = event->timestamp;
                    hover_event.data.mousePos = event->data.mousePos;
                    
                    return generic_script_event_handler(runtime, element, &hover_event);
                }
            }
            break;
            
        default:
            break;
    }
    
    return false;
}

static void clickable_destroy(struct KryonElement* element) {
    // No special cleanup needed for clickable behavior
    (void)element;
}

static void* clickable_get_state(struct KryonElement* element) {
    return element_get_behavior_state(element);
}

// =============================================================================
// BEHAVIOR DEFINITION
// =============================================================================

static const ElementBehavior g_clickable_behavior = {
    .name = "Clickable",
    .init = clickable_init,
    .render = clickable_render,
    .handle_event = clickable_handle_event,
    .destroy = clickable_destroy,
    .get_state = clickable_get_state
};

// =============================================================================
// REGISTRATION FUNCTION
// =============================================================================

__attribute__((constructor))
void register_clickable_behavior(void) {
    element_behavior_register(&g_clickable_behavior);
}