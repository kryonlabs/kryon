/**
 * @file dropdown.c
 * @brief Clean Dropdown Element - Self-Managing Hover Logic
 * 
 * Perfect abstraction:
 * - Handles only abstract events (CLICKED, MOUSE_MOVED, etc.)
 * - No coordinate calculations in infrastructure
 * - Element calculates its own option hover from mouse position
 * - Clean separation of concerns
 * 
 * 0BSD License
 */

#include "internal/elements.h"
#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/renderer_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// External function declaration
extern bool kryon_element_set_property_by_name(struct KryonElement *element, const char *name, const void *value);

// =============================================================================
// DROPDOWN STATE (Element-Owned)
// =============================================================================

typedef struct {
    int selected_option_index;       // Currently selected option (-1 = none)
    bool is_open;                   // Is this dropdown open?
    int hovered_option_index;       // Which option is mouse-hovered (-1 = none)  
    int keyboard_selected_index;    // Index selected via keyboard navigation
    int option_count;               // Total number of options
    bool use_keyboard_selection;    // Whether to use keyboard or mouse selection
    bool has_focus;                 // Whether this dropdown has focus
    
    // Advanced features (for future)
    bool is_multi_select;
    float max_height;
    int scroll_offset;
    int visible_options;
} DropdownState;

// =============================================================================
// DROPDOWN IMPLEMENTATION
// =============================================================================

static DropdownState* get_dropdown_state(struct KryonElement* element) {
    return (DropdownState*)element->user_data;
}

static DropdownState* ensure_dropdown_state(struct KryonElement* element) {
    DropdownState* state = get_dropdown_state(element);
    if (!state) {
        state = kryon_alloc(sizeof(DropdownState));
        if (state) {
            memset(state, 0, sizeof(DropdownState));
            
            // Initialize with current property value if available
            state->selected_option_index = get_element_property_int(element, "selectedIndex", -1);
            state->hovered_option_index = -1;
            state->keyboard_selected_index = state->selected_option_index;
            state->is_open = false;
            state->use_keyboard_selection = false;
            state->has_focus = false;
            state->max_height = 200.0f;
            state->visible_options = 6;
            element->user_data = state;
            
        }
    }
    return state;
}

static void destroy_dropdown_state(DropdownState* state) {
    if (state) {
        kryon_free(state);
    }
}

// =============================================================================
// DROPDOWN HOVER CALCULATION (Self-Managed)
// =============================================================================

// Forward declarations
static void close_dropdowns_in_tree(struct KryonElement* element, struct KryonElement* except_this_one);

static int calculate_hovered_option_from_mouse(struct KryonElement* element, float mouse_x, float mouse_y) {
    DropdownState* state = get_dropdown_state(element);
    if (!state || !state->is_open) {
        return -1;
    }
    
    // Get dropdown bounds
    ElementBounds bounds = element_get_bounds(element);
    float popup_x = bounds.x;
    float popup_y = bounds.y + bounds.height;
    float popup_width = bounds.width;
    float option_height = 32.0f;
    
    // Check if mouse is within popup area
    if (mouse_x >= popup_x && mouse_x <= popup_x + popup_width && mouse_y >= popup_y) {
        // Calculate which option is being hovered
        int option_index = (int)((mouse_y - popup_y) / option_height);
        
        // Make sure option index is valid
        if (option_index >= 0 && option_index < state->option_count) {
            return option_index;
        }
    }
    
    return -1; // No option hovered
}

static void close_all_other_dropdowns(struct KryonRuntime* runtime, struct KryonElement* except_this_one) {
    if (!runtime || !runtime->root) return;
    
    close_dropdowns_in_tree(runtime->root, except_this_one);
}

static void close_dropdowns_in_tree(struct KryonElement* element, struct KryonElement* except_this_one) {
    if (!element) return;
    
    // Check if this element is a dropdown and close it if it's not the exception
    if (element != except_this_one && element->type_name && strcmp(element->type_name, "Dropdown") == 0) {
        if (element->user_data) {
            typedef struct {
                int selected_option_index;       // Field #1
                bool is_open;                   // Field #2
                int hovered_option_index;       // Field #3
                int keyboard_selected_index;    // Field #4 - MUST MATCH actual DropdownState!
                int option_count;               // Field #5
                // ... other fields
            } DropdownState;
            
            DropdownState* state = (DropdownState*)element->user_data;
            if (state->is_open) {
                state->is_open = false;
                state->hovered_option_index = -1;
            }
        }
    }
    
    // Recursively check children
    for (size_t i = 0; i < element->child_count; i++) {
        close_dropdowns_in_tree(element->children[i], except_this_one);
    }
}

// =============================================================================
// DROPDOWN EVENT HANDLING (Clean Abstract Events Only)
// =============================================================================

static bool dropdown_handle_event(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event) return false;
    
    DropdownState* state = ensure_dropdown_state(element);
    if (!state) return false;
    
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED: {
            if (get_element_property_bool(element, "disabled", false)) {
                return false;
            }
            
            // Get click position from event data
            float click_x = event->data.mousePos.x;
            float click_y = event->data.mousePos.y;
            
            if (state->is_open) {
                // Check if click is on an option
                int clicked_option = calculate_hovered_option_from_mouse(element, click_x, click_y);
                
                if (clicked_option >= 0) {
                    // Valid option clicked - select it
                    int old_selected = state->selected_option_index;
                    state->selected_option_index = clicked_option;
                    
                    // Close dropdown after selection
                    state->is_open = false;
                    state->hovered_option_index = -1;
                    state->use_keyboard_selection = false;
                    
                    return true;
                }
                
                // Click outside options or on dropdown button - close dropdown
                state->is_open = false;
                state->hovered_option_index = -1;
                state->use_keyboard_selection = false;
            } else {
                // Dropdown is closed - open it
                close_all_other_dropdowns(runtime, element);
                
                size_t options_count = 0;
                const char** options = get_element_property_array(element, "options", &options_count);
                state->option_count = (int)options_count;
                state->selected_option_index = get_element_property_int(element, "selectedIndex", -1);
                state->keyboard_selected_index = state->selected_option_index;
                
                state->is_open = true;
            }
            
            return true;
        }
        
        case ELEMENT_EVENT_MOUSE_MOVED: {
            if (state->is_open) {
                // Element calculates its own option hover from mouse position
                float mouse_x = event->data.mousePos.x;
                float mouse_y = event->data.mousePos.y;
                
                int new_hovered = calculate_hovered_option_from_mouse(element, mouse_x, mouse_y);
                
                if (new_hovered != state->hovered_option_index) {
                    state->hovered_option_index = new_hovered;
                    
                    // Option hover state updated
                    
                    return true; // Trigger re-render for hover effect
                }
            }
            return false;
        }
        
        case ELEMENT_EVENT_FOCUSED: {
            state->has_focus = true;
            return true;
        }
        
        case ELEMENT_EVENT_UNFOCUSED: {
            state->has_focus = false;
            if (state->is_open) {
                state->is_open = false;
                state->hovered_option_index = -1;
                state->use_keyboard_selection = false;
            }
            return true;
        }
        
        case ELEMENT_EVENT_KEY_PRESSED: {
            if (!state->has_focus) return false;
            
            bool handled = true;
            switch (event->data.keyPressed.keyCode) {
                case 32:   // SPACE
                case 257:  // ENTER
                    if (!state->is_open) {
                        state->is_open = true;
                        state->use_keyboard_selection = true;
                        state->keyboard_selected_index = state->selected_option_index;
                        
                        size_t options_count = 0;
                        get_element_property_array(element, "options", &options_count);
                        state->option_count = (int)options_count;
                    } else if (event->data.keyPressed.keyCode == 257) { // ENTER
                        if (state->keyboard_selected_index >= 0 && state->keyboard_selected_index < state->option_count) {
                            int selected_idx = state->keyboard_selected_index;
                            state->selected_option_index = selected_idx;
                        }
                        state->is_open = false;
                        state->hovered_option_index = -1;
                        state->use_keyboard_selection = false;
                    }
                    break;
                    
                case 265: // UP arrow
                    if (state->is_open && state->option_count > 0) {
                        state->use_keyboard_selection = true;
                        if (state->keyboard_selected_index <= 0) {
                            state->keyboard_selected_index = state->option_count - 1;
                        } else {
                            state->keyboard_selected_index--;
                        }
                    }
                    break;
                    
                case 264: // DOWN arrow
                    if (!state->is_open) {
                        state->is_open = true;
                        state->use_keyboard_selection = true;
                        
                        size_t options_count = 0;
                        get_element_property_array(element, "options", &options_count);
                        state->option_count = (int)options_count;
                        state->keyboard_selected_index = (state->selected_option_index >= 0) ? 
                                                       state->selected_option_index : 0;
                    } else if (state->option_count > 0) {
                        state->use_keyboard_selection = true;
                        if (state->keyboard_selected_index >= state->option_count - 1) {
                            state->keyboard_selected_index = 0;
                        } else {
                            state->keyboard_selected_index++;
                        }
                    }
                    break;
                    
                case 256: // ESCAPE
                    if (state->is_open) {
                        state->is_open = false;
                        state->hovered_option_index = -1;
                        state->use_keyboard_selection = false;
                    } else {
                        handled = false;
                    }
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
// DROPDOWN RENDERING
// =============================================================================

static void dropdown_render(struct KryonRuntime* runtime, struct KryonElement* element,
                           KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 10) return;

    DropdownState* state = ensure_dropdown_state(element);
    if (!state) return;
    
    // Get dropdown properties
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 200.0f);
    float height = get_element_property_float(element, "height", 32.0f);
    
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF);
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0xCCCCCCFF);
    uint32_t text_color_val = get_element_property_color(element, "color", 0x333333FF);
    
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 4.0f);
    float font_size = get_element_property_float(element, "fontSize", 14.0f);
    
    const char* placeholder = get_element_property_string(element, "placeholder");
    if (!placeholder) placeholder = "Select option...";
    
    size_t options_count = 0;
    const char** options = get_element_property_array(element, "options", &options_count);
    
    int selected_index = state->selected_option_index;  // Use state value first
    if (selected_index < 0) {
        selected_index = get_element_property_int(element, "selectedIndex", -1);  // Fallback to property
    }
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    
    state->option_count = (int)options_count;
    
    // Convert colors
    KryonColor bg_color = {
        .r = (float)((bg_color_val >> 24) & 0xFF) / 255.0f,
        .g = (float)((bg_color_val >> 16) & 0xFF) / 255.0f,
        .b = (float)((bg_color_val >> 8) & 0xFF) / 255.0f,
        .a = (float)(bg_color_val & 0xFF) / 255.0f
    };
    
    KryonColor border_color = {
        .r = (float)((border_color_val >> 24) & 0xFF) / 255.0f,
        .g = (float)((border_color_val >> 16) & 0xFF) / 255.0f,
        .b = (float)((border_color_val >> 8) & 0xFF) / 255.0f,
        .a = (float)(border_color_val & 0xFF) / 255.0f
    };
    
    KryonColor text_color = {
        .r = (float)((text_color_val >> 24) & 0xFF) / 255.0f,
        .g = (float)((text_color_val >> 16) & 0xFF) / 255.0f,
        .b = (float)((text_color_val >> 8) & 0xFF) / 255.0f,
        .a = (float)(text_color_val & 0xFF) / 255.0f
    };
    
    // Adjust colors based on state
    if (is_disabled) {
        bg_color.a *= 0.5f;
        text_color.a *= 0.5f;
        border_color.a *= 0.5f;
    } else if (state->has_focus) {
        border_color.r = fminf(1.0f, border_color.r + 0.2f);
        border_color.g = fminf(1.0f, border_color.g + 0.2f);
        border_color.b = fminf(1.0f, border_color.b + 0.2f);
    }
    
    // Draw main dropdown button
    KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
        (KryonVec2){posX, posY},
        (KryonVec2){width, height},
        bg_color,
        border_radius
    );
    bg_cmd.z_index = 10;
    commands[(*command_count)++] = bg_cmd;
    
    // Draw dropdown text
    const char* display_text = placeholder;
    if (selected_index >= 0 && selected_index < (int)options_count && options) {
        display_text = options[selected_index];
    }
    
    if (*command_count < max_commands && display_text) {
        KryonRenderCommand text_cmd = kryon_cmd_draw_text(
            (KryonVec2){posX + 8.0f, posY + (height - font_size) / 2},
            display_text,
            font_size,
            text_color
        );
        text_cmd.z_index = 12;
        commands[(*command_count)++] = text_cmd;
    }
    
    // Draw dropdown arrow
    if (*command_count < max_commands) {
        float arrow_size = 8.0f;
        float arrow_x = posX + width - arrow_size - 8.0f;
        float arrow_y = posY + (height - arrow_size) / 2;
        
        KryonRenderCommand arrow_cmd = kryon_cmd_draw_text(
            (KryonVec2){arrow_x, arrow_y},
            state->is_open ? "^" : "v",
            font_size * 0.8f,
            text_color
        );
        arrow_cmd.z_index = 12;
        commands[(*command_count)++] = arrow_cmd;
    }
    
    // Draw dropdown popup if open
    if (state->is_open && options && options_count > 0 && *command_count < max_commands - (int)options_count) {
        float popup_x = posX;
        float popup_y = posY + height;
        float popup_width = width;
        float option_height = 32.0f;
        float popup_height = fminf((float)options_count * option_height, state->max_height);
        
        // Draw popup background
        KryonColor popup_bg = bg_color;
        popup_bg.a = 0.98f;
        
        KryonRenderCommand popup_bg_cmd = kryon_cmd_draw_rect(
            (KryonVec2){popup_x, popup_y},
            (KryonVec2){popup_width, popup_height},
            popup_bg,
            border_radius
        );
        popup_bg_cmd.z_index = 1000;
        commands[(*command_count)++] = popup_bg_cmd;
        
        // Draw options with hover effects
        int visible_count = (int)(popup_height / option_height);
        int end_index = fminf((int)options_count, visible_count);
        
        for (int i = 0; i < end_index && *command_count < max_commands - 2; i++) {
            float option_y = popup_y + i * option_height;
            const char* option_text = options[i];
            
            // Determine option colors based on state
            bool is_selected = (i == selected_index);
            bool is_keyboard_highlighted = (state->use_keyboard_selection && i == state->keyboard_selected_index);
            bool is_mouse_hovered = (i == state->hovered_option_index);
            
            KryonColor option_bg_color = popup_bg;
            KryonColor option_text_color = text_color;
            
            if (is_selected) {
                // Selected option - blue background (highest priority)
                option_bg_color = (KryonColor){0.2f, 0.4f, 0.8f, 1.0f};
                option_text_color = (KryonColor){1.0f, 1.0f, 1.0f, 1.0f};
            } else if (is_keyboard_highlighted) {
                // Keyboard highlighted option - light blue background
                option_bg_color = (KryonColor){0.8f, 0.9f, 1.0f, 1.0f};
                option_text_color = (KryonColor){0.0f, 0.0f, 0.0f, 1.0f};
            } else if (is_mouse_hovered) {
                // Mouse hovered option - light gray background
                option_bg_color = (KryonColor){0.9f, 0.9f, 0.9f, 1.0f};
                option_text_color = (KryonColor){0.1f, 0.1f, 0.1f, 1.0f};
            }
            
            // Draw option background
            KryonRenderCommand option_bg_cmd = kryon_cmd_draw_rect(
                (KryonVec2){popup_x, option_y},
                (KryonVec2){popup_width, option_height},
                option_bg_color,
                0.0f
            );
            option_bg_cmd.z_index = 1001;
            commands[(*command_count)++] = option_bg_cmd;
            
            // Draw option text
            if (*command_count < max_commands) {
                KryonRenderCommand option_text_cmd = kryon_cmd_draw_text(
                    (KryonVec2){popup_x + 8.0f, option_y + (option_height - font_size) / 2},
                    option_text ? option_text : "",
                    font_size,
                    option_text_color
                );
                option_text_cmd.z_index = 1002;
                commands[(*command_count)++] = option_text_cmd;
            }
        }
    }
}

// =============================================================================
// DROPDOWN CLEANUP
// =============================================================================

static void dropdown_destroy(struct KryonRuntime* runtime, struct KryonElement* element) {
    DropdownState* state = get_dropdown_state(element);
    if (state) {
        destroy_dropdown_state(state);
        element->user_data = NULL;
    }
}

// =============================================================================
// DROPDOWN VTABLE AND REGISTRATION
// =============================================================================

static const ElementVTable dropdown_vtable = {
    .render = dropdown_render,
    .handle_event = dropdown_handle_event,
    .destroy = dropdown_destroy
};

bool register_dropdown_element(void) {
    return element_register_type("Dropdown", &dropdown_vtable);
}