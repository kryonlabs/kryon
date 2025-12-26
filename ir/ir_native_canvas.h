#ifndef IR_NATIVE_CANVAS_H
#define IR_NATIVE_CANVAS_H

#include "ir_core.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * NativeCanvas Component - Direct Backend Access
 *
 * Provides a canvas element that allows direct access to the active rendering backend.
 * Users can call backend-specific rendering functions (raylib, SDL3, etc.) within
 * a render callback.
 *
 * This component bypasses Kryon's command buffer system for maximum performance
 * and flexibility when working with native rendering APIs.
 */

/**
 * NativeCanvas component data
 */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t background_color;

    // Callback function pointer (set by language bindings)
    // The callback is invoked during rendering and receives the component ID
    void (*render_callback)(uint32_t component_id);

    // User data pointer (optional, can be set by bindings)
    void* user_data;
} IRNativeCanvasData;

/**
 * Create NativeCanvas component
 *
 * @param width Canvas width in pixels
 * @param height Canvas height in pixels
 * @return New NativeCanvas component, or NULL on failure
 */
IRComponent* ir_create_native_canvas(uint16_t width, uint16_t height);

/**
 * Register render callback for NativeCanvas component
 *
 * @param component_id The component ID
 * @param callback Function pointer to the render callback
 */
void ir_native_canvas_set_callback(uint32_t component_id, void (*callback)(uint32_t));

/**
 * Set user data for NativeCanvas component
 *
 * @param component_id The component ID
 * @param user_data Pointer to user data
 */
void ir_native_canvas_set_user_data(uint32_t component_id, void* user_data);

/**
 * Get user data from NativeCanvas component
 *
 * @param component_id The component ID
 * @return User data pointer, or NULL if not set
 */
void* ir_native_canvas_get_user_data(uint32_t component_id);

/**
 * Invoke render callback (called by backend during rendering)
 *
 * This function is called by the rendering backend when it encounters
 * a NativeCanvas component in the IR tree.
 *
 * @param component_id The component ID
 * @return true if callback was invoked, false if no callback registered
 */
bool ir_native_canvas_invoke_callback(uint32_t component_id);

/**
 * Set background color for NativeCanvas
 *
 * @param component_id The component ID
 * @param color Background color in RGBA format (0xRRGGBBAA)
 */
void ir_native_canvas_set_background_color(uint32_t component_id, uint32_t color);

/**
 * Get NativeCanvas data from component
 *
 * @param component Component to get data from
 * @return NativeCanvas data, or NULL if component is not a NativeCanvas
 */
IRNativeCanvasData* ir_native_canvas_get_data(IRComponent* component);

#endif // IR_NATIVE_CANVAS_H
