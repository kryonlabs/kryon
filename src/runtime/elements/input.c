/**
 * @file input.c
 * @brief Clean Text Input Element - Full Implementation
 * 
 * Provides text input functionality with cursor, selection, keyboard navigation,
 * focus management, and all standard input features.
 * 
 * 0BSD License
 */

#include "internal/elements.h"
#include "internal/runtime.h"
#include "internal/memory.h"
#include "internal/color_utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// =============================================================================
// INPUT STATE (Element-Owned)
// =============================================================================

typedef struct {
    char* text;                     // Current text content
    size_t text_capacity;           // Buffer capacity
    size_t text_length;             // Current text length
    
    size_t cursor_pos;              // Cursor position (character index)
    size_t selection_start;         // Selection start position
    size_t selection_end;           // Selection end position
    bool has_selection;             // Whether there's an active selection
    
    bool has_focus;                 // Whether this input has focus
    bool is_readonly;               // Whether input is read-only
    bool is_password;               // Whether to mask input text
    
    float cursor_blink_timer;       // Timer for cursor blinking
    bool cursor_visible;            // Current cursor visibility state
    
    float scroll_offset;            // Horizontal scroll offset for long text
    size_t max_length;              // Maximum allowed text length (0 = unlimited)
    
    // Event callbacks (for future extension)
    bool text_changed;              // Flag to indicate text was modified
} InputState;

// =============================================================================
// INPUT STATE MANAGEMENT
// =============================================================================

static InputState* get_input_state(struct KryonElement* element) {
    return (InputState*)element->user_data;
}

static InputState* ensure_input_state(struct KryonElement* element) {
    InputState* state = get_input_state(element);
    if (!state) {
        state = kryon_calloc(1, sizeof(InputState));
        if (state) {
            // Initialize with default values
            state->text_capacity = 256;
            state->text = kryon_calloc(state->text_capacity, sizeof(char));
            state->text_length = 0;
            state->cursor_pos = 0;
            state->selection_start = 0;
            state->selection_end = 0;
            state->has_selection = false;
            state->has_focus = false;
            state->is_readonly = false;
            state->is_password = false;
            state->cursor_blink_timer = 0.0f;
            state->cursor_visible = true;
            state->scroll_offset = 0.0f;
            state->max_length = 0; // Unlimited
            state->text_changed = false;
            
            // Load initial text from element property
            const char* initial_text = get_element_property_string(element, "value");
            if (initial_text) {
                size_t len = strlen(initial_text);
                if (len < state->text_capacity) {
                    strcpy(state->text, initial_text);
                    state->text_length = len;
                    state->cursor_pos = len; // Cursor at end
                }
            }
            
            // Check input type for special behavior
            const char* input_type = get_element_property_string(element, "type");
            if (input_type) {
                if (strcmp(input_type, "password") == 0) {
                    state->is_password = true;
                } else if (strcmp(input_type, "readonly") == 0) {
                    state->is_readonly = true;
                }
            }
            
            // Set max length from property
            int max_length = get_element_property_int(element, "maxlength", 0);
            if (max_length > 0) {
                state->max_length = (size_t)max_length;
            }
            
            element->user_data = state;
        }
    }
    return state;
}

static void destroy_input_state(InputState* state) {
    if (state) {
        if (state->text) {
            kryon_free(state->text);
        }
        kryon_free(state);
    }
}

// =============================================================================
// TEXT MANIPULATION HELPERS
// =============================================================================

static void input_clear_selection(InputState* state) {
    state->has_selection = false;
    state->selection_start = state->cursor_pos;
    state->selection_end = state->cursor_pos;
}

static void input_delete_selection(InputState* state) {
    if (!state->has_selection) return;
    
    size_t start = (state->selection_start < state->selection_end) ? 
                   state->selection_start : state->selection_end;
    size_t end = (state->selection_start < state->selection_end) ? 
                 state->selection_end : state->selection_start;
    
    // Move text after selection to fill the gap
    memmove(state->text + start, state->text + end, state->text_length - end + 1);
    state->text_length -= (end - start);
    state->cursor_pos = start;
    
    input_clear_selection(state);
    state->text_changed = true;
}

static bool is_valid_char_for_type(char c, const char* input_type) {
    if (!input_type) return true; // Default: allow all characters
    
    if (strcmp(input_type, "number") == 0) {
        return isdigit(c) || c == '.' || c == '-' || c == '+';
    } else if (strcmp(input_type, "email") == 0) {
        return isalnum(c) || c == '@' || c == '.' || c == '_' || c == '-' || c == '+';
    } else if (strcmp(input_type, "tel") == 0 || strcmp(input_type, "phone") == 0) {
        return isdigit(c) || c == '+' || c == '-' || c == '(' || c == ')' || c == ' ';
    } else if (strcmp(input_type, "url") == 0) {
        return isalnum(c) || c == ':' || c == '/' || c == '.' || c == '-' || c == '_' || c == '?' || c == '=' || c == '&' || c == '#';
    }
    
    return true; // Allow all characters for other types
}

static bool input_insert_char(InputState* state, char c, const char* input_type) {
    if (state->is_readonly) return false;
    if (state->max_length > 0 && state->text_length >= state->max_length) return false;
    
    // Validate character for input type
    if (!is_valid_char_for_type(c, input_type)) return false;
    
    // Delete selection first if exists
    if (state->has_selection) {
        input_delete_selection(state);
    }
    
    // Check if we need to expand buffer
    if (state->text_length + 1 >= state->text_capacity) {
        size_t new_capacity = state->text_capacity * 2;
        char* new_text = kryon_realloc(state->text, new_capacity);
        if (!new_text) return false;
        state->text = new_text;
        state->text_capacity = new_capacity;
    }
    
    // Insert character at cursor position
    memmove(state->text + state->cursor_pos + 1, 
            state->text + state->cursor_pos, 
            state->text_length - state->cursor_pos + 1);
    
    state->text[state->cursor_pos] = c;
    state->text_length++;
    state->cursor_pos++;
    
    state->text_changed = true;
    return true;
}

static void input_delete_char_backward(InputState* state) {
    if (state->is_readonly) return;
    
    if (state->has_selection) {
        input_delete_selection(state);
        return;
    }
    
    if (state->cursor_pos > 0) {
        memmove(state->text + state->cursor_pos - 1,
                state->text + state->cursor_pos,
                state->text_length - state->cursor_pos + 1);
        
        state->cursor_pos--;
        state->text_length--;
        state->text_changed = true;
    }
}

static void input_delete_char_forward(InputState* state) {
    if (state->is_readonly) return;
    
    if (state->has_selection) {
        input_delete_selection(state);
        return;
    }
    
    if (state->cursor_pos < state->text_length) {
        memmove(state->text + state->cursor_pos,
                state->text + state->cursor_pos + 1,
                state->text_length - state->cursor_pos);
        
        state->text_length--;
        state->text_changed = true;
    }
}

static void input_move_cursor(InputState* state, int delta, bool extend_selection) {
    size_t old_cursor = state->cursor_pos;
    
    if (delta < 0 && (size_t)(-delta) > state->cursor_pos) {
        state->cursor_pos = 0;
    } else if (delta > 0 && state->cursor_pos + delta > state->text_length) {
        state->cursor_pos = state->text_length;
    } else {
        state->cursor_pos += delta;
    }
    
    if (extend_selection) {
        if (!state->has_selection) {
            state->selection_start = old_cursor;
            state->selection_end = state->cursor_pos;
            state->has_selection = true;
        } else {
            state->selection_end = state->cursor_pos;
        }
    } else {
        input_clear_selection(state);
    }
}

// =============================================================================
// INPUT EVENT HANDLING
// =============================================================================

static bool input_handle_event(struct KryonRuntime* runtime, struct KryonElement* element, const ElementEvent* event) {
    if (!runtime || !element || !event) return false;
    
    InputState* state = ensure_input_state(element);
    if (!state) return false;
    
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    if (is_disabled && event->type != ELEMENT_EVENT_UNFOCUSED) return false;
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED: {
            if (!state->has_focus) {
                // Focus the input
                state->has_focus = true;
                state->cursor_visible = true;
                state->cursor_blink_timer = 0.0f;
                
                // Move cursor to end for now
                state->cursor_pos = state->text_length;
                input_clear_selection(state);
                
                return true;
            }
            return false;
        }
        
        case ELEMENT_EVENT_FOCUSED: {
            state->has_focus = true;
            state->cursor_visible = true;
            state->cursor_blink_timer = 0.0f;
            return true;
        }
        
        case ELEMENT_EVENT_UNFOCUSED: {
            state->has_focus = false;
            input_clear_selection(state);
            return true;
        }
        
        case ELEMENT_EVENT_KEY_PRESSED: {
            if (!state->has_focus) return false;
            
            int keyCode = event->data.keyPressed.keyCode;
            bool ctrlPressed = event->data.keyPressed.ctrl;
            bool shiftPressed = event->data.keyPressed.shift;
            
            // Reset cursor blink timer on any key press
            state->cursor_blink_timer = 0.0f;
            state->cursor_visible = true;
            
            switch (keyCode) {
                case 259: // Backspace
                    input_delete_char_backward(state);
                    return true;
                    
                case 261: // Delete
                    input_delete_char_forward(state);
                    return true;
                    
                case 263: // Left arrow
                    if (ctrlPressed) {
                        // Move to beginning of previous word
                        while (state->cursor_pos > 0 && !isalnum(state->text[state->cursor_pos - 1])) {
                            input_move_cursor(state, -1, shiftPressed);
                        }
                        while (state->cursor_pos > 0 && isalnum(state->text[state->cursor_pos - 1])) {
                            input_move_cursor(state, -1, shiftPressed);
                        }
                    } else {
                        input_move_cursor(state, -1, shiftPressed);
                    }
                    return true;
                    
                case 262: // Right arrow  
                    if (ctrlPressed) {
                        // Move to beginning of next word
                        while (state->cursor_pos < state->text_length && isalnum(state->text[state->cursor_pos])) {
                            input_move_cursor(state, 1, shiftPressed);
                        }
                        while (state->cursor_pos < state->text_length && !isalnum(state->text[state->cursor_pos])) {
                            input_move_cursor(state, 1, shiftPressed);
                        }
                    } else {
                        input_move_cursor(state, 1, shiftPressed);
                    }
                    return true;
                    
                case 268: // Home
                    state->cursor_pos = 0;
                    if (!shiftPressed) input_clear_selection(state);
                    else {
                        if (!state->has_selection) {
                            state->selection_start = state->text_length;
                            state->has_selection = true;
                        }
                        state->selection_end = 0;
                    }
                    return true;
                    
                case 269: // End
                    state->cursor_pos = state->text_length;
                    if (!shiftPressed) input_clear_selection(state);
                    else {
                        if (!state->has_selection) {
                            state->selection_start = 0;
                            state->has_selection = true;
                        }
                        state->selection_end = state->text_length;
                    }
                    return true;
                    
                case 257: // Enter
                    // For single-line input, Enter usually submits or moves focus
                    // For now, just indicate the event was handled
                    return true;
                    
                case 256: // Escape
                    // Clear focus or selection
                    if (state->has_selection) {
                        input_clear_selection(state);
                    } else {
                        state->has_focus = false;
                    }
                    return true;
                    
                case 65: // 'A' - Select All (Ctrl+A)
                    if (ctrlPressed) {
                        state->selection_start = 0;
                        state->selection_end = state->text_length;
                        state->has_selection = (state->text_length > 0);
                        state->cursor_pos = state->text_length;
                        return true;
                    }
                    break;
                    
                case 67: // 'C' - Copy (Ctrl+C)
                    if (ctrlPressed && state->has_selection) {
                        // Clipboard operations handled by platform layer
                        return true;
                    }
                    break;
                    
                case 86: // 'V' - Paste (Ctrl+V) 
                    if (ctrlPressed) {
                        // Clipboard operations handled by platform layer
                        return true;
                    }
                    break;
                    
                case 88: // 'X' - Cut (Ctrl+X)
                    if (ctrlPressed && state->has_selection) {
                        input_delete_selection(state);
                        return true;
                    }
                    break;
            }
            
            return false;
        }
        
        case ELEMENT_EVENT_KEY_TYPED: {
            if (!state->has_focus) return false;
            
            char c = event->data.keyTyped.character;
            
            // Filter out control characters (except tab)
            if (c < 32 && c != '\t') return false;
            
            const char* input_type = get_element_property_string(element, "type");
            
            if (!input_insert_char(state, c, input_type)) {
                return false; // Failed to insert (buffer full, validation failed, etc.)
            }
            
            // Reset cursor blink
            state->cursor_blink_timer = 0.0f;
            state->cursor_visible = true;
            
            return true;
        }
        
        default:
            return false;
    }
    
    return false;
}

// =============================================================================
// INPUT HELPERS
// =============================================================================

static const char* get_default_placeholder_for_type(const char* input_type) {
    if (!input_type) return "Enter text...";
    
    if (strcmp(input_type, "password") == 0) {
        return "Enter password...";
    } else if (strcmp(input_type, "email") == 0) {
        return "Enter email address...";
    } else if (strcmp(input_type, "number") == 0) {
        return "Enter number...";
    } else if (strcmp(input_type, "tel") == 0 || strcmp(input_type, "phone") == 0) {
        return "Enter phone number...";
    } else if (strcmp(input_type, "url") == 0) {
        return "Enter URL...";
    } else if (strcmp(input_type, "search") == 0) {
        return "Search...";
    }
    
    return "Enter text...";
}

// =============================================================================
// INPUT RENDERING
// =============================================================================

static void input_render(struct KryonRuntime* runtime, struct KryonElement* element,
                        KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 8) return;
    
    InputState* state = ensure_input_state(element);
    if (!state) return;
    
    // Update cursor blink timer
    state->cursor_blink_timer += 0.016f; // Assume ~60 FPS
    if (state->cursor_blink_timer >= 1.0f) {
        state->cursor_visible = !state->cursor_visible;
        state->cursor_blink_timer = 0.0f;
    }
    
    // Get input properties
    float posX = get_element_property_float(element, "posX", 0.0f);
    float posY = get_element_property_float(element, "posY", 0.0f);
    float width = get_element_property_float(element, "width", 200.0f);
    float height = get_element_property_float(element, "height", 32.0f);
    
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF);
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0xCCCCCCFF);
    uint32_t text_color_val = get_element_property_color(element, "color", 0x333333FF);
    uint32_t selection_color_val = get_element_property_color(element, "selectionColor", 0x0078D4AA);
    
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 4.0f);
    float font_size = get_element_property_float(element, "fontSize", 14.0f);
    
    const char* input_type = get_element_property_string(element, "type");
    const char* placeholder = get_element_property_string(element, "placeholder");
    if (!placeholder) {
        placeholder = get_default_placeholder_for_type(input_type);
    }
    bool is_disabled = get_element_property_bool(element, "disabled", false);
    
    // Convert colors
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    KryonColor text_color = color_u32_to_f32(text_color_val);
    KryonColor selection_color = color_u32_to_f32(selection_color_val);
    
    // Adjust colors based on state
    if (is_disabled) {
        bg_color = color_desaturate(bg_color, 0.5f);
        text_color.a *= 0.5f;
        border_color.a *= 0.5f;
    } else if (state->has_focus) {
        border_color = color_lighten(border_color, 0.3f);
        border_width *= 1.5f;
    }
    
    // Draw input background
    KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
        (KryonVec2){posX, posY},
        (KryonVec2){width, height},
        bg_color,
        border_radius
    );
    bg_cmd.z_index = 10;
    commands[(*command_count)++] = bg_cmd;
    
    // Draw border (using a slightly larger background rect)
    if (border_width > 0.0f && *command_count < max_commands) {
        KryonRenderCommand border_cmd = kryon_cmd_draw_rect(
            (KryonVec2){posX - border_width, posY - border_width},
            (KryonVec2){width + 2*border_width, height + 2*border_width},
            border_color,
            border_radius
        );
        border_cmd.z_index = 9;
        commands[(*command_count)++] = border_cmd;
    }
    
    // Prepare display text
    const char* display_text = state->text;
    bool show_placeholder = (state->text_length == 0 && placeholder && !state->has_focus);
    
    if (show_placeholder) {
        display_text = placeholder;
        text_color.a *= 0.6f; // Make placeholder dimmer
    } else if (state->is_password && state->text_length > 0) {
        // Create password mask
        static char password_mask[256];
        size_t mask_len = (state->text_length < 255) ? state->text_length : 255;
        memset(password_mask, '*', mask_len);
        password_mask[mask_len] = '\0';
        display_text = password_mask;
    }
    
    // Calculate text position with padding
    float text_padding = 8.0f;
    float text_y = posY + (height - font_size) / 2;
    
    // Calculate accurate text dimensions using renderer
    float cursor_text_pos = 0.0f;
    if (display_text && display_text[0] != '\0' && runtime->renderer) {
        KryonRenderer* renderer = (KryonRenderer*)runtime->renderer;
        if (renderer->vtable && renderer->vtable->measure_text_width) {
            // Create substring up to cursor position for accurate measurement
            if (state->cursor_pos > 0 && state->cursor_pos <= state->text_length) {
                char cursor_text[256];
                size_t copy_len = (state->cursor_pos < 255) ? state->cursor_pos : 255;
                strncpy(cursor_text, state->text, copy_len);
                cursor_text[copy_len] = '\0';
                cursor_text_pos = renderer->vtable->measure_text_width(cursor_text, font_size);
            } else {
                cursor_text_pos = 0.0f;
            }
        } else {
            // Fallback to approximation if measurement not available
            float char_width = font_size * 0.6f;
            cursor_text_pos = state->cursor_pos * char_width;
        }
    }
    
    // Adjust scroll to keep cursor visible
    float visible_width = width - 2 * text_padding;
    if (cursor_text_pos - state->scroll_offset > visible_width - 20.0f) {
        state->scroll_offset = cursor_text_pos - visible_width + 20.0f;
    }
    if (cursor_text_pos < state->scroll_offset + 20.0f) {
        state->scroll_offset = fmaxf(0.0f, cursor_text_pos - 20.0f);
    }
    
    // Draw selection background if active
    if (state->has_selection && !show_placeholder && *command_count < max_commands && runtime->renderer) {
        size_t sel_start = (state->selection_start < state->selection_end) ? 
                          state->selection_start : state->selection_end;
        size_t sel_end = (state->selection_start < state->selection_end) ? 
                        state->selection_end : state->selection_start;
        
        KryonRenderer* renderer = (KryonRenderer*)runtime->renderer;
        if (renderer->vtable && renderer->vtable->measure_text_width) {
            // Measure text width up to selection start and end
            float sel_start_pos = 0.0f;
            float sel_end_pos = 0.0f;
            
            if (sel_start > 0 && sel_start <= state->text_length) {
                char start_text[256];
                size_t start_copy_len = (sel_start < 255) ? sel_start : 255;
                strncpy(start_text, state->text, start_copy_len);
                start_text[start_copy_len] = '\0';
                sel_start_pos = renderer->vtable->measure_text_width(start_text, font_size);
            }
            
            if (sel_end > 0 && sel_end <= state->text_length) {
                char end_text[256];
                size_t end_copy_len = (sel_end < 255) ? sel_end : 255;
                strncpy(end_text, state->text, end_copy_len);
                end_text[end_copy_len] = '\0';
                sel_end_pos = renderer->vtable->measure_text_width(end_text, font_size);
            }
            
            float sel_x = posX + text_padding + sel_start_pos - state->scroll_offset;
            float sel_width = sel_end_pos - sel_start_pos;
            
            // Clip selection to visible area
            float input_left = posX + text_padding;
            float input_right = posX + width - text_padding;
            
            if (sel_x + sel_width > input_left && sel_x < input_right) {
                // Adjust selection bounds to fit within visible area
                if (sel_x < input_left) {
                    sel_width -= (input_left - sel_x);
                    sel_x = input_left;
                }
                if (sel_x + sel_width > input_right) {
                    sel_width = input_right - sel_x;
                }
                
                if (sel_width > 0) {
                    KryonRenderCommand sel_cmd = kryon_cmd_draw_rect(
                        (KryonVec2){sel_x, posY + 2.0f},
                        (KryonVec2){sel_width, height - 4.0f},
                        selection_color,
                        0.0f
                    );
                    sel_cmd.z_index = 11; // Below text but above background
                    commands[(*command_count)++] = sel_cmd;
                }
            }
        }
    }
    
    // Draw text with proper scrolling via positioning
    if (display_text && display_text[0] != '\0' && *command_count < max_commands - 2) {
        float text_base_x = posX + text_padding;
        
        // Push clip rectangle to prevent text overflow - covers entire input container
        KryonRenderCommand clip_push = {0};
        clip_push.type = KRYON_CMD_PUSH_CLIP;
        clip_push.z_index = 12;
        clip_push.data.draw_rect = (KryonDrawRectData){
            .position = {posX, posY + 2.0f},  // Start from container left edge
            .size = {width, height - 4.0f},  // Cover entire container width
            .color = {0, 0, 0, 0}, // Invisible
            .border_radius = 0.0f
        };
        commands[(*command_count)++] = clip_push;
        
        // Draw the full text, positioned to account for scroll offset
        // This ensures cursor and text use the same coordinate system
        KryonRenderCommand text_cmd = kryon_cmd_draw_text(
            (KryonVec2){text_base_x - state->scroll_offset, text_y},
            display_text,
            font_size,
            text_color
        );
        text_cmd.z_index = 13;
        commands[(*command_count)++] = text_cmd;
        
        // Pop clip rectangle
        KryonRenderCommand clip_pop = {0};
        clip_pop.type = KRYON_CMD_POP_CLIP;
        clip_pop.z_index = 14;
        commands[(*command_count)++] = clip_pop;
    }
    
    // Draw cursor
    if (state->has_focus && state->cursor_visible && !show_placeholder && *command_count < max_commands) {
        // Calculate cursor position based on accurate text measurements
        float cursor_x = posX + text_padding + cursor_text_pos - state->scroll_offset;
        
        // Only draw cursor if it's visible in the input area
        if (cursor_x >= posX + text_padding && cursor_x <= posX + width - text_padding) {
            KryonRenderCommand cursor_cmd = kryon_cmd_draw_rect(
                (KryonVec2){cursor_x, posY + 4.0f},
                (KryonVec2){1.0f, height - 8.0f},
                text_color,
                0.0f
            );
            cursor_cmd.z_index = 15; // Higher z-index than text
            commands[(*command_count)++] = cursor_cmd;
        }
    }
}

// =============================================================================
// INPUT CLEANUP
// =============================================================================

static void input_destroy(struct KryonRuntime* runtime, struct KryonElement* element) {
    (void)runtime;
    InputState* state = get_input_state(element);
    if (state) {
        destroy_input_state(state);
        element->user_data = NULL;
    }
}

// =============================================================================
// INPUT VTABLE AND REGISTRATION
// =============================================================================

static const ElementVTable input_vtable = {
    .render = input_render,
    .handle_event = input_handle_event,
    .destroy = input_destroy
};

bool register_input_element(void) {
    return element_register_type("Input", &input_vtable);
}