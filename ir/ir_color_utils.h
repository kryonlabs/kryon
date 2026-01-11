/**
 * @file ir_color_utils.h
 * @brief Color-related utility functions
 *
 * This module provides color utilities that are NOT in ir_style_types.c.
 * ir_style_types.c has: ir_color_rgba(), ir_color_equals(), ir_color_free()
 * This module has: named colors, RGB helper, transparent helper
 */

#ifndef IR_COLOR_UTILS_H
#define IR_COLOR_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration for IRColor (defined in ir_style_types.h)
typedef struct IRColor IRColor;

/**
 * @brief Create a solid RGB color (alpha = 255)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return IRColor structure
 */
IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Create a transparent color (all zeros)
 * @return IRColor structure with alpha = 0
 */
IRColor ir_color_transparent(void);

/**
 * @brief Create a color from a named color (CSS-like)
 * @param name Color name (e.g., "red", "blue", "white", "transparent")
 * @return IRColor structure, defaults to white if name not found
 */
IRColor ir_color_named(const char* name);

#endif // IR_COLOR_UTILS_H
