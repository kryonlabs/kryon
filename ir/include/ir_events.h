#ifndef IR_EVENT_TYPES_H
#define IR_EVENT_TYPES_H

#include <stdint.h>

// Event Types
typedef enum {
    // Core events (0-99) - hardcoded for performance
    IR_EVENT_CLICK = 0,
    IR_EVENT_HOVER = 1,
    IR_EVENT_FOCUS = 2,
    IR_EVENT_BLUR = 3,
    IR_EVENT_TEXT_CHANGE = 4,  // Text input change event (for Input components)
    IR_EVENT_KEY = 5,
    IR_EVENT_SCROLL = 6,
    IR_EVENT_TIMER = 7,
    IR_EVENT_CUSTOM = 8,

    // Plugin event range (100-255) - dynamically registered
    IR_EVENT_PLUGIN_START = 100,
    IR_EVENT_PLUGIN_END = 255
} IREventType;

// Convert event type to string
const char* ir_event_type_to_string(IREventType type);

#endif // IR_EVENT_TYPES_H
