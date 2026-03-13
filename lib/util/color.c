/*
 * Kryon Color Utilities
 * C89/C90 compliant
 *
 * Color parsing and manipulation functions
 */

#include "graphics.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Parse hex color string "#RRGGBB" to uint32_t 0xRRGGBBAA
 * Returns color value, or 0 on error
 */
unsigned long util_parse_color(const char *hex_color)
{
    unsigned long r, g, b;

    if (hex_color == NULL || hex_color[0] != '#') {
        return 0;
    }

    /* Skip '#' */
    hex_color++;

    /* Parse RGB */
    if (sscanf(hex_color, "%2lx%2lx%2lx", &r, &g, &b) != 3) {
        return 0;
    }

    /* Return in 0xRRGGBBAA format (AA = FF for opaque) */
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

/*
 * Lighten a color by ~30% for highlight edges (Tk-style bevel)
 */
unsigned long util_lighten_color(unsigned long color)
{
    int r, g, b, a;

    r = ((color >> 24) & 0xFF);
    g = ((color >> 16) & 0xFF);
    b = ((color >> 8) & 0xFF);
    a = color & 0xFF;

    /* Lighten by blending with white (7/8 original + 1/8 white) */
    r = (r * 7 + 255) / 8;
    g = (g * 7 + 255) / 8;
    b = (b * 7 + 255) / 8;

    return ((unsigned long)r << 24) | ((unsigned long)g << 16) |
           ((unsigned long)b << 8) | a;
}

/*
 * Darken a color by ~30% for shadow edges (Tk-style bevel)
 */
unsigned long util_darken_color(unsigned long color)
{
    int r, g, b, a;

    r = ((color >> 24) & 0xFF);
    g = ((color >> 16) & 0xFF);
    b = ((color >> 8) & 0xFF);
    a = color & 0xFF;

    /* Darken to 2/3 of original value */
    r = r * 2 / 3;
    g = g * 2 / 3;
    b = b * 2 / 3;

    return ((unsigned long)r << 24) | ((unsigned long)g << 16) |
           ((unsigned long)b << 8) | a;
}
