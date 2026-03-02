/*
 * Kryon Utility Functions
 * C89/C90 compliant
 *
 * Common utility functions for color, geometry, and string operations
 */

#ifndef KRYON_UTIL_H
#define KRYON_UTIL_H

#include <stddef.h>
#include "graphics.h"  /* For Rectangle and Point types */

/*
 * Common macros (if not already defined)
 */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)
#endif

/*
 * Color utilities
 * Format: 0xRRGGBBAA (red in bits 24-31, alpha in bits 0-7)
 */

/*
 * Parse hex color string "#RRGGBB" to uint32_t 0xRRGGBBAA
 * Returns color value, or 0 on error
 */
unsigned long util_parse_color(const char *hex_color);

/*
 * Lighten a color by ~30% for highlight edges (Tk-style bevel)
 */
unsigned long util_lighten_color(unsigned long color);

/*
 * Darken a color by ~30% for shadow edges (Tk-style bevel)
 */
unsigned long util_darken_color(unsigned long color);

/*
 * Geometry utilities
 */

/*
 * Calculate handle center position for slider
 * track_rect: The track rectangle
 * value: Current value (0-100)
 * handle_radius: Radius of the handle (unused, for API compatibility)
 * Returns: Center point of the handle
 */
Point util_calculate_handle_position(Rectangle track_rect,
                                     float value, int handle_radius);

/*
 * Check if point is inside handle (square region for detection)
 */
int util_is_point_in_handle(Point p, Point center, int radius);

/*
 * Calculate slider value from mouse position
 * Returns value in range [0.0, 100.0]
 */
float util_calculate_value_from_position(int mouse_x, Rectangle track_rect);

/*
 * Get the bounding rectangle for a two-square handle (Tk style)
 */
Rectangle util_get_two_square_handle_rect(Point center);

#endif /* KRYON_UTIL_H */
