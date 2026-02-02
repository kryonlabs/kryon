/**
 * @file color_utils.c
 * @brief Color Utility Functions Implementation
 *
 * Implementation of shared color normalization and manipulation utilities.
 */

#define _POSIX_C_SOURCE 200809L

#include "color_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Named Color Table
// ============================================================================

typedef struct {
    const char* name;
    const char* hex;
} NamedColor;

static const NamedColor named_colors[] = {
    // Basic colors
    {"black",   "#000000"},
    {"white",   "#ffffff"},
    {"red",     "#ff0000"},
    {"green",   "#00ff00"},
    {"blue",    "#0000ff"},
    {"yellow",  "#ffff00"},
    {"cyan",    "#00ffff"},
    {"magenta", "#ff00ff"},

    // Extended colors
    {"gray",    "#808080"},
    {"grey",    "#808080"},
    {"orange",  "#ff8000"},
    {"pink",    "#ffc0cb"},
    {"purple",  "#800080"},
    {"brown",   "#a52a2a"},
    {"violet",  "#ee82ee"},

    // Light variants
    {"lightgray",    "#d3d3d3"},
    {"lightgrey",    "#d3d3d3"},
    {"darkgray",     "#a9a9a9"},
    {"darkgrey",     "#a9a9a9"},

    // Transparent
    {"transparent", "#00000000"},

    {NULL, NULL}
};

// ============================================================================
// Helper Functions
// ============================================================================

static char* lowercase_strdup(const char* str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char* result = malloc(len + 1);
    if (!result) return NULL;

    for (size_t i = 0; i < len; i++) {
        result[i] = tolower((unsigned char)str[i]);
    }
    result[len] = '\0';

    return result;
}

// ============================================================================
// Color Normalization
// ============================================================================

char* color_normalize_for_tcl(const char* color) {
    return color_normalize_for_target(color, "tcl");
}

char* color_normalize_for_target(const char* color, const char* target) {
    if (!color || !*color) {
        return NULL;
    }

    // Check if it's a hex color starting with #
    if (color[0] != '#') {
        // Not a hex color - check if it's a named color
        const char* hex = color_name_to_hex(color);
        if (hex) {
            color = hex;  // Use the hex version
        } else {
            // Unknown named color, return as-is
            return strdup(color);
        }
    }

    size_t len = strlen(color);

    // RGBA format: #RRGGBBAA (9 characters with #)
    // RGB format: #RRGGBB (7 characters with #)

    bool is_css = (target && strcmp(target, "css") == 0);
    bool is_c = (target && strcmp(target, "c") == 0);

    if (len == 9) {
        // Has alpha channel
        if (is_css) {
            // Convert to rgba(r,g,b,a) format
            if (len >= 9) {
                uint8_t r, g, b, a;
                if (sscanf(color + 1, "%2hhx%2hhx%2hhx%2hhx", &r, &g, &b, &a) == 4) {
                    char* result = malloc(32);
                    if (result) {
                        snprintf(result, 32, "rgba(%d,%d,%d,%.2f)", r, g, b, a / 255.0f);
                        return result;
                    }
                }
            }
            return NULL;
        } else if (is_c) {
            // Return as 0xRRGGBB
            char* result = malloc(10);
            if (result) {
                strncpy(result, color, 7);  // Copy #RRGGBB
                result[0] = '0';
                result[1] = 'x';
                result[7] = '\0';
                return result;
            }
            return NULL;
        } else {
            // Strip alpha channel (last 2 characters)
            char* normalized = malloc(8);  // # + 6 hex digits + null terminator
            if (normalized) {
                strncpy(normalized, color, 7);  // Copy #RRGGBB
                normalized[7] = '\0';
                return normalized;
            }
            return NULL;
        }
    }

    // No alpha, return as-is
    if (is_c && len == 7) {
        char* result = malloc(10);
        if (result) {
            result[0] = '0';
            result[1] = 'x';
            strncpy(result + 2, color + 1, 6);
            result[8] = '\0';
            return result;
        }
        return NULL;
    }

    return strdup(color);
}

bool color_is_transparent(const char* color) {
    if (!color || !*color) {
        return false;
    }

    // Check for named "transparent"
    char* lower = lowercase_strdup(color);
    bool is_transparent = (strcmp(lower, "transparent") == 0);
    free(lower);

    if (is_transparent) {
        return true;
    }

    // Check for #00000000 or any color with alpha = 00
    if (color[0] == '#') {
        size_t len = strlen(color);
        if (len == 9 && strcmp(color, "#00000000") == 0) {
            return true;
        }
    }

    return false;
}

bool color_parse_rgba(const char* color,
                      uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_a) {
    if (!color || !out_r || !out_g || !out_b) {
        return false;
    }

    // Default alpha
    if (out_a) *out_a = 255;

    // Check for named color
    if (color[0] != '#') {
        const char* hex = color_name_to_hex(color);
        if (hex) {
            color = hex;
        } else {
            return false;
        }
    }

    size_t len = strlen(color);

    // #RGB format (3 hex digits + #)
    if (len == 4) {
        unsigned int r, g, b;
        if (sscanf(color + 1, "%1x%1x%1x", &r, &g, &b) == 3) {
            *out_r = r * 17;  // 0xf -> 0xff
            *out_g = g * 17;
            *out_b = b * 17;
            return true;
        }
        return false;
    }

    // #RRGGBB format (6 hex digits + #)
    if (len == 7) {
        unsigned int r, g, b;
        if (sscanf(color + 1, "%2x%2x%2x", &r, &g, &b) == 3) {
            *out_r = (uint8_t)r;
            *out_g = (uint8_t)g;
            *out_b = (uint8_t)b;
            return true;
        }
        return false;
    }

    // #RRGGBBAA format (8 hex digits + #)
    if (len == 9 && out_a) {
        unsigned int r, g, b, a;
        if (sscanf(color + 1, "%2x%2x%2x%2x", &r, &g, &b, &a) == 4) {
            *out_r = (uint8_t)r;
            *out_g = (uint8_t)g;
            *out_b = (uint8_t)b;
            *out_a = (uint8_t)a;
            return true;
        }
        return false;
    }

    return false;
}

const char* color_format_hex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                             bool include_alpha) {
    // Static buffer for thread safety (single-threaded use case)
    static char buffer[10];

    if (include_alpha) {
        snprintf(buffer, sizeof(buffer), "#%02x%02x%02x%02x", r, g, b, a);
    } else {
        snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", r, g, b);
    }

    return buffer;
}

const char* color_name_to_hex(const char* name) {
    if (!name || !*name) {
        return NULL;
    }

    // Convert to lowercase for case-insensitive lookup
    char* lower = lowercase_strdup(name);
    if (!lower) {
        return NULL;
    }

    // Search in named color table
    const char* result = NULL;
    for (int i = 0; named_colors[i].name != NULL; i++) {
        if (strcmp(lower, named_colors[i].name) == 0) {
            result = named_colors[i].hex;
            break;
        }
    }

    free(lower);
    return result;
}
