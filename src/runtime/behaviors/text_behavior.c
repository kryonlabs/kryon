/**
 * @file text_behavior.c
 * @brief Text Behavior Implementation
 * 
 * Handles text rendering for elements that display text content.
 * Supports text properties like color, font size, alignment, etc.
 * 
 * 0BSD License
 */

#include "element_behaviors.h"
#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// TEXT BEHAVIOR IMPLEMENTATION
// =============================================================================

static bool text_init(struct KryonElement* element) {
    // No special initialization needed for text rendering
    return true;
}

static void text_render(struct KryonRuntime* runtime, 
                       struct KryonElement* element,
                       KryonRenderCommand* commands,
                       size_t* command_count,
                       size_t max_commands) {
    
    if (*command_count >= max_commands - 1) return;
    
    // Get text content and properties
    const char* text = get_element_property_string_with_runtime(runtime, element, "text");
    if (!text || strlen(text) == 0) {
        return; // No text to render
    }
    
    // Get text styling properties with fallbacks
    uint32_t text_color_val = get_element_property_color(element, "color", 0x000000FF); // Black default
    
    // Fallback: if no explicit color property, try textColor
    if (text_color_val == 0x000000FF) {
        text_color_val = get_element_property_color(element, "textColor", 0x000000FF);
    }
    
    // Fallback: for Button elements without color, use a reasonable default (white)
    if (text_color_val == 0x000000FF && element->type_name && strcmp(element->type_name, "Button") == 0) {
        text_color_val = 0xFFFFFFFF; // White text for buttons
    }
    float font_size = get_element_property_float(element, "fontSize", 16.0f);
    const char* text_align = get_element_property_string(element, "textAlign");
    const char* font_family = get_element_property_string(element, "fontFamily");
    bool bold = get_element_property_bool(element, "bold", false);
    bool italic = get_element_property_bool(element, "italic", false);
    int z_index = get_element_property_int(element, "zIndex", 1); // Text usually above background
    
    // Convert color
    KryonColor text_color = color_u32_to_f32(text_color_val);
    
    // Check if element is disabled and adjust text color accordingly
    bool enabled = get_element_property_bool(element, "enabled", true);
    if (!enabled) {
        text_color.a *= 0.7f; // Make text more transparent when disabled
    }
    
    // Check hover state for interactive text elements
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (state && state->hovered) {
        // Could modify text color for hover feedback
        // For now, leave it to custom render functions
    }
    
    // Calculate text position
    KryonVec2 text_position = { element->x, element->y };
    
    // Apply text alignment
    if (text_align) {
        if (strcmp(text_align, "center") == 0) {
            text_position.x = element->x + element->width / 2.0f;
            text_position.y = element->y + element->height / 2.0f;
        } else if (strcmp(text_align, "right") == 0) {
            text_position.x = element->x + element->width;
            text_position.y = element->y + element->height / 2.0f;
        } else {
            // Default to left alignment
            text_position.x = element->x;
            text_position.y = element->y + element->height / 2.0f;
        }
    } else {
        // Default positioning - center vertically, left align horizontally
        text_position.x = element->x;
        text_position.y = element->y + element->height / 2.0f;
    }
    
    // Create text render command
    KryonRenderCommand cmd = kryon_cmd_draw_text(
        text_position,
        text,
        font_size,
        text_color
    );
    
    // Set additional text properties
    if (font_family) {
        // Note: This would need to be implemented in the renderer
        // For now, just use default font
    }
    
    if (bold) {
        // Note: Bold rendering would need renderer support
    }
    
    if (italic) {
        // Note: Italic rendering would need renderer support
    }
    
    // Set text alignment in render command (0=left, 1=center, 2=right)
    if (text_align) {
        if (strcmp(text_align, "center") == 0) {
            cmd.data.draw_text.text_align = 1;
        } else if (strcmp(text_align, "right") == 0) {
            cmd.data.draw_text.text_align = 2;
        } else {
            cmd.data.draw_text.text_align = 0;
        }
    } else {
        cmd.data.draw_text.text_align = 0;
    }
    
    cmd.z_index = z_index;
    commands[(*command_count)++] = cmd;
}

static bool text_handle_event(struct KryonRuntime* runtime,
                             struct KryonElement* element,
                             const ElementEvent* event) {
    
    // Text behavior doesn't handle events directly
    // Interactive text elements should also include Clickable behavior
    return false;
}

static void text_destroy(struct KryonElement* element) {
    // No special cleanup needed for text behavior
    (void)element;
}

static void* text_get_state(struct KryonElement* element) {
    return element_get_behavior_state(element);
}

// =============================================================================
// BEHAVIOR DEFINITION
// =============================================================================

static const ElementBehavior g_text_behavior = {
    .name = "Text",
    .init = text_init,
    .render = text_render,
    .handle_event = text_handle_event,
    .destroy = text_destroy,
    .get_state = text_get_state
};

// =============================================================================
// REGISTRATION FUNCTION
// =============================================================================

__attribute__((constructor))
void register_text_behavior(void) {
    element_behavior_register(&g_text_behavior);
}