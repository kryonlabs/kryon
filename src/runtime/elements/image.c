/**
 * @file image.c
 * @brief Implementation of the Image element.
 *
 * This file contains the rendering logic for Image elements.
 * Image elements display images from files or URLs with optional
 * opacity and aspect ratio maintenance.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "element_mixins.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for the VTable functions
static void image_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool image_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void image_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific image functions.
static const ElementVTable g_image_vtable = {
    .render = image_render,
    .handle_event = image_handle_event,
    .destroy = image_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Image element type with the element registry.
 */
bool register_image_element(void) {
    return element_register_type("Image", &g_image_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Image element by generating a draw_image command.
 */
static void image_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 2) return; // Need space for background + image
    
    // --- 1. Render background and border using mixin ---
    render_background_and_border(element, commands, command_count, max_commands);
    
    // --- 2. Get Image Properties ---
    const char* source = get_element_property_string(element, "source");
    const char* src = get_element_property_string(element, "src"); // Alternative property name
    float opacity = get_element_property_float(element, "opacity", 1.0f);
    bool maintain_aspect = get_element_property_bool(element, "keepAspectRatio", true);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Use 'src' if 'source' is not provided (common naming convention)
    if (!source && src) {
        source = src;
    }
    
    if (!source || source[0] == '\0') {
        // No image source provided, skip rendering
        return;
    }
    
    // --- 3. Use Unified Layout Engine Position ---
    // The unified layout engine has already calculated the correct position and size
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Auto-size using mixin if needed
    float width = size.x;
    float height = size.y;
    calculate_auto_size_simple(element, &width, &height, 100.0f, 100.0f);
    size.x = width;
    size.y = height;
    
    // --- 4. Create and Add the Render Command ---
    KryonRenderCommand cmd = {0};
    cmd.type = KRYON_CMD_DRAW_IMAGE;
    cmd.z_index = z_index;
    cmd.data.draw_image = (KryonDrawImageData){
        .position = position,
        .size = size,
        .source = strdup(source),
        .opacity = opacity,
        .maintain_aspect = maintain_aspect
    };
    
    commands[(*command_count)++] = cmd;
}

/**
 * @brief Handles events for the Image element.
 * Images can handle script-based events like onClick.
 */
static bool image_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the mixin script event handler for consistent behavior
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for an Image element.
 */
static void image_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic image element.
    (void)runtime;
    (void)element;
}