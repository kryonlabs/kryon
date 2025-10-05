/**
 * @file link.c
 * @brief Implementation of the Link element for navigation.
 *
 * This file contains the rendering, event handling, and navigation logic
 * for the Link element, supporting both internal (.kry/.krb) and external URL navigation.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include "element_mixins.h"
#include "navigation_utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for the VTable functions
static void link_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool link_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void link_destroy(KryonRuntime* runtime, KryonElement* element);

// Helper functions
static bool ends_with(const char* str, const char* suffix);
static KryonComponentDefinition* find_component_by_name(KryonRuntime* runtime, const char* name);

// The VTable binds the generic element interface to our specific link functions.
static const ElementVTable g_link_vtable = {
    .render = link_render,
    .handle_event = link_handle_event,
    .destroy = link_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================


/**
 * @brief Registers the Link element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_link_element(void) {
    return element_register_type("Link", &g_link_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Link element as clickable text with styling.
 */
static void link_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 2) return; // Reserve space for overlay
    
    // Ensure navigation manager exists if link has navigation properties
    navigation_check_and_init(runtime, element, "Link");
    
    // Get link properties
    const char* text = get_element_property_string(element, "text");
    if (!text) text = "Link";
    const char* to = get_element_property_string(element, "to");
    if (!to) to = "";
    bool external = get_element_property_bool(element, "external", false);
    
    // Debug output for render - only print once per render cycle to avoid spam
    static void* last_rendered_element = NULL;
    static char last_to_value[256] = "";
    if (element != last_rendered_element || strcmp(to, last_to_value) != 0) {
        printf("🎨 RENDER DEBUG: Link element %p, text='%s', to='%s'\n", (void*)element, text, to);
        last_rendered_element = element;
        strncpy(last_to_value, to, sizeof(last_to_value) - 1);
        last_to_value[sizeof(last_to_value) - 1] = '\0';
    }
    
    // Get styling properties
    uint32_t text_color_val = get_element_property_color(element, "textColor", 0x0099FFFF); // Blue default for links
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF);
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    float font_size = get_element_property_float(element, "fontSize", 16.0f);
    bool underline = get_element_property_bool(element, "underline", false);
    const char* text_align = get_element_property_string(element, "textAlign");
    if (!text_align) text_align = "left";
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Use element position from layout engine
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Handle background rendering first
    render_background_and_border(element, commands, command_count, max_commands);
    
    // Calculate text bounds for cursor management (same logic as hit testing)
    float text_width = strlen(text) * (font_size * 0.6f); // Rough character width estimate
    float text_height = font_size * 1.2f; // Line height estimate
    
    float text_x = position.x;
    float text_y = position.y;
    
    // Handle text alignment within the element bounds
    if (strcmp(text_align, "center") == 0) {
        text_x += (size.x - text_width) / 2.0f;
    } else if (strcmp(text_align, "right") == 0) {
        text_x += size.x - text_width;
    }
    
    // Custom cursor management for Link text area only
    KryonVec2 mouse_pos = runtime->mouse_position;
    bool is_text_hovered = (mouse_pos.x >= text_x && mouse_pos.x <= text_x + text_width &&
                           mouse_pos.y >= text_y && mouse_pos.y <= text_y + text_height);
    
    // Set cursor only when hovering over text area
    if (is_text_hovered && runtime->renderer) {
        kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
    }
    
    // Convert remaining colors
    KryonColor text_color = color_u32_to_f32(text_color_val);
    
    // Render link text
    KryonRenderCommand text_cmd = kryon_cmd_draw_text(
        position,
        text,
        font_size,
        text_color
    );
    
    // Set text alignment
    if (strcmp(text_align, "center") == 0) {
        text_cmd.data.draw_text.text_align = 1; // center
    } else if (strcmp(text_align, "right") == 0) {
        text_cmd.data.draw_text.text_align = 2; // right
    } else {
        text_cmd.data.draw_text.text_align = 0; // left
    }
    
    // Add underline if specified (will be handled by font styling)
    if (underline) {
        // TODO: Add underline support to renderer
    }
    
    // Visual feedback for external links
    if (external || navigation_is_external_url(to)) {
        text_cmd.data.draw_text.italic = true;
    }
    
    text_cmd.z_index = z_index + 1;
    commands[(*command_count)++] = text_cmd;
    
    // Check for overlay content property (supports both component references and text)
    const char* overlay_content = get_element_property_string(element, "overlay");
    if (overlay_content && strlen(overlay_content) > 0) {
        // Check if overlay should be shown (when hovering or on certain conditions)
        bool show_overlay = get_element_property_bool(element, "showOverlay", false);
        
        if (show_overlay && *command_count < max_commands) {
            // Try to find component by name first
            KryonComponentDefinition* component = find_component_by_name(runtime, overlay_content);
            
            if (component) {
                // Component overlay - render the component instance
                printf("🔍 Link: Rendering component overlay '%s'\n", overlay_content);
                
                // TODO: Implement component instance rendering for overlays
                // For now, render the component as positioned element
                // This would need to create a temporary component instance and render it
                
                // Fallback to component name as text for now
                KryonVec2 overlay_pos = { position.x, position.y - 40.0f };
                if (overlay_pos.y < 0) {
                    overlay_pos.y = position.y + size.y + 5.0f;
                }
                
                KryonRenderCommand overlay_text_cmd = kryon_cmd_draw_text(
                    overlay_pos,
                    overlay_content,
                    12.0f,
                    (KryonColor){ 1.0f, 1.0f, 1.0f, 1.0f }
                );
                overlay_text_cmd.z_index = z_index + 11;
                commands[(*command_count)++] = overlay_text_cmd;
                
            } else {
                // Text overlay - render as before
                printf("🔍 Link: Rendering text overlay '%s'\n", overlay_content);
                
                KryonVec2 overlay_pos = { position.x, position.y - 40.0f };
                KryonVec2 overlay_size = { size.x + 20.0f, 30.0f };
                
                if (overlay_pos.y < 0) {
                    overlay_pos.y = position.y + size.y + 5.0f;
                }
                
                // Render overlay background
                KryonColor overlay_bg = { 0.1f, 0.1f, 0.1f, 0.9f };
                KryonRenderCommand overlay_bg_cmd = kryon_cmd_draw_rect(
                    overlay_pos,
                    overlay_size,
                    overlay_bg,
                    4.0f
                );
                overlay_bg_cmd.z_index = z_index + 10;
                commands[(*command_count)++] = overlay_bg_cmd;
                
                // Render overlay text
                if (*command_count < max_commands) {
                    KryonVec2 overlay_text_pos = { overlay_pos.x + 10.0f, overlay_pos.y + 5.0f };
                    KryonColor overlay_text_color = { 1.0f, 1.0f, 1.0f, 1.0f };
                    
                    KryonRenderCommand overlay_text_cmd = kryon_cmd_draw_text(
                        overlay_text_pos,
                        overlay_content,
                        12.0f,
                        overlay_text_color
                    );
                    overlay_text_cmd.z_index = z_index + 11;
                    commands[(*command_count)++] = overlay_text_cmd;
                }
            }
        }
    }
}

/**
 * @brief Handles click events for navigation and overlay events.
 */
static bool link_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (event->type == ELEMENT_EVENT_CLICKED) {
        // Debug output for click event
        const char* clicked_text = get_element_property_string(element, "text");
        const char* clicked_to = get_element_property_string(element, "to");
        printf("🖱️ CLICK DEBUG: Link element %p clicked, text='%s', to='%s'\n", 
               (void*)element, clicked_text ? clicked_text : "NULL", clicked_to ? clicked_to : "NULL");
        
        // Try navigation first using shared utility
        if (navigation_handle_click(runtime, element, "Link")) {
            return true;
        }
    } else if (event->type == ELEMENT_EVENT_HOVERED) {
        // Show overlay on hover if overlay content exists
        const char* overlay_content = get_element_property_string(element, "overlay");
        if (overlay_content && strlen(overlay_content) > 0) {
            printf("🔍 Link: Showing overlay on hover\n");
            return true;
        }
    } else if (event->type == ELEMENT_EVENT_UNHOVERED) {
        // Hide overlay when not hovering
        const char* overlay_content = get_element_property_string(element, "overlay");
        if (overlay_content && strlen(overlay_content) > 0) {
            printf("🔍 Link: Hiding overlay on unhover\n");
            return true;
        }
    }
    
    // Fall back to generic script event handler for custom onClick handlers
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Link element.
 */
static void link_destroy(KryonRuntime* runtime, KryonElement* element) {
    // TODO: Clean up overlay when property system supports nested elements
    
    // No custom data to free for a basic link element
    (void)runtime;
    (void)element;
}

// =============================================================================
//  Helper Functions
// =============================================================================


/**
 * @brief Checks if string ends with suffix.
 */
static bool ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/**
 * @brief Handles navigation based on target type.
 */
/**
 * @brief Finds a component definition by name in the runtime's component registry.
 */
static KryonComponentDefinition* find_component_by_name(KryonRuntime* runtime, const char* name) {
    if (!runtime || !name || strlen(name) == 0) {
        return NULL;
    }
    
    // Check if runtime has components
    if (!runtime->components || runtime->component_count == 0) {
        printf("⚠️  No components available in runtime\n");
        return NULL;
    }
    
    // Search through registered components
    for (size_t i = 0; i < runtime->component_count; i++) {
        KryonComponentDefinition* component = runtime->components[i];
        if (component && component->name && strcmp(component->name, name) == 0) {
            printf("✅ Found component: %s\n", name);
            return component;
        }
    }
    
    printf("⚠️  Component '%s' not found in registry\n", name);
    return NULL;
}


