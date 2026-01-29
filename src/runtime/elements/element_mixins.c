/**

 * @file element_mixins.c
 * @brief Shared element functionality - mixins for common element behaviors
 * 
 * This file contains reusable functions that multiple elements can use
 * to avoid code duplication while maintaining the VTable architecture.
 * 
 * 0BSD License
 */
#include "lib9.h"


#include "element_mixins.h"
#include "elements.h" 
#include "runtime.h"
#include "color_utils.h"
#include "../navigation/navigation.h"
#include "../../shared/kryon_mappings.h"

// =============================================================================
// SHARED RENDERING MIXINS
// =============================================================================

/**
 * @brief Renders background and border for elements that support visual styling.
 * This consolidates the duplicate rendering logic found in app.c and container.c.
 * 
 * @param element Element to render background/border for
 * @param commands Render command buffer
 * @param command_count Current command count (will be incremented)
 * @param max_commands Maximum number of commands allowed
 * @return true if background/border was rendered, false if skipped
 */
bool render_background_and_border(struct KryonElement* element, 
                                KryonRenderCommand* commands, 
                                size_t* command_count, 
                                size_t max_commands) {
    if (*command_count >= max_commands - 1) return false;
    
    // Get visual properties with defaults
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    
    // Fallback to "background" property for compatibility
    if (bg_color_val == 0x00000000) {
        bg_color_val = get_element_property_color(element, "background", 0x00000000);
    }
    
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF); // Black default
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Use element position from layout engine
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Convert color values to KryonColor format
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Only render if background is visible or has border
    if (bg_color.a > 0.0f || border_width > 0.0f) {
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        // Set border properties if needed
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        cmd.z_index = z_index;
        commands[(*command_count)++] = cmd;
        
        return true; // Background/border was rendered
    }
    
    return false; // Nothing was rendered
}

// =============================================================================
// SHARED EVENT HANDLING MIXINS
// =============================================================================

/**
 * @brief Generic script event handler that dispatches to element script functions.
 * This consolidates the event handling logic used by most elements.
 * 
 * @param runtime Runtime context
 * @param element Element that received the event
 * @param event Event to handle
 * @return true if event was handled, false otherwise
 */
bool handle_script_events(struct KryonRuntime* runtime, struct KryonElement* element, const struct ElementEvent* event) {
    // Use the existing generic script event handler from elements system
    return generic_script_event_handler(runtime, element, (const ElementEvent*)event);
}

// =============================================================================
// SHARED SIZING MIXINS  
// =============================================================================

/**
 * @brief Calculate automatic size for elements with text content.
 * This consolidates auto-sizing logic used by buttons and other text-based elements.
 * 
 * @param element Element to calculate size for
 * @param width Pointer to width (modified if < 0)
 * @param height Pointer to height (modified if < 0)
 * @param text_content Text content to measure (can be NULL)
 * @param padding_x Total horizontal padding to add
 * @param padding_y Total vertical padding to add
 * @param min_width Minimum width to enforce
 * @param min_height Minimum height to enforce
 */
void calculate_auto_size_with_text(struct KryonElement* element, 
                                 float* width, 
                                 float* height,
                                 const char* text_content,
                                 float padding_x,
                                 float padding_y,
                                 float min_width,
                                 float min_height) {
    if (!element || !width || !height) return;
    
    // Use the existing auto_size_element function as base
    auto_size_element(element, width, height);
    
    // Apply text-specific adjustments if we have text content
    if (text_content && strlen(text_content) > 0) {
        // Estimate text dimensions (this is a simple heuristic)
        if (*width < 0) {
            float estimated_text_width = strlen(text_content) * 8.0f; // Rough estimate
            *width = estimated_text_width + padding_x;
        }
        
        if (*height < 0) {
            *height = 20.0f + padding_y; // Standard text height + padding
        }
    }
    
    // Enforce minimums
    if (*width > 0 && *width < min_width) *width = min_width;
    if (*height > 0 && *height < min_height) *height = min_height;
}

/**
 * @brief Simple auto-size calculation for basic elements.
 * 
 * @param element Element to calculate size for
 * @param width Pointer to width (modified if < 0)
 * @param height Pointer to height (modified if < 0)
 * @param default_width Default width to use if auto-sizing
 * @param default_height Default height to use if auto-sizing
 */
void calculate_auto_size_simple(struct KryonElement* element,
                              float* width,
                              float* height, 
                              float default_width,
                              float default_height) {
    if (!element || !width || !height) return;
    
    // Use existing auto_size_element function, then apply defaults
    auto_size_element(element, width, height);
    
    // Apply defaults if still unspecified
    if (*width < 0) *width = default_width;
    if (*height < 0) *height = default_height;
}

// =============================================================================
// INTERACTION STATE MIXINS
// =============================================================================

/**
 * @brief Check if mouse is hovering over an element and handle cursor.
 * This consolidates hover detection logic used by interactive elements.
 * 
 * @param runtime Runtime context (for mouse position and cursor)
 * @param element Element to check hover for
 * @param enabled Whether the element is enabled (affects cursor)
 * @return true if element is being hovered
 */
bool check_hover_and_cursor(struct KryonRuntime* runtime, struct KryonElement* element, bool enabled) {
    if (!runtime || !element) return false;
    
    KryonVec2 mouse_pos = runtime->mouse_position;
    float posX = element->x;
    float posY = element->y;
    float width = element->width;
    float height = element->height;
    
    bool is_hovered = (mouse_pos.x >= posX && mouse_pos.x <= posX + width &&
                       mouse_pos.y >= posY && mouse_pos.y <= posY + height);
    
    // Set cursor for interactive elements
    if (is_hovered && enabled && runtime->renderer) {
        kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
    }
    
    return is_hovered;
}

/**
 * @brief Apply visual state modifications to colors based on interaction.
 * This consolidates the color modification logic for hover/disabled states.
 * 
 * @param bg_color Background color to modify
 * @param text_color Text color to modify
 * @param is_hovered Whether element is hovered
 * @param is_enabled Whether element is enabled
 */
void apply_interaction_colors(KryonColor* bg_color, KryonColor* text_color, bool is_hovered, bool is_enabled) {
    if (!bg_color || !text_color) return;
    
    if (is_hovered) {
        *bg_color = color_lighten(*bg_color, 0.15f);
    }
    
    if (!is_enabled) {
        *bg_color = color_desaturate(*bg_color, 0.5f);
        text_color->a *= 0.7f;
    }
}

/**
 * @brief Renders background and border for input-like elements with focus support.
 * This handles more complex input styling including focus states and border emphasis.
 */
bool render_input_background_and_border(struct KryonElement* element,
                                       KryonRenderCommand* commands,
                                       size_t* command_count,
                                       size_t max_commands,
                                       bool has_focus,
                                       bool is_disabled) {
    if (*command_count >= max_commands - 2) return false; // Need space for bg + border
    
    // Get visual properties with input-specific defaults
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF); // White default
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0xCCCCCCFF); // Light gray default
    float border_width = get_element_property_float(element, "borderWidth", 1.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 4.0f);
    int z_index = get_element_property_int(element, "zIndex", 10);
    
    // Use element position from layout engine
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Convert color values to KryonColor format
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Apply state-based modifications
    if (is_disabled) {
        bg_color = color_desaturate(bg_color, 0.5f);
        border_color.a *= 0.5f;
    } else if (has_focus) {
        border_color = color_lighten(border_color, 0.3f);
        border_width *= 1.5f;
    }
    
    // Draw input background
    KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
        position,
        size,
        bg_color,
        border_radius
    );
    bg_cmd.z_index = z_index;
    commands[(*command_count)++] = bg_cmd;
    
    // Draw border (using a slightly larger rect)
    if (border_width > 0.0f) {
        KryonRenderCommand border_cmd = kryon_cmd_draw_rect(
            (KryonVec2){position.x - border_width, position.y - border_width},
            (KryonVec2){size.x + 2*border_width, size.y + 2*border_width},
            border_color,
            border_radius
        );
        border_cmd.z_index = z_index - 1; // Behind background
        commands[(*command_count)++] = border_cmd;
    }
    
    return true;
}
