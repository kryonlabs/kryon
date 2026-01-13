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
// GRID PARSING HELPERS
// ============================================================================

// Parse a single grid track value (1fr, 100px, auto, min-content, etc.)
static bool parse_grid_track_value(const char* token, IRGridTrack* out) {
    if (!token || !out) return false;

    // Skip leading whitespace
    while (*token && isspace(*token)) token++;
    if (!*token) return false;

    float num;
    if (strstr(token, "fr") && sscanf(token, "%ffr", &num) == 1) {
        out->type = IR_GRID_TRACK_FR;
        out->value = num;
        return true;
    }
    if (strstr(token, "px") && sscanf(token, "%fpx", &num) == 1) {
        out->type = IR_GRID_TRACK_PX;
        out->value = num;
        return true;
    }
    if (strstr(token, "%") && sscanf(token, "%f%%", &num) == 1) {
        out->type = IR_GRID_TRACK_PERCENT;
        out->value = num;
        return true;
    }
    if (strncmp(token, "auto", 4) == 0) {
        out->type = IR_GRID_TRACK_AUTO;
        out->value = 0;
        return true;
    }
    if (strncmp(token, "min-content", 11) == 0) {
        out->type = IR_GRID_TRACK_MIN_CONTENT;
        out->value = 0;
        return true;
    }
    if (strncmp(token, "max-content", 11) == 0) {
        out->type = IR_GRID_TRACK_MAX_CONTENT;
        out->value = 0;
        return true;
    }
    return false;
}

// Parse minmax(min, max) function
static bool parse_grid_minmax(const char* value, IRGridMinMax* out) {
    if (!value || !out) return false;

    // Skip leading whitespace
    while (*value && isspace(*value)) value++;

    // Must start with "minmax("
    if (strncmp(value, "minmax(", 7) != 0) return false;

    const char* inner = value + 7;

    // Find the comma - need to handle nested parens carefully
    int paren_depth = 0;
    const char* comma = NULL;
    for (const char* p = inner; *p; p++) {
        if (*p == '(') paren_depth++;
        else if (*p == ')') {
            if (paren_depth == 0) break;  // End of minmax
            paren_depth--;
        }
        else if (*p == ',' && paren_depth == 0) {
            comma = p;
            break;
        }
    }

    if (!comma) return false;

    // Find closing paren
    const char* close = strrchr(value, ')');
    if (!close || close < comma) return false;

    // Extract min value
    char min_str[64] = {0};
    size_t min_len = comma - inner;
    if (min_len >= sizeof(min_str)) min_len = sizeof(min_str) - 1;
    strncpy(min_str, inner, min_len);
    trim_whitespace(min_str);

    // Extract max value
    char max_str[64] = {0};
    size_t max_len = close - (comma + 1);
    if (max_len >= sizeof(max_str)) max_len = sizeof(max_str) - 1;
    strncpy(max_str, comma + 1, max_len);
    trim_whitespace(max_str);

    // Parse both values
    IRGridTrack min_track = {0}, max_track = {0};
    if (!parse_grid_track_value(min_str, &min_track)) return false;
    if (!parse_grid_track_value(max_str, &max_track)) return false;

    out->min_type = min_track.type;
    out->min_value = min_track.value;
    out->max_type = max_track.type;
    out->max_value = max_track.value;
    return true;
}

// Parse repeat(mode, track) function
static bool parse_grid_repeat(const char* value, IRGridRepeat* out) {
    if (!value || !out) return false;

    // Skip leading whitespace
    while (*value && isspace(*value)) value++;

    // Must start with "repeat("
    if (strncmp(value, "repeat(", 7) != 0) return false;

    const char* inner = value + 7;

    // Find the comma separating mode from track
    const char* comma = strchr(inner, ',');
    if (!comma) return false;

    // Extract and parse mode
    char mode_str[32] = {0};
    size_t mode_len = comma - inner;
    if (mode_len >= sizeof(mode_str)) mode_len = sizeof(mode_str) - 1;
    strncpy(mode_str, inner, mode_len);
    trim_whitespace(mode_str);

    if (strcmp(mode_str, "auto-fit") == 0) {
        out->mode = IR_GRID_REPEAT_AUTO_FIT;
        out->count = 0;
    } else if (strcmp(mode_str, "auto-fill") == 0) {
        out->mode = IR_GRID_REPEAT_AUTO_FILL;
        out->count = 0;
    } else {
        out->mode = IR_GRID_REPEAT_COUNT;
        out->count = (uint8_t)atoi(mode_str);
        if (out->count == 0) out->count = 1;
    }

    // Parse track definition (skip comma and whitespace)
    const char* track_start = comma + 1;
    while (*track_start && isspace(*track_start)) track_start++;

    // Find the final closing paren (accounting for nested parens like minmax())
    int paren_depth = 1;  // We're inside repeat()
    const char* close = track_start;
    while (*close && paren_depth > 0) {
        if (*close == '(') paren_depth++;
        else if (*close == ')') paren_depth--;
        close++;
    }
    close--;  // Back to the final ')'

    // Extract track value
    char track_str[128] = {0};
    size_t track_len = close - track_start;
    if (track_len >= sizeof(track_str)) track_len = sizeof(track_str) - 1;
    strncpy(track_str, track_start, track_len);
    trim_whitespace(track_str);

    // Check for minmax()
    if (strncmp(track_str, "minmax(", 7) == 0) {
        out->has_minmax = true;
        return parse_grid_minmax(track_str, &out->minmax);
    } else {
        out->has_minmax = false;
        return parse_grid_track_value(track_str, &out->track);
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

    // Handle var(--name) references - store full var() reference for direct output
    if (strncmp(value, "var(", 4) == 0) {
        const char* end = strchr(value, ')');
        if (end) {
            // Store the full var(--name) reference including var() wrapper
            size_t full_len = (end - value) + 1;  // Include closing paren
            char* var_ref = malloc(full_len + 1);
            if (var_ref) {
                strncpy(var_ref, value, full_len);
                var_ref[full_len] = '\0';
                out_color->var_name = var_ref;  // e.g., "var(--primary)"
                out_color->type = IR_COLOR_VAR_REF;
                out_color->data.var_id = 0;
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

    // Try named color (e.g., "white", "red", "blue")
    uint8_t r, g, b;
    if (parse_named_color(value, &r, &g, &b)) {
        out_color->type = IR_COLOR_SOLID;
        out_color->data.r = r;
        out_color->data.g = g;
        out_color->data.b = b;
        out_color->data.a = 255;
        return true;
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
        } else if (strcmp(unit, "rem") == 0) {
            out_dimension->type = IR_DIMENSION_REM;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "em") == 0) {
            out_dimension->type = IR_DIMENSION_EM;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "vw") == 0) {
            out_dimension->type = IR_DIMENSION_VW;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "vh") == 0) {
            out_dimension->type = IR_DIMENSION_VH;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "vmin") == 0) {
            out_dimension->type = IR_DIMENSION_VMIN;
            out_dimension->value = num;
            return true;
        } else if (strcmp(unit, "vmax") == 0) {
            out_dimension->type = IR_DIMENSION_VMAX;
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
        out_spacing->set_flags = IR_SPACING_SET_ALL;
        return true;
    } else if (count == 3) {
        // 3 values: top left/right bottom
        out_spacing->top = parse_single_spacing_value(parts[0]);
        out_spacing->right = parse_single_spacing_value(parts[1]);
        out_spacing->left = out_spacing->right;
        out_spacing->bottom = parse_single_spacing_value(parts[2]);
        out_spacing->set_flags = IR_SPACING_SET_ALL;
        return true;
    } else if (count == 2) {
        // 2 values: top/bottom left/right
        out_spacing->top = parse_single_spacing_value(parts[0]);
        out_spacing->bottom = out_spacing->top;
        out_spacing->right = parse_single_spacing_value(parts[1]);
        out_spacing->left = out_spacing->right;
        out_spacing->set_flags = IR_SPACING_SET_ALL;
        return true;
    } else if (count == 1) {
        // 1 value: all sides
        float val = parse_single_spacing_value(parts[0]);
        out_spacing->top = val;
        out_spacing->right = val;
        out_spacing->bottom = val;
        out_spacing->left = val;
        out_spacing->set_flags = IR_SPACING_SET_ALL;
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

        // Spacing - shorthand
        else if (strcmp(prop, "padding") == 0) {
            ir_css_parse_spacing(val, &style->padding);
        } else if (strcmp(prop, "margin") == 0) {
            ir_css_parse_spacing(val, &style->margin);
        }
        // Spacing - individual margin properties
        else if (strcmp(prop, "margin-top") == 0) {
            style->margin.top = parse_single_spacing_value(val);
            style->margin.set_flags |= IR_SPACING_SET_TOP;
        } else if (strcmp(prop, "margin-right") == 0) {
            style->margin.right = parse_single_spacing_value(val);
            style->margin.set_flags |= IR_SPACING_SET_RIGHT;
        } else if (strcmp(prop, "margin-bottom") == 0) {
            style->margin.bottom = parse_single_spacing_value(val);
            style->margin.set_flags |= IR_SPACING_SET_BOTTOM;
        } else if (strcmp(prop, "margin-left") == 0) {
            style->margin.left = parse_single_spacing_value(val);
            style->margin.set_flags |= IR_SPACING_SET_LEFT;
        }
        // Spacing - individual padding properties
        else if (strcmp(prop, "padding-top") == 0) {
            style->padding.top = parse_single_spacing_value(val);
            style->padding.set_flags |= IR_SPACING_SET_TOP;
        } else if (strcmp(prop, "padding-right") == 0) {
            style->padding.right = parse_single_spacing_value(val);
            style->padding.set_flags |= IR_SPACING_SET_RIGHT;
        } else if (strcmp(prop, "padding-bottom") == 0) {
            style->padding.bottom = parse_single_spacing_value(val);
            style->padding.set_flags |= IR_SPACING_SET_BOTTOM;
        } else if (strcmp(prop, "padding-left") == 0) {
            style->padding.left = parse_single_spacing_value(val);
            style->padding.set_flags |= IR_SPACING_SET_LEFT;
        }

        // Border
        else if (strcmp(prop, "border") == 0) {
            // Parse "2px solid rgba(255, 0, 0, 1.0)" or "2px solid var(--border)"
            float width;
            char rest[512];
            if (sscanf(val, "%fpx %511[^\n]", &width, rest) == 2) {
                style->border.width = width;

                // Look for var() reference first
                char* var_start = strstr(rest, "var(");
                if (var_start) {
                    char* name_end = strchr(var_start, ')');
                    if (name_end) {
                        size_t full_len = (name_end - var_start) + 1;
                        char* var_ref = malloc(full_len + 1);
                        strncpy(var_ref, var_start, full_len);
                        var_ref[full_len] = '\0';
                        style->border.color.var_name = var_ref;
                        style->border.color.type = IR_COLOR_VAR_REF;
                    }
                } else {
                    // Extract color from "solid rgba(...)" or "solid #fff"
                    char* color_start = strstr(rest, "rgba(");
                    if (!color_start) color_start = strstr(rest, "rgb(");
                    if (!color_start) color_start = strchr(rest, '#');

                    if (color_start) {
                        ir_css_parse_color(color_start, &style->border.color);
                    }
                }
            }
        } else if (strcmp(prop, "border-radius") == 0) {
            // Parse border-radius shorthand: 1-4 values
            // border-radius: 10px        -> all corners 10px
            // border-radius: 10px 20px   -> TL/BR=10px, TR/BL=20px
            // border-radius: 10px 20px 30px -> TL=10px, TR/BL=20px, BR=30px
            // border-radius: 10px 20px 30px 40px -> TL=10px, TR=20px, BR=30px, BL=40px
            char buf[256];
            strncpy(buf, val, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            char* parts[4] = {NULL, NULL, NULL, NULL};
            int count = 0;
            char* token = strtok(buf, " \t");
            while (token && count < 4) {
                parts[count++] = token;
                token = strtok(NULL, " \t");
            }

            float r0 = 0, r1 = 0, r2 = 0, r3 = 0;
            if (count == 1 && sscanf(parts[0], "%fpx", &r0) == 1) {
                // All corners same
                style->border.radius = (uint8_t)r0;
                style->border.radius_top_left = (uint8_t)r0;
                style->border.radius_top_right = (uint8_t)r0;
                style->border.radius_bottom_right = (uint8_t)r0;
                style->border.radius_bottom_left = (uint8_t)r0;
                style->border.radius_flags = IR_RADIUS_SET_ALL;
            } else if (count == 2) {
                if (sscanf(parts[0], "%fpx", &r0) == 1 && sscanf(parts[1], "%fpx", &r1) == 1) {
                    style->border.radius = (uint8_t)r0;  // Legacy
                    style->border.radius_top_left = (uint8_t)r0;
                    style->border.radius_bottom_right = (uint8_t)r0;
                    style->border.radius_top_right = (uint8_t)r1;
                    style->border.radius_bottom_left = (uint8_t)r1;
                    style->border.radius_flags = IR_RADIUS_SET_ALL;
                }
            } else if (count == 3) {
                if (sscanf(parts[0], "%fpx", &r0) == 1 &&
                    sscanf(parts[1], "%fpx", &r1) == 1 &&
                    sscanf(parts[2], "%fpx", &r2) == 1) {
                    style->border.radius = (uint8_t)r0;  // Legacy
                    style->border.radius_top_left = (uint8_t)r0;
                    style->border.radius_top_right = (uint8_t)r1;
                    style->border.radius_bottom_left = (uint8_t)r1;
                    style->border.radius_bottom_right = (uint8_t)r2;
                    style->border.radius_flags = IR_RADIUS_SET_ALL;
                }
            } else if (count == 4) {
                if (sscanf(parts[0], "%fpx", &r0) == 1 &&
                    sscanf(parts[1], "%fpx", &r1) == 1 &&
                    sscanf(parts[2], "%fpx", &r2) == 1 &&
                    sscanf(parts[3], "%fpx", &r3) == 1) {
                    style->border.radius = (uint8_t)r0;  // Legacy
                    style->border.radius_top_left = (uint8_t)r0;
                    style->border.radius_top_right = (uint8_t)r1;
                    style->border.radius_bottom_right = (uint8_t)r2;
                    style->border.radius_bottom_left = (uint8_t)r3;
                    style->border.radius_flags = IR_RADIUS_SET_ALL;
                }
            }
        }
        // Individual corner radius properties
        else if (strcmp(prop, "border-top-left-radius") == 0) {
            float radius;
            if (sscanf(val, "%fpx", &radius) == 1) {
                style->border.radius_top_left = (uint8_t)radius;
                style->border.radius_flags |= IR_RADIUS_SET_TOP_LEFT;
            }
        }
        else if (strcmp(prop, "border-top-right-radius") == 0) {
            float radius;
            if (sscanf(val, "%fpx", &radius) == 1) {
                style->border.radius_top_right = (uint8_t)radius;
                style->border.radius_flags |= IR_RADIUS_SET_TOP_RIGHT;
            }
        }
        else if (strcmp(prop, "border-bottom-right-radius") == 0) {
            float radius;
            if (sscanf(val, "%fpx", &radius) == 1) {
                style->border.radius_bottom_right = (uint8_t)radius;
                style->border.radius_flags |= IR_RADIUS_SET_BOTTOM_RIGHT;
            }
        }
        else if (strcmp(prop, "border-bottom-left-radius") == 0) {
            float radius;
            if (sscanf(val, "%fpx", &radius) == 1) {
                style->border.radius_bottom_left = (uint8_t)radius;
                style->border.radius_flags |= IR_RADIUS_SET_BOTTOM_LEFT;
            }
        }
        // Border color only (for hover states, etc.)
        else if (strcmp(prop, "border-color") == 0) {
            // Check for var() reference
            if (strncmp(val, "var(", 4) == 0) {
                char* var_ref = strdup(val);
                if (var_ref) {
                    style->border.color.var_name = var_ref;
                    style->border.color.type = IR_COLOR_VAR_REF;
                }
            } else {
                ir_css_parse_color(val, &style->border.color);
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
                    // Extract full var() reference including "var(" and ")"
                    char* name_end = strchr(var_start, ')');
                    if (name_end) {
                        size_t full_len = (name_end - var_start) + 1;  // Include closing paren
                        char* var_ref = malloc(full_len + 1);
                        strncpy(var_ref, var_start, full_len);
                        var_ref[full_len] = '\0';
                        style->border.color.var_name = var_ref;  // e.g., "var(--border)"
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
                // Store the entire font-family string for CSS roundtrip
                free(style->font.family);
                style->font.family = strdup(val);
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
        // Text overflow (for truncating text with ellipsis)
        else if (strcmp(prop, "text-overflow") == 0) {
            if (strcmp(val, "clip") == 0) {
                style->text_effect.overflow = IR_TEXT_OVERFLOW_CLIP;
            } else if (strcmp(val, "ellipsis") == 0) {
                style->text_effect.overflow = IR_TEXT_OVERFLOW_ELLIPSIS;
            } else if (strcmp(val, "fade") == 0) {
                style->text_effect.overflow = IR_TEXT_OVERFLOW_FADE;
            } else if (strcmp(val, "visible") == 0) {
                style->text_effect.overflow = IR_TEXT_OVERFLOW_VISIBLE;
            }
        }
        // White space (for text wrapping control)
        else if (strcmp(prop, "white-space") == 0) {
            if (strcmp(val, "normal") == 0) {
                style->text_effect.white_space = IR_WHITE_SPACE_NORMAL;
            } else if (strcmp(val, "nowrap") == 0) {
                style->text_effect.white_space = IR_WHITE_SPACE_NOWRAP;
            } else if (strcmp(val, "pre") == 0) {
                style->text_effect.white_space = IR_WHITE_SPACE_PRE;
            } else if (strcmp(val, "pre-wrap") == 0) {
                style->text_effect.white_space = IR_WHITE_SPACE_PRE_WRAP;
            } else if (strcmp(val, "pre-line") == 0) {
                style->text_effect.white_space = IR_WHITE_SPACE_PRE_LINE;
            }
        }
        // Object-fit (for images/videos)
        else if (strcmp(prop, "object-fit") == 0) {
            if (strcmp(val, "fill") == 0) {
                style->object_fit = IR_OBJECT_FIT_FILL;
            } else if (strcmp(val, "contain") == 0) {
                style->object_fit = IR_OBJECT_FIT_CONTAIN;
            } else if (strcmp(val, "cover") == 0) {
                style->object_fit = IR_OBJECT_FIT_COVER;
            } else if (strcmp(val, "none") == 0) {
                style->object_fit = IR_OBJECT_FIT_NONE;
            } else if (strcmp(val, "scale-down") == 0) {
                style->object_fit = IR_OBJECT_FIT_SCALE_DOWN;
            }
        }
        // Overflow properties (for scroll behavior)
        else if (strcmp(prop, "overflow") == 0) {
            // Parse overflow shorthand (applies to both x and y)
            IROverflowMode mode = IR_OVERFLOW_VISIBLE;
            if (strcmp(val, "visible") == 0) {
                mode = IR_OVERFLOW_VISIBLE;
            } else if (strcmp(val, "hidden") == 0) {
                mode = IR_OVERFLOW_HIDDEN;
            } else if (strcmp(val, "scroll") == 0) {
                mode = IR_OVERFLOW_SCROLL;
            } else if (strcmp(val, "auto") == 0) {
                mode = IR_OVERFLOW_AUTO;
            }
            style->overflow_x = mode;
            style->overflow_y = mode;
        }
        else if (strcmp(prop, "overflow-x") == 0) {
            if (strcmp(val, "visible") == 0) {
                style->overflow_x = IR_OVERFLOW_VISIBLE;
            } else if (strcmp(val, "hidden") == 0) {
                style->overflow_x = IR_OVERFLOW_HIDDEN;
            } else if (strcmp(val, "scroll") == 0) {
                style->overflow_x = IR_OVERFLOW_SCROLL;
            } else if (strcmp(val, "auto") == 0) {
                style->overflow_x = IR_OVERFLOW_AUTO;
            }
        }
        else if (strcmp(prop, "overflow-y") == 0) {
            if (strcmp(val, "visible") == 0) {
                style->overflow_y = IR_OVERFLOW_VISIBLE;
            } else if (strcmp(val, "hidden") == 0) {
                style->overflow_y = IR_OVERFLOW_HIDDEN;
            } else if (strcmp(val, "scroll") == 0) {
                style->overflow_y = IR_OVERFLOW_SCROLL;
            } else if (strcmp(val, "auto") == 0) {
                style->overflow_y = IR_OVERFLOW_AUTO;
            }
        }
        // Box shadow (for drop shadows and elevation effects)
        else if (strcmp(prop, "box-shadow") == 0) {
            // box-shadow: none
            if (strcmp(val, "none") == 0) {
                style->box_shadow.enabled = false;
            } else {
                // Parse box-shadow: <inset>? <offset-x> <offset-y> <blur-radius>? <spread-radius>? <color>?
                // Examples: "10px 10px", "10px 10px 5px", "10px 10px 5px 5px rgba(0,0,0,0.5)", "inset 2px 2px 5px"

                char buf[512];
                strncpy(buf, val, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';

                // Default values
                style->box_shadow.offset_x = 0;
                style->box_shadow.offset_y = 0;
                style->box_shadow.blur_radius = 0;
                style->box_shadow.spread_radius = 0;
                style->box_shadow.inset = false;
                style->box_shadow.enabled = true;
                style->box_shadow.color.type = IR_COLOR_SOLID;
                style->box_shadow.color.data.r = 0;
                style->box_shadow.color.data.g = 0;
                style->box_shadow.color.data.b = 0;
                style->box_shadow.color.data.a = 128;  // Semi-transparent black default

                char* token = strtok(buf, " \t");
                int token_count = 0;

                while (token && token_count < 6) {
                    // Check for inset keyword
                    if (strcmp(token, "inset") == 0) {
                        style->box_shadow.inset = true;
                        token = strtok(NULL, " \t");
                        continue;
                    }

                    // Try to parse as offset-x (required)
                    float offset_x;
                    if (token_count == 0 && sscanf(token, "%fpx", &offset_x) == 1) {
                        style->box_shadow.offset_x = offset_x;
                        token_count++;
                        token = strtok(NULL, " \t");
                        continue;
                    }

                    // Try to parse as offset-y (required)
                    float offset_y;
                    if (token_count == 1 && sscanf(token, "%fpx", &offset_y) == 1) {
                        style->box_shadow.offset_y = offset_y;
                        token_count++;
                        token = strtok(NULL, " \t");
                        continue;
                    }

                    // Try to parse as blur-radius (optional)
                    float blur;
                    if (token_count == 2 && sscanf(token, "%fpx", &blur) == 1) {
                        style->box_shadow.blur_radius = blur;
                        token_count++;
                        token = strtok(NULL, " \t");
                        continue;
                    }

                    // Try to parse as spread-radius (optional)
                    float spread;
                    if (token_count == 3 && sscanf(token, "%fpx", &spread) == 1) {
                        style->box_shadow.spread_radius = spread;
                        token_count++;
                        token = strtok(NULL, " \t");
                        continue;
                    }

                    // Try to parse as color (optional)
                    // The remaining tokens might be the color (e.g., "rgba(0,", "0,", "0,", "0.5)")
                    // Reconstruct the rest of the string for color parsing
                    if (token) {
                        char color_str[256] = "";
                        strncat(color_str, token, sizeof(color_str) - 1);
                        char* next_token = strtok(NULL, "");
                        if (next_token) {
                            // Remove leading whitespace from next_token
                            while (*next_token == ' ' || *next_token == '\t') next_token++;
                            strncat(color_str, " ", sizeof(color_str) - strlen(color_str) - 1);
                            strncat(color_str, next_token, sizeof(color_str) - strlen(color_str) - 1);
                        }
                        ir_css_parse_color(color_str, &style->box_shadow.color);
                    }
                    break;
                }
            }
        }
        // Position property (for absolute/fixed/sticky positioning)
        else if (strcmp(prop, "position") == 0) {
            if (strcmp(val, "static") == 0) {
                style->position_mode = IR_POSITION_STATIC;
            } else if (strcmp(val, "relative") == 0) {
                style->position_mode = IR_POSITION_RELATIVE;
            } else if (strcmp(val, "absolute") == 0) {
                style->position_mode = IR_POSITION_ABSOLUTE;
            } else if (strcmp(val, "fixed") == 0) {
                style->position_mode = IR_POSITION_FIXED;
            } else if (strcmp(val, "sticky") == 0) {
                style->position_mode = IR_POSITION_STICKY;
            }
        }
        // Position offsets (for absolute/fixed positioning)
        else if (strcmp(prop, "top") == 0) {
            float offset;
            if (sscanf(val, "%fpx", &offset) == 1) {
                style->absolute_y = offset;
            } else if (strcmp(val, "auto") == 0) {
                // Keep default
            }
        }
        else if (strcmp(prop, "right") == 0) {
            float offset;
            if (sscanf(val, "%fpx", &offset) == 1) {
                // Note: right is distance from right edge, stored separately
                // For now we just parse the value (full implementation requires layout engine support)
            }
        }
        else if (strcmp(prop, "bottom") == 0) {
            float offset;
            if (sscanf(val, "%fpx", &offset) == 1) {
                // Note: bottom is distance from bottom edge (requires layout engine support)
            }
        }
        else if (strcmp(prop, "left") == 0) {
            float offset;
            if (sscanf(val, "%fpx", &offset) == 1) {
                style->absolute_x = offset;
            }
        }
        // CSS filters (blur, brightness, contrast, etc.)
        else if (strcmp(prop, "filter") == 0) {
            // filter: none | <filter-function-list>
            if (strcmp(val, "none") == 0) {
                style->filter_count = 0;
            } else {
                // Parse filter functions: blur(5px), brightness(1.5), contrast(0.8), etc.
                // For now, parse a single filter function
                // Full implementation would parse multiple space-separated functions

                char buf[512];
                strncpy(buf, val, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';

                // Try to parse each filter type
                // blur()
                if (strncmp(buf, "blur(", 5) == 0) {
                    float blur;
                    if (sscanf(buf + 5, "%fpx)", &blur) == 1 || sscanf(buf + 5, "%f)", &blur) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_BLUR;
                            style->filters[style->filter_count].value = blur;
                            style->filter_count++;
                        }
                    }
                }
                // brightness()
                else if (strncmp(buf, "brightness(", 11) == 0) {
                    float brightness;
                    if (sscanf(buf + 11, "%f)", &brightness) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_BRIGHTNESS;
                            style->filters[style->filter_count].value = brightness;
                            style->filter_count++;
                        }
                    }
                }
                // contrast()
                else if (strncmp(buf, "contrast(", 8) == 0) {
                    float contrast;
                    if (sscanf(buf + 8, "%f)", &contrast) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_CONTRAST;
                            style->filters[style->filter_count].value = contrast;
                            style->filter_count++;
                        }
                    }
                }
                // grayscale()
                else if (strncmp(buf, "grayscale(", 10) == 0) {
                    float grayscale;
                    if (sscanf(buf + 10, "%f)", &grayscale) == 1 || sscanf(buf + 10, "%f%%)", &grayscale) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_GRAYSCALE;
                            style->filters[style->filter_count].value = grayscale;
                            style->filter_count++;
                        }
                    }
                }
                // hue-rotate()
                else if (strncmp(buf, "hue-rotate(", 11) == 0) {
                    float angle;
                    if (sscanf(buf + 11, "%fdeg)", &angle) == 1 || sscanf(buf + 11, "%frad)", &angle) == 1 || sscanf(buf + 11, "%f)", &angle) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_HUE_ROTATE;
                            style->filters[style->filter_count].value = angle;
                            style->filter_count++;
                        }
                    }
                }
                // invert()
                else if (strncmp(buf, "invert(", 7) == 0) {
                    float invert;
                    if (sscanf(buf + 7, "%f)", &invert) == 1 || sscanf(buf + 7, "%f%%)", &invert) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_INVERT;
                            style->filters[style->filter_count].value = invert;
                            style->filter_count++;
                        }
                    }
                }
                // opacity()
                else if (strncmp(buf, "opacity(", 8) == 0) {
                    float opacity;
                    if (sscanf(buf + 8, "%f)", &opacity) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_OPACITY;
                            style->filters[style->filter_count].value = opacity;
                            style->filter_count++;
                        }
                    }
                }
                // saturate()
                else if (strncmp(buf, "saturate(", 9) == 0) {
                    float saturate;
                    if (sscanf(buf + 9, "%f)", &saturate) == 1 || sscanf(buf + 9, "%f%%)", &saturate) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_SATURATE;
                            style->filters[style->filter_count].value = saturate;
                            style->filter_count++;
                        }
                    }
                }
                // sepia()
                else if (strncmp(buf, "sepia(", 6) == 0) {
                    float sepia;
                    if (sscanf(buf + 6, "%f)", &sepia) == 1 || sscanf(buf + 6, "%f%%)", &sepia) == 1) {
                        if (style->filter_count < IR_MAX_FILTERS) {
                            style->filters[style->filter_count].type = IR_FILTER_SEPIA;
                            style->filters[style->filter_count].value = sepia;
                            style->filter_count++;
                        }
                    }
                }
            }
        }
        // Grid item placement (grid-column, grid-row, grid-area)
        else if (strcmp(prop, "grid-column") == 0) {
            // Parse grid-column: <start-line> / <end-line> or just <start-line>
            // Examples: "1", "1 / 3", "span 2", "1 / span 2"
            char buf[256];
            strncpy(buf, val, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            char* slash_pos = strchr(buf, '/');
            if (slash_pos) {
                // Has both start and end
                *slash_pos = '\0';
                char* start = buf;
                char* end = slash_pos + 1;

                // Trim whitespace
                while (*start == ' ') start++;
                while (*end == ' ') end++;

                // Parse start
                if (strncmp(start, "span ", 5) == 0) {
                    int span;
                    if (sscanf(start + 5, "%d", &span) == 1) {
                        style->grid_item.column_start = -span;  // Negative indicates span
                    }
                } else {
                    int line;
                    if (sscanf(start, "%d", &line) == 1) {
                        style->grid_item.column_start = line;
                    }
                }

                // Parse end
                if (strncmp(end, "span ", 5) == 0) {
                    int span;
                    if (sscanf(end + 5, "%d", &span) == 1) {
                        style->grid_item.column_end = -span;
                    }
                } else {
                    int line;
                    if (sscanf(end, "%d", &line) == 1) {
                        style->grid_item.column_end = line;
                    }
                }
            } else {
                // Only start line provided
                char* ptr = buf;
                while (*ptr == ' ') ptr++;
                if (strncmp(ptr, "span ", 5) == 0) {
                    int span;
                    if (sscanf(ptr + 5, "%d", &span) == 1) {
                        style->grid_item.column_start = -span;
                    }
                } else {
                    int line;
                    if (sscanf(ptr, "%d", &line) == 1) {
                        style->grid_item.column_start = line;
                    }
                }
            }
        }
        else if (strcmp(prop, "grid-row") == 0) {
            // Parse grid-row: <start-line> / <end-line> or just <start-line>
            char buf[256];
            strncpy(buf, val, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            char* slash_pos = strchr(buf, '/');
            if (slash_pos) {
                // Has both start and end
                *slash_pos = '\0';
                char* start = buf;
                char* end = slash_pos + 1;

                // Trim whitespace
                while (*start == ' ') start++;
                while (*end == ' ') end++;

                // Parse start
                if (strncmp(start, "span ", 5) == 0) {
                    int span;
                    if (sscanf(start + 5, "%d", &span) == 1) {
                        style->grid_item.row_start = -span;
                    }
                } else {
                    int line;
                    if (sscanf(start, "%d", &line) == 1) {
                        style->grid_item.row_start = line;
                    }
                }

                // Parse end
                if (strncmp(end, "span ", 5) == 0) {
                    int span;
                    if (sscanf(end + 5, "%d", &span) == 1) {
                        style->grid_item.row_end = -span;
                    }
                } else {
                    int line;
                    if (sscanf(end, "%d", &line) == 1) {
                        style->grid_item.row_end = line;
                    }
                }
            } else {
                // Only start line provided
                char* ptr = buf;
                while (*ptr == ' ') ptr++;
                if (strncmp(ptr, "span ", 5) == 0) {
                    int span;
                    if (sscanf(ptr + 5, "%d", &span) == 1) {
                        style->grid_item.row_start = -span;
                    }
                } else {
                    int line;
                    if (sscanf(ptr, "%d", &line) == 1) {
                        style->grid_item.row_start = line;
                    }
                }
            }
        }
        else if (strcmp(prop, "grid-area") == 0) {
            // Parse grid-area: <row-start> / <column-start> / <row-end> / <column-end>
            // Or: <row-start> / <column-start> (implicit end)
            char buf[256];
            strncpy(buf, val, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            // Split by '/'
            char* parts[4] = {NULL, NULL, NULL, NULL};
            int count = 0;
            char* token = buf;
            char* slash = buf;

            while (slash && count < 4) {
                slash = strchr(token, '/');
                if (slash) {
                    *slash = '\0';
                    parts[count++] = token;
                    token = slash + 1;
                } else if (count < 4) {
                    parts[count++] = token;
                }
            }

            // Parse row-start
            if (parts[0]) {
                while (*parts[0] == ' ') parts[0]++;
                if (strncmp(parts[0], "span ", 5) == 0) {
                    int span;
                    if (sscanf(parts[0] + 5, "%d", &span) == 1) {
                        style->grid_item.row_start = -span;
                    }
                } else {
                    int line;
                    if (sscanf(parts[0], "%d", &line) == 1) {
                        style->grid_item.row_start = line;
                    }
                }
            }

            // Parse column-start
            if (parts[1]) {
                while (*parts[1] == ' ') parts[1]++;
                if (strncmp(parts[1], "span ", 5) == 0) {
                    int span;
                    if (sscanf(parts[1] + 5, "%d", &span) == 1) {
                        style->grid_item.column_start = -span;
                    }
                } else {
                    int line;
                    if (sscanf(parts[1], "%d", &line) == 1) {
                        style->grid_item.column_start = line;
                    }
                }
            }

            // Parse row-end (optional)
            if (parts[2]) {
                while (*parts[2] == ' ') parts[2]++;
                if (strncmp(parts[2], "span ", 5) == 0) {
                    int span;
                    if (sscanf(parts[2] + 5, "%d", &span) == 1) {
                        style->grid_item.row_end = -span;
                    }
                } else {
                    int line;
                    if (sscanf(parts[2], "%d", &line) == 1) {
                        style->grid_item.row_end = line;
                    }
                }
            }

            // Parse column-end (optional)
            if (parts[3]) {
                while (*parts[3] == ' ') parts[3]++;
                if (strncmp(parts[3], "span ", 5) == 0) {
                    int span;
                    if (sscanf(parts[3] + 5, "%d", &span) == 1) {
                        style->grid_item.column_end = -span;
                    }
                } else {
                    int line;
                    if (sscanf(parts[3], "%d", &line) == 1) {
                        style->grid_item.column_end = line;
                    }
                }
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
            } else if (strcmp(val, "inline-flex") == 0) {
                layout->mode = IR_LAYOUT_MODE_INLINE_FLEX;
            } else if (strcmp(val, "grid") == 0) {
                layout->mode = IR_LAYOUT_MODE_GRID;
            } else if (strcmp(val, "inline-grid") == 0) {
                layout->mode = IR_LAYOUT_MODE_INLINE_GRID;
            } else if (strcmp(val, "block") == 0) {
                layout->mode = IR_LAYOUT_MODE_BLOCK;
            } else if (strcmp(val, "inline") == 0) {
                layout->mode = IR_LAYOUT_MODE_INLINE;
            } else if (strcmp(val, "inline-block") == 0) {
                layout->mode = IR_LAYOUT_MODE_INLINE_BLOCK;
            } else if (strcmp(val, "none") == 0) {
                layout->mode = IR_LAYOUT_MODE_NONE;
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
        else if (strcmp(prop, "flex-basis") == 0) {
            // Parse flex-basis: <length> | <percentage> | auto | content
            if (strcmp(val, "auto") == 0 || strcmp(val, "content") == 0) {
                layout->flex.basis = 0;  // 0 means auto/content
            } else {
                float basis;
                char unit[16] = "";
                if (sscanf(val, "%f%15s", &basis, unit) >= 1) {
                    if (strcmp(unit, "px") == 0 || unit[0] == '\0') {
                        layout->flex.basis = basis;
                    } else if (strcmp(unit, "%") == 0) {
                        // For percentage, store as negative value to indicate percentage
                        // The layout engine would need to handle this
                        layout->flex.basis = -basis;  // Negative indicates percentage
                    } else if (strcmp(unit, "rem") == 0) {
                        layout->flex.basis = basis * 16;  // Convert rem to px (assuming 16px base)
                    } else if (strcmp(unit, "em") == 0) {
                        layout->flex.basis = basis * 16;  // Convert em to px (approximate)
                    } else {
                        layout->flex.basis = basis;  // Unknown unit, store as-is
                    }
                }
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
        // Aspect ratio (for maintaining proportional dimensions)
        else if (strcmp(prop, "aspect-ratio") == 0) {
            // Parse aspect-ratio: 16/9, 4/3, 1/1, etc.
            float width, height;
            if (sscanf(val, "%f/%f", &width, &height) == 2) {
                if (height > 0) {
                    layout->aspect_ratio = width / height;
                }
            } else if (sscanf(val, "%f", &width) == 1) {
                // Single value is treated as width/height ratio
                layout->aspect_ratio = width;
            }
        }

        // Grid properties
        else if (strcmp(prop, "grid-template-columns") == 0) {
            // Parse grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)) or simple values
            layout->mode = IR_LAYOUT_MODE_GRID;
            layout->grid.column_count = 0;
            layout->grid.use_column_repeat = false;

            // Check for repeat() function first (before tokenizing by space)
            const char* val_trimmed = val;
            while (*val_trimmed && isspace(*val_trimmed)) val_trimmed++;

            if (strncmp(val_trimmed, "repeat(", 7) == 0) {
                // Parse repeat() syntax: repeat(auto-fit, minmax(240px, 1fr))
                if (parse_grid_repeat(val_trimmed, &layout->grid.column_repeat)) {
                    layout->grid.use_column_repeat = true;
                }
            } else {
                // Parse simple space-separated values: "1fr 1fr 1fr" or "240px 1fr"
                char* val_copy = strdup(val);
                char* token = strtok(val_copy, " ");
                while (token && layout->grid.column_count < IR_MAX_GRID_TRACKS) {
                    IRGridTrack track = {0};
                    if (parse_grid_track_value(token, &track)) {
                        layout->grid.columns[layout->grid.column_count] = track;
                        layout->grid.column_count++;
                    }
                    token = strtok(NULL, " ");
                }
                free(val_copy);
            }
        }
        else if (strcmp(prop, "grid-template-rows") == 0) {
            // Parse grid-template-rows: repeat() or simple values
            layout->mode = IR_LAYOUT_MODE_GRID;
            layout->grid.row_count = 0;
            layout->grid.use_row_repeat = false;

            // Check for repeat() function first
            const char* val_trimmed = val;
            while (*val_trimmed && isspace(*val_trimmed)) val_trimmed++;

            if (strncmp(val_trimmed, "repeat(", 7) == 0) {
                if (parse_grid_repeat(val_trimmed, &layout->grid.row_repeat)) {
                    layout->grid.use_row_repeat = true;
                }
            } else {
                // Parse simple space-separated values
                char* val_copy = strdup(val);
                char* token = strtok(val_copy, " ");
                while (token && layout->grid.row_count < IR_MAX_GRID_TRACKS) {
                    IRGridTrack track = {0};
                    if (parse_grid_track_value(token, &track)) {
                        layout->grid.rows[layout->grid.row_count] = track;
                        layout->grid.row_count++;
                    }
                    token = strtok(NULL, " ");
                }
                free(val_copy);
            }
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
