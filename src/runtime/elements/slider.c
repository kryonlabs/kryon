/**
 * @file slider.c
 * @brief Slider Element - Full Implementation
 * 
 * Provides slider functionality with value adjustment, range constraints,
 * keyboard navigation, focus management, and drag interaction.
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
// SLIDER STATE (Element-Owned)
// =============================================================================

typedef struct {
    float value;                    // Current slider value
    float min_value;                // Minimum allowed value
    float max_value;                // Maximum allowed value
    float step;                     // Step size for value changes
    
    bool has_focus;                 // Whether this slider has focus
    bool is_dragging;               // Whether currently being dragged
    bool is_hovered;                // Whether currently hovered
    
    float drag_start_x;             // X position where drag started
    float drag_start_value;         // Value when drag started
    
    bool value_changed;             // Flag to indicate value was modified
    bool show_value;                // Whether to show value text
} SliderState;

// =============================================================================
// SLIDER STATE MANAGEMENT
// =============================================================================

static SliderState* get_slider_state(struct KryonElement* element) {
    return (SliderState*)element->user_data;
}

static SliderState* ensure_slider_state(struct KryonElement* element) {
    SliderState* state = get_slider_state(element);
    if (!state) {
        state = kryon_calloc(1, sizeof(SliderState));
        if (state) {
            // Initialize with property values
            state->min_value = get_element_property_float(element, "min", 0.0f);
            state->max_value = get_element_property_float(element, "max", 100.0f);
            state->step = get_element_property_float(element, "step", 1.0f);
            state->value = get_element_property_float(element, "value", state->min_value);
            state->show_value = get_element_property_bool(element, "showValue", true);
            
            // Clamp value to valid range
            if (state->value < state->min_value) state->value = state->min_value;
            if (state->value > state->max_value) state->value = state->max_value;
            
            state->has_focus = false;
            state->is_dragging = false;
            state->is_hovered = false;
            state->drag_start_x = 0.0f;
            state->drag_start_value = 0.0f;
            state->value_changed = false;
            
            element->user_data = state;
        }
    }
    return state;
}

static void destroy_slider_state(SliderState* state) {
    if (state) {
        kryon_free(state);
    }
}

// =============================================================================
// SLIDER HELPERS
// =============================================================================

static float slider_clamp_value(SliderState* state, float value) {
    if (value < state->min_value) return state->min_value;
    if (value > state->max_value) return state->max_value;
    
    // Round to nearest step
    if (state->step > 0.0f) {
        float steps = roundf((value - state->min_value) / state->step);
        return state->min_value + steps * state->step;
    }
    
    return value;
}

static void slider_set_value(SliderState* state, float new_value) {
    float clamped_value = slider_clamp_value(state, new_value);
    if (fabsf(clamped_value - state->value) > 0.001f) {
        state->value = clamped_value;
        state->value_changed = true;
    }
}

static float slider_value_from_position(SliderState* state, float x, float track_x, float track_width) {
    if (track_width <= 0.0f) return state->value;
    
    float ratio = (x - track_x) / track_width;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    
    return state->min_value + ratio * (state->max_value - state->min_value);
}

static float slider_position_from_value(SliderState* state, float track_width) {
    if (state->max_value <= state->min_value) return 0.0f;
    
    float ratio = (state->value - state->min_value) / (state->max_value - state->min_value);
    return ratio * track_width;
}

// =============================================================================
// SLIDER EVENT HANDLING
// =============================================================================

static bool slider_handle_event(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event) return false;
    
    SliderState* state = ensure_slider_state(element);
    if (!state) return false;
    
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    if (is_disabled && event->type != ELEMENT_EVENT_UNFOCUSED) return false;
    
    // Get slider layout info
    ElementBounds bounds = element_get_bounds(element);
    float thumb_size = 20.0f;
    float track_height = 4.0f;
    float track_x = bounds.x + thumb_size/2;
    float track_width = bounds.width - thumb_size;
    float track_y = bounds.y + (bounds.height - track_height) / 2;
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED: {
            float mouse_x = event->data.mousePos.x;
            float mouse_y = event->data.mousePos.y;
            
            // Check if click is within slider bounds
            if (mouse_x >= bounds.x && mouse_x <= bounds.x + bounds.width &&
                mouse_y >= bounds.y && mouse_y <= bounds.y + bounds.height) {
                
                state->is_dragging = true;
                state->drag_start_x = mouse_x;
                state->drag_start_value = state->value;
                
                // Set value based on click position
                float new_value = slider_value_from_position(state, mouse_x, track_x, track_width);
                slider_set_value(state, new_value);
                
                return true;
            }
            return false;
        }
        
        case ELEMENT_EVENT_MOUSE_MOVED: {
            float mouse_x = event->data.mousePos.x;
            float mouse_y = event->data.mousePos.y;
            
            // Update hover state
            bool was_hovered = state->is_hovered;
            state->is_hovered = (mouse_x >= bounds.x && mouse_x <= bounds.x + bounds.width &&
                                mouse_y >= bounds.y && mouse_y <= bounds.y + bounds.height);
            
            // Handle dragging
            if (state->is_dragging) {
                float new_value = slider_value_from_position(state, mouse_x, track_x, track_width);
                slider_set_value(state, new_value);
                return true;
            }
            
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
            if (state->is_dragging) {
                state->is_dragging = false;
            }
            return true;
        }
        
        case ELEMENT_EVENT_KEY_PRESSED: {
            if (!state->has_focus) return false;
            
            int keyCode = event->data.keyPressed.keyCode;
            bool ctrlPressed = event->data.keyPressed.ctrl;
            
            float delta = state->step;
            if (ctrlPressed) delta *= 10.0f; // Bigger steps with Ctrl
            
            bool handled = true;
            switch (keyCode) {
                case 262: // Right arrow
                case 265: // Up arrow
                    slider_set_value(state, state->value + delta);
                    break;
                    
                case 263: // Left arrow  
                case 264: // Down arrow
                    slider_set_value(state, state->value - delta);
                    break;
                    
                case 268: // Home
                    slider_set_value(state, state->min_value);
                    break;
                    
                case 269: // End
                    slider_set_value(state, state->max_value);
                    break;
                    
                case 266: // Page Up
                    slider_set_value(state, state->value + delta * 10.0f);
                    break;
                    
                case 267: // Page Down
                    slider_set_value(state, state->value - delta * 10.0f);
                    break;
                    
                default:
                    handled = false;
                    break;
            }
            
            return handled;
        }
        
        default:
            return false;
    }
    
    return false;
}

// =============================================================================
// SLIDER RENDERING
// =============================================================================

static void slider_render(struct KryonRuntime* runtime, struct KryonElement* element,
                         KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 5) return;
    
    SliderState* state = ensure_slider_state(element);
    if (!state) return;
    
    // Get slider properties
    float posX = element->x;
    float posY = element->y;
    float width = get_element_property_float(element, "width", 200.0f);
    float height = get_element_property_float(element, "height", 24.0f);
    
    uint32_t track_color_val = get_element_property_color(element, "trackColor", 0xDDDDDDFF);
    uint32_t fill_color_val = get_element_property_color(element, "fillColor", 0x0078D4FF);
    uint32_t thumb_color_val = get_element_property_color(element, "thumbColor", 0x0078D4FF);
    uint32_t text_color_val = get_element_property_color(element, "color", 0x333333FF);
    
    float font_size = get_element_property_float(element, "fontSize", 12.0f);
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    
    // Calculate layout
    float thumb_size = 20.0f;
    float track_height = 4.0f;
    float track_x = posX + thumb_size/2;
    float track_y = posY + (height - track_height) / 2;
    float track_width = width - thumb_size;
    
    float thumb_pos_x = slider_position_from_value(state, track_width);
    float thumb_x = track_x + thumb_pos_x - thumb_size/2;
    float thumb_y = posY + (height - thumb_size) / 2;
    
    // Convert colors
    KryonColor track_color = color_u32_to_f32(track_color_val);
    KryonColor fill_color = color_u32_to_f32(fill_color_val);
    KryonColor thumb_color = color_u32_to_f32(thumb_color_val);
    KryonColor text_color = color_u32_to_f32(text_color_val);
    
    // Adjust colors based on state
    if (is_disabled) {
        track_color = color_desaturate(track_color, 0.5f);
        fill_color = color_desaturate(fill_color, 0.5f);
        thumb_color = color_desaturate(thumb_color, 0.5f);
        text_color.a *= 0.5f;
    } else if (state->is_dragging) {
        thumb_color = color_lighten(thumb_color, -0.2f); // Use negative factor to darken
        fill_color = color_lighten(fill_color, -0.1f);
    } else if (state->is_hovered) {
        thumb_color = color_lighten(thumb_color, 0.1f);
    } else if (state->has_focus) {
        thumb_color = color_lighten(thumb_color, 0.2f);
    }
    
    // Draw track background
    KryonRenderCommand track_cmd = kryon_cmd_draw_rect(
        (KryonVec2){track_x, track_y},
        (KryonVec2){track_width, track_height},
        track_color,
        track_height/2
    );
    track_cmd.z_index = 10;
    commands[(*command_count)++] = track_cmd;
    
    // Draw filled portion of track
    if (thumb_pos_x > 0.0f && *command_count < max_commands) {
        KryonRenderCommand fill_cmd = kryon_cmd_draw_rect(
            (KryonVec2){track_x, track_y},
            (KryonVec2){thumb_pos_x, track_height},
            fill_color,
            track_height/2
        );
        fill_cmd.z_index = 11;
        commands[(*command_count)++] = fill_cmd;
    }
    
    // Draw thumb
    if (*command_count < max_commands) {
        KryonRenderCommand thumb_cmd = kryon_cmd_draw_rect(
            (KryonVec2){thumb_x, thumb_y},
            (KryonVec2){thumb_size, thumb_size},
            thumb_color,
            thumb_size/2 // Circular thumb
        );
        thumb_cmd.z_index = 12;
        commands[(*command_count)++] = thumb_cmd;
    }
    
    // Draw focus ring
    if (state->has_focus && *command_count < max_commands) {
        KryonColor focus_color = color_lighten(thumb_color, 0.4f);
        focus_color.a = 0.6f;
        
        KryonRenderCommand focus_cmd = kryon_cmd_draw_rect(
            (KryonVec2){thumb_x - 2.0f, thumb_y - 2.0f},
            (KryonVec2){thumb_size + 4.0f, thumb_size + 4.0f},
            focus_color,
            (thumb_size + 4.0f)/2
        );
        focus_cmd.z_index = 9;
        commands[(*command_count)++] = focus_cmd;
    }
    
    // Draw value text if enabled
    if (state->show_value && *command_count < max_commands) {
        static char value_text[32];
        if (state->step >= 1.0f) {
            snprintf(value_text, sizeof(value_text), "%.0f", state->value);
        } else {
            snprintf(value_text, sizeof(value_text), "%.2f", state->value);
        }
        
        // Position text above or below slider
        float text_x = posX + width/2 - font_size; // Approximate centering
        float text_y = posY - font_size - 4.0f; // Above slider
        
        // If there's not enough room above, put it below
        if (text_y < 0.0f) {
            text_y = posY + height + 4.0f;
        }
        
        KryonRenderCommand text_cmd = kryon_cmd_draw_text(
            (KryonVec2){text_x, text_y},
            value_text,
            font_size,
            text_color
        );
        text_cmd.z_index = 13;
        commands[(*command_count)++] = text_cmd;
    }
}

// =============================================================================
// SLIDER CLEANUP
// =============================================================================

static void slider_destroy(struct KryonRuntime* runtime, struct KryonElement* element) {
    (void)runtime;
    SliderState* state = get_slider_state(element);
    if (state) {
        destroy_slider_state(state);
        element->user_data = NULL;
    }
}

// =============================================================================
// SLIDER VTABLE AND REGISTRATION
// =============================================================================

static const ElementVTable slider_vtable = {
    .render = slider_render,
    .handle_event = slider_handle_event,
    .destroy = slider_destroy
};

bool register_slider_element(void) {
    return element_register_type("Slider", &slider_vtable);
}