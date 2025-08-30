/**
 * @file button_element.c
 * @brief Implementation of the Button element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the Button element, encapsulating its behavior in a modular way.
 *
 * 0BSD License
 */

 #include "elements.h"
 #include "runtime.h"
 #include "memory.h"
 #include "color_utils.h"
 #include "element_mixins.h"
 #include "../navigation/navigation.h"
 #include "../../shared/kryon_mappings.h"
 #include <stdio.h>
 #include <string.h>
 #include <math.h>
 
 // Forward declarations for the VTable functions
 static void button_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
 static bool button_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
 static void button_destroy(KryonRuntime* runtime, KryonElement* element);

 // Helper functions for navigation (shared with Link)
 static bool is_external_url(const char* path);
 static void handle_button_navigation(KryonRuntime* runtime, const char* target, bool external);
 static void ensure_navigation_manager(KryonRuntime* runtime);
 
 // The VTable binds the generic element interface to our specific button functions.
 static const ElementVTable g_button_vtable = {
     .render = button_render,
     .handle_event = button_handle_event,
     .destroy = button_destroy
 };
 
 // =============================================================================
 //  Public Registration Function
 // =============================================================================
 
 /**
  * @brief Registers the Button element type with the element registry.
  * This is called once at runtime startup.
  */
 bool register_button_element(void) {
     return element_register_type("Button", &g_button_vtable);
 }
 
 // =============================================================================
 //  Element VTable Implementations
 // =============================================================================
 
 /**
  * @brief Renders the Button element by generating render commands.
  * This function contains the logic previously in `element_button_to_commands`.
  */
 static void button_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
     if (*command_count >= max_commands) return;

     // Ensure navigation manager exists if button has navigation properties
     const char* to = get_element_property_string(element, "to");
     if (to && strlen(to) > 0) {
         ensure_navigation_manager(runtime);
     }
 
     // --- 1. Get Visual Properties ---
     float posX = element->x;
     float posY = element->y;
     
     // Use layout-calculated position
     float width = get_element_property_float(element, "width", -1.0f); // -1 triggers auto-sizing
     float height = get_element_property_float(element, "height", -1.0f);
     const char* text = get_element_property_string(element, "text");
     uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x3B82F6FF); // Blue-500
     uint32_t text_color_val = get_element_property_color(element, "color", 0xFFFFFFFF); // White
     uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x2563EBFF); // Blue-600
     float border_width = get_element_property_float(element, "borderWidth", 1.0f);
     float border_radius = get_element_property_float(element, "borderRadius", 8.0f);
     bool enabled = get_element_property_bool(element, "enabled", true);
 
     // --- 2. Calculate Final Layout (using shared mixin) ---
     calculate_auto_size_with_text(element, &width, &height, text, 20.0f, 12.0f, 60.0f, 32.0f);
    
    // Update element dimensions if auto-sizing was applied (similar to text.c)
    if (element->width != width) element->width = width;
    if (element->height != height) element->height = height;
 
     // --- 3. Determine Visual State based on Interaction (using shared mixin) ---
     bool is_hovered = check_hover_and_cursor(runtime, element, enabled);
 
     // --- 4. Prepare Colors and Final State (using shared mixin) ---
     KryonColor bg_color = color_u32_to_f32(bg_color_val);
     KryonColor text_color = color_u32_to_f32(text_color_val);
     KryonColor border_color = color_u32_to_f32(border_color_val);
     
     apply_interaction_colors(&bg_color, &text_color, is_hovered, enabled);
 
     // --- 5. Generate a Unique and Correct ID for the Renderer ---
     const char* button_id_str = element->element_id;
     static char id_buffer[4][32];
     static int buffer_index = 0;
     if (!button_id_str || button_id_str[0] == '\0') {
         char* current_buffer = id_buffer[buffer_index];
         buffer_index = (buffer_index + 1) % 4;
         snprintf(current_buffer, 32, "kryon-id-%u", element->id);
         button_id_str = current_buffer;
     }
     
     // --- 6. Create and Add the Render Command ---
     KryonRenderCommand cmd = kryon_cmd_draw_button(
         button_id_str,
         (KryonVec2){posX, posY},
         (KryonVec2){width, height},
         text ? text : "",
         bg_color,
         text_color
     );
     
     cmd.data.draw_button.border_color = border_color;
     cmd.data.draw_button.border_width = border_width;
     cmd.data.draw_button.border_radius = border_radius;
     cmd.data.draw_button.state = is_hovered ? KRYON_ELEMENT_STATE_HOVERED : KRYON_ELEMENT_STATE_NORMAL;
     cmd.data.draw_button.enabled = enabled;
     
     commands[(*command_count)++] = cmd;
 }
 
 /**
  * @brief Handles abstract events for the Button.
  * Supports both navigation (like Link) and script events.
  */
 static bool button_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
     if (event->type == ELEMENT_EVENT_CLICKED) {
         const char* to = get_element_property_string(element, "to");
         bool external = get_element_property_bool(element, "external", false);
         
         if (to && strlen(to) > 0) {
             handle_button_navigation(runtime, to, external || is_external_url(to));
             return true;
         }
     }
     
     // Fall back to generic script event handler for custom onClick handlers
     return handle_script_events(runtime, element, event);
 }
 
 /**
  * @brief Cleans up any custom data for a Button element.
  * A simple button has no custom allocated data in `user_data`, so this is empty.
  * It's good practice to have it for future extensibility.
  */
 static void button_destroy(KryonRuntime* runtime, KryonElement* element) {
     // No custom data to free for a simple button.
     (void)runtime;
     (void)element;
 }

 // =============================================================================
 //  Helper Functions for Navigation (shared logic from Link)
 // =============================================================================

 /**
  * @brief Ensures navigation manager is created when first button with navigation is encountered
  */
 static void ensure_navigation_manager(KryonRuntime* runtime) {
     if (!runtime->navigation_manager) {
         runtime->navigation_manager = kryon_navigation_create(runtime);
         if (!runtime->navigation_manager) {
             printf("⚠️  Failed to create navigation manager for button element\n");
         } else {
             printf("🧭 Navigation manager created (button element with navigation detected)\n");
             
             // Set current path from runtime's loaded file if available
             if (runtime->current_file_path) {
                 kryon_navigation_set_current_path(runtime->navigation_manager, runtime->current_file_path);
                 printf("🧭 Set navigation path from runtime: %s\n", runtime->current_file_path);
             }
         }
     }
 }

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
  * @brief Handles navigation based on target type.
  */
 static void handle_button_navigation(KryonRuntime* runtime, const char* target, bool external) {
     if (!target || strlen(target) == 0) {
         printf("⚠️  Button: Empty target specified\n");
         return;
     }
     
     printf("🔗 Button navigation: %s %s\n", target, external ? "(external)" : "(internal)");
     
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
             printf("🌐 External button link: %s (Windows support coming soon)\n", target);
         #else
             printf("🌐 External button link: %s (Platform not supported)\n", target);
         #endif
     } else {
         // Handle internal navigation using the navigation manager
         if (runtime->navigation_manager) {
             printf("📄 Button navigating to internal file: %s\n", target);
             KryonNavigationResult result = kryon_navigate_to(runtime->navigation_manager, target, false);
             
             if (result == KRYON_NAV_SUCCESS) {
                 printf("✅ Button navigation successful\n");
             } else {
                 printf("❌ Button navigation failed with result: %d\n", result);
             }
         } else {
             printf("⚠️  Navigation manager not available for button internal navigation\n");
         }
     }
 }
 