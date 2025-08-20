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
 #include <stdio.h>
 #include <string.h>
 #include <math.h>
 
 // Forward declarations for the VTable functions
 static void button_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
 static bool button_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
 static void button_destroy(KryonRuntime* runtime, KryonElement* element);
 
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
 
     // --- 2. Calculate Final Layout ---
     auto_size_element(element, &width, &height);
 
     // --- 3. Determine Visual State based on Interaction ---
     bool is_hovered = false;
     if (enabled) {
         KryonVec2 mouse_pos = runtime->mouse_position;
         if (mouse_pos.x >= posX && mouse_pos.x <= posX + width &&
             mouse_pos.y >= posY && mouse_pos.y <= posY + height) {
             is_hovered = true;
             // Cursor management moved to renderer layer
             if (runtime->renderer) {
                 kryon_renderer_set_cursor((KryonRenderer*)runtime->renderer, KRYON_CURSOR_POINTER);
             }
         }
     }
 
     // --- 4. Prepare Colors and Final State ---
     KryonColor bg_color = color_u32_to_f32(bg_color_val);
     KryonColor text_color = color_u32_to_f32(text_color_val);
     KryonColor border_color = color_u32_to_f32(border_color_val);
     
     if (is_hovered) {
         bg_color = color_lighten(bg_color, 0.15f);
     }
     if (!enabled) {
         bg_color = color_desaturate(bg_color, 0.5f);
         text_color.a *= 0.7f;
     }
 
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
  * For a standard button, we can delegate directly to the generic script handler,
  * which will automatically call script functions like "onClick", "onHover", etc.
  */
 static bool button_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
     // A button's logic is almost always defined in script. We can reuse the
     // powerful generic handler from the element system.
     return generic_script_event_handler(runtime, element, event);
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
 