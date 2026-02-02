/**
 * @file color_utils.h
 * @brief Color Utility Functions
 *
 * Shared color normalization and manipulation utilities for code generators.
 * Extracted from tcltk_from_wir.c for reuse across multiple emitters.
 *
 * @copyright Kryon UI Framework
 * @version alpha
 */

#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Color Normalization
// ============================================================================

/**
 * Normalize color for Tcl/Tk.
 * Converts RGBA (8-digit hex) colors to RGB (6-digit hex) by stripping alpha channel.
 * Tcl/Tk doesn't support alpha channels in color specifications.
 *
 * This is the default behavior for most toolkits. If a specific format is needed,
 * use color_normalize_for_target() instead.
 *
 * @param color Input color string (e.g., "#ff0000" or "#ff000080")
 * @return Normalized color string (caller must free), or NULL on error
 */
char* color_normalize_for_tcl(const char* color);

/**
 * Normalize color for a specific target format.
 *
 * Supported formats:
 * - "tcl": Strip alpha, return #RRGGBB
 * - "tk": Strip alpha, return #RRGGBB (same as tcl)
 * - "css": Keep alpha if present, return rgba(r,g,b,a) or #rrggbb
 * - "limbo": Strip alpha, return #RRGGBB
 * - "c": Return hex string 0xRRGGBB
 *
 * @param color Input color string
 * @param target Target format (e.g., "tcl", "css", "c")
 * @return Normalized color string (caller must free), or NULL on error
 */
char* color_normalize_for_target(const char* color, const char* target);

/**
 * Check if a color string is transparent.
 * Recognizes "#00000000", "transparent", and fully transparent alpha values.
 *
 * @param color Color string
 * @return true if color is transparent, false otherwise
 */
bool color_is_transparent(const char* color);

/**
 * Parse hex color to RGBA components.
 * Supports #RGB, #RRGGBB, #RRGGBBAA formats.
 *
 * @param color Color string
 * @param out_r Output for red (0-255)
 * @param out_g Output for green (0-255)
 * @param out_b Output for blue (0-255)
 * @param out_a Output for alpha (0-255), defaults to 255 if not specified
 * @return true on success, false on parse error
 */
bool color_parse_rgba(const char* color,
                      uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_a);

/**
 * Format RGBA components to a hex color string.
 * The returned string is statically allocated and must not be freed.
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 * @param include_alpha Whether to include alpha (#RRGGBBAA vs #RRGGBB)
 * @return Static buffer containing hex color string
 */
const char* color_format_hex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                             bool include_alpha);

/**
 * Convert named color to hex.
 * Supports common color names: red, green, blue, white, black, etc.
 *
 * @param name Named color (e.g., "red", "blue", "white")
 * @return Hex color string (statically allocated, do not free), or NULL if unknown
 */
const char* color_name_to_hex(const char* name);

#ifdef __cplusplus
}
#endif

#endif // COLOR_UTILS_H
