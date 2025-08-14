#ifndef KRYON_COLOR_UTILS_H
#define KRYON_COLOR_UTILS_H

#include <stdint.h>
#include "types.h"

/**
 * @file color_utils.h
 * @brief Provides a high-performance utility for parsing color values from strings.
 * 
 * This module supports the full set of CSS named colors, as well as hex, rgb,
 * and rgba formats.
 */

/**
 * @brief Parses a color string into a 32-bit RGBA value.
 *
 * This function can parse the following formats:
 * - **Named Colors:** "red", "lightgoldenrodyellow", "transparent", etc. (all CSS names).
 * - **Hex3:** "#f0c" -> 0xFF00CCFF
 * - **Hex6:** "#ff00cc" -> 0xFF00CCFF
 * - **Hex8:** "#ff00cc80" -> 0xFF00CC80
 * - **RGB:** "rgb(255, 0, 204)" -> 0xFF00CCFF
 * - **RGBA:** "rgba(255, 0, 204, 0.5)" -> 0xFF00CC80
 *
 * The function is case-insensitive for named colors.
 *
 * @param color_str The null-terminated string representing the color.
 * @return A 32-bit unsigned integer in 0xRRGGBBAA format.
 *         Returns 0x000000FF (opaque black) if the string is null or cannot be parsed.
 */
uint32_t kryon_color_parse_string(const char *color_str);


/**
 * @brief Converts a 32-bit RGBA integer (0xRRGGBBAA) to a KryonColor (4 floats).
 */
 KryonColor color_u32_to_f32(uint32_t c);

 /**
  * @brief Lightens a color by a given factor (0.0 to 1.0).
  */
 KryonColor color_lighten(KryonColor color, float factor);
 
 /**
  * @brief Desaturates a color by a given factor (0.0 to 1.0).
  */
 KryonColor color_desaturate(KryonColor color, float factor);

#endif // KRYON_COLOR_UTILS_H
