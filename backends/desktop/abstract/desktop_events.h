/**
 * Desktop Events - Platform-Agnostic Event Abstraction
 *
 * This header defines platform-agnostic event types and structures
 * that can be used across different desktop renderers (SDL3, raylib, GLFW, etc.)
 */

#ifndef DESKTOP_EVENTS_H
#define DESKTOP_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-agnostic event types
typedef enum {
    DESKTOP_EVENT_QUIT,
    DESKTOP_EVENT_MOUSE_CLICK,
    DESKTOP_EVENT_MOUSE_MOVE,
    DESKTOP_EVENT_KEY_PRESS,
    DESKTOP_EVENT_KEY_RELEASE,
    DESKTOP_EVENT_TEXT_INPUT,
    DESKTOP_EVENT_WINDOW_RESIZE,
    DESKTOP_EVENT_WINDOW_FOCUS,
    DESKTOP_EVENT_WINDOW_UNFOCUS
} DesktopEventType;

// Platform-agnostic event structure
typedef struct {
    DesktopEventType type;
    uint64_t timestamp;  // Milliseconds since start

    union {
        // Mouse event data
        struct {
            float x, y;
            uint8_t button;  // 1 = left, 2 = middle, 3 = right
            bool pressed;    // true = pressed, false = released
        } mouse;

        // Keyboard event data
        struct {
            uint32_t key_code;
            bool shift;
            bool ctrl;
            bool alt;
        } keyboard;

        // Text input event data
        struct {
            char text[32];
        } text_input;

        // Window resize event data
        struct {
            int width;
            int height;
        } resize;
    } data;
} DesktopEvent;

// Event callback function type
typedef void (*DesktopEventCallback)(DesktopEvent* event, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_EVENTS_H
