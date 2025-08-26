/**
 * @file element_mixins.h
 * @brief Shared element functionality - header for element mixins
 * 
 * This header provides reusable functions that multiple elements can use
 * to avoid code duplication while maintaining the VTable architecture.
 * 
 * 0BSD License
 */

#ifndef KRYON_ELEMENT_MIXINS_H
#define KRYON_ELEMENT_MIXINS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include "renderer_interface.h"

// Forward declarations
struct KryonElement;
struct KryonRuntime;
struct ElementEvent;

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
                                size_t max_commands);

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
bool handle_script_events(struct KryonRuntime* runtime, 
                         struct KryonElement* element, 
                         const struct ElementEvent* event);

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
                                 float min_height);

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
                              float default_height);

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
bool check_hover_and_cursor(struct KryonRuntime* runtime, 
                           struct KryonElement* element, 
                           bool enabled);

/**
 * @brief Apply visual state modifications to colors based on interaction.
 * This consolidates the color modification logic for hover/disabled states.
 * 
 * @param bg_color Background color to modify
 * @param text_color Text color to modify
 * @param is_hovered Whether element is hovered
 * @param is_enabled Whether element is enabled
 */
void apply_interaction_colors(KryonColor* bg_color, 
                            KryonColor* text_color, 
                            bool is_hovered, 
                            bool is_enabled);

/**
 * @brief Renders background and border for input-like elements with focus support.
 * Similar to render_background_and_border but handles focus states and custom border logic.
 * 
 * @param element Element to render background/border for
 * @param commands Render command buffer
 * @param command_count Current command count (will be incremented)  
 * @param max_commands Maximum number of commands allowed
 * @param has_focus Whether the element currently has focus
 * @param is_disabled Whether the element is disabled
 * @return true if background/border was rendered, false if skipped
 */
bool render_input_background_and_border(struct KryonElement* element,
                                       KryonRenderCommand* commands,
                                       size_t* command_count,
                                       size_t max_commands,
                                       bool has_focus,
                                       bool is_disabled);

#ifdef __cplusplus
}
#endif

#endif // KRYON_ELEMENT_MIXINS_H