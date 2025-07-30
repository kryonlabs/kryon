/**
 * @file input_handler.c
 * @brief Kryon Input Handler Implementation
 */

#include "internal/events.h"
#include "internal/platform.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// INPUT STATE TRACKING
// =============================================================================

typedef struct {
    bool keys[KRYON_KEY_COUNT];
    bool mouse_buttons[KRYON_MOUSE_BUTTON_COUNT];
    float mouse_x, mouse_y;
    float mouse_wheel_x, mouse_wheel_y;
    uint32_t modifiers;
    
    // Touch state
    KryonTouch active_touches[KRYON_MAX_TOUCHES];
    size_t active_touch_count;
    
    // Input focus
    uint32_t focused_element_id;
    
    // Input history for gesture recognition
    KryonInputEvent input_history[KRYON_INPUT_HISTORY_SIZE];
    size_t input_history_head;
    size_t input_history_count;
    
    // Timing
    double last_update_time;
    double key_repeat_delay;
    double key_repeat_rate;
    
    // Configuration
    bool mouse_capture;
    bool relative_mouse_mode;
    float mouse_sensitivity;
    float touch_sensitivity;
} KryonInputState;

static KryonInputState g_input_state = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void add_to_input_history(const KryonInputEvent *event) {
    g_input_state.input_history[g_input_state.input_history_head] = *event;
    g_input_state.input_history_head = (g_input_state.input_history_head + 1) % KRYON_INPUT_HISTORY_SIZE;
    
    if (g_input_state.input_history_count < KRYON_INPUT_HISTORY_SIZE) {
        g_input_state.input_history_count++;
    }
}

static KryonTouch* find_touch_by_id(uint32_t touch_id) {
    for (size_t i = 0; i < g_input_state.active_touch_count; i++) {
        if (g_input_state.active_touches[i].id == touch_id) {
            return &g_input_state.active_touches[i];
        }
    }
    return NULL;
}

static void remove_touch_by_id(uint32_t touch_id) {
    for (size_t i = 0; i < g_input_state.active_touch_count; i++) {
        if (g_input_state.active_touches[i].id == touch_id) {
            // Shift remaining touches
            for (size_t j = i + 1; j < g_input_state.active_touch_count; j++) {
                g_input_state.active_touches[j - 1] = g_input_state.active_touches[j];
            }
            g_input_state.active_touch_count--;
            break;
        }
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_input_init(const KryonInputConfig *config) {
    memset(&g_input_state, 0, sizeof(g_input_state));
    
    if (config) {
        g_input_state.key_repeat_delay = config->key_repeat_delay;
        g_input_state.key_repeat_rate = config->key_repeat_rate;
        g_input_state.mouse_sensitivity = config->mouse_sensitivity;
        g_input_state.touch_sensitivity = config->touch_sensitivity;
    } else {
        // Default configuration
        g_input_state.key_repeat_delay = 0.5; // 500ms
        g_input_state.key_repeat_rate = 20.0; // 20 Hz
        g_input_state.mouse_sensitivity = 1.0;
        g_input_state.touch_sensitivity = 1.0;
    }
    
    return true;
}

void kryon_input_shutdown(void) {
    memset(&g_input_state, 0, sizeof(g_input_state));
}

void kryon_input_update(double delta_time) {
    g_input_state.last_update_time += delta_time;
    
    // Update touch states
    for (size_t i = 0; i < g_input_state.active_touch_count; i++) {
        KryonTouch *touch = &g_input_state.active_touches[i];
        touch->duration += delta_time;
        
        // Calculate velocity
        touch->velocity_x = (touch->x - touch->prev_x) / delta_time;
        touch->velocity_y = (touch->y - touch->prev_y) / delta_time;
        
        touch->prev_x = touch->x;
        touch->prev_y = touch->y;
    }
}

bool kryon_input_handle_platform_event(const KryonPlatformEvent *platform_event) {
    if (!platform_event) return false;
    
    KryonInputEvent input_event = {0};
    input_event.timestamp = kryon_platform_get_time();
    
    switch (platform_event->type) {
        case KRYON_PLATFORM_EVENT_KEY_DOWN: {
            KryonKeyCode key = platform_event->data.key.code;
            if (key < KRYON_KEY_COUNT) {
                g_input_state.keys[key] = true;
                g_input_state.modifiers = platform_event->data.key.modifiers;
                
                input_event.type = KRYON_INPUT_EVENT_KEY_DOWN;
                input_event.data.key.code = key;
                input_event.data.key.modifiers = g_input_state.modifiers;
                input_event.data.key.repeat = false; // TODO: Track key repeats
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_KEY_UP: {
            KryonKeyCode key = platform_event->data.key.code;
            if (key < KRYON_KEY_COUNT) {
                g_input_state.keys[key] = false;
                g_input_state.modifiers = platform_event->data.key.modifiers;
                
                input_event.type = KRYON_INPUT_EVENT_KEY_UP;
                input_event.data.key.code = key;
                input_event.data.key.modifiers = g_input_state.modifiers;
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_MOUSE_MOVE: {
            float new_x = platform_event->data.mouse.x * g_input_state.mouse_sensitivity;
            float new_y = platform_event->data.mouse.y * g_input_state.mouse_sensitivity;
            
            input_event.type = KRYON_INPUT_EVENT_MOUSE_MOVE;
            input_event.data.mouse.x = new_x;
            input_event.data.mouse.y = new_y;
            input_event.data.mouse.delta_x = new_x - g_input_state.mouse_x;
            input_event.data.mouse.delta_y = new_y - g_input_state.mouse_y;
            input_event.data.mouse.buttons = 0;
            
            // Set button flags
            for (int i = 0; i < KRYON_MOUSE_BUTTON_COUNT; i++) {
                if (g_input_state.mouse_buttons[i]) {
                    input_event.data.mouse.buttons |= (1 << i);
                }
            }
            
            g_input_state.mouse_x = new_x;
            g_input_state.mouse_y = new_y;
            
            add_to_input_history(&input_event);
            return true;
        }
        
        case KRYON_PLATFORM_EVENT_MOUSE_DOWN: {
            KryonMouseButton button = platform_event->data.mouse.button;
            if (button < KRYON_MOUSE_BUTTON_COUNT) {
                g_input_state.mouse_buttons[button] = true;
                
                input_event.type = KRYON_INPUT_EVENT_MOUSE_DOWN;
                input_event.data.mouse.x = g_input_state.mouse_x;
                input_event.data.mouse.y = g_input_state.mouse_y;
                input_event.data.mouse.button = button;
                input_event.data.mouse.clicks = 1; // TODO: Track multi-clicks
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_MOUSE_UP: {
            KryonMouseButton button = platform_event->data.mouse.button;
            if (button < KRYON_MOUSE_BUTTON_COUNT) {
                g_input_state.mouse_buttons[button] = false;
                
                input_event.type = KRYON_INPUT_EVENT_MOUSE_UP;
                input_event.data.mouse.x = g_input_state.mouse_x;
                input_event.data.mouse.y = g_input_state.mouse_y;
                input_event.data.mouse.button = button;
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_MOUSE_WHEEL: {
            g_input_state.mouse_wheel_x += platform_event->data.wheel.x;
            g_input_state.mouse_wheel_y += platform_event->data.wheel.y;
            
            input_event.type = KRYON_INPUT_EVENT_MOUSE_WHEEL;
            input_event.data.wheel.x = platform_event->data.wheel.x;
            input_event.data.wheel.y = platform_event->data.wheel.y;
            input_event.data.wheel.mouse_x = g_input_state.mouse_x;
            input_event.data.wheel.mouse_y = g_input_state.mouse_y;
            
            add_to_input_history(&input_event);
            return true;
        }
        
        case KRYON_PLATFORM_EVENT_TOUCH_DOWN: {
            if (g_input_state.active_touch_count < KRYON_MAX_TOUCHES) {
                KryonTouch *touch = &g_input_state.active_touches[g_input_state.active_touch_count++];
                touch->id = platform_event->data.touch.id;
                touch->x = platform_event->data.touch.x * g_input_state.touch_sensitivity;
                touch->y = platform_event->data.touch.y * g_input_state.touch_sensitivity;
                touch->prev_x = touch->x;
                touch->prev_y = touch->y;
                touch->pressure = platform_event->data.touch.pressure;
                touch->start_time = input_event.timestamp;
                touch->duration = 0.0;
                touch->velocity_x = 0.0;
                touch->velocity_y = 0.0;
                
                input_event.type = KRYON_INPUT_EVENT_TOUCH_DOWN;
                input_event.data.touch = *touch;
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_TOUCH_UP: {
            KryonTouch *touch = find_touch_by_id(platform_event->data.touch.id);
            if (touch) {
                input_event.type = KRYON_INPUT_EVENT_TOUCH_UP;
                input_event.data.touch = *touch;
                
                add_to_input_history(&input_event);
                remove_touch_by_id(platform_event->data.touch.id);
                return true;
            }
            break;
        }
        
        case KRYON_PLATFORM_EVENT_TOUCH_MOVE: {
            KryonTouch *touch = find_touch_by_id(platform_event->data.touch.id);
            if (touch) {
                touch->prev_x = touch->x;
                touch->prev_y = touch->y;
                touch->x = platform_event->data.touch.x * g_input_state.touch_sensitivity;
                touch->y = platform_event->data.touch.y * g_input_state.touch_sensitivity;
                touch->pressure = platform_event->data.touch.pressure;
                
                input_event.type = KRYON_INPUT_EVENT_TOUCH_MOVE;
                input_event.data.touch = *touch;
                
                add_to_input_history(&input_event);
                return true;
            }
            break;
        }
        
        default:
            break;
    }
    
    return false;
}

// =============================================================================
// INPUT STATE QUERIES
// =============================================================================

bool kryon_input_is_key_down(KryonKeyCode key) {
    return key < KRYON_KEY_COUNT ? g_input_state.keys[key] : false;
}

bool kryon_input_is_mouse_button_down(KryonMouseButton button) {
    return button < KRYON_MOUSE_BUTTON_COUNT ? g_input_state.mouse_buttons[button] : false;
}

void kryon_input_get_mouse_position(float *x, float *y) {
    if (x) *x = g_input_state.mouse_x;
    if (y) *y = g_input_state.mouse_y;
}

void kryon_input_get_mouse_wheel(float *x, float *y) {
    if (x) *x = g_input_state.mouse_wheel_x;
    if (y) *y = g_input_state.mouse_wheel_y;
}

uint32_t kryon_input_get_modifiers(void) {
    return g_input_state.modifiers;
}

const KryonTouch* kryon_input_get_touches(size_t *count) {
    if (count) *count = g_input_state.active_touch_count;
    return g_input_state.active_touches;
}

const KryonInputEvent* kryon_input_get_history(size_t *count) {
    if (count) *count = g_input_state.input_history_count;
    return g_input_state.input_history;
}

// =============================================================================
// INPUT FOCUS MANAGEMENT
// =============================================================================

void kryon_input_set_focus(uint32_t element_id) {
    g_input_state.focused_element_id = element_id;
}

uint32_t kryon_input_get_focus(void) {
    return g_input_state.focused_element_id;
}

void kryon_input_clear_focus(void) {
    g_input_state.focused_element_id = 0;
}

// =============================================================================
// INPUT CONFIGURATION
// =============================================================================

void kryon_input_set_mouse_capture(bool capture) {
    g_input_state.mouse_capture = capture;
    kryon_platform_set_mouse_capture(capture);
}

bool kryon_input_get_mouse_capture(void) {
    return g_input_state.mouse_capture;
}

void kryon_input_set_relative_mouse_mode(bool relative) {
    g_input_state.relative_mouse_mode = relative;
    kryon_platform_set_relative_mouse_mode(relative);
}

bool kryon_input_get_relative_mouse_mode(void) {
    return g_input_state.relative_mouse_mode;
}

void kryon_input_set_mouse_sensitivity(float sensitivity) {
    g_input_state.mouse_sensitivity = fmaxf(0.1f, fminf(10.0f, sensitivity));
}

float kryon_input_get_mouse_sensitivity(void) {
    return g_input_state.mouse_sensitivity;
}

void kryon_input_set_touch_sensitivity(float sensitivity) {
    g_input_state.touch_sensitivity = fmaxf(0.1f, fminf(10.0f, sensitivity));
}

float kryon_input_get_touch_sensitivity(void) {
    return g_input_state.touch_sensitivity;
}