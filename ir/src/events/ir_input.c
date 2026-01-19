/*
 * Kryon Game Input System Implementation
 *
 * Unified input handling for games with state tracking and virtual input mapping
 */

#include "ir_input.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Input State Structures
// ============================================================================

typedef struct {
    bool current;    // Current state (this frame)
    bool previous;   // Previous state (last frame)
} InputButtonState;

typedef struct {
    InputButtonState keys[IR_KEY_COUNT];
    InputButtonState mouse_buttons[IR_MOUSE_BUTTON_COUNT];
    int32_t mouse_x, mouse_y;
    int32_t mouse_prev_x, mouse_prev_y;
    float mouse_wheel_x, mouse_wheel_y;
} InputKeyboardMouseState;

#define IR_MAX_GAMEPADS 4

typedef struct {
    bool connected;
    InputButtonState buttons[IR_GAMEPAD_BUTTON_COUNT];
    float axes[IR_GAMEPAD_AXIS_COUNT];
} InputGamepadState;

// Virtual input action mapping
typedef enum {
    IR_INPUT_BINDING_KEY,
    IR_INPUT_BINDING_MOUSE_BUTTON,
    IR_INPUT_BINDING_GAMEPAD_BUTTON
} InputBindingType;

typedef struct {
    InputBindingType type;
    union {
        IRKey key;
        struct {
            IRMouseButton button;
        } mouse;
        struct {
            int gamepad_index;
            IRGamepadButton button;
        } gamepad;
    } binding;
} InputBinding;

typedef struct {
    char name[IR_MAX_ACTION_NAME];
    InputBinding bindings[IR_MAX_BINDINGS_PER_ACTION];
    int binding_count;
} InputAction;

// Global input state
static struct {
    bool initialized;
    InputKeyboardMouseState kb_mouse;
    InputGamepadState gamepads[IR_MAX_GAMEPADS];
    InputAction actions[IR_MAX_ACTIONS];
    int action_count;
} g_input = {0};

// ============================================================================
// Initialization & Frame Management
// ============================================================================

void ir_input_init(void) {
    memset(&g_input, 0, sizeof(g_input));
    g_input.initialized = true;
}

void ir_input_shutdown(void) {
    memset(&g_input, 0, sizeof(g_input));
}

void ir_input_frame_begin(void) {
    // Clear mouse wheel delta
    g_input.kb_mouse.mouse_wheel_x = 0.0f;
    g_input.kb_mouse.mouse_wheel_y = 0.0f;
}

void ir_input_frame_end(void) {
    // Update previous states for next frame
    for (int i = 0; i < IR_KEY_COUNT; i++) {
        g_input.kb_mouse.keys[i].previous = g_input.kb_mouse.keys[i].current;
    }

    for (int i = 0; i < IR_MOUSE_BUTTON_COUNT; i++) {
        g_input.kb_mouse.mouse_buttons[i].previous = g_input.kb_mouse.mouse_buttons[i].current;
    }

    g_input.kb_mouse.mouse_prev_x = g_input.kb_mouse.mouse_x;
    g_input.kb_mouse.mouse_prev_y = g_input.kb_mouse.mouse_y;

    for (int pad = 0; pad < IR_MAX_GAMEPADS; pad++) {
        for (int i = 0; i < IR_GAMEPAD_BUTTON_COUNT; i++) {
            g_input.gamepads[pad].buttons[i].previous = g_input.gamepads[pad].buttons[i].current;
        }
    }
}

// ============================================================================
// Backend Event Feeding
// ============================================================================

void ir_input_key_event(IRKey key, bool is_down) {
    if (key >= 0 && key < IR_KEY_COUNT) {
        g_input.kb_mouse.keys[key].current = is_down;
    }
}

void ir_input_mouse_button_event(IRMouseButton button, bool is_down) {
    if (button > 0 && button < IR_MOUSE_BUTTON_COUNT) {
        g_input.kb_mouse.mouse_buttons[button].current = is_down;
    }
}

void ir_input_mouse_position_event(int32_t x, int32_t y) {
    g_input.kb_mouse.mouse_x = x;
    g_input.kb_mouse.mouse_y = y;
}

void ir_input_mouse_wheel_event(float delta_x, float delta_y) {
    g_input.kb_mouse.mouse_wheel_x += delta_x;
    g_input.kb_mouse.mouse_wheel_y += delta_y;
}

void ir_input_gamepad_connection_event(int gamepad_index, bool connected) {
    if (gamepad_index >= 0 && gamepad_index < IR_MAX_GAMEPADS) {
        g_input.gamepads[gamepad_index].connected = connected;
        if (!connected) {
            // Reset gamepad state on disconnect
            memset(&g_input.gamepads[gamepad_index], 0, sizeof(InputGamepadState));
        }
    }
}

void ir_input_gamepad_button_event(int gamepad_index, IRGamepadButton button, bool is_down) {
    if (gamepad_index >= 0 && gamepad_index < IR_MAX_GAMEPADS &&
        button >= 0 && button < IR_GAMEPAD_BUTTON_COUNT) {
        g_input.gamepads[gamepad_index].buttons[button].current = is_down;
    }
}

void ir_input_gamepad_axis_event(int gamepad_index, IRGamepadAxis axis, float value) {
    if (gamepad_index >= 0 && gamepad_index < IR_MAX_GAMEPADS &&
        axis >= 0 && axis < IR_GAMEPAD_AXIS_COUNT) {
        g_input.gamepads[gamepad_index].axes[axis] = value;
    }
}

// ============================================================================
// Keyboard Input Queries
// ============================================================================

bool ir_input_key_pressed(IRKey key) {
    if (key < 0 || key >= IR_KEY_COUNT) return false;
    return g_input.kb_mouse.keys[key].current && !g_input.kb_mouse.keys[key].previous;
}

bool ir_input_key_down(IRKey key) {
    if (key < 0 || key >= IR_KEY_COUNT) return false;
    return g_input.kb_mouse.keys[key].current;
}

bool ir_input_key_released(IRKey key) {
    if (key < 0 || key >= IR_KEY_COUNT) return false;
    return !g_input.kb_mouse.keys[key].current && g_input.kb_mouse.keys[key].previous;
}

// ============================================================================
// Mouse Input Queries
// ============================================================================

void ir_input_mouse_position(int32_t* x, int32_t* y) {
    if (x) *x = g_input.kb_mouse.mouse_x;
    if (y) *y = g_input.kb_mouse.mouse_y;
}

void ir_input_mouse_delta(int32_t* dx, int32_t* dy) {
    if (dx) *dx = g_input.kb_mouse.mouse_x - g_input.kb_mouse.mouse_prev_x;
    if (dy) *dy = g_input.kb_mouse.mouse_y - g_input.kb_mouse.mouse_prev_y;
}

bool ir_input_mouse_button_pressed(IRMouseButton button) {
    if (button <= 0 || button >= IR_MOUSE_BUTTON_COUNT) return false;
    return g_input.kb_mouse.mouse_buttons[button].current &&
           !g_input.kb_mouse.mouse_buttons[button].previous;
}

bool ir_input_mouse_button_down(IRMouseButton button) {
    if (button <= 0 || button >= IR_MOUSE_BUTTON_COUNT) return false;
    return g_input.kb_mouse.mouse_buttons[button].current;
}

bool ir_input_mouse_button_released(IRMouseButton button) {
    if (button <= 0 || button >= IR_MOUSE_BUTTON_COUNT) return false;
    return !g_input.kb_mouse.mouse_buttons[button].current &&
           g_input.kb_mouse.mouse_buttons[button].previous;
}

void ir_input_mouse_wheel_delta(float* dx, float* dy) {
    if (dx) *dx = g_input.kb_mouse.mouse_wheel_x;
    if (dy) *dy = g_input.kb_mouse.mouse_wheel_y;
}

// ============================================================================
// Gamepad Input Queries
// ============================================================================

bool ir_input_gamepad_connected(int gamepad_index) {
    if (gamepad_index < 0 || gamepad_index >= IR_MAX_GAMEPADS) return false;
    return g_input.gamepads[gamepad_index].connected;
}

bool ir_input_gamepad_button_pressed(int gamepad_index, IRGamepadButton button) {
    if (gamepad_index < 0 || gamepad_index >= IR_MAX_GAMEPADS) return false;
    if (button < 0 || button >= IR_GAMEPAD_BUTTON_COUNT) return false;
    if (!g_input.gamepads[gamepad_index].connected) return false;

    return g_input.gamepads[gamepad_index].buttons[button].current &&
           !g_input.gamepads[gamepad_index].buttons[button].previous;
}

bool ir_input_gamepad_button_down(int gamepad_index, IRGamepadButton button) {
    if (gamepad_index < 0 || gamepad_index >= IR_MAX_GAMEPADS) return false;
    if (button < 0 || button >= IR_GAMEPAD_BUTTON_COUNT) return false;
    if (!g_input.gamepads[gamepad_index].connected) return false;

    return g_input.gamepads[gamepad_index].buttons[button].current;
}

bool ir_input_gamepad_button_released(int gamepad_index, IRGamepadButton button) {
    if (gamepad_index < 0 || gamepad_index >= IR_MAX_GAMEPADS) return false;
    if (button < 0 || button >= IR_GAMEPAD_BUTTON_COUNT) return false;
    if (!g_input.gamepads[gamepad_index].connected) return false;

    return !g_input.gamepads[gamepad_index].buttons[button].current &&
           g_input.gamepads[gamepad_index].buttons[button].previous;
}

float ir_input_gamepad_axis(int gamepad_index, IRGamepadAxis axis) {
    if (gamepad_index < 0 || gamepad_index >= IR_MAX_GAMEPADS) return 0.0f;
    if (axis < 0 || axis >= IR_GAMEPAD_AXIS_COUNT) return 0.0f;
    if (!g_input.gamepads[gamepad_index].connected) return 0.0f;

    return g_input.gamepads[gamepad_index].axes[axis];
}

void ir_input_gamepad_left_stick(int gamepad_index, float* x, float* y) {
    if (x) *x = ir_input_gamepad_axis(gamepad_index, IR_GAMEPAD_AXIS_LEFT_X);
    if (y) *y = ir_input_gamepad_axis(gamepad_index, IR_GAMEPAD_AXIS_LEFT_Y);
}

void ir_input_gamepad_right_stick(int gamepad_index, float* x, float* y) {
    if (x) *x = ir_input_gamepad_axis(gamepad_index, IR_GAMEPAD_AXIS_RIGHT_X);
    if (y) *y = ir_input_gamepad_axis(gamepad_index, IR_GAMEPAD_AXIS_RIGHT_Y);
}

// ============================================================================
// Virtual Input (Action Mapping)
// ============================================================================

static InputAction* find_action(const char* name) {
    for (int i = 0; i < g_input.action_count; i++) {
        if (strcmp(g_input.actions[i].name, name) == 0) {
            return &g_input.actions[i];
        }
    }
    return NULL;
}

static InputAction* get_or_create_action(const char* name) {
    InputAction* action = find_action(name);
    if (action) return action;

    if (g_input.action_count >= IR_MAX_ACTIONS) {
        return NULL; // Too many actions
    }

    action = &g_input.actions[g_input.action_count++];
    strncpy(action->name, name, IR_MAX_ACTION_NAME - 1);
    action->name[IR_MAX_ACTION_NAME - 1] = '\0';
    action->binding_count = 0;
    return action;
}

void ir_input_map_key_to_action(const char* action, IRKey key) {
    InputAction* act = get_or_create_action(action);
    if (!act || act->binding_count >= IR_MAX_BINDINGS_PER_ACTION) return;

    InputBinding* binding = &act->bindings[act->binding_count++];
    binding->type = IR_INPUT_BINDING_KEY;
    binding->binding.key = key;
}

void ir_input_map_gamepad_button_to_action(const char* action, int gamepad_index, IRGamepadButton button) {
    InputAction* act = get_or_create_action(action);
    if (!act || act->binding_count >= IR_MAX_BINDINGS_PER_ACTION) return;

    InputBinding* binding = &act->bindings[act->binding_count++];
    binding->type = IR_INPUT_BINDING_GAMEPAD_BUTTON;
    binding->binding.gamepad.gamepad_index = gamepad_index;
    binding->binding.gamepad.button = button;
}

void ir_input_map_mouse_button_to_action(const char* action, IRMouseButton button) {
    InputAction* act = get_or_create_action(action);
    if (!act || act->binding_count >= IR_MAX_BINDINGS_PER_ACTION) return;

    InputBinding* binding = &act->bindings[act->binding_count++];
    binding->type = IR_INPUT_BINDING_MOUSE_BUTTON;
    binding->binding.mouse.button = button;
}

void ir_input_clear_action(const char* action) {
    for (int i = 0; i < g_input.action_count; i++) {
        if (strcmp(g_input.actions[i].name, action) == 0) {
            // Remove by shifting remaining actions
            memmove(&g_input.actions[i], &g_input.actions[i + 1],
                    (g_input.action_count - i - 1) * sizeof(InputAction));
            g_input.action_count--;
            return;
        }
    }
}

bool ir_input_action_pressed(const char* action) {
    InputAction* act = find_action(action);
    if (!act) return false;

    for (int i = 0; i < act->binding_count; i++) {
        InputBinding* binding = &act->bindings[i];
        switch (binding->type) {
            case IR_INPUT_BINDING_KEY:
                if (ir_input_key_pressed(binding->binding.key)) return true;
                break;
            case IR_INPUT_BINDING_MOUSE_BUTTON:
                if (ir_input_mouse_button_pressed(binding->binding.mouse.button)) return true;
                break;
            case IR_INPUT_BINDING_GAMEPAD_BUTTON:
                if (ir_input_gamepad_button_pressed(binding->binding.gamepad.gamepad_index,
                                                    binding->binding.gamepad.button)) return true;
                break;
        }
    }
    return false;
}

bool ir_input_action_down(const char* action) {
    InputAction* act = find_action(action);
    if (!act) return false;

    for (int i = 0; i < act->binding_count; i++) {
        InputBinding* binding = &act->bindings[i];
        switch (binding->type) {
            case IR_INPUT_BINDING_KEY:
                if (ir_input_key_down(binding->binding.key)) return true;
                break;
            case IR_INPUT_BINDING_MOUSE_BUTTON:
                if (ir_input_mouse_button_down(binding->binding.mouse.button)) return true;
                break;
            case IR_INPUT_BINDING_GAMEPAD_BUTTON:
                if (ir_input_gamepad_button_down(binding->binding.gamepad.gamepad_index,
                                                 binding->binding.gamepad.button)) return true;
                break;
        }
    }
    return false;
}

bool ir_input_action_released(const char* action) {
    InputAction* act = find_action(action);
    if (!act) return false;

    for (int i = 0; i < act->binding_count; i++) {
        InputBinding* binding = &act->bindings[i];
        switch (binding->type) {
            case IR_INPUT_BINDING_KEY:
                if (ir_input_key_released(binding->binding.key)) return true;
                break;
            case IR_INPUT_BINDING_MOUSE_BUTTON:
                if (ir_input_mouse_button_released(binding->binding.mouse.button)) return true;
                break;
            case IR_INPUT_BINDING_GAMEPAD_BUTTON:
                if (ir_input_gamepad_button_released(binding->binding.gamepad.gamepad_index,
                                                     binding->binding.gamepad.button)) return true;
                break;
        }
    }
    return false;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* ir_input_key_name(IRKey key) {
    switch (key) {
        case IR_KEY_A: return "A";
        case IR_KEY_B: return "B";
        case IR_KEY_C: return "C";
        case IR_KEY_D: return "D";
        case IR_KEY_E: return "E";
        case IR_KEY_F: return "F";
        case IR_KEY_G: return "G";
        case IR_KEY_H: return "H";
        case IR_KEY_I: return "I";
        case IR_KEY_J: return "J";
        case IR_KEY_K: return "K";
        case IR_KEY_L: return "L";
        case IR_KEY_M: return "M";
        case IR_KEY_N: return "N";
        case IR_KEY_O: return "O";
        case IR_KEY_P: return "P";
        case IR_KEY_Q: return "Q";
        case IR_KEY_R: return "R";
        case IR_KEY_S: return "S";
        case IR_KEY_T: return "T";
        case IR_KEY_U: return "U";
        case IR_KEY_V: return "V";
        case IR_KEY_W: return "W";
        case IR_KEY_X: return "X";
        case IR_KEY_Y: return "Y";
        case IR_KEY_Z: return "Z";
        case IR_KEY_SPACE: return "Space";
        case IR_KEY_RETURN: return "Enter";
        case IR_KEY_ESCAPE: return "Escape";
        case IR_KEY_UP: return "Up";
        case IR_KEY_DOWN: return "Down";
        case IR_KEY_LEFT: return "Left";
        case IR_KEY_RIGHT: return "Right";
        default: return "Unknown";
    }
}

const char* ir_input_gamepad_button_name(IRGamepadButton button) {
    switch (button) {
        case IR_GAMEPAD_A: return "A";
        case IR_GAMEPAD_B: return "B";
        case IR_GAMEPAD_X: return "X";
        case IR_GAMEPAD_Y: return "Y";
        case IR_GAMEPAD_START: return "Start";
        case IR_GAMEPAD_BACK: return "Back";
        case IR_GAMEPAD_DPAD_UP: return "D-Pad Up";
        case IR_GAMEPAD_DPAD_DOWN: return "D-Pad Down";
        case IR_GAMEPAD_DPAD_LEFT: return "D-Pad Left";
        case IR_GAMEPAD_DPAD_RIGHT: return "D-Pad Right";
        case IR_GAMEPAD_LEFT_SHOULDER: return "LB";
        case IR_GAMEPAD_RIGHT_SHOULDER: return "RB";
        default: return "Unknown";
    }
}

bool ir_input_any_key_pressed(void) {
    for (int i = 0; i < IR_KEY_COUNT; i++) {
        if (ir_input_key_pressed(i)) return true;
    }
    return false;
}

bool ir_input_any_gamepad_button_pressed(void) {
    for (int pad = 0; pad < IR_MAX_GAMEPADS; pad++) {
        if (!g_input.gamepads[pad].connected) continue;
        for (int btn = 0; btn < IR_GAMEPAD_BUTTON_COUNT; btn++) {
            if (ir_input_gamepad_button_pressed(pad, btn)) return true;
        }
    }
    return false;
}
