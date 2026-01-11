/**
 * @file ir_color_utils.c
 * @brief Color-related utility functions implementation
 */

#define _GNU_SOURCE
#include "ir_color_utils.h"
#include "ir_style_types.h"
#include <string.h>
#include <ctype.h>

// Case-insensitive string comparison
static int ir_str_ieq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

IRColor ir_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return IR_COLOR_RGBA(r, g, b, 255);
}

IRColor ir_color_transparent(void) {
    return IR_COLOR_RGBA(0, 0, 0, 0);
}

IRColor ir_color_named(const char* name) {
    if (!name) return ir_color_rgba(255, 255, 255, 255);

    // Core named colors (CSS-like)
    struct { const char* name; uint8_t r, g, b; } colors[] = {
        {"white", 255, 255, 255}, {"black", 0, 0, 0},
        {"gray", 128, 128, 128}, {"grey", 128, 128, 128},
        {"lightgray", 211, 211, 211}, {"lightgrey", 211, 211, 211},
        {"darkgray", 169, 169, 169}, {"darkgrey", 169, 169, 169},
        {"silver", 192, 192, 192}, {"red", 255, 0, 0},
        {"green", 0, 255, 0}, {"blue", 0, 0, 255},
        {"yellow", 255, 255, 0}, {"orange", 255, 165, 0},
        {"purple", 128, 0, 128}, {"pink", 255, 192, 203},
        {"brown", 165, 42, 42}, {"cyan", 0, 255, 255},
        {"magenta", 255, 0, 255}, {"lime", 0, 255, 0},
        {"olive", 128, 128, 0}, {"navy", 0, 0, 128},
        {"teal", 0, 128, 128}, {"aqua", 0, 255, 255},
        {"maroon", 128, 0, 0}, {"fuchsia", 255, 0, 255},
        {"lightblue", 173, 216, 230}, {"lightgreen", 144, 238, 144},
        {"lightpink", 255, 182, 193}, {"lightyellow", 255, 255, 224},
        {"lightcyan", 224, 255, 255}, {"darkred", 139, 0, 0},
        {"darkgreen", 0, 100, 0}, {"darkblue", 0, 0, 139},
        {"darkorange", 255, 140, 0}, {"darkviolet", 148, 0, 211},
        {"transparent", 0, 0, 0}, {NULL, 0, 0, 0}
    };

    for (int i = 0; colors[i].name; i++) {
        if (ir_str_ieq(name, colors[i].name)) {
            if (ir_str_ieq(name, "transparent")) {
                return ir_color_rgba(0, 0, 0, 0);
            }
            return ir_color_rgba(colors[i].r, colors[i].g, colors[i].b, 255);
        }
    }
    return ir_color_rgba(255, 255, 255, 255); // default to white
}
