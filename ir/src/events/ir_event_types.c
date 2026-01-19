/**
 * @file ir_event_types.c
 * @brief Event type definitions and conversion functions
 */

#include "../include/ir_event_types.h"

const char* ir_event_type_to_string(IREventType type) {
    switch (type) {
        case IR_EVENT_CLICK: return "Click";
        case IR_EVENT_HOVER: return "Hover";
        case IR_EVENT_FOCUS: return "Focus";
        case IR_EVENT_BLUR: return "Blur";
        case IR_EVENT_TEXT_CHANGE: return "TextChange";
        case IR_EVENT_KEY: return "Key";
        case IR_EVENT_SCROLL: return "Scroll";
        case IR_EVENT_TIMER: return "Timer";
        case IR_EVENT_CUSTOM: return "Custom";
        default:
            if (type >= IR_EVENT_PLUGIN_START && type <= IR_EVENT_PLUGIN_END) {
                return "Plugin";
            }
            return "Unknown";
    }
}
