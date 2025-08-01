/**
 * @file events.h
 * @brief Kryon Event System
 * 
 * Unified event handling for all platforms and renderers
 */

#ifndef KRYON_INTERNAL_EVENTS_H
#define KRYON_INTERNAL_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// EVENT TYPES
// =============================================================================

typedef enum {
    // Window events
    KRYON_EVENT_WINDOW_CLOSE,
    KRYON_EVENT_WINDOW_RESIZE,
    KRYON_EVENT_WINDOW_FOCUS,
    KRYON_EVENT_WINDOW_LOST_FOCUS,
    KRYON_EVENT_WINDOW_MOVED,
    
    // Mouse events
    KRYON_EVENT_MOUSE_MOVED,
    KRYON_EVENT_MOUSE_BUTTON_DOWN,
    KRYON_EVENT_MOUSE_BUTTON_UP,
    KRYON_EVENT_MOUSE_WHEEL,
    KRYON_EVENT_MOUSE_ENTER,
    KRYON_EVENT_MOUSE_LEAVE,
    
    // Keyboard events
    KRYON_EVENT_KEY_DOWN,
    KRYON_EVENT_KEY_UP,
    KRYON_EVENT_TEXT_INPUT,
    
    // Touch events
    KRYON_EVENT_TOUCH_BEGIN,
    KRYON_EVENT_TOUCH_MOVE,
    KRYON_EVENT_TOUCH_END,
    KRYON_EVENT_TOUCH_CANCEL,
    
    // Widget events
    KRYON_EVENT_WIDGET_CLICKED,
    KRYON_EVENT_WIDGET_VALUE_CHANGED,
    KRYON_EVENT_WIDGET_FOCUS_GAINED,
    KRYON_EVENT_WIDGET_FOCUS_LOST,
    KRYON_EVENT_WIDGET_HOVERED,
    KRYON_EVENT_WIDGET_UNHOVERED,
    
    // Custom events
    KRYON_EVENT_CUSTOM
} KryonEventType;

// =============================================================================
// EVENT DATA STRUCTURES
// =============================================================================

typedef struct {
    int width;
    int height;
} KryonWindowResizeEvent;

typedef struct {
    int x;
    int y;
} KryonWindowMoveEvent;

typedef struct {
    float x;
    float y;
    float deltaX;
    float deltaY;
} KryonMouseMoveEvent;

typedef struct {
    int button; // 0=left, 1=right, 2=middle
    float x;
    float y;
    int clickCount; // For double/triple clicks
} KryonMouseButtonEvent;

typedef struct {
    float deltaX;
    float deltaY;
} KryonMouseWheelEvent;

typedef struct {
    int keyCode;
    int scanCode;
    bool isRepeat;
    bool shiftPressed;
    bool ctrlPressed;
    bool altPressed;
    bool metaPressed;
} KryonKeyEvent;

typedef struct {
    char text[32]; // UTF-8 encoded
    size_t length;
} KryonTextInputEvent;

typedef struct {
    int touchId;
    float x;
    float y;
    float pressure;
} KryonTouchEvent;

typedef struct {
    char* widgetId;
    char* widgetType;
    union {
        struct {
            bool pressed;
        } button;
        struct {
            char* text;
            int cursorPos;
        } input;
        struct {
            int selectedIndex;
            char* selectedValue;
        } dropdown;
        struct {
            bool checked;
        } checkbox;
        struct {
            float value;
        } slider;
    } data;
} KryonWidgetEvent;

typedef struct {
    uint32_t eventId;
    void* userData;
} KryonCustomEvent;

// =============================================================================
// EVENT STRUCTURE
// =============================================================================

typedef struct {
    KryonEventType type;
    uint64_t timestamp; // Milliseconds since app start
    bool handled;       // Set to true to stop propagation
    
    union {
        KryonWindowResizeEvent windowResize;
        KryonWindowMoveEvent windowMove;
        KryonMouseMoveEvent mouseMove;
        KryonMouseButtonEvent mouseButton;
        KryonMouseWheelEvent mouseWheel;
        KryonKeyEvent key;
        KryonTextInputEvent textInput;
        KryonTouchEvent touch;
        KryonWidgetEvent widget;
        KryonCustomEvent custom;
    } data;
} KryonEvent;

// =============================================================================
// EVENT HANDLER
// =============================================================================

typedef bool (*KryonEventHandler)(const KryonEvent* event, void* userData);

typedef struct KryonEventListener {
    KryonEventType eventType;
    KryonEventHandler handler;
    void* userData;
    struct KryonEventListener* next;
} KryonEventListener;

typedef struct {
    KryonEventListener* listeners;
    KryonEvent* eventQueue;
    size_t queueCapacity;
    size_t queueSize;
    size_t queueHead;
    size_t queueTail;
} KryonEventSystem;

// =============================================================================
// EVENT SYSTEM API
// =============================================================================

/**
 * Initialize the event system
 */
KryonEventSystem* kryon_event_system_create(size_t queueCapacity);

/**
 * Destroy the event system
 */
void kryon_event_system_destroy(KryonEventSystem* system);

/**
 * Add an event listener
 */
void kryon_event_add_listener(KryonEventSystem* system,
                              KryonEventType type,
                              KryonEventHandler handler,
                              void* userData);

/**
 * Remove an event listener
 */
void kryon_event_remove_listener(KryonEventSystem* system,
                                 KryonEventType type,
                                 KryonEventHandler handler);

/**
 * Push an event to the queue
 */
bool kryon_event_push(KryonEventSystem* system, const KryonEvent* event);

/**
 * Process all queued events
 */
void kryon_event_process_all(KryonEventSystem* system);

/**
 * Poll a single event (non-blocking)
 */
bool kryon_event_poll(KryonEventSystem* system, KryonEvent* event);

/**
 * Wait for an event (blocking)
 */
bool kryon_event_wait(KryonEventSystem* system, KryonEvent* event, uint32_t timeoutMs);

// =============================================================================
// GLOBAL EVENT REGISTRATION (for @event directive)
// =============================================================================

/**
 * Register a global keyboard shortcut handler
 */
bool kryon_event_register_keyboard_handler(KryonEventSystem* system,
                                           const char* shortcut,
                                           KryonEventHandler handler,
                                           void* userData);

/**
 * Register a global mouse event handler
 */
bool kryon_event_register_mouse_handler(KryonEventSystem* system,
                                        const char* mouseEvent,
                                        KryonEventHandler handler,
                                        void* userData);

/**
 * Parse a keyboard shortcut string (e.g., "Ctrl+I") into key event parameters
 */
bool kryon_event_parse_keyboard_shortcut(const char* shortcut,
                                         int* keyCode,
                                         bool* ctrl,
                                         bool* shift,
                                         bool* alt,
                                         bool* meta);

/**
 * Check if a key event matches a shortcut pattern
 */
bool kryon_event_matches_shortcut(const KryonKeyEvent* keyEvent,
                                  const char* shortcut);

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Create a window resize event
 */
static inline KryonEvent kryon_event_window_resize(int width, int height) {
    KryonEvent event = {0};
    event.type = KRYON_EVENT_WINDOW_RESIZE;
    event.data.windowResize.width = width;
    event.data.windowResize.height = height;
    return event;
}

/**
 * Create a mouse move event
 */
static inline KryonEvent kryon_event_mouse_move(float x, float y, float dx, float dy) {
    KryonEvent event = {0};
    event.type = KRYON_EVENT_MOUSE_MOVED;
    event.data.mouseMove.x = x;
    event.data.mouseMove.y = y;
    event.data.mouseMove.deltaX = dx;
    event.data.mouseMove.deltaY = dy;
    return event;
}

/**
 * Create a key down event
 */
static inline KryonEvent kryon_event_key_down(int keyCode, bool shift, bool ctrl, bool alt) {
    KryonEvent event = {0};
    event.type = KRYON_EVENT_KEY_DOWN;
    event.data.key.keyCode = keyCode;
    event.data.key.shiftPressed = shift;
    event.data.key.ctrlPressed = ctrl;
    event.data.key.altPressed = alt;
    return event;
}

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_EVENTS_H