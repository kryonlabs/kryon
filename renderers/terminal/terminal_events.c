/**
 * Kryon Terminal Renderer - Event Handling
 *
 * Handles terminal input events and converts them to Kryon event system.
 * Supports keyboard, mouse, and resize events with proper event
 * translation and management.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <tickit.h>

#include "../../core/include/kryon.h"

#include "terminal_backend.h"

// ============================================================================
// Event State Management
// ============================================================================

typedef struct {
    // Tickit event binding
    struct tickit* tickit;
    struct tickit_window* root;

    // Event queue
    kryon_event_t event_queue[32];
    size_t event_queue_head;
    size_t event_queue_tail;
    size_t event_queue_count;

    // Mouse state
    struct {
        bool enabled;
        int last_x;
        int last_y;
        kryon_event_type_t last_button;
        bool drag_active;
    } mouse;

    // Keyboard state
    struct {
        bool ctrl_pressed;
        bool shift_pressed;
        bool alt_pressed;
        char last_key[8];
    } keyboard;

    // Resize state
    struct {
        bool pending;
        uint16_t new_width;
        uint16_t new_height;
    } resize;

} kryon_terminal_event_state_t;

// ============================================================================
// Event Queue Management
// ============================================================================

static bool queue_event(kryon_terminal_event_state_t* event_state, const kryon_event_t* event) {
    if (event_state->event_queue_count >= 32) {
        return false; // Queue full
    }

    event_state->event_queue[event_state->event_queue_tail] = *event;
    event_state->event_queue_tail = (event_state->event_queue_tail + 1) % 32;
    event_state->event_queue_count++;
    return true;
}

static bool dequeue_event(kryon_terminal_event_state_t* event_state, kryon_event_t* event) {
    if (event_state->event_queue_count == 0) {
        return false; // Queue empty
    }

    *event = event_state->event_queue[event_state->event_queue_head];
    event_state->event_queue_head = (event_state->event_queue_head + 1) % 32;
    event_state->event_queue_count--;
    return true;
}

// ============================================================================
// Event Translation Functions
// ============================================================================

// Convert Tickit key event to Kryon key code
static uint32_t translate_key_code(TickitKeyEventInfo* key_info) {
    switch (key_info->type) {
        case TICKIT_KEYEV_TEXT:
            return (unsigned char)key_info->str[0]; // ASCII character

        case TICKIT_KEYEV_KEY:
            switch (key_info->key) {
                case TICKIT_KEY_ESCAPE:    return 27;
                case TICKIT_KEY_ENTER:     return 13;
                case TICKIT_KEY_TAB:       return 9;
                case TICKIT_KEY_BACKSPACE: return 8;
                case TICKIT_KEY_DELETE:    return 127;
                case TICKIT_KEY_UP:        return 256 + 'A';
                case TICKIT_KEY_DOWN:      return 256 + 'B';
                case TICKIT_KEY_LEFT:      return 256 + 'D';
                case TICKIT_KEY_RIGHT:     return 256 + 'C';
                case TICKIT_KEY_HOME:      return 256 + 'H';
                case TICKIT_KEY_END:       return 256 + 'F';
                case TICKIT_KEY_PAGEUP:    return 256 + 'V';
                case TICKIT_KEY_PAGEDOWN:  return 256 + 'U';
                default: return key_info->key;
            }

        default:
            return 0;
    }
}

// Convert Tickit mouse event to Kryon mouse event
static kryon_event_type_t translate_mouse_event(TickitMouseEventInfo* mouse_event) {
    switch (mouse_event->type) {
        case TICKIT_MOUSEEV_PRESS:
            switch (mouse_event->button) {
                case TICKIT_MOUSEEV_LEFT:   return KRYON_EVT_CLICK;
                case TICKIT_MOUSEEV_MIDDLE: return KRYON_EVT_CLICK;
                case TICKIT_MOUSEEV_RIGHT:  return KRYON_EVT_CLICK;
                default: return KRYON_EVT_CLICK;
            }

        case TICKIT_MOUSEEV_DRAG:
            return KRYON_EVT_TOUCH; // Use touch event for drag

        case TICKIT_MOUSEEV_RELEASE:
            return KRYON_EVT_CLICK; // Release still counts as click

        case TICKIT_MOUSEEV_WHEEL:
            return KRYON_EVT_TOUCH; // Use touch event for wheel

        default:
            return KRYON_EVT_CLICK;
    }
}

// ============================================================================
// Tickit Event Handlers
// ============================================================================

// Handle keyboard events
static int handle_key_event(Tickit* tickit, TickitEventType evtype, void* info, void* user_data) {
    (void)tickit; (void)evtype; // Unused parameters
    kryon_terminal_event_state_t* event_state = (kryon_terminal_event_state_t*)user_data;
    TickitKeyEventInfo* key_info = (TickitKeyEventInfo*)info;

    kryon_event_t event = {0};
    event.type = KRYON_EVT_KEY;
    event.x = 0; // Keyboard events don't have coordinates
    event.y = 0;
    event.param = translate_key_code(key_info);
    event.timestamp = (uint32_t)time(NULL);

    // Store key text for debugging
    if (key_info->type == TICKIT_KEYEV_TEXT && key_info->str) {
        strncpy((char*)&event.data, key_info->str, sizeof(event.data) - 1);
    }

    queue_event(event_state, &event);
    return 1; // Event handled
}

// Handle mouse events
static int handle_mouse_event(Tickit* tickit, TickitEventType evtype, void* info, void* user_data) {
    (void)tickit; (void)evtype; // Unused parameters
    kryon_terminal_event_state_t* event_state = (kryon_terminal_event_state_t*)user_data;
    TickitMouseEventInfo* mouse_event = (TickitMouseEventInfo*)info;

    kryon_event_t event = {0};
    event.type = translate_mouse_event(mouse_event);
    event.x = mouse_event->col;
    event.y = mouse_event->line;
    event.param = mouse_event->button; // Store button in param
    event.timestamp = (uint32_t)time(NULL);

    // Update mouse state
    event_state->mouse.last_x = mouse_event->col;
    event_state->mouse.last_y = mouse_event->line;
    event_state->mouse.last_button = event.type;

    if (mouse_event->type == TICKIT_MOUSEEV_DRAG) {
        event_state->mouse.drag_active = true;
    } else if (mouse_event->type == TICKIT_MOUSEEV_RELEASE) {
        event_state->mouse.drag_active = false;
    }

    queue_event(event_state, &event);
    return 1; // Event handled
}

// Handle window resize events
static int handle_resize_event(Tickit* tickit, TickitEventType evtype, void* info, void* user_data) {
    (void)tickit; (void)evtype; // Unused parameters
    kryon_terminal_event_state_t* event_state = (kryon_terminal_event_state_t*)user_data;
    TickitResizeInfo* resize_info = (TickitResizeInfo*)info;

    // Store resize information
    event_state->resize.pending = true;
    event_state->resize.new_width = resize_info->col;
    event_state->resize.new_height = resize_info->line;

    // Create a custom resize event
    kryon_event_t event = {0};
    event.type = KRYON_EVT_CUSTOM;
    event.x = resize_info->col;
    event.y = resize_info->line;
    event.param = 0; // Resize event code
    event.timestamp = (uint32_t)time(NULL);
    event.data = "resize";

    queue_event(event_state, &event);
    return 1; // Event handled
}

// ============================================================================
// Public Event API
// ============================================================================

// Initialize event handling system
kryon_terminal_event_state_t* kryon_terminal_events_init(Tickit* tickit, TickitWindow* root) {
    kryon_terminal_event_state_t* event_state = malloc(sizeof(kryon_terminal_event_state_t));
    if (!event_state) return NULL;

    memset(event_state, 0, sizeof(kryon_terminal_event_state_t));

    event_state->tickit = tickit;
    event_state->root = root;

    // Enable mouse support
    tickit_term_setctl_int(tickit_get_term(tickit), TICKIT_TERMCTL_MOUSE, TICKIT_MOUSEEV_DRAG);
    event_state->mouse.enabled = true;

    // Bind event handlers
    tickit_bind_event(tickit, TICKIT_EV_KEY, handle_key_event, event_state);
    tickit_bind_event(tickit, TICKIT_EV_MOUSE, handle_mouse_event, event_state);
    tickit_bind_event(tickit, TICKIT_EV_RESIZE, handle_resize_event, event_state);

    return event_state;
}

// Cleanup event handling system
void kryon_terminal_events_shutdown(kryon_terminal_event_state_t* event_state) {
    if (!event_state) return;

    // Unbind events (Tickit doesn't provide explicit unbind, so this is handled by shutdown)

    free(event_state);
}

// Poll for events (non-blocking)
bool kryon_terminal_events_poll(kryon_terminal_event_state_t* event_state, kryon_event_t* event) {
    if (!event_state || !event) return false;

    // Process tickit events first
    tickit_tick(event_state->tickit, TICKIT_RUN_NOHANG);

    // Then check our event queue
    return dequeue_event(event_state, event);
}

// Wait for events (blocking)
bool kryon_terminal_events_wait(kryon_terminal_event_state_t* event_state, kryon_event_t* event, int timeout_ms) {
    if (!event_state || !event) return false;

    // Process tickit events with timeout
    tickit_tick(event_state->tickit, timeout_ms > 0 ? (timeout_ms + 999) / 1000 : TICKIT_RUN_ONCE);

    // Check if we got any events
    return dequeue_event(event_state, event);
}

// Check if resize is pending
bool kryon_terminal_events_has_resize(kryon_terminal_event_state_t* event_state, uint16_t* width, uint16_t* height) {
    if (!event_state || !event_state->resize.pending) return false;

    if (width) *width = event_state->resize.new_width;
    if (height) *height = event_state->resize.new_height;

    return true;
}

// Clear resize pending flag
void kryon_terminal_events_clear_resize(kryon_terminal_event_state_t* event_state) {
    if (event_state) {
        event_state->resize.pending = false;
    }
}

// Get current mouse position
void kryon_terminal_events_get_mouse_position(kryon_terminal_event_state_t* event_state, int* x, int* y) {
    if (event_state) {
        if (x) *x = event_state->mouse.last_x;
        if (y) *y = event_state->mouse.last_y;
    }
}

// Check if mouse drag is active
bool kryon_terminal_events_is_drag_active(kryon_terminal_event_state_t* event_state) {
    return event_state ? event_state->mouse.drag_active : false;
}

// Enable/disable mouse support
void kryon_terminal_events_set_mouse_enabled(kryon_terminal_event_state_t* event_state, bool enabled) {
    if (!event_state) return;

    event_state->mouse.enabled = enabled;
    if (enabled) {
        tickit_term_setctl_int(tickit_get_term(event_state->tickit), TICKIT_TERMCTL_MOUSE, TICKIT_MOUSEEV_DRAG);
    } else {
        tickit_term_setctl_int(tickit_get_term(event_state->tickit), TICKIT_TERMCTL_MOUSE, 0);
    }
}

// Get event queue statistics
void kryon_terminal_events_get_stats(kryon_terminal_event_state_t* event_state, size_t* queued, size_t* capacity) {
    if (!event_state) return;

    if (queued) *queued = event_state->event_queue_count;
    if (capacity) *capacity = 32; // Fixed queue size
}