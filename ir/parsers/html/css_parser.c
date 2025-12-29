#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "css_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper: Skip whitespace
static const char* skip_whitespace(const char* str) {
    while (*str && isspace(*str)) str++;
    return str;
}

// Helper: Trim whitespace from string (modifies string in place)
static void trim_whitespace(char* str) {
    if (!str) return;

    // Trim leading
    char* start = str;
    while (*start && isspace(*start)) start++;

    // Trim trailing
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    *(end + 1) = '\0';

    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// ============================================================================
// GRADIENT PARSING HELPERS
// ============================================================================

// Named CSS color lookup table
typedef struct {
    const char* name;
    uint8_t r, g, b;
} NamedColor;

static const NamedColor named_colors[] = {
    {"black", 0, 0, 0},
    {"white", 255, 255, 255},
    {"red", 255, 0, 0},
    {"green", 0, 128, 0},
    {"blue", 0, 0, 255},
    {"yellow", 255, 255, 0},
    {"cyan", 0, 255, 255},
    {"magenta", 255, 0, 255},
    {"orange", 255, 165, 0},
    {"purple", 128, 0, 128},
    {"pink", 255, 192, 203},
    {"brown", 165, 42, 42},
    {"gray", 128, 128, 128},
    {"grey", 128, 128, 128},
    {"lime", 0, 255, 0},
    {"navy", 0, 0, 128},
    {"teal", 0, 128, 128},
    {"olive", 128, 128, 0},
    {"maroon", 128, 0, 0},
    {"silver", 192, 192, 192},
    {"aqua", 0, 255, 255},
    {"fuchsia", 255, 0, 255},
    {"gold", 255, 215, 0},
    {"indigo", 75, 0, 130},
    {"violet", 238, 130, 238},
    {"coral", 255, 127, 80},
    {"salmon", 250, 128, 114},
    {"khaki", 240, 230, 140},
    {"plum", 221, 160, 221},
    {"orchid", 218, 112, 214},
    {"tomato", 255, 99, 71},
    {"turquoise", 64, 224, 208},
    {"wheat", 245, 222, 179},
    {"beige", 245, 245, 220},
    {"ivory", 255, 255, 240},
    {"lavender", 230, 230, 250},
    {"linen", 250, 240, 230},
    {"snow", 255, 250, 250},
    {"azure", 240, 255, 255},
    {"honeydew", 240, 255, 240},
    {"mintcream", 245, 255, 250},
    {"aliceblue", 240, 248, 255},
    {"ghostwhite", 248, 248, 255},
    {"seashell", 255, 245, 238},
    {"oldlace", 253, 245, 230},
    {"floralwhite", 255, 250, 240},
    {"antiquewhite", 250, 235, 215},
    {"papayawhip", 255, 239, 213},
    {"blanchedalmond", 255, 235, 205},
    {"bisque", 255, 228, 196},
    {"moccasin", 255, 228, 181},
    {"navajowhite", 255, 222, 173},
    {"peachpuff", 255, 218, 185},
    {"mistyrose", 255, 228, 225},
    {"lavenderblush", 255, 240, 245},
    {"cornsilk", 255, 248, 220},
    {"lemonchiffon", 255, 250, 205},
    {"lightyellow", 255, 255, 224},
    {"lightgoldenrodyellow", 250, 250, 210},
    {NULL, 0, 0, 0}  // Sentinel
};

/**
 * Parse a named CSS color to RGB values
 */
static bool parse_named_color(const char* name, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!name || !r || !g || !b) return false;

    // Convert to lowercase for comparison
    char lower[64];
    size_t len = strlen(name);
    if (len >= sizeof(lower)) return false;

    for (size_t i = 0; i <= len; i++) {
        lower[i] = tolower((unsigned char)name[i]);
    }

    // Search in named colors table
    for (const NamedColor* nc = named_colors; nc->name != NULL; nc++) {
        if (strcmp(lower, nc->name) == 0) {
            *r = nc->r;
            *g = nc->g;
            *b = nc->b;
            return true;
        }
    }

    return false;
}

/**
 * Parse gradient angle from string (e.g., "45deg", "0.5turn", "100grad", "1rad")
 * Returns angle in degrees
 */
static bool parse_gradient_angle(const char* str, float* out_degrees) {
    if (!str || !out_degrees) return false;

    float value;
    char unit[16] = "";

    if (sscanf(str, "%f%15s", &value, unit) < 1) {
        return false;
    }

    // Convert to degrees based on unit
    if (strcmp(unit, "deg") == 0 || unit[0] == '\0') {
        *out_degrees = value;
    } else if (strcmp(unit, "turn") == 0) {
        *out_degrees = value * 360.0f;
    } else if (strcmp(unit, "grad") == 0) {
        *out_degrees = value * 0.9f;  // 400 gradians = 360 degrees
    } else if (strcmp(unit, "rad") == 0) {
        *out_degrees = value * (180.0f / (float)M_PI);
    } else {
        return false;
    }

    // Normalize to 0-360 range
    while (*out_degrees < 0) *out_degrees += 360.0f;
    while (*out_degrees >= 360.0f) *out_degrees -= 360.0f;

    return true;
}

/**
 * Parse gradient direction keyword (e.g., "to top", "to bottom right")
 * Returns angle in degrees
 */
static bool parse_gradient_direction(const char* str, float* out_degrees) {
    if (!str || !out_degrees) return false;

    // Skip "to " prefix
    const char* dir = str;
    if (strncmp(dir, "to ", 3) == 0) {
        dir += 3;
    } else {
        return false;  // Direction must start with "to"
    }

    // Skip whitespace
    while (*dir && isspace(*dir)) dir++;

    // Parse direction keywords
    // CSS gradient angles: 0deg = to top, 90deg = to right, 180deg = to bottom, 270deg = to left
    if (strcmp(dir, "top") == 0) {
        *out_degrees = 0.0f;
    } else if (strcmp(dir, "right") == 0) {
        *out_degrees = 90.0f;
    } else if (strcmp(dir, "bottom") == 0) {
        *out_degrees = 180.0f;
    } else if (strcmp(dir, "left") == 0) {
        *out_degrees = 270.0f;
    } else if (strcmp(dir, "top right") == 0 || strcmp(dir, "right top") == 0) {
        *out_degrees = 45.0f;
    } else if (strcmp(dir, "bottom right") == 0 || strcmp(dir, "right bottom") == 0) {
        *out_degrees = 135.0f;
    } else if (strcmp(dir, "bottom left") == 0 || strcmp(dir, "left bottom") == 0) {
        *out_degrees = 225.0f;
    } else if (strcmp(dir, "top left") == 0 || strcmp(dir, "left top") == 0) {
        *out_degrees = 315.0f;
    } else {
        return false;
    }

    return true;
}

/**
 * Parse a single color value (hex, rgb, rgba, or named) into RGBA
 */
static bool parse_color_value(const char* str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (!str || !r || !g || !b || !a) return false;

    // Skip whitespace
    while (*str && isspace(*str)) str++;

    // Default alpha
    *a = 255;

    // Try hex color
    if (str[0] == '#') {
        size_t len = strlen(str);
        unsigned int hex;

        if (len == 7 && sscanf(str, "#%06x", &hex) == 1) {
            *r = (hex >> 16) & 0xFF;
            *g = (hex >> 8) & 0xFF;
            *b = hex & 0xFF;
            return true;
        } else if (len == 4 && sscanf(str, "#%03x", &hex) == 1) {
            *r = ((hex >> 8) & 0xF) * 17;
            *g = ((hex >> 4) & 0xF) * 17;
            *b = (hex & 0xF) * 17;
            return true;
        } else if (len == 9 && sscanf(str, "#%08x", &hex) == 1) {
            // #RRGGBBAA
            *r = (hex >> 24) & 0xFF;
            *g = (hex >> 16) & 0xFF;
            *b = (hex >> 8) & 0xFF;
            *a = hex & 0xFF;
            return true;
        }
        return false;
    }

    // Try rgba()
    if (strncmp(str, "rgba(", 5) == 0) {
        unsigned int ri, gi, bi;
        float af;
        if (sscanf(str, "rgba(%u, %u, %u, %f)", &ri, &gi, &bi, &af) == 4 ||
            sscanf(str, "rgba(%u,%u,%u,%f)", &ri, &gi, &bi, &af) == 4) {
            *r = (uint8_t)ri;
            *g = (uint8_t)gi;
            *b = (uint8_t)bi;
            *a = (uint8_t)(af * 255.0f);
            return true;
        }
        return false;
    }

    // Try rgb()
    if (strncmp(str, "rgb(", 4) == 0) {
        unsigned int ri, gi, bi;
        if (sscanf(str, "rgb(%u, %u, %u)", &ri, &gi, &bi) == 3 ||
            sscanf(str, "rgb(%u,%u,%u)", &ri, &gi, &bi) == 3) {
            *r = (uint8_t)ri;
            *g = (uint8_t)gi;
            *b = (uint8_t)bi;
            return true;
        }
        return false;
    }

    // Try named color
    // Extract just the color name (before any whitespace or position)
    char name[64];
    int i = 0;
    while (str[i] && !isspace(str[i]) && str[i] != ')' && i < 63) {
        name[i] = str[i];
        i++;
    }
    name[i] = '\0';

    return parse_named_color(name, r, g, b);
}

/**
 * Parse a gradient color stop (e.g., "red", "#ff0000 50%", "rgba(255,0,0,1) 10%")
 * Returns the color and position (if specified)
 */
static bool parse_gradient_color_stop(const char* str, IRGradientStop* out_stop, bool* has_position) {
    if (!str || !out_stop || !has_position) return false;

    *has_position = false;
    out_stop->position = -1.0f;  // Sentinel for auto-position

    // Skip leading whitespace
    while (*str && isspace(*str)) str++;

    // Find where the color ends and position begins
    const char* pos_start = NULL;
    const char* p = str;

    // Handle function-based colors (rgb, rgba)
    if (strncmp(p, "rgb", 3) == 0) {
        // Find closing parenthesis
        int paren_depth = 0;
        while (*p) {
            if (*p == '(') paren_depth++;
            else if (*p == ')') {
                paren_depth--;
                if (paren_depth == 0) {
                    p++;
                    break;
                }
            }
            p++;
        }
        // Skip whitespace after closing paren
        while (*p && isspace(*p)) p++;
        if (*p) pos_start = p;
    } else if (*p == '#') {
        // Hex color - find end
        p++;
        while (*p && (isxdigit(*p))) p++;
        while (*p && isspace(*p)) p++;
        if (*p) pos_start = p;
    } else {
        // Named color - find end of name
        while (*p && !isspace(*p) && *p != ')') p++;
        while (*p && isspace(*p)) p++;
        if (*p && *p != ')') pos_start = p;
    }

    // Parse color
    char color_str[128];
    size_t color_len = pos_start ? (size_t)(pos_start - str) : strlen(str);
    if (color_len >= sizeof(color_str)) color_len = sizeof(color_str) - 1;
    strncpy(color_str, str, color_len);
    color_str[color_len] = '\0';
    trim_whitespace(color_str);

    if (!parse_color_value(color_str, &out_stop->r, &out_stop->g, &out_stop->b, &out_stop->a)) {
        return false;
    }

    // Parse position if present
    if (pos_start) {
        float pos;
        if (sscanf(pos_start, "%f%%", &pos) == 1) {
            out_stop->position = pos / 100.0f;
            *has_position = true;
        } else if (sscanf(pos_start, "%fpx", &pos) == 1) {
            // Pixel positions need context - store as-is for now
            // Will be converted to percentage based on gradient size
            out_stop->position = pos / 100.0f;  // Rough approximation
            *has_position = true;
        }
    }

    return true;
}

/**
 * Split gradient arguments by comma, respecting nested parentheses
 */
static char** split_gradient_args(const char* content, int* count) {
    if (!content || !count) return NULL;

    *count = 0;

    // Count arguments
    int arg_count = 1;
    int paren_depth = 0;
    for (const char* p = content; *p; p++) {
        if (*p == '(') paren_depth++;
        else if (*p == ')') paren_depth--;
        else if (*p == ',' && paren_depth == 0) arg_count++;
    }

    // Allocate array
    char** args = calloc(arg_count, sizeof(char*));
    if (!args) return NULL;

    // Split
    const char* start = content;
    int idx = 0;
    paren_depth = 0;

    for (const char* p = content; ; p++) {
        if (*p == '(') paren_depth++;
        else if (*p == ')') paren_depth--;
        else if ((*p == ',' && paren_depth == 0) || *p == '\0') {
            size_t len = p - start;
            args[idx] = malloc(len + 1);
            if (args[idx]) {
                strncpy(args[idx], start, len);
                args[idx][len] = '\0';
                trim_whitespace(args[idx]);
            }
            idx++;
            if (*p == '\0') break;
            start = p + 1;
        }
    }

    *count = idx;
    return args;
}

/**
 * Free split gradient arguments
 */
static void free_gradient_args(char** args, int count) {
    if (!args) return;
    for (int i = 0; i < count; i++) {
        free(args[i]);
    }
    free(args);
}

/**
 * Parse linear-gradient() CSS function
 * Syntax: linear-gradient([angle | to direction], color-stop1, color-stop2, ...)
 */
bool ir_css_parse_linear_gradient(const char* value, IRGradient** out_gradient) {
    if (!value || !out_gradient) return false;

    *out_gradient = NULL;

    // Check prefix
    if (strncmp(value, "linear-gradient(", 16) != 0) {
        return false;
    }

    // Find content between parentheses
    const char* start = value + 16;
    const char* end = strrchr(start, ')');
    if (!end) return false;

    // Extract content
    size_t content_len = end - start;
    char* content = malloc(content_len + 1);
    if (!content) return false;
    strncpy(content, start, content_len);
    content[content_len] = '\0';

    // Split by commas
    int arg_count;
    char** args = split_gradient_args(content, &arg_count);
    free(content);

    if (!args || arg_count < 2) {
        free_gradient_args(args, arg_count);
        return false;
    }

    // Allocate gradient
    IRGradient* gradient = calloc(1, sizeof(IRGradient));
    if (!gradient) {
        free_gradient_args(args, arg_count);
        return false;
    }

    gradient->type = IR_GRADIENT_LINEAR;
    gradient->angle = 180.0f;  // Default: to bottom
    gradient->stop_count = 0;

    // Parse first argument - could be angle, direction, or color stop
    int color_start_idx = 0;
    float angle;

    if (parse_gradient_angle(args[0], &angle)) {
        gradient->angle = angle;
        color_start_idx = 1;
    } else if (parse_gradient_direction(args[0], &angle)) {
        gradient->angle = angle;
        color_start_idx = 1;
    }
    // else: first arg is a color stop

    // Parse color stops
    IRGradientStop stops[8];
    bool has_positions[8];
    int stop_count = 0;

    for (int i = color_start_idx; i < arg_count && stop_count < 8; i++) {
        if (parse_gradient_color_stop(args[i], &stops[stop_count], &has_positions[stop_count])) {
            stop_count++;
        }
    }

    free_gradient_args(args, arg_count);

    if (stop_count < 2) {
        free(gradient);
        return false;
    }

    // Auto-distribute positions for stops without explicit positions
    // First stop defaults to 0%, last to 100%
    if (!has_positions[0] || stops[0].position < 0) {
        stops[0].position = 0.0f;
    }
    if (!has_positions[stop_count - 1] || stops[stop_count - 1].position < 0) {
        stops[stop_count - 1].position = 1.0f;
    }

    // Distribute middle stops evenly between surrounding positioned stops
    for (int i = 1; i < stop_count - 1; i++) {
        if (!has_positions[i] || stops[i].position < 0) {
            // Find previous and next positioned stops
            int prev_idx = i - 1;
            int next_idx = i + 1;
            while (next_idx < stop_count && (!has_positions[next_idx] || stops[next_idx].position < 0)) {
                next_idx++;
            }

            // Evenly distribute between prev and next
            float prev_pos = stops[prev_idx].position;
            float next_pos = stops[next_idx].position;
            int unpositioned_count = next_idx - prev_idx - 1;
            float step = (next_pos - prev_pos) / (unpositioned_count + 1);

            for (int j = prev_idx + 1; j < next_idx; j++) {
                stops[j].position = prev_pos + step * (j - prev_idx);
            }
        }
    }

    // Copy stops to gradient
    for (int i = 0; i < stop_count; i++) {
        gradient->stops[i] = stops[i];
    }
    gradient->stop_count = stop_count;

    *out_gradient = gradient;
    return true;
}

// Parse inline CSS into property/value pairs
CSSProperty* ir_css_parse_inline(const char* style_string, uint32_t* count) {
    if (!style_string || !count) return NULL;

    *count = 0;

    // Count properties (count semicolons)
    uint32_t prop_count = 0;
    for (const char* p = style_string; *p; p++) {
        if (*p == ':') prop_count++;
    }

    if (prop_count == 0) return NULL;

    CSSProperty* props = calloc(prop_count, sizeof(CSSProperty));
    if (!props) return NULL;

    // Parse properties
    char* style_copy = strdup(style_string);
    if (!style_copy) {
        free(props);
        return NULL;
    }

    char* token = strtok(style_copy, ";");
    uint32_t idx = 0;

    while (token && idx < prop_count) {
        // Split on colon
        char* colon = strchr(token, ':');
        if (!colon) {
            token = strtok(NULL, ";");
            continue;
        }

        *colon = '\0';
        char* property = token;
        char* value = colon + 1;

        // Trim whitespace
        trim_whitespace(property);
        trim_whitespace(value);

        // Store (duplicate strings so we can free style_copy)
        props[idx].property = strdup(property);
        props[idx].value = strdup(value);
        idx++;

        token = strtok(NULL, ";");
    }

    *count = idx;
    free(style_copy);

    return props;
}

// Free CSS property array
void ir_css_free_properties(CSSProperty* props, uint32_t count) {
    if (!props) return;

    for (uint32_t i = 0; i < count; i++) {
        free((char*)props[i].property);
        free((char*)props[i].value);
    }
    free(props);
}

// Parse CSS color (rgba, rgb, hex, named, var(), linear-gradient())
bool ir_css_parse_color(const char* value, IRColor* out_color) {
    if (!value || !out_color) return false;

    // Initialize var_name to NULL
    out_color->var_name = NULL;

    // Handle linear-gradient()
    if (strncmp(value, "linear-gradient(", 16) == 0) {
        IRGradient* gradient = NULL;
        if (ir_css_parse_linear_gradient(value, &gradient)) {
            out_color->type = IR_COLOR_GRADIENT;
            out_color->data.gradient = gradient;
            return true;
        }
        return false;
    }

    // Handle "transparent"
    if (strcmp(value, "transparent") == 0) {
        out_color->type = IR_COLOR_TRANSPARENT;
        return true;
    }

    // Handle var(--name) references
    if (strncmp(value, "var(", 4) == 0) {
        // Extract variable name from var(--name)
        const char* start = value + 4;
        const char* end = strchr(start, ')');
        if (end) {
            size_t name_len = end - start;
            char* var_name = malloc(name_len + 1);
            if (var_name) {
                strncpy(var_name, start, name_len);
                var_name[name_len] = '\0';
                out_color->var_name = var_name;
                // Set type to VAR_REF but also provide a fallback color
                out_color->type = IR_COLOR_VAR_REF;
                out_color->data.var_id = 0;  // Will be resolved if needed
                return true;
            }
        }
        return false;
    }

    // Handle rgba(r, g, b, a)
    if (strncmp(value, "rgba(", 5) == 0) {
        unsigned int r, g, b;
        float a;
        if (sscanf(value, "rgba(%u, %u, %u, %f)", &r, &g, &b, &a) == 4) {
            out_color->type = IR_COLOR_SOLID;
            out_color->data.r = (uint8_t)r;
            out_color->data.g = (uint8_t)g;
            out_color->data.b = (uint8_t)b;
            out_color->data.a = (uint8_t)(a * 255.0f);
            return true;
        }
    }

    // Handle rgb(r, g, b)
    if (strncmp(value, "rgb(", 4) == 0) {
        unsigned int r, g, b;
        if (sscanf(value, "rgb(%u, %u, %u)", &r, &g, &b) == 3) {
            out_color->type = IR_COLOR_SOLID;
            out_color->data.r = (uint8_t)r;
            out_color->data.g = (uint8_t)g;
            out_color->data.b = (uint8_t)b;
            out_color->data.a = 255;
            return true;
        }
    }

    // Handle hex colors (#RGB, #RRGGBB)
    if (value[0] == '#') {
        size_t len = strlen(value);
        unsigned int hex;

        if (len == 7 && sscanf(value, "#%06x", &hex) == 1) {
            // #RRGGBB
            out_color->type = IR_COLOR_SOLID;
            out_color->data.r = (hex >> 16) & 0xFF;
            out_color->data.g = (hex >> 8) & 0xFF;
            out_color->data.b = hex & 0xFF;
            out_color->data.a = 255;
            return true;
        } else if (len == 4 && sscanf(value, "#%03x", &hex) == 1) {
            // #RGB -> expand to #RRGGBB
            out_color->type = IR_COLOR_SOLID;
            out_color->data.r = ((hex >> 8) & 0xF) * 17;
            out_color->data.g = ((hex >> 4) & 0xF) * 17;
            out_color->data.b = (hex & 0xF) * 17;
            out_color->data.a = 255;
            return true;
        }
    }

    // Unknown format
    return false;
}

// Parse CSS dimension (px, %, auto)
bool ir_css_parse_dimension(const char* value, IRDimension* out_dimension) {
    if (!value || !out_dimension) return false;

    // Handle "auto"
    if (strcmp(value, "auto") == 0) {
        out_dimension->type = IR_DIMENSION_AUTO;
        out_dimension->value = 0;
        return true;
    }

    // Parse numeric value with unit
    float num;
    char unit[16] = "";

    if (sscanf(value, "%f%15s", &num, unit) >= 1) {
        if (strcmp(unit, "px") == 0 || unit[0] == '\0') {
            out_dimension->type = IR_DIMENSION_PX;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "%") == 0) {
            out_dimension->type = IR_DIMENSION_PERCENT;
            out_dimension->value = num;
            return true;
        }
    }

    return false;
}

// Helper: parse a single spacing value (handles "10px", "10", "0", "auto", etc.)
static float parse_single_spacing_value(const char* s) {
    if (!s) return 0;
    // Check for "auto" keyword
    if (strcmp(s, "auto") == 0) return IR_SPACING_AUTO;
    float val = 0;
    // Try parsing with px suffix
    if (sscanf(s, "%fpx", &val) == 1) return val;
    // Try parsing with rem suffix
    if (sscanf(s, "%frem", &val) == 1) return val * 16;  // Approximate rem to px
    // Try parsing as plain number (for "0")
    if (sscanf(s, "%f", &val) == 1) return val;
    return 0;
}

// Parse CSS spacing (margin/padding: top right bottom left)
bool ir_css_parse_spacing(const char* value, IRSpacing* out_spacing) {
    if (!value || !out_spacing) return false;

    // Make a mutable copy
    char buf[256];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Split by whitespace
    char* parts[4] = {NULL, NULL, NULL, NULL};
    int count = 0;
    char* token = strtok(buf, " \t");
    while (token && count < 4) {
        parts[count++] = token;
        token = strtok(NULL, " \t");
    }

    if (count == 4) {
        // 4 values: top right bottom left
        out_spacing->top = parse_single_spacing_value(parts[0]);
        out_spacing->right = parse_single_spacing_value(parts[1]);
        out_spacing->bottom = parse_single_spacing_value(parts[2]);
        out_spacing->left = parse_single_spacing_value(parts[3]);
        return true;
    } else if (count == 3) {
        // 3 values: top left/right bottom
        out_spacing->top = parse_single_spacing_value(parts[0]);
        out_spacing->right = parse_single_spacing_value(parts[1]);
        out_spacing->left = out_spacing->right;
        out_spacing->bottom = parse_single_spacing_value(parts[2]);
        return true;
    } else if (count == 2) {
        // 2 values: top/bottom left/right
        out_spacing->top = parse_single_spacing_value(parts[0]);
        out_spacing->bottom = out_spacing->top;
        out_spacing->right = parse_single_spacing_value(parts[1]);
        out_spacing->left = out_spacing->right;
        return true;
    } else if (count == 1) {
        // 1 value: all sides
        float val = parse_single_spacing_value(parts[0]);
        out_spacing->top = val;
        out_spacing->right = val;
        out_spacing->bottom = val;
        out_spacing->left = val;
        return true;
    }

    return false;
}

// Apply CSS properties to IR style
void ir_css_apply_to_style(IRStyle* style, const CSSProperty* props, uint32_t count) {
    if (!style || !props) return;

    for (uint32_t i = 0; i < count; i++) {
        const char* prop = props[i].property;
        const char* val = props[i].value;

        // Dimensions
        if (strcmp(prop, "width") == 0) {
            ir_css_parse_dimension(val, &style->width);
        } else if (strcmp(prop, "height") == 0) {
            ir_css_parse_dimension(val, &style->height);
        }

        // Colors
        else if (strcmp(prop, "background-color") == 0) {
            ir_css_parse_color(val, &style->background);
        } else if (strcmp(prop, "background") == 0) {
            // Check if it's a gradient
            if (strncmp(val, "linear-gradient", 15) == 0 ||
                strncmp(val, "radial-gradient", 15) == 0 ||
                strncmp(val, "-webkit-linear-gradient", 23) == 0) {
                // Store raw gradient string for round-trip
                if (style->background_image) free(style->background_image);
                style->background_image = strdup(val);
            } else {
                ir_css_parse_color(val, &style->background);
            }
        } else if (strcmp(prop, "color") == 0) {
            ir_css_parse_color(val, &style->font.color);
        }
        // Background clipping (for gradient text effects)
        else if (strcmp(prop, "background-clip") == 0 ||
                 strcmp(prop, "-webkit-background-clip") == 0) {
            if (strcmp(val, "text") == 0) {
                style->background_clip = IR_BACKGROUND_CLIP_TEXT;
            } else if (strcmp(val, "content-box") == 0) {
                style->background_clip = IR_BACKGROUND_CLIP_CONTENT_BOX;
            } else if (strcmp(val, "padding-box") == 0) {
                style->background_clip = IR_BACKGROUND_CLIP_PADDING_BOX;
            } else {
                style->background_clip = IR_BACKGROUND_CLIP_BORDER_BOX;
            }
        }
        // Text fill color (for gradient text effects)
        else if (strcmp(prop, "-webkit-text-fill-color") == 0) {
            ir_css_parse_color(val, &style->text_fill_color);
        }

        // Spacing
        else if (strcmp(prop, "padding") == 0) {
            ir_css_parse_spacing(val, &style->padding);
        } else if (strcmp(prop, "margin") == 0) {
            ir_css_parse_spacing(val, &style->margin);
        }

        // Border
        else if (strcmp(prop, "border") == 0) {
            // Parse "2px solid rgba(255, 0, 0, 1.0)"
            float width;
            char rest[512];
            if (sscanf(val, "%fpx %511[^\n]", &width, rest) == 2) {
                style->border.width = width;

                // Extract color from "solid rgba(...)" or "solid #fff"
                char* color_start = strstr(rest, "rgba(");
                if (!color_start) color_start = strstr(rest, "rgb(");
                if (!color_start) color_start = strchr(rest, '#');

                if (color_start) {
                    ir_css_parse_color(color_start, &style->border.color);
                }
            }
        } else if (strcmp(prop, "border-radius") == 0) {
            float radius;
            if (sscanf(val, "%fpx", &radius) == 1) {
                style->border.radius = (uint32_t)radius;
            }
        }
        // Border sides (border-bottom, border-top, etc.)
        else if (strcmp(prop, "border-bottom") == 0 ||
                 strcmp(prop, "border-top") == 0 ||
                 strcmp(prop, "border-left") == 0 ||
                 strcmp(prop, "border-right") == 0) {
            // Parse "1px solid var(--border)" or "1px solid #color"
            float width;
            char rest[512];
            if (sscanf(val, "%fpx %511[^\n]", &width, rest) == 2) {
                // Set per-side width
                if (strcmp(prop, "border-bottom") == 0) {
                    style->border.width_bottom = width;
                } else if (strcmp(prop, "border-top") == 0) {
                    style->border.width_top = width;
                } else if (strcmp(prop, "border-left") == 0) {
                    style->border.width_left = width;
                } else if (strcmp(prop, "border-right") == 0) {
                    style->border.width_right = width;
                }

                // Look for var() reference first
                char* var_start = strstr(rest, "var(");
                if (var_start) {
                    // Extract var name
                    char* name_start = var_start + 4;
                    char* name_end = strchr(name_start, ')');
                    if (name_end) {
                        size_t name_len = name_end - name_start;
                        char* var_name = malloc(name_len + 1);
                        strncpy(var_name, name_start, name_len);
                        var_name[name_len] = '\0';
                        style->border.color.var_name = var_name;
                        style->border.color.type = IR_COLOR_VAR_REF;
                    }
                } else {
                    // Look for color
                    char* color_start = strstr(rest, "rgba(");
                    if (!color_start) color_start = strstr(rest, "rgb(");
                    if (!color_start) color_start = strchr(rest, '#');
                    if (color_start) {
                        ir_css_parse_color(color_start, &style->border.color);
                    }
                }
            }
        }

        // Typography
        else if (strcmp(prop, "font-size") == 0) {
            float size;
            char unit[16] = "";
            if (sscanf(val, "%f%15s", &size, unit) >= 1) {
                if (strcmp(unit, "px") == 0 || unit[0] == '\0') {
                    style->font.size = size;
                } else if (strcmp(unit, "rem") == 0) {
                    style->font.size = size * 16;  // Convert rem to px (assuming 16px base)
                } else if (strcmp(unit, "em") == 0) {
                    style->font.size = size * 16;  // Convert em to px (approximate)
                } else {
                    // Unknown unit, store as-is
                    style->font.size = size;
                }
            }
        } else if (strcmp(prop, "font-weight") == 0) {
            if (strcmp(val, "bold") == 0) {
                style->font.bold = true;
                style->font.weight = 700;
            } else {
                unsigned int weight;
                if (sscanf(val, "%u", &weight) == 1) {
                    style->font.weight = weight;
                    style->font.bold = (weight >= 600);
                }
            }
        } else if (strcmp(prop, "font-style") == 0) {
            style->font.italic = (strcmp(val, "italic") == 0);
        } else if (strcmp(prop, "text-align") == 0) {
            if (strcmp(val, "left") == 0) style->font.align = IR_TEXT_ALIGN_LEFT;
            else if (strcmp(val, "right") == 0) style->font.align = IR_TEXT_ALIGN_RIGHT;
            else if (strcmp(val, "center") == 0) style->font.align = IR_TEXT_ALIGN_CENTER;
            else if (strcmp(val, "justify") == 0) style->font.align = IR_TEXT_ALIGN_JUSTIFY;
        }
        // Additional typography properties
        else if (strcmp(prop, "font-family") == 0) {
            // Strip quotes if present
            const char* family = val;
            if (val[0] == '"' || val[0] == '\'') {
                family = val + 1;
                size_t len = strlen(family);
                if (len > 0 && (family[len-1] == '"' || family[len-1] == '\'')) {
                    char* clean = strdup(family);
                    if (clean) {
                        clean[len-1] = '\0';
                        free(style->font.family);
                        style->font.family = clean;
                    }
                } else {
                    free(style->font.family);
                    style->font.family = strdup(family);
                }
            } else {
                // Take first font family before comma
                char* comma = strchr(val, ',');
                if (comma) {
                    size_t len = comma - val;
                    char* clean = malloc(len + 1);
                    if (clean) {
                        memcpy(clean, val, len);
                        clean[len] = '\0';
                        // Trim trailing whitespace
                        while (len > 0 && isspace(clean[len-1])) clean[--len] = '\0';
                        free(style->font.family);
                        style->font.family = clean;
                    }
                } else {
                    free(style->font.family);
                    style->font.family = strdup(val);
                }
            }
        }
        else if (strcmp(prop, "line-height") == 0) {
            float line_height;
            if (sscanf(val, "%f", &line_height) == 1) {
                // If the value has no unit or is just a number, it's a multiplier
                // If it has "px", convert to multiplier based on font size
                if (strstr(val, "px")) {
                    // Store as-is for now, will be computed later
                    style->font.line_height = line_height;
                } else {
                    style->font.line_height = line_height;
                }
            }
        }
        else if (strcmp(prop, "letter-spacing") == 0) {
            float spacing;
            if (sscanf(val, "%fpx", &spacing) == 1) {
                style->font.letter_spacing = spacing;
            } else if (sscanf(val, "%fem", &spacing) == 1) {
                // Convert em to approximate pixels (assuming 16px base)
                style->font.letter_spacing = spacing * 16.0f;
            }
        }
        else if (strcmp(prop, "text-decoration") == 0) {
            // Handle text-decoration: none, underline, line-through
            if (strcmp(val, "none") == 0) {
                style->font.decoration = IR_TEXT_DECORATION_NONE;
            } else if (strcmp(val, "underline") == 0) {
                style->font.decoration = IR_TEXT_DECORATION_UNDERLINE;
            } else if (strcmp(val, "line-through") == 0) {
                style->font.decoration = IR_TEXT_DECORATION_LINE_THROUGH;
            } else if (strcmp(val, "overline") == 0) {
                style->font.decoration = IR_TEXT_DECORATION_OVERLINE;
            }
        }
    }
}

// Apply CSS flexbox properties to IR layout
void ir_css_apply_to_layout(IRLayout* layout, const CSSProperty* props, uint32_t count) {
    if (!layout || !props) return;

    for (uint32_t i = 0; i < count; i++) {
        const char* prop = props[i].property;
        const char* val = props[i].value;

        // Display mode
        if (strcmp(prop, "display") == 0) {
            layout->display_explicit = true;  // Mark that display was explicitly set
            if (strcmp(val, "flex") == 0) {
                layout->mode = IR_LAYOUT_MODE_FLEX;
            } else if (strcmp(val, "grid") == 0) {
                layout->mode = IR_LAYOUT_MODE_GRID;
            } else if (strcmp(val, "block") == 0) {
                layout->mode = IR_LAYOUT_MODE_BLOCK;
            }
        }
        else if (strcmp(prop, "flex-direction") == 0) {
            if (strcmp(val, "row") == 0) {
                layout->flex.direction = 1;  // 1 = row
            } else if (strcmp(val, "column") == 0) {
                layout->flex.direction = 0;  // 0 = column
            }
        }
        else if (strcmp(prop, "justify-content") == 0) {
            if (strcmp(val, "flex-start") == 0) layout->flex.justify_content = IR_ALIGNMENT_START;
            else if (strcmp(val, "center") == 0) layout->flex.justify_content = IR_ALIGNMENT_CENTER;
            else if (strcmp(val, "flex-end") == 0) layout->flex.justify_content = IR_ALIGNMENT_END;
            else if (strcmp(val, "space-between") == 0) layout->flex.justify_content = IR_ALIGNMENT_SPACE_BETWEEN;
            else if (strcmp(val, "space-around") == 0) layout->flex.justify_content = IR_ALIGNMENT_SPACE_AROUND;
            else if (strcmp(val, "space-evenly") == 0) layout->flex.justify_content = IR_ALIGNMENT_SPACE_EVENLY;
        }
        else if (strcmp(prop, "align-items") == 0) {
            if (strcmp(val, "flex-start") == 0) layout->flex.cross_axis = IR_ALIGNMENT_START;
            else if (strcmp(val, "center") == 0) layout->flex.cross_axis = IR_ALIGNMENT_CENTER;
            else if (strcmp(val, "flex-end") == 0) layout->flex.cross_axis = IR_ALIGNMENT_END;
            else if (strcmp(val, "stretch") == 0) layout->flex.cross_axis = IR_ALIGNMENT_STRETCH;
        }
        else if (strcmp(prop, "gap") == 0) {
            unsigned int gap;
            if (sscanf(val, "%upx", &gap) == 1) {
                layout->flex.gap = gap;
            }
        }
        else if (strcmp(prop, "flex-wrap") == 0) {
            layout->flex.wrap = (strcmp(val, "wrap") == 0);
        }
        else if (strcmp(prop, "flex-grow") == 0) {
            unsigned int grow;
            if (sscanf(val, "%u", &grow) == 1) {
                layout->flex.grow = grow;
            }
        }
        else if (strcmp(prop, "flex-shrink") == 0) {
            unsigned int shrink;
            if (sscanf(val, "%u", &shrink) == 1) {
                layout->flex.shrink = shrink;
            }
        }

        // Size constraints
        else if (strcmp(prop, "min-width") == 0) {
            ir_css_parse_dimension(val, &layout->min_width);
        }
        else if (strcmp(prop, "max-width") == 0) {
            ir_css_parse_dimension(val, &layout->max_width);
        }
        else if (strcmp(prop, "min-height") == 0) {
            ir_css_parse_dimension(val, &layout->min_height);
        }
        else if (strcmp(prop, "max-height") == 0) {
            ir_css_parse_dimension(val, &layout->max_height);
        }

        // Grid properties
        else if (strcmp(prop, "grid-template-columns") == 0) {
            // Parse grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)) or simple values
            // For now, handle simple cases like "1fr 1fr 1fr" or "240px 1fr"
            layout->mode = IR_LAYOUT_MODE_GRID;
            layout->grid.column_count = 0;

            char* val_copy = strdup(val);
            char* token = strtok(val_copy, " ");
            while (token && layout->grid.column_count < IR_MAX_GRID_TRACKS) {
                float num;
                if (strstr(token, "fr")) {
                    if (sscanf(token, "%ffr", &num) == 1) {
                        layout->grid.columns[layout->grid.column_count].type = IR_GRID_TRACK_FR;
                        layout->grid.columns[layout->grid.column_count].value = num;
                        layout->grid.column_count++;
                    }
                } else if (strstr(token, "px")) {
                    if (sscanf(token, "%fpx", &num) == 1) {
                        layout->grid.columns[layout->grid.column_count].type = IR_GRID_TRACK_PX;
                        layout->grid.columns[layout->grid.column_count].value = num;
                        layout->grid.column_count++;
                    }
                } else if (strcmp(token, "auto") == 0) {
                    layout->grid.columns[layout->grid.column_count].type = IR_GRID_TRACK_AUTO;
                    layout->grid.columns[layout->grid.column_count].value = 0;
                    layout->grid.column_count++;
                } else if (strstr(token, "repeat")) {
                    // Handle repeat(auto-fit, minmax(240px, 1fr)) - simplified: use 3 x 1fr
                    for (int r = 0; r < 3 && layout->grid.column_count < IR_MAX_GRID_TRACKS; r++) {
                        layout->grid.columns[layout->grid.column_count].type = IR_GRID_TRACK_FR;
                        layout->grid.columns[layout->grid.column_count].value = 1.0f;
                        layout->grid.column_count++;
                    }
                    break;  // Skip rest of token parsing for repeat
                }
                token = strtok(NULL, " ");
            }
            free(val_copy);
        }
        else if (strcmp(prop, "grid-template-rows") == 0) {
            // Similar parsing for rows
            layout->mode = IR_LAYOUT_MODE_GRID;
            layout->grid.row_count = 0;

            char* val_copy = strdup(val);
            char* token = strtok(val_copy, " ");
            while (token && layout->grid.row_count < IR_MAX_GRID_TRACKS) {
                float num;
                if (strstr(token, "fr")) {
                    if (sscanf(token, "%ffr", &num) == 1) {
                        layout->grid.rows[layout->grid.row_count].type = IR_GRID_TRACK_FR;
                        layout->grid.rows[layout->grid.row_count].value = num;
                        layout->grid.row_count++;
                    }
                } else if (strstr(token, "px")) {
                    if (sscanf(token, "%fpx", &num) == 1) {
                        layout->grid.rows[layout->grid.row_count].type = IR_GRID_TRACK_PX;
                        layout->grid.rows[layout->grid.row_count].value = num;
                        layout->grid.row_count++;
                    }
                } else if (strcmp(token, "auto") == 0) {
                    layout->grid.rows[layout->grid.row_count].type = IR_GRID_TRACK_AUTO;
                    layout->grid.rows[layout->grid.row_count].value = 0;
                    layout->grid.row_count++;
                }
                token = strtok(NULL, " ");
            }
            free(val_copy);
        }
        else if (strcmp(prop, "row-gap") == 0) {
            float gap;
            if (sscanf(val, "%fpx", &gap) == 1) {
                layout->grid.row_gap = gap;
            }
        }
        else if (strcmp(prop, "column-gap") == 0) {
            float gap;
            if (sscanf(val, "%fpx", &gap) == 1) {
                layout->grid.column_gap = gap;
            }
        }
    }
}
