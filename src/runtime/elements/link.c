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
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for the VTable functions
static void link_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool link_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void link_destroy(KryonRuntime* runtime, KryonElement* element);

// Helper functions
static bool is_external_url(const char* path);
static bool ends_with(const char* str, const char* suffix);
static void handle_link_navigation(KryonRuntime* runtime, const char* target, bool external);

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
    
    // Get link properties
    const char* text = get_element_property_string(element, "text");
    if (!text) text = "Link";
    const char* to = get_element_property_string(element, "to");
    if (!to) to = "";
    bool external = get_element_property_bool(element, "external", false);
    
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
    
    // Convert colors
    KryonColor text_color = color_u32_to_f32(text_color_val);
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Render background if visible
    if (bg_color.a > 0.0f || border_width > 0.0f) {
        KryonRenderCommand bg_cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        if (border_width > 0.0f) {
            bg_cmd.data.draw_rect.border_width = border_width;
            bg_cmd.data.draw_rect.border_color = border_color;
        }
        
        bg_cmd.z_index = z_index;
        commands[(*command_count)++] = bg_cmd;
    }
    
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
    if (external || is_external_url(to)) {
        text_cmd.data.draw_text.italic = true;
    }
    
    text_cmd.z_index = z_index + 1;
    commands[(*command_count)++] = text_cmd;
    
    // TODO: Render overlay component when element property system supports it
    // KryonElement* overlay = get_element_property_element(element, "overlay");
    // This will be implemented when the property system supports nested elements
}

/**
 * @brief Handles click events for navigation.
 */
static bool link_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (event->type == ELEMENT_EVENT_CLICKED) {
        const char* to = get_element_property_string(element, "to");
        bool external = get_element_property_bool(element, "external", false);
        
        if (to && strlen(to) > 0) {
            handle_link_navigation(runtime, to, external || is_external_url(to));
            return true;
        }
    }
    
    // TODO: Handle overlay events when property system supports nested elements
    
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
 * @brief Checks if a path is an external URL.
 */
static bool is_external_url(const char* path) {
    if (!path) return false;
    
    return (strncmp(path, "http://", 7) == 0 ||
            strncmp(path, "https://", 8) == 0 ||
            strncmp(path, "mailto:", 7) == 0 ||
            strncmp(path, "file://", 7) == 0 ||
            strncmp(path, "ftp://", 6) == 0);
}

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
static void handle_link_navigation(KryonRuntime* runtime, const char* target, bool external) {
    if (!target || strlen(target) == 0) {
        printf("⚠️  Link: Empty target specified\n");
        return;
    }
    
    printf("🔗 Link navigation: %s %s\n", target, external ? "(external)" : "(internal)");
    
    if (external) {
        // Handle external URLs
        #ifdef __linux__
            char command[1024];
            snprintf(command, sizeof(command), "xdg-open '%s' 2>/dev/null &", target);
            system(command);
        #elif __APPLE__
            char command[1024];
            snprintf(command, sizeof(command), "open '%s' &", target);
            system(command);
        #elif _WIN32
            // Will need to include windows.h for ShellExecuteA
            // ShellExecuteA(NULL, "open", target, NULL, NULL, SW_SHOWNORMAL);
            printf("🌐 External link: %s (Windows support coming soon)\n", target);
        #else
            printf("🌐 External link: %s (Platform not supported)\n", target);
        #endif
    } else {
        // Handle internal navigation
        if (ends_with(target, ".krb")) {
            // Direct KRB loading - will implement with navigation manager
            printf("📄 Loading KRB file: %s\n", target);
            // TODO: kryon_navigation_load_krb(runtime, target);
        } else if (ends_with(target, ".kry")) {
            // On-the-fly compilation - will implement with runtime compiler
            printf("⚡ Compiling and loading KRY file: %s\n", target);
            // TODO: kryon_navigation_compile_and_load(runtime, target);
        } else {
            printf("❓ Unknown file type: %s\n", target);
        }
    }
}