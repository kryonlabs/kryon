/**

 * @file event_serializer.c
 * @brief Event handler serialization for KRB format
 * 
 * Handles serialization of @event directives and event handlers to the KRB format.
 * Implements Phase 4 event handler section.
 */
#include "lib9.h"


#include "codegen.h"
#include "memory.h"
#include "krb_format.h"
#include "binary_io.h"
#include "events.h"
#include "krb_sections.h"
#include "string_table.h"

/**
 * @brief Parse event type string to enum
 * @param event_name Event name string (e.g., "click", "change")
 * @return Event type enum value
 */
static KryonEventType parse_event_type(const char *event_name) {
    if (!event_name) return KRYON_EVENT_CUSTOM;
    
    // Map high-level event names to existing event types
    
    // Mouse events
    if (strcmp(event_name, "click") == 0) return KRYON_EVENT_ELEMENT_CLICKED;
    if (strcmp(event_name, "mousedown") == 0) return KRYON_EVENT_MOUSE_BUTTON_DOWN;
    if (strcmp(event_name, "mouseup") == 0) return KRYON_EVENT_MOUSE_BUTTON_UP;
    if (strcmp(event_name, "mouseenter") == 0) return KRYON_EVENT_MOUSE_ENTER;
    if (strcmp(event_name, "mouseleave") == 0) return KRYON_EVENT_MOUSE_LEAVE;
    if (strcmp(event_name, "mousemove") == 0) return KRYON_EVENT_MOUSE_MOVED;
    if (strcmp(event_name, "hover") == 0) return KRYON_EVENT_ELEMENT_HOVERED;
    if (strcmp(event_name, "unhover") == 0) return KRYON_EVENT_ELEMENT_UNHOVERED;
    
    // Keyboard events
    if (strcmp(event_name, "keydown") == 0) return KRYON_EVENT_KEY_DOWN;
    if (strcmp(event_name, "keyup") == 0) return KRYON_EVENT_KEY_UP;
    if (strcmp(event_name, "textinput") == 0) return KRYON_EVENT_TEXT_INPUT;
    
    // Focus events
    if (strcmp(event_name, "focus") == 0) return KRYON_EVENT_ELEMENT_FOCUS_GAINED;
    if (strcmp(event_name, "blur") == 0) return KRYON_EVENT_ELEMENT_FOCUS_LOST;
    
    // Widget events
    if (strcmp(event_name, "change") == 0) return KRYON_EVENT_ELEMENT_VALUE_CHANGED;
    if (strcmp(event_name, "input") == 0) return KRYON_EVENT_ELEMENT_VALUE_CHANGED;
    
    // Touch events
    if (strcmp(event_name, "touchstart") == 0) return KRYON_EVENT_TOUCH_BEGIN;
    if (strcmp(event_name, "touchmove") == 0) return KRYON_EVENT_TOUCH_MOVE;
    if (strcmp(event_name, "touchend") == 0) return KRYON_EVENT_TOUCH_END;
    if (strcmp(event_name, "touchcancel") == 0) return KRYON_EVENT_TOUCH_CANCEL;
    
    // Any other event is custom
    return KRYON_EVENT_CUSTOM;
}

/**
 * @brief Write event handler to KRB format
 * @param codegen Code generator instance
 * @param event Event directive node
 * @return true on success, false on failure
 */
bool kryon_write_event_handler(KryonCodeGenerator *codegen, const KryonASTNode *event) {
    if (!event || event->type != KRYON_AST_EVENT_DIRECTIVE) {
        return false;
    }
    
    // Event directives use the element structure to store their properties
    // The event type and handler are stored as properties
    const char *event_name = NULL;
    const char *handler_name = NULL;
    
    // Extract event type and handler from properties
    for (size_t i = 0; i < event->data.element.property_count; i++) {
        const KryonASTNode *prop = event->data.element.properties[i];
        if (prop && prop->type == KRYON_AST_PROPERTY) {
            if (strcmp(prop->data.property.name, "type") == 0 && 
                prop->data.property.value && 
                prop->data.property.value->type == KRYON_AST_LITERAL) {
                event_name = prop->data.property.value->data.literal.value.data.string_value;
            } else if (strcmp(prop->data.property.name, "handler") == 0 &&
                       prop->data.property.value &&
                       prop->data.property.value->type == KRYON_AST_LITERAL) {
                handler_name = prop->data.property.value->data.literal.value.data.string_value;
            }
        }
    }
    
    // Default to "click" if no event type specified
    if (!event_name) {
        event_name = "click";
    }
    
    // Event type (as enum)
    KryonEventType event_type = parse_event_type(event_name);
    if (!write_uint8(codegen, (uint8_t)event_type)) {
        return false;
    }
    
    // If custom event, write the event name string
    if (event_type == KRYON_EVENT_CUSTOM) {
        uint32_t name_ref = add_string_to_table(codegen, event_name);
        if (!write_uint32(codegen, name_ref)) {
            return false;
        }
    }
    
    // Target element ID (if specified, 0 otherwise)
    uint32_t target_id = 0; // TODO: Implement element ID assignment
    if (!write_uint32(codegen, target_id)) {
        return false;
    }
    
    // Handler function name
    if (!handler_name) {
        handler_name = "handleEvent"; // Default handler name
    }
    uint32_t handler_ref = add_string_to_table(codegen, handler_name);
    if (!write_uint32(codegen, handler_ref)) {
        return false;
    }
    
    // Event flags (capture, passive, once, etc.)
    uint8_t flags = 0x00; // TODO: Parse event modifiers
    if (!write_uint8(codegen, flags)) {
        return false;
    }
    
    return true;
}

/**
 * @brief Write all event handlers in the AST to KRB format
 * @param codegen Code generator instance
 * @param ast_root Root AST node
 * @return true on success, false on failure
 */
bool kryon_write_event_section(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return false;
    }
    
    // Count event handlers
    uint32_t event_count = 0;
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_EVENT_DIRECTIVE) {
                event_count++;
            }
        }
    }
    
    // If no events, skip section
    if (event_count == 0) {
        return true;
    }
    
    // Write event section header
    if (!write_uint32(codegen, KRB_MAGIC_EVENTS)) {
        codegen_error(codegen, "Failed to write event section magic");
        return false;
    }
    
    // Write event count
    if (!write_uint32(codegen, event_count)) {
        codegen_error(codegen, "Failed to write event count");
        return false;
    }
    
    // Write each event handler
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_EVENT_DIRECTIVE) {
                if (!kryon_write_event_handler(codegen, child)) {
                    codegen_error(codegen, "Failed to write event handler");
                    return false;
                }
            }
        }
    }
    
    return true;
}

/**
 * @brief Count event handlers in the AST
 * @param ast_root Root AST node
 * @return Number of event handlers
 */
uint32_t kryon_count_event_handlers(const KryonASTNode *ast_root) {
    uint32_t count = 0;
    if (!ast_root) return 0;
    
    if (ast_root->type == KRYON_AST_EVENT_DIRECTIVE) {
        count++;
    }
    
    // Count in children recursively
    if (ast_root->type == KRYON_AST_ROOT || ast_root->type == KRYON_AST_ELEMENT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            count += kryon_count_event_handlers(ast_root->data.element.children[i]);
        }
    }
    
    return count;
}
