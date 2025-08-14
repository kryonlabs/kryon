#include "color_utils.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


// Struct to hold a named color and its 32-bit RGBA value
typedef struct {
    const char *name;
    uint32_t value; // Format: 0xRRGGBBAA
} ColorNameEntry;

// =============================================================================
//  CSS NAMED COLOR TABLE
// =============================================================================
// This array contains all 148 standard CSS named colors.
//
// CRITICAL: THIS ARRAY MUST BE KEPT SORTED ALPHABETICALLY BY NAME
//           FOR THE BINARY SEARCH TO FUNCTION CORRECTLY.
//
static const ColorNameEntry color_name_map[] = {
    {"aliceblue", 0xf0f8ffff}, {"antiquewhite", 0xfaebd7ff}, {"aqua", 0x00ffffff},
    {"aquamarine", 0x7fffd4ff}, {"azure", 0xf0ffffff}, {"beige", 0xf5f5dcff},
    {"bisque", 0xffe4c4ff}, {"black", 0x000000ff}, {"blanchedalmond", 0xffebcdff},
    {"blue", 0x0000ffff}, {"blueviolet", 0x8a2be2ff}, {"brown", 0xa52a2aff},
    {"burlywood", 0xdeb887ff}, {"cadetblue", 0x5f9ea0ff}, {"chartreuse", 0x7fff00ff},
    {"chocolate", 0xd2691eff}, {"coral", 0xff7f50ff}, {"cornflowerblue", 0x6495edff},
    {"cornsilk", 0xfff8dcff}, {"crimson", 0xdc143cff}, {"cyan", 0x00ffffff},
    {"darkblue", 0x00008bff}, {"darkcyan", 0x008b8bff}, {"darkgoldenrod", 0xb8860bff},
    {"darkgray", 0xa9a9a9ff}, {"darkgreen", 0x006400ff}, {"darkgrey", 0xa9a9a9ff},
    {"darkkhaki", 0xbdb76bff}, {"darkmagenta", 0x8b008bff}, {"darkolivegreen", 0x556b2fff},
    {"darkorange", 0xff8c00ff}, {"darkorchid", 0x9932ccff}, {"darkred", 0x8b0000ff},
    {"darksalmon", 0xe9967aff}, {"darkseagreen", 0x8fbc8fff}, {"darkslateblue", 0x483d8bff},
    {"darkslategray", 0x2f4f4fff}, {"darkslategrey", 0x2f4f4fff}, {"darkturquoise", 0x00ced1ff},
    {"darkviolet", 0x9400d3ff}, {"deeppink", 0xff1493ff}, {"deepskyblue", 0x00bfffff},
    {"dimgray", 0x696969ff}, {"dimgrey", 0x696969ff}, {"dodgerblue", 0x1e90ffff},
    {"firebrick", 0xb22222ff}, {"floralwhite", 0xfffaf0ff}, {"forestgreen", 0x228b22ff},
    {"fuchsia", 0xff00ffff}, {"gainsboro", 0xdcdcdcff}, {"ghostwhite", 0xf8f8ffff},
    {"gold", 0xffd700ff}, {"goldenrod", 0xdaa520ff}, {"gray", 0x808080ff},
    {"green", 0x008000ff}, {"greenyellow", 0xadff2fff}, {"grey", 0x808080ff},
    {"honeydew", 0xf0fff0ff}, {"hotpink", 0xff69b4ff}, {"indianred", 0xcd5c5cff},
    {"indigo", 0x4b0082ff}, {"ivory", 0xfffff0ff}, {"khaki", 0xf0e68cff},
    {"lavender", 0xe6e6faff}, {"lavenderblush", 0xfff0f5ff}, {"lawngreen", 0x7cfc00ff},
    {"lemonchiffon", 0xfffacdff}, {"lightblue", 0xadd8e6ff}, {"lightcoral", 0xf08080ff},
    {"lightcyan", 0xe0ffffff}, {"lightgoldenrodyellow", 0xfafad2ff}, {"lightgray", 0xd3d3d3ff},
    {"lightgreen", 0x90ee90ff}, {"lightgrey", 0xd3d3d3ff}, {"lightpink", 0xffb6c1ff},
    {"lightsalmon", 0xffa07aff}, {"lightseagreen", 0x20b2aaff}, {"lightskyblue", 0x87cefaff},
    {"lightslategray", 0x778899ff}, {"lightslategrey", 0x778899ff}, {"lightsteelblue", 0xb0c4deff},
    {"lightyellow", 0xffffe0ff}, {"lime", 0x00ff00ff}, {"limegreen", 0x32cd32ff},
    {"linen", 0xfaf0e6ff}, {"magenta", 0xff00ffff}, {"maroon", 0x800000ff},
    {"mediumaquamarine", 0x66cdaaff}, {"mediumblue", 0x0000cdff}, {"mediumorchid", 0xba55d3ff},
    {"mediumpurple", 0x9370dbff}, {"mediumseagreen", 0x3cb371ff}, {"mediumslateblue", 0x7b68eeff},
    {"mediumspringgreen", 0x00fa9aff}, {"mediumturquoise", 0x48d1ccff}, {"mediumvioletred", 0xc71585ff},
    {"midnightblue", 0x191970ff}, {"mintcream", 0xf5fffaff}, {"mistyrose", 0xffe4e1ff},
    {"moccasin", 0xffe4b5ff}, {"navajowhite", 0xffdeadff}, {"navy", 0x000080ff},
    {"oldlace", 0xfdf5e6ff}, {"olive", 0x808000ff}, {"olivedrab", 0x6b8e23ff},
    {"orange", 0xffa500ff}, {"orangered", 0xff4500ff}, {"orchid", 0xda70d6ff},
    {"palegoldenrod", 0xeee8aaff}, {"palegreen", 0x98fb98ff}, {"paleturquoise", 0xafeeeeff},
    {"palevioletred", 0xdb7093ff}, {"papayawhip", 0xffefd5ff}, {"peachpuff", 0xffdab9ff},
    {"peru", 0xcd853fff}, {"pink", 0xffc0cbff}, {"plum", 0xdda0ddff},
    {"powderblue", 0xb0e0e6ff}, {"purple", 0x800080ff}, {"rebeccapurple", 0x663399ff},
    {"red", 0xff0000ff}, {"rosybrown", 0xbc8f8fff}, {"royalblue", 0x4169e1ff},
    {"saddlebrown", 0x8b4513ff}, {"salmon", 0xfa8072ff}, {"sandybrown", 0xf4a460ff},
    {"seagreen", 0x2e8b57ff}, {"seashell", 0xfff5eeff}, {"sienna", 0xa0522dff},
    {"silver", 0xc0c0c0ff}, {"skyblue", 0x87ceebff}, {"slateblue", 0x6a5acdff},
    {"slategray", 0x708090ff}, {"slategrey", 0x708090ff}, {"snow", 0xfffafaff},
    {"springgreen", 0x00ff7fff}, {"steelblue", 0x4682b4ff}, {"tan", 0xd2b48cff},
    {"teal", 0x008080ff}, {"thistle", 0xd8bfd8ff}, {"tomato", 0xff6347ff},
    {"transparent", 0x00000000}, {"turquoise", 0x40e0d0ff}, {"violet", 0xee82eeff},
    {"wheat", 0xf5deb3ff}, {"white", 0xffffffff}, {"whitesmoke", 0xf5f5f5ff},
    {"yellow", 0xffff00ff}, {"yellowgreen", 0x9acd32ff}
};
static const size_t color_name_map_size = sizeof(color_name_map) / sizeof(color_name_map[0]);

/**
 * @brief Performs a binary search for a color name in the sorted map.
 * @param name The color name to search for.
 * @return A pointer to the matching ColorNameEntry, or NULL if not found.
 */
static const ColorNameEntry* find_color_by_name(const char* name) {
    int low = 0;
    int high = color_name_map_size - 1;
    
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(name, color_name_map[mid].name);
        
        if (cmp == 0) {
            return &color_name_map[mid]; // Found
        } else if (cmp < 0) {
            high = mid - 1; // Search in lower half
        } else {
            low = mid + 1;  // Search in upper half
        }
    }
    
    return NULL; // Not found
}

uint32_t kryon_color_parse_string(const char *color_str) {
    if (!color_str || *color_str == '\0') {
        return 0x000000FF; // Default to opaque black
    }
    
    // 1. Handle Hex Formats: #rgb, #rrggbb, #rrggbbaa
    if (color_str[0] == '#') {
        const char *hex_part = color_str + 1;
        size_t len = strlen(hex_part);
        unsigned int r, g, b, a = 255;

        if (len == 3 && sscanf(hex_part, "%1x%1x%1x", &r, &g, &b) == 3) {
            return (uint32_t)((r * 17) << 24 | (g * 17) << 16 | (b * 17) << 8 | a);
        }
        if (len == 6 && sscanf(hex_part, "%2x%2x%2x", &r, &g, &b) == 3) {
            return (uint32_t)(r << 24 | g << 16 | b << 8 | a);
        }
        if (len == 8 && sscanf(hex_part, "%2x%2x%2x%2x", &r, &g, &b, &a) == 4) {
            return (uint32_t)(r << 24 | g << 16 | b << 8 | a);
        }
    }

    // 2. Handle rgb() and rgba() formats
    if (strncmp(color_str, "rgb", 3) == 0) {
        int r, g, b;
        float a = 1.0f;
        
        if (sscanf(color_str, "rgba(%d, %d, %d, %f)", &r, &g, &b, &a) == 4 ||
            sscanf(color_str, "rgb(%d, %d, %d)", &r, &g, &b) == 3) {
            
            uint8_t alpha_val = (uint8_t)(a * 255.0f);
            return (uint32_t)(((uint8_t)r) << 24 | ((uint8_t)g) << 16 | ((uint8_t)b) << 8 | alpha_val);
        }
    }

    // 3. Handle Named Colors (case-insensitive)
    char lower_name[32]; // Longest color name is "lightgoldenrodyellow" (22 chars)
    size_t len = strlen(color_str);
    if (len >= sizeof(lower_name)) {
        return 0x000000FF; // Input too long to be a standard color name
    }

    for (size_t i = 0; i < len; ++i) {
        lower_name[i] = tolower((unsigned char)color_str[i]);
    }
    lower_name[len] = '\0';
    
    const ColorNameEntry* entry = find_color_by_name(lower_name);
    if (entry) {
        return entry->value;
    }

    // 4. Default if no format matches
    return 0x000000FF;
}



KryonColor color_u32_to_f32(uint32_t c) {
    return (KryonColor){
        ((c >> 24) & 0xFF) / 255.0f,
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >>  8) & 0xFF) / 255.0f,
        ((c >>  0) & 0xFF) / 255.0f
    };
}

KryonColor color_lighten(KryonColor color, float factor) {
    color.r = MIN(1.0f, color.r + color.r * factor);
    color.g = MIN(1.0f, color.g + color.g * factor);
    color.b = MIN(1.0f, color.b + color.b * factor);
    return color;
}

KryonColor color_desaturate(KryonColor color, float factor) {
    float gray = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
    float inv_factor = 1.0f - factor;
    color.r = color.r * inv_factor + gray * factor;
    color.g = color.g * inv_factor + gray * factor;
    color.b = color.b * inv_factor + gray * factor;
    return color;
}
