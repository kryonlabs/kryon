/**
 * @file tabbar_element.c
 * @brief TabBar Element using Behavior Composition
 * 
 * This demonstrates the power of the behavior system by implementing a complex
 * TabBar element using only behavior composition + minimal custom logic.
 * 
 * TabBar = Renderable + Layout + Selectable + Clickable + custom tab rendering
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
#include <math.h>

// =============================================================================
// TABBAR CUSTOM FUNCTIONS
// =============================================================================

/**
 * @brief Custom render function for TabBar - renders individual tabs
 * This runs after all behaviors have rendered, so we can add tab-specific visuals
 */
static void tabbar_custom_render(struct KryonRuntime* runtime,
                                struct KryonElement* element,
                                KryonRenderCommand* commands,
                                size_t* command_count,
                                size_t max_commands) {
    
    if (*command_count >= max_commands - 5) return; // Need space for multiple tabs
    
    // Get TabBar properties
    const char* position = get_element_property_string(element, "position");
    if (!position) position = "top";
    
    int tab_spacing = get_element_property_int(element, "tabSpacing", 5);
    int indicator_thickness = get_element_property_int(element, "indicatorThickness", 2);
    uint32_t active_color_val = get_element_property_color(element, "activeColor", 0x3B82F6FF);
    uint32_t inactive_color_val = get_element_property_color(element, "inactiveColor", 0x6B7280FF);
    uint32_t indicator_color_val = get_element_property_color(element, "indicatorColor", 0x3B82F6FF);
    
    KryonColor active_color = color_u32_to_f32(active_color_val);
    KryonColor inactive_color = color_u32_to_f32(inactive_color_val);
    KryonColor indicator_color = color_u32_to_f32(indicator_color_val);
    
    // Get selection state
    ElementBehaviorState* state = element_get_behavior_state(element);
    int selected_index = state ? state->selected_index : 0;
    
    // Count Tab children
    int tab_count = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i] && strcmp(element->children[i]->type_name, "Tab") == 0) {
            tab_count++;
        }
    }
    
    if (tab_count == 0) return;
    
    // Calculate tab dimensions
    float tab_width = (element->width - (tab_count - 1) * tab_spacing) / (float)tab_count;
    float tab_height = element->height;
    
    // Render individual tabs
    int tab_index = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (!child || strcmp(child->type_name, "Tab") != 0) continue;
        
        bool is_active = (tab_index == selected_index);
        
        // Calculate tab position
        float tab_x = element->x + tab_index * (tab_width + tab_spacing);
        float tab_y = element->y;
        
        // Get tab text
        const char* tab_text = get_element_property_string_with_runtime(runtime, child, "text");
        if (!tab_text) tab_text = "Tab";
        
        // Render tab background (subtle)
        KryonColor tab_bg_color = is_active ? active_color : inactive_color;
        tab_bg_color.a = 0.1f; // Very subtle background
        
        KryonRenderCommand tab_bg_cmd = kryon_cmd_draw_rect(
            (KryonVec2){tab_x, tab_y},
            (KryonVec2){tab_width, tab_height},
            tab_bg_color,
            4.0f
        );
        commands[(*command_count)++] = tab_bg_cmd;
        
        // Render tab text
        KryonColor text_color = is_active ? active_color : inactive_color;
        
        KryonRenderCommand text_cmd = kryon_cmd_draw_text(
            (KryonVec2){tab_x + tab_width / 2.0f, tab_y + tab_height / 2.0f},
            tab_text,
            14.0f,
            text_color
        );
        text_cmd.data.draw_text.text_align = 1; // center
        text_cmd.z_index = 2;
        commands[(*command_count)++] = text_cmd;
        
        // Render selection indicator for active tab
        if (is_active) {
            KryonVec2 indicator_pos, indicator_size;
            
            if (strcmp(position, "bottom") == 0) {
                // Indicator at top of tab
                indicator_pos = (KryonVec2){tab_x, tab_y};
                indicator_size = (KryonVec2){tab_width, indicator_thickness};
            } else {
                // Default: indicator at bottom of tab
                indicator_pos = (KryonVec2){tab_x, tab_y + tab_height - indicator_thickness};
                indicator_size = (KryonVec2){tab_width, indicator_thickness};
            }
            
            KryonRenderCommand indicator_cmd = kryon_cmd_draw_rect(
                indicator_pos,
                indicator_size,
                indicator_color,
                0.0f
            );
            indicator_cmd.z_index = 3;
            commands[(*command_count)++] = indicator_cmd;
        }
        
        tab_index++;
        
        if (*command_count >= max_commands - 1) break;
    }
}

/**
 * @brief Custom event handler for TabBar - handles tab switching logic
 */
static bool tabbar_custom_handle_event(struct KryonRuntime* runtime,
                                      struct KryonElement* element,
                                      const ElementEvent* event) {
    
    if (event->type == ELEMENT_EVENT_CLICKED) {
        // Calculate which tab was clicked
        float mouse_x = event->data.mousePos.x;
        
        // Count Tab children
        int tab_count = 0;
        for (size_t i = 0; i < element->child_count; i++) {
            if (element->children[i] && strcmp(element->children[i]->type_name, "Tab") == 0) {
                tab_count++;
            }
        }
        
        if (tab_count > 0) {
            int tab_spacing = get_element_property_int(element, "tabSpacing", 5);
            float tab_width = (element->width - (tab_count - 1) * tab_spacing) / (float)tab_count;
            
            // Find which tab was clicked
            float relative_x = mouse_x - element->x;
            int clicked_tab = -1;
            
            for (int i = 0; i < tab_count; i++) {
                float tab_start = i * (tab_width + tab_spacing);
                float tab_end = tab_start + tab_width;
                
                if (relative_x >= tab_start && relative_x <= tab_end) {
                    clicked_tab = i;
                    break;
                }
            }
            
            if (clicked_tab >= 0 && clicked_tab != element_get_selected_index(element)) {
                // Change selection
                element_set_selected_index(element, clicked_tab);
                
                // Generate selection changed event
                ElementEvent selection_event = {0};
                selection_event.type = ELEMENT_EVENT_SELECTION_CHANGED;
                selection_event.timestamp = event->timestamp;
                selection_event.data.selectionChanged.oldValue = element_get_selected_index(element);
                selection_event.data.selectionChanged.newValue = clicked_tab;
                
                // Trigger script handler
                generic_script_event_handler(runtime, element, &selection_event);
                
                return true; // Event handled
            }
        }
    }
    
    return false; // Event not handled by custom logic
}

// =============================================================================
// TABBAR ELEMENT DEFINITION
// =============================================================================

// Forward declaration for registration function
static void register_TabBar_element_manual(void);

// TabBar behaviors array
static const char* TabBar_behaviors[] = {"Renderable", "Layout", "Selectable", "Clickable", NULL};

// TabBar element definition
static ElementDefinition TabBar_def = {
    .type_name = "TabBar",
    .behavior_names = TabBar_behaviors,
    .custom_render = tabbar_custom_render,
    .custom_handle_event = tabbar_custom_handle_event,
    .custom_init = NULL,
    .custom_destroy = NULL
};

// Manual registration function
__attribute__((constructor)) 
static void register_TabBar_element_manual(void) {
    element_definition_register(&TabBar_def);
}

// =============================================================================
// LEGACY COMPATIBILITY FUNCTIONS  
// =============================================================================

// NOTE: Legacy functions removed to avoid conflicts with existing tabbar.c
// The new behavior system provides these functions through:
// - element_get_selected_index()
// - element_set_selected_index()
// - element_is_selected()