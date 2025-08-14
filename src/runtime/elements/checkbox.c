/**
 * @file checkbox.c
 * @brief Checkbox Element - Full Implementation
 * 
 * Provides checkbox functionality with checked/unchecked states, labels,
 * keyboard navigation, focus management, and event handling.
 * 
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// =============================================================================
// CHECKBOX STATE (Element-Owned)  
// =============================================================================

typedef struct {
    bool is_checked;                // Current checked state
    bool has_focus;                 // Whether this checkbox has focus
    bool is_pressed;                // Whether currently being pressed
    bool is_hovered;                // Whether currently hovered
    bool is_indeterminate;          // Whether in indeterminate state (for tri-state)
    bool state_changed;             // Flag to indicate state was modified
} CheckboxState;

// =============================================================================
// CHECKBOX STATE MANAGEMENT
// =============================================================================

static CheckboxState* get_checkbox_state(struct KryonElement* element) {
    return (CheckboxState*)element->user_data;
}

static CheckboxState* ensure_checkbox_state(struct KryonElement* element) {
    CheckboxState* state = get_checkbox_state(element);
    if (!state) {
        state = kryon_calloc(1, sizeof(CheckboxState));
        if (state) {
            // Initialize with current property value if available
            state->is_checked = get_element_property_bool(element, "checked", false);
            state->has_focus = false;
            state->is_pressed = false;
            state->is_hovered = false;
            state->is_indeterminate = get_element_property_bool(element, "indeterminate", false);
            state->state_changed = false;
            
            element->user_data = state;
        }
    }
    return state;
}

static void destroy_checkbox_state(CheckboxState* state) {
    if (state) {
        kryon_free(state);
    }
}

// =============================================================================
// CHECKBOX EVENT HANDLING
// =============================================================================

static bool checkbox_handle_event(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event) return false;
    
    CheckboxState* state = ensure_checkbox_state(element);
    if (!state) return false;
    
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    if (is_disabled && event->type != ELEMENT_EVENT_UNFOCUSED) return false;
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED: {
            // Toggle checkbox state
            if (state->is_indeterminate) {
                // If indeterminate, first click makes it checked
                state->is_indeterminate = false;
                state->is_checked = true;
            } else {
                // Normal toggle behavior
                state->is_checked = !state->is_checked;
            }
            
            state->state_changed = true;
            return true;
        }
        
        // Note: Mouse press/release handled via CLICKED events
        // We track pressed state internally during CLICKED event
        
        case ELEMENT_EVENT_MOUSE_MOVED: {
            // Update hover state based on mouse position
            ElementBounds bounds = element_get_bounds(element);
            float mouse_x = event->data.mousePos.x;
            float mouse_y = event->data.mousePos.y;
            
            bool was_hovered = state->is_hovered;
            state->is_hovered = (mouse_x >= bounds.x && mouse_x <= bounds.x + bounds.width &&
                                mouse_y >= bounds.y && mouse_y <= bounds.y + bounds.height);
            
            if (was_hovered != state->is_hovered) {
                return true; // Trigger re-render for hover effect
            }
            return false;
        }
        
        case ELEMENT_EVENT_FOCUSED: {
            state->has_focus = true;
            return true;
        }
        
        case ELEMENT_EVENT_UNFOCUSED: {
            state->has_focus = false;
            state->is_pressed = false;
            return true;
        }
        
        case ELEMENT_EVENT_KEY_PRESSED: {
            if (!state->has_focus) return false;
            
            int keyCode = event->data.keyPressed.keyCode;
            
            switch (keyCode) {
                case 32:   // Space
                case 257:  // Enter
                    // Toggle checkbox with keyboard
                    if (state->is_indeterminate) {
                        state->is_indeterminate = false;
                        state->is_checked = true;
                    } else {
                        state->is_checked = !state->is_checked;
                    }
                    
                    state->state_changed = true;
                    return true;
                    
                default:
                    return false;
            }
        }
        
        default:
            return false;
    }
    
    return false;
}

// =============================================================================
// CHECKBOX RENDERING
// =============================================================================

static void checkbox_render(struct KryonRuntime* runtime, struct KryonElement* element,
                           KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 3) return;
    
    CheckboxState* state = ensure_checkbox_state(element);
    if (!state) return;
    
    // Get checkbox properties
    float posX = element->x;
    float posY = element->y;
    float width = get_element_property_float(element, "width", 200.0f);
    float height = get_element_property_float(element, "height", 24.0f);
    
    const char* label = get_element_property_string(element, "label");
    if (!label) label = get_element_property_string(element, "text");
    if (!label) label = "";
    
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF);
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x999999FF);
    uint32_t text_color_val = get_element_property_color(element, "color", 0x333333FF);
    uint32_t check_color_val = get_element_property_color(element, "checkColor", 0x0078D4FF);
    
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 2.0f);
    float font_size = get_element_property_float(element, "fontSize", 14.0f);
    
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    
    // Calculate checkbox dimensions
    float checkbox_size = fminf(height - 4.0f, 18.0f); // Max 18px checkbox
    float checkbox_x = posX;
    float checkbox_y = posY + (height - checkbox_size) / 2;
    float label_x = checkbox_x + checkbox_size + 8.0f; // 8px gap
    float label_y = posY + (height - font_size) / 2;
    
    // Convert colors
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    KryonColor text_color = color_u32_to_f32(text_color_val);
    KryonColor check_color = color_u32_to_f32(check_color_val);
    
    // Adjust colors based on state
    if (is_disabled) {
        bg_color = color_desaturate(bg_color, 0.5f);
        text_color.a *= 0.5f;
        border_color.a *= 0.5f;
        check_color.a *= 0.5f;
    } else if (state->is_pressed && state->has_focus) {
        bg_color = color_lighten(bg_color, -0.1f); // Use negative factor to darken
        border_color = color_lighten(check_color, 0.2f);
    } else if (state->is_hovered) {
        bg_color = color_lighten(bg_color, 0.05f);
        border_color = color_lighten(border_color, 0.2f);
    } else if (state->has_focus) {
        border_color = color_lighten(check_color, 0.3f);
        border_width *= 1.5f;
    }
    
    // Draw checkbox background
    KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
        (KryonVec2){checkbox_x, checkbox_y},
        (KryonVec2){checkbox_size, checkbox_size},
        bg_color,
        border_radius
    );
    bg_cmd.z_index = 10;
    commands[(*command_count)++] = bg_cmd;
    
    // Draw checkbox border
    if (border_width > 0.0f && *command_count < max_commands) {
        KryonRenderCommand border_cmd = kryon_cmd_draw_rect(
            (KryonVec2){checkbox_x - border_width, checkbox_y - border_width},
            (KryonVec2){checkbox_size + 2*border_width, checkbox_size + 2*border_width},
            border_color,
            border_radius
        );
        border_cmd.z_index = 9;
        commands[(*command_count)++] = border_cmd;
    }
    
    // Draw check mark or indeterminate mark
    if (*command_count < max_commands) {
        const char* mark_text = NULL;
        if (state->is_indeterminate) {
            mark_text = "-"; // Indeterminate mark
        } else if (state->is_checked) {
            mark_text = "✓"; // Check mark (or use "✔" if preferred)
        }
        
        if (mark_text) {
            KryonRenderCommand check_cmd = kryon_cmd_draw_text(
                (KryonVec2){checkbox_x + checkbox_size/2 - font_size*0.3f, checkbox_y + checkbox_size/2 - font_size/2},
                mark_text,
                font_size * 0.8f,
                check_color
            );
            check_cmd.z_index = 12;
            commands[(*command_count)++] = check_cmd;
        }
    }
    
    // Draw label text
    if (label && label[0] != '\0' && *command_count < max_commands) {
        KryonRenderCommand text_cmd = kryon_cmd_draw_text(
            (KryonVec2){label_x, label_y},
            label,
            font_size,
            text_color
        );
        text_cmd.z_index = 11;
        commands[(*command_count)++] = text_cmd;
    }
}

// =============================================================================
// CHECKBOX CLEANUP
// =============================================================================

static void checkbox_destroy(struct KryonRuntime* runtime, struct KryonElement* element) {
    (void)runtime;
    CheckboxState* state = get_checkbox_state(element);
    if (state) {
        destroy_checkbox_state(state);
        element->user_data = NULL;
    }
}

// =============================================================================
// CHECKBOX VTABLE AND REGISTRATION
// =============================================================================

static const ElementVTable checkbox_vtable = {
    .render = checkbox_render,
    .handle_event = checkbox_handle_event,
    .destroy = checkbox_destroy
};

bool register_checkbox_element(void) {
    return element_register_type("Checkbox", &checkbox_vtable);
}