/**
 * @file tab.c
 * @brief Implementation of the Tab element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the Tab element, which represents individual tab headers in a TabBar.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include "element_mixins.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for TabBar state management  
// Note: This is a simple approach - in production we'd use proper header sharing
extern void* ensure_tabbar_state(KryonElement* element);
extern void tabbar_set_selected_index(KryonRuntime* runtime, KryonElement* tabbar_element, int index);
extern int tabbar_get_selected_index(KryonElement* tabbar_element);

// Forward declarations for the VTable functions
static void tab_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool tab_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void tab_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific tab functions.
static const ElementVTable g_tab_vtable = {
    .render = tab_render,
    .handle_event = tab_handle_event,
    .destroy = tab_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Tab element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_tab_element(void) {
    return element_register_type("Tab", &g_tab_vtable);
}

// =============================================================================
//  Helper Functions
// =============================================================================

/**
 * @brief Handle Tab cursor behavior (always pointer unless explicitly overridden)
 */
static void handle_tab_cursor(KryonRuntime* runtime, KryonElement* element, bool is_disabled) {
    if (!runtime || !element || !runtime->renderer) return;

    KryonVec2 mouse_pos = runtime->mouse_position;
    bool is_hovered = (mouse_pos.x >= element->x && mouse_pos.x <= element->x + element->width &&
                       mouse_pos.y >= element->y && mouse_pos.y <= element->y + element->height);

    if (is_hovered && !is_disabled) {
        // Check if element has explicit cursor property
        const char* explicit_cursor = get_element_property_string(element, "cursor");

        if (explicit_cursor) {
            // User explicitly set cursor - use that instead of default
            if (strcmp(explicit_cursor, "pointer") == 0) {
                kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
            } else if (strcmp(explicit_cursor, "text") == 0) {
                kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_TEXT);
            } else if (strcmp(explicit_cursor, "default") == 0) {
                kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_DEFAULT);
            }
        } else {
            // No explicit cursor set - use Tab default (pointer)
            kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
        }
    }
}

/**
 * @brief Gets the tab title, defaults to "Tab"
 */
static const char* get_tab_title(KryonElement* element) {
    const char* title = get_element_property_string(element, "title");
    return title ? title : "Tab";
}

/**
 * @brief Gets whether this tab is active by checking parent TabBar's selectedIndex
 */
static bool is_tab_active(KryonElement* element) {
    if (!element || !element->parent) return false;
    
    // Find parent TabBar
    KryonElement* parent = element->parent;
    if (!parent || strcmp(parent->type_name, "TabBar") != 0) return false;
    
    // Get this tab's index within the parent
    int tab_index = -1;
    int current_tab_index = 0;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        KryonElement* child = parent->children[i];
        if (child && strcmp(child->type_name, "Tab") == 0) {
            if (child == element) {
                tab_index = current_tab_index;
                break;
            }
            current_tab_index++;
        }
    }
    
    if (tab_index == -1) return false;
    
    // Check if this tab's index matches the TabBar's current selectedIndex (from state)
    int selected_index = tabbar_get_selected_index(parent);
    return tab_index == selected_index;
}

/**
 * @brief Gets whether this tab is disabled
 */
static bool is_tab_disabled(KryonElement* element) {
    return get_element_property_bool(element, "disabled", false);
}

/**
 * @brief Gets tab font size, defaults to 14
 */
static int get_tab_font_size(KryonElement* element) {
    return get_element_property_int(element, "fontSize", 14);
}

/**
 * @brief Gets tab padding, defaults to 12
 */
static int get_tab_padding(KryonElement* element) {
    return get_element_property_int(element, "padding", 12);
}

/**
 * @brief Gets tab border radius, defaults to 4
 */
static int get_tab_border_radius(KryonElement* element) {
    return get_element_property_int(element, "borderRadius", 4);
}

/**
 * @brief Gets minimum tab width, defaults to 80
 */
static int get_tab_min_width(KryonElement* element) {
    return get_element_property_int(element, "minWidth", 80);
}

/**
 * @brief Gets custom tab width, returns -1 if not set (auto-size)
 */
static int get_tab_custom_width(KryonElement* element) {
    return get_element_property_int(element, "width", -1);
}

/**
 * @brief Checks if tab has a custom onClick handler that overrides default navigation
 */
static bool has_custom_click_handler(KryonElement* element) {
    const char* onClick = get_element_property_string(element, "onClick");
    return onClick != NULL;
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Tab element by generating render commands.
 */
static void tab_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !commands || !command_count) return;

    // If Tab has children, act as a simple container (don't render tab header graphics)
    if (element->child_count > 0) {
        // Handle cursor (Tab always shows pointer by default)
        handle_tab_cursor(runtime, element, is_tab_disabled(element));

        // Tab is being used as a container - just render background if specified
        uint32_t bg_val = get_element_property_color(element, "backgroundColor", 0x00000000);
        if (bg_val != 0x00000000 && *command_count < max_commands) {
            KryonColor bg_color = color_u32_to_f32(bg_val);
            KryonVec2 position = { element->x, element->y };
            KryonVec2 size = { element->width, element->height };
            commands[*command_count] = kryon_cmd_draw_rect(position, size, bg_color, 0.0f);
            (*command_count)++;
        }
        // Children will be rendered automatically by the rendering system
        return;
    }

    // Original tab header rendering (when used in TabBar with no children)
    bool is_active = is_tab_active(element);
    bool is_disabled = is_tab_disabled(element);
    const char* title = get_tab_title(element);
    int font_size = get_tab_font_size(element);
    int padding = get_tab_padding(element);
    int border_radius = get_tab_border_radius(element);


    // Get element bounds
    float x = element->x;
    float y = element->y;
    float width = element->width;
    float height = element->height;

    // Debug: Log tab bounds for hit testing analysis
    printf("🔍 TAB RENDER: Tab '%s' bounds: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
           title ? title : "?", x, y, width, height);

    // Handle cursor (Tab always shows pointer by default)
    handle_tab_cursor(runtime, element, is_disabled);

    // Check for hover state for visual effects
    KryonVec2 mouse_pos = runtime->mouse_position;
    bool is_hovered = (mouse_pos.x >= x && mouse_pos.x <= x + width &&
                       mouse_pos.y >= y && mouse_pos.y <= y + height);

    // Get colors based on state
    KryonColor bg_color, text_color;

    if (is_disabled) {
        uint32_t bg_val = get_element_property_color(element, "disabledBackgroundColor", 0xF5F5F5FF);
        uint32_t text_val = get_element_property_color(element, "disabledTextColor", 0x9CA3AFFF);
        bg_color = color_u32_to_f32(bg_val);
        text_color = color_u32_to_f32(text_val);
    } else if (is_active) {
        uint32_t bg_val = get_element_property_color(element, "activeBackgroundColor", 0xE3F2FDFF);
        uint32_t text_val = get_element_property_color(element, "activeTextColor", 0x1565C0FF);
        bg_color = color_u32_to_f32(bg_val);
        text_color = color_u32_to_f32(text_val);
    } else {
        uint32_t bg_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF);
        uint32_t text_val = get_element_property_color(element, "textColor", 0x2D3748FF);
        bg_color = color_u32_to_f32(bg_val);
        text_color = color_u32_to_f32(text_val);
    }

    // Apply hover effects using mixin (only if not disabled and not special states)
    if (!is_disabled && !is_active) {
        apply_interaction_colors(&bg_color, &text_color, is_hovered, true);
    }

    // Render tab background
    if (*command_count < max_commands) {
        KryonVec2 position = { x, y };
        KryonVec2 size = { width, height };
        commands[*command_count] = kryon_cmd_draw_rect(position, size, bg_color, (float)border_radius);
        (*command_count)++;
    }

    // Render tab border if specified
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x00000000);
    int border_width = get_element_property_int(element, "borderWidth", 0);

    if (border_width > 0 && border_color_val != 0x00000000 && *command_count < max_commands) {
        KryonColor border_color = color_u32_to_f32(border_color_val);
        KryonVec2 position = { x, y };
        KryonVec2 size = { width, height };
        KryonRenderCommand cmd = kryon_cmd_draw_rect(position, size, bg_color, (float)border_radius);
        cmd.data.draw_rect.border_width = (float)border_width;
        cmd.data.draw_rect.border_color = border_color;
        commands[*command_count] = cmd;
        (*command_count)++;
    }

    // Render tab text
    if (title && strlen(title) > 0 && *command_count < max_commands) {
        // Calculate text position (centered)
        KryonVec2 text_pos = { x + width / 2, y + height / 2 };
        commands[*command_count] = kryon_cmd_draw_text(text_pos, (char*)title, (float)font_size, text_color);
        commands[*command_count].data.draw_text.text_align = 1; // Center align
        (*command_count)++;
    }

    // Add hover effect if not disabled
    if (!is_disabled && !is_active) {
        // This would be handled by the parent TabBar's event system
        // or could be implemented with a hover state property
    }
}

/**
 * @brief Handles events for the Tab element.
 */
static bool tab_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!element || !event) return false;

    printf("🔍 TAB EVENT: Tab '%s' received event type %d\n", 
           get_tab_title(element), event->type);

    bool is_disabled = is_tab_disabled(element);
    if (is_disabled) {
        printf("🔍 TAB EVENT: Tab is disabled, ignoring event\n");
        return false;
    }

    // Handle click events to notify parent TabBar
    if (event->type == ELEMENT_EVENT_CLICKED) {
        printf("🔍 TAB CLICK: Tab '%s' processing click event\n", get_tab_title(element));

        // Check if this tab has a custom onClick handler that overrides default navigation
        if (has_custom_click_handler(element)) {
            printf("🔍 TAB CLICK: Tab has custom onClick handler, delegating to script handler\n");
            return generic_script_event_handler(runtime, element, event);
        }

        // Default behavior: navigate to this tab
        // Find parent TabBar
        KryonElement* parent = element->parent;
        if (parent && strcmp(parent->type_name, "TabBar") == 0) {
            // Find this tab's index within the parent
            int current_tab_index = 0;
            for (size_t i = 0; i < parent->child_count; i++) {
                KryonElement* child = parent->children[i];
                if (child && strcmp(child->type_name, "Tab") == 0) {
                    if (child == element) {
                        printf("🔍 TAB CLICK: Found tab at index %d, setting as selected\n", current_tab_index);
                        // Use the helper function to set selected index
                        tabbar_set_selected_index(runtime, parent, current_tab_index);
                        printf("🔍 TAB CLICK: Tab selection completed\n");
                        return true;
                    }
                    current_tab_index++;
                }
            }
        }
    }

    // For script-defined behavior, delegate to generic handler
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Destroys the Tab element and cleans up resources.
 */
static void tab_destroy(KryonRuntime* runtime, KryonElement* element) {
    if (!element) return;
    
    // No additional cleanup needed for Tab elements
}