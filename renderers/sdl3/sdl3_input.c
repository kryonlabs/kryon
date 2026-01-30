#include "../../core/include/kryon.h"
#include <string.h>

// Try different SDL3 header locations
#if defined(__has_include)
  #if __has_include(<SDL3/SDL.h>)
    #include <SDL3/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #else
    #error "SDL3 headers not found"
  #endif
#else
  // Fallback to standard locations
  #include <SDL3/SDL.h>
#endif

// ============================================================================
// SDL3 Input System
// ============================================================================

typedef struct {
    bool mouse_buttons[8];
    int16_t mouse_x, mouse_y;
    bool keys[SDL_SCANCODE_COUNT];
    uint32_t key_states[SDL_SCANCODE_COUNT];
    bool text_input_enabled;
    char composing_text[KRYON_MAX_TEXT_LENGTH];
    uint8_t composing_length;
} kryon_sdl3_input_state_t;

static kryon_sdl3_input_state_t input_state = {0};

// Global window reference for SDL3 text input and cursor operations
static SDL_Window* g_window = NULL;
static SDL_Cursor* cursor_cache[SDL_SYSTEM_CURSOR_COUNT] = {0};
static SDL_SystemCursor current_cursor = SDL_SYSTEM_CURSOR_DEFAULT;

void kryon_sdl3_input_set_window(SDL_Window* window) {
    g_window = window;
}

// Keyboard state handling
void kryon_sdl3_input_init(void) {
    memset(&input_state, 0, sizeof(kryon_sdl3_input_state_t));
}

void kryon_sdl3_input_shutdown(void) {
    memset(&input_state, 0, sizeof(kryon_sdl3_input_state_t));
    for (int i = 0; i < SDL_SYSTEM_CURSOR_COUNT; i++) {
        if (cursor_cache[i] != NULL) {
            SDL_DestroyCursor(cursor_cache[i]);
            cursor_cache[i] = NULL;
        }
    }
    current_cursor = SDL_SYSTEM_CURSOR_DEFAULT;
}

// Update input state from SDL3 events
bool kryon_sdl3_process_event(const SDL_Event* sdl_event, kryon_event_t* kryon_event) {
    if (sdl_event == NULL || kryon_event == NULL) {
        return false;
    }

    switch (sdl_event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            input_state.mouse_x = sdl_event->motion.x;
            input_state.mouse_y = sdl_event->motion.y;
            return false; // Not a Kryon event yet

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (sdl_event->button.button < 8) {
                input_state.mouse_buttons[sdl_event->button.button] = true;
            }
            // Mouse down alone should not generate a click event
            // Only mouse up should generate click to prevent double-firing
            return false; // Don't generate Kryon event on mouse down

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (sdl_event->button.button < 8) {
                input_state.mouse_buttons[sdl_event->button.button] = false;
            }
            kryon_event->type = KRYON_EVT_CLICK;
            kryon_event->x = KRYON_FP_FROM_INT((int)sdl_event->button.x);
            kryon_event->y = KRYON_FP_FROM_INT((int)sdl_event->button.y);
            kryon_event->param = sdl_event->button.button;
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        case SDL_EVENT_KEY_DOWN:
            if (sdl_event->key.scancode < SDL_SCANCODE_COUNT) {
                input_state.keys[sdl_event->key.scancode] = true;
                input_state.key_states[sdl_event->key.scancode] = sdl_event->key.timestamp;
            }
            kryon_event->type = KRYON_EVT_KEY;
            kryon_event->param = sdl_event->key.scancode;
            kryon_event->data = (void*)1; // Key pressed
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        case SDL_EVENT_KEY_UP:
            if (sdl_event->key.scancode < SDL_SCANCODE_COUNT) {
                input_state.keys[sdl_event->key.scancode] = false;
            }
            kryon_event->type = KRYON_EVT_KEY;
            kryon_event->param = sdl_event->key.scancode;
            kryon_event->data = (void*)0; // Key released
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        case SDL_EVENT_TEXT_INPUT:
            if (input_state.text_input_enabled) {
                // Handle text input for text components
                const char* text = sdl_event->text.text;
                size_t text_len = strlen(text);
                if (input_state.composing_length + text_len < KRYON_MAX_TEXT_LENGTH - 1) {
                    memcpy(input_state.composing_text + input_state.composing_length,
                           text, text_len);
                    input_state.composing_length += text_len;
                    input_state.composing_text[input_state.composing_length] = '\0';
                }
            }
            return false;

        case SDL_EVENT_TEXT_EDITING:
            if (input_state.text_input_enabled) {
                // Handle IME editing
                const char* text = sdl_event->edit.text;
                size_t text_len = strlen(text);
                if (text_len < KRYON_MAX_TEXT_LENGTH) {
                    memcpy(input_state.composing_text, text, text_len);
                    input_state.composing_length = text_len;
                    input_state.composing_text[input_state.composing_length] = '\0';
                }
            }
            return false;

        case SDL_EVENT_MOUSE_WHEEL:
            kryon_event->type = KRYON_EVT_SCROLL;
            kryon_event->x = KRYON_FP_FROM_INT((int)sdl_event->wheel.x);
            kryon_event->y = KRYON_FP_FROM_INT((int)sdl_event->wheel.y);
            kryon_event->param = sdl_event->wheel.direction;
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            kryon_event->type = KRYON_EVT_FOCUS;
            kryon_event->param = 1; // Gained focus
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            kryon_event->type = KRYON_EVT_BLUR;
            kryon_event->param = 0; // Lost focus
            kryon_event->timestamp = SDL_GetTicks();
            return true;

        default:
            return false;
    }
}

// Input state query functions
bool kryon_sdl3_is_mouse_button_down(uint8_t button) {
    if (button < 8) {
        return input_state.mouse_buttons[button];
    }
    return false;
}

void kryon_sdl3_get_mouse_position(int16_t* x, int16_t* y) {
    if (x) *x = input_state.mouse_x;
    if (y) *y = input_state.mouse_y;
}

bool kryon_sdl3_is_key_down(SDL_Scancode scancode) {
    if (scancode < SDL_SCANCODE_COUNT) {
        return input_state.keys[scancode];
    }
    return false;
}

uint32_t kryon_sdl3_get_key_timestamp(SDL_Scancode scancode) {
    if (scancode < SDL_SCANCODE_COUNT) {
        return input_state.key_states[scancode];
    }
    return 0;
}

// Text input management
void kryon_sdl3_start_text_input(void) {
    input_state.text_input_enabled = true;
    input_state.composing_length = 0;
    input_state.composing_text[0] = '\0';
    SDL_StartTextInput(g_window);
}

void kryon_sdl3_stop_text_input(void) {
    input_state.text_input_enabled = false;
    input_state.composing_length = 0;
    input_state.composing_text[0] = '\0';
    SDL_StopTextInput(g_window);
}

const char* kryon_sdl3_get_composing_text(void) {
    return input_state.composing_text;
}

void kryon_sdl3_set_composing_text(const char* text) {
    if (text != NULL) {
        size_t text_len = strlen(text);
        if (text_len < KRYON_MAX_TEXT_LENGTH) {
            memcpy(input_state.composing_text, text, text_len);
            input_state.composing_length = text_len;
            input_state.composing_text[text_len] = '\0';
        }
    }
}

void kryon_sdl3_clear_composing_text(void) {
    input_state.composing_length = 0;
    input_state.composing_text[0] = '\0';
}

// Advanced input features
bool kryon_sdl3_is_key_combo_down(SDL_Scancode key1, SDL_Scancode key2) {
    return (key1 < SDL_SCANCODE_COUNT && input_state.keys[key1]) &&
           (key2 < SDL_SCANCODE_COUNT && input_state.keys[key2]);
}

bool kryon_sdl3_is_key_combo_pressed(SDL_Scancode key1, SDL_Scancode key2, uint32_t current_time, uint32_t threshold_ms) {
    if (key1 >= SDL_SCANCODE_COUNT || key2 >= SDL_SCANCODE_COUNT) {
        return false;
    }

    return input_state.keys[key1] && input_state.keys[key2] &&
           (current_time - input_state.key_states[key1] < threshold_ms ||
            current_time - input_state.key_states[key2] < threshold_ms);
}

// Cursor management
void kryon_sdl3_set_cursor(SDL_SystemCursor cursor) {
    if (cursor < 0 || cursor >= SDL_SYSTEM_CURSOR_COUNT) {
        cursor = SDL_SYSTEM_CURSOR_DEFAULT;
    }

    if (cursor_cache[cursor] == NULL) {
        cursor_cache[cursor] = SDL_CreateSystemCursor(cursor);
    }

    if (cursor_cache[cursor] != NULL) {
        SDL_SetCursor(cursor_cache[cursor]);
        current_cursor = cursor;
    }
}

void kryon_sdl3_show_cursor(bool show) {
    (void)show;  // Parameter reserved for future use
    SDL_ShowCursor();
}

void kryon_sdl3_apply_cursor_shape(uint8_t shape) {
    SDL_SystemCursor target = SDL_SYSTEM_CURSOR_DEFAULT;
    if (shape == 1) {
        target = SDL_SYSTEM_CURSOR_POINTER;
    }

    if (current_cursor == target) {
        return;
    }

    kryon_sdl3_set_cursor(target);
}

// Clipboard operations
bool kryon_sdl3_has_clipboard_text(void) {
    return SDL_HasClipboardText();
}

char* kryon_sdl3_get_clipboard_text(void) {
    return SDL_GetClipboardText();
}

bool kryon_sdl3_set_clipboard_text(const char* text) {
    return SDL_SetClipboardText(text) == 0;
}
