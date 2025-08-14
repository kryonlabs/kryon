/**
 * @file text_element.c
 * @brief Implementation of the Text element.
 *
 * This file contains the rendering logic for the Text element. Text elements
 * are typically not interactive, so event handling is a no-op.
 *
 * 0BSD License
 */

 #include "elements.h"
 #include "runtime.h"
 #include "memory.h"
 #include "color_utils.h"
 #include <stdio.h>
 #include <string.h>
 
 // Forward declarations for the VTable functions
 static void text_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
 static bool text_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
 static void text_destroy(KryonRuntime* runtime, KryonElement* element);
 
 // The VTable binds the generic element interface to our specific text functions.
 static const ElementVTable g_text_vtable = {
     .render = text_render,
     .handle_event = text_handle_event,
     .destroy = text_destroy
 };
 
 // =============================================================================
 //  Public Registration Function
 // =============================================================================
 
 /**
  * @brief Registers the Text element type with the element registry.
  */
 bool register_text_element(void) {
     return element_register_type("Text", &g_text_vtable);
 }
 
 // =============================================================================
 //  Element VTable Implementations
 // =============================================================================


 // In src/runtime/elements/text_element.c

/**
 * @brief Renders the Text element by generating a draw_text command.
 * This function includes the complete and correct logic for determining its
 * final on-screen position.
 */
static void text_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands) return;

    // --- 1. Get Text Properties ---  
    // Use runtime-aware property access for reactive variable support
    const char* text = get_element_property_string_with_runtime(runtime, element, "text");
    
    if (!text || text[0] == '\0') {
        text = ""; // Use empty string if no text property
    }
    
    if (!text || text[0] == '\0') {
        return; // Nothing to render
    }
    
    uint32_t text_color_val = get_element_property_color(element, "color", 0x000000FF);
    float font_size = get_element_property_float(element, "fontSize", 16.0f);
    const char* font_family = get_element_property_string(element, "fontFamily");
    const char* font_weight_str = get_element_property_string(element, "fontWeight");
    const char* text_align_str = get_element_property_string(element, "textAlign");
    int z_index = get_element_property_int(element, "zIndex", 0);
    float max_width = get_element_property_float(element, "maxWidth", 0.0f);
    
    // Auto-set maxWidth based on element width for better text wrapping
    if (max_width <= 0.0f && element->width > 0.0f) {
        max_width = element->width - 10.0f; // Leave small margin for readability
    }

    // --- 2. Use Unified Layout Engine Position ---
    // The unified layout engine in elements.c has already calculated the correct position
    // during ensure_layout_positions_calculated(). We simply use those results.
    KryonVec2 position = {
        .x = element->x,
        .y = element->y
    };

    // --- 3. Process Other Properties ---
    KryonColor text_color = color_u32_to_f32(text_color_val);

    int align_code = 0; // 0=left, 1=center, 2=right
    if (text_align_str) {
        if (strcmp(text_align_str, "center") == 0) align_code = 1;
        else if (strcmp(text_align_str, "right") == 0) align_code = 2;
    }

    bool is_bold = (font_weight_str && (strcmp(font_weight_str, "bold") == 0 || strcmp(font_weight_str, "700") == 0));

    // --- 4. Create and Add the Render Command ---
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_TEXT;
    cmd.z_index = z_index;
    cmd.data.draw_text = (KryonDrawTextData){
        .position = position,
        .text = strdup(text),
        .font_size = font_size,
        .color = text_color,
        .font_family = font_family ? strdup(font_family) : NULL,
        .bold = is_bold,
        .italic = false,
        .max_width = max_width,
        .text_align = align_code
    };
    
    commands[(*command_count)++] = cmd;
}
 
 /**
  * @brief Handles events for the Text element.
  * By default, text is not interactive and does not handle events.
  */
 static bool text_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
     (void)runtime;
     (void)element;
     (void)event;
     return false; // Text does not handle events, so they can propagate
 }
 
 /**
  * @brief Cleans up any custom data for a Text element.
  */
 static void text_destroy(KryonRuntime* runtime, KryonElement* element) {
     // No custom data to free for a simple text element.
     (void)runtime;
     (void)element;
 }
 