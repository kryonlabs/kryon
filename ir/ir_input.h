#ifndef IR_INPUT_H
#define IR_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Game Input System
 *
 * Provides a unified, backend-agnostic input API for games.
 * Features:
 * - Keyboard state tracking (pressed/held/released)
 * - Mouse button and position tracking
 * - Gamepad support (buttons and analog sticks)
 * - Virtual input mapping (map multiple keys to actions)
 * - Frame-accurate input buffering
 *
 * Usage:
 *   // In game loop initialization:
 *   ir_input_init();
 *
 *   // At start of each frame:
 *   ir_input_frame_begin();
 *
 *   // Backend feeds events:
 *   ir_input_key_event(IR_KEY_SPACE, true);  // Key down
 *
 *   // In game code:
 *   if (ir_input_key_pressed(IR_KEY_SPACE)) {
 *       player_jump();
 *   }
 *
 *   // At end of frame:
 *   ir_input_frame_end();
 */

// ============================================================================
// Key Codes (SDL scancode compatible)
// ============================================================================

typedef enum {
    IR_KEY_UNKNOWN = 0,

    // Letters
    IR_KEY_A = 4,
    IR_KEY_B = 5,
    IR_KEY_C = 6,
    IR_KEY_D = 7,
    IR_KEY_E = 8,
    IR_KEY_F = 9,
    IR_KEY_G = 10,
    IR_KEY_H = 11,
    IR_KEY_I = 12,
    IR_KEY_J = 13,
    IR_KEY_K = 14,
    IR_KEY_L = 15,
    IR_KEY_M = 16,
    IR_KEY_N = 17,
    IR_KEY_O = 18,
    IR_KEY_P = 19,
    IR_KEY_Q = 20,
    IR_KEY_R = 21,
    IR_KEY_S = 22,
    IR_KEY_T = 23,
    IR_KEY_U = 24,
    IR_KEY_V = 25,
    IR_KEY_W = 26,
    IR_KEY_X = 27,
    IR_KEY_Y = 28,
    IR_KEY_Z = 29,

    // Numbers
    IR_KEY_1 = 30,
    IR_KEY_2 = 31,
    IR_KEY_3 = 32,
    IR_KEY_4 = 33,
    IR_KEY_5 = 34,
    IR_KEY_6 = 35,
    IR_KEY_7 = 36,
    IR_KEY_8 = 37,
    IR_KEY_9 = 38,
    IR_KEY_0 = 39,

    // Function keys
    IR_KEY_RETURN = 40,
    IR_KEY_ESCAPE = 41,
    IR_KEY_BACKSPACE = 42,
    IR_KEY_TAB = 43,
    IR_KEY_SPACE = 44,

    // Arrow keys
    IR_KEY_RIGHT = 79,
    IR_KEY_LEFT = 80,
    IR_KEY_DOWN = 81,
    IR_KEY_UP = 82,

    // Modifiers
    IR_KEY_LCTRL = 224,
    IR_KEY_LSHIFT = 225,
    IR_KEY_LALT = 226,
    IR_KEY_RCTRL = 228,
    IR_KEY_RSHIFT = 229,
    IR_KEY_RALT = 230,

    IR_KEY_COUNT = 512  // Max key codes
} IRKey;

// Mouse buttons
typedef enum {
    IR_MOUSE_LEFT = 1,
    IR_MOUSE_MIDDLE = 2,
    IR_MOUSE_RIGHT = 3,
    IR_MOUSE_X1 = 4,
    IR_MOUSE_X2 = 5,
    IR_MOUSE_BUTTON_COUNT = 8
} IRMouseButton;

// Gamepad buttons (Xbox layout)
typedef enum {
    IR_GAMEPAD_A = 0,
    IR_GAMEPAD_B = 1,
    IR_GAMEPAD_X = 2,
    IR_GAMEPAD_Y = 3,
    IR_GAMEPAD_BACK = 4,
    IR_GAMEPAD_GUIDE = 5,
    IR_GAMEPAD_START = 6,
    IR_GAMEPAD_LEFT_STICK = 7,
    IR_GAMEPAD_RIGHT_STICK = 8,
    IR_GAMEPAD_LEFT_SHOULDER = 9,
    IR_GAMEPAD_RIGHT_SHOULDER = 10,
    IR_GAMEPAD_DPAD_UP = 11,
    IR_GAMEPAD_DPAD_DOWN = 12,
    IR_GAMEPAD_DPAD_LEFT = 13,
    IR_GAMEPAD_DPAD_RIGHT = 14,
    IR_GAMEPAD_BUTTON_COUNT = 15
} IRGamepadButton;

// Gamepad axes
typedef enum {
    IR_GAMEPAD_AXIS_LEFT_X = 0,
    IR_GAMEPAD_AXIS_LEFT_Y = 1,
    IR_GAMEPAD_AXIS_RIGHT_X = 2,
    IR_GAMEPAD_AXIS_RIGHT_Y = 3,
    IR_GAMEPAD_AXIS_TRIGGER_LEFT = 4,
    IR_GAMEPAD_AXIS_TRIGGER_RIGHT = 5,
    IR_GAMEPAD_AXIS_COUNT = 6
} IRGamepadAxis;

// ============================================================================
// Initialization & Frame Management
// ============================================================================

/**
 * Initialize input system
 * Call once at startup
 */
void ir_input_init(void);

/**
 * Shutdown input system
 * Call once at cleanup
 */
void ir_input_shutdown(void);

/**
 * Begin new input frame
 * Call at start of each frame to update pressed/released states
 */
void ir_input_frame_begin(void);

/**
 * End input frame
 * Call at end of each frame
 */
void ir_input_frame_end(void);

// ============================================================================
// Backend Event Feeding (called by renderer backends)
// ============================================================================

/**
 * Feed keyboard event from backend
 */
void ir_input_key_event(IRKey key, bool is_down);

/**
 * Feed mouse button event from backend
 */
void ir_input_mouse_button_event(IRMouseButton button, bool is_down);

/**
 * Feed mouse position event from backend
 */
void ir_input_mouse_position_event(int32_t x, int32_t y);

/**
 * Feed mouse wheel event from backend
 */
void ir_input_mouse_wheel_event(float delta_x, float delta_y);

/**
 * Feed gamepad connection event from backend
 */
void ir_input_gamepad_connection_event(int gamepad_index, bool connected);

/**
 * Feed gamepad button event from backend
 */
void ir_input_gamepad_button_event(int gamepad_index, IRGamepadButton button, bool is_down);

/**
 * Feed gamepad axis event from backend
 */
void ir_input_gamepad_axis_event(int gamepad_index, IRGamepadAxis axis, float value);

// ============================================================================
// Keyboard Input Queries
// ============================================================================

/**
 * Check if key was just pressed this frame
 * Returns true only on the first frame the key is down
 */
bool ir_input_key_pressed(IRKey key);

/**
 * Check if key is currently held down
 * Returns true every frame while key is down
 */
bool ir_input_key_down(IRKey key);

/**
 * Check if key was just released this frame
 * Returns true only on the first frame the key is up after being down
 */
bool ir_input_key_released(IRKey key);

// ============================================================================
// Mouse Input Queries
// ============================================================================

/**
 * Get current mouse position
 */
void ir_input_mouse_position(int32_t* x, int32_t* y);

/**
 * Get mouse delta (movement since last frame)
 */
void ir_input_mouse_delta(int32_t* dx, int32_t* dy);

/**
 * Check if mouse button was just pressed this frame
 */
bool ir_input_mouse_button_pressed(IRMouseButton button);

/**
 * Check if mouse button is currently held down
 */
bool ir_input_mouse_button_down(IRMouseButton button);

/**
 * Check if mouse button was just released this frame
 */
bool ir_input_mouse_button_released(IRMouseButton button);

/**
 * Get mouse wheel delta (scrolling since last frame)
 */
void ir_input_mouse_wheel_delta(float* dx, float* dy);

// ============================================================================
// Gamepad Input Queries
// ============================================================================

/**
 * Check if gamepad is connected
 */
bool ir_input_gamepad_connected(int gamepad_index);

/**
 * Check if gamepad button was just pressed this frame
 */
bool ir_input_gamepad_button_pressed(int gamepad_index, IRGamepadButton button);

/**
 * Check if gamepad button is currently held down
 */
bool ir_input_gamepad_button_down(int gamepad_index, IRGamepadButton button);

/**
 * Check if gamepad button was just released this frame
 */
bool ir_input_gamepad_button_released(int gamepad_index, IRGamepadButton button);

/**
 * Get gamepad axis value [-1.0, 1.0]
 */
float ir_input_gamepad_axis(int gamepad_index, IRGamepadAxis axis);

/**
 * Get gamepad left stick values
 */
void ir_input_gamepad_left_stick(int gamepad_index, float* x, float* y);

/**
 * Get gamepad right stick values
 */
void ir_input_gamepad_right_stick(int gamepad_index, float* x, float* y);

// ============================================================================
// Virtual Input (Action Mapping)
// ============================================================================

#define IR_MAX_ACTION_NAME 32
#define IR_MAX_ACTIONS 64
#define IR_MAX_BINDINGS_PER_ACTION 4

/**
 * Map a key to an action name
 * Example: ir_input_map_key_to_action("jump", IR_KEY_SPACE)
 */
void ir_input_map_key_to_action(const char* action, IRKey key);

/**
 * Map a gamepad button to an action name
 */
void ir_input_map_gamepad_button_to_action(const char* action, int gamepad_index, IRGamepadButton button);

/**
 * Map a mouse button to an action name
 */
void ir_input_map_mouse_button_to_action(const char* action, IRMouseButton button);

/**
 * Clear all mappings for an action
 */
void ir_input_clear_action(const char* action);

/**
 * Check if action was just triggered this frame
 * Returns true if any mapped input was pressed
 */
bool ir_input_action_pressed(const char* action);

/**
 * Check if action is currently active
 * Returns true if any mapped input is down
 */
bool ir_input_action_down(const char* action);

/**
 * Check if action was just released this frame
 */
bool ir_input_action_released(const char* action);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get name of key as string
 */
const char* ir_input_key_name(IRKey key);

/**
 * Get name of gamepad button as string
 */
const char* ir_input_gamepad_button_name(IRGamepadButton button);

/**
 * Check if any key is pressed
 */
bool ir_input_any_key_pressed(void);

/**
 * Check if any gamepad button is pressed (any gamepad)
 */
bool ir_input_any_gamepad_button_pressed(void);

#ifdef __cplusplus
}
#endif

#endif // IR_INPUT_H
