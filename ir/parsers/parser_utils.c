/**
 * Parser Utilities
 * Shared utilities for all parsers to reduce code duplication
 */

#include "parser_utils.h"
#include "../ir_builder.h"
#include "../ir_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Component Creation Helpers
// ============================================================================

IRComponent* parser_create_component(IRComponentType type) {
    IRComponent* comp = ir_create_component(type);
    if (!comp) {
        PARSER_ERROR("Failed to create component of type %d", type);
        return NULL;
    }
    return comp;
}

bool parser_set_component_text(IRComponent* comp, const char* text) {
    if (!comp) {
        PARSER_ERROR("Cannot set text on NULL component");
        return false;
    }
    if (!text) {
        // Setting NULL text is valid (clears text)
        if (comp->text_content) {
            free(comp->text_content);
            comp->text_content = NULL;
        }
        return true;
    }

    char* new_text = strdup(text);
    if (!new_text) {
        PARSER_ERROR("Failed to allocate memory for text");
        return false;
    }

    if (comp->text_content) {
        free(comp->text_content);
    }
    comp->text_content = new_text;
    return true;
}

bool parser_add_child_component(IRComponent* parent, IRComponent* child) {
    if (!parent) {
        PARSER_ERROR("Cannot add child to NULL parent");
        return false;
    }
    if (!child) {
        PARSER_ERROR("Cannot add NULL child to parent");
        return false;
    }

    ir_add_child(parent, child);
    return true;
}

// ============================================================================
// Style Parsing Helpers
// ============================================================================

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parser_parse_color(const char* color_str, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (!color_str || !r || !g || !b || !a) {
        return false;
    }

    // Default alpha to fully opaque
    *a = 255;

    // Handle hex colors
    if (color_str[0] == '#') {
        size_t len = strlen(color_str + 1);
        const char* hex = color_str + 1;

        if (len == 3) {
            // #RGB
            int rv = hex_char_to_int(hex[0]);
            int gv = hex_char_to_int(hex[1]);
            int bv = hex_char_to_int(hex[2]);
            if (rv < 0 || gv < 0 || bv < 0) return false;
            *r = (uint8_t)(rv * 17);
            *g = (uint8_t)(gv * 17);
            *b = (uint8_t)(bv * 17);
            return true;
        } else if (len == 4) {
            // #RGBA
            int rv = hex_char_to_int(hex[0]);
            int gv = hex_char_to_int(hex[1]);
            int bv = hex_char_to_int(hex[2]);
            int av = hex_char_to_int(hex[3]);
            if (rv < 0 || gv < 0 || bv < 0 || av < 0) return false;
            *r = (uint8_t)(rv * 17);
            *g = (uint8_t)(gv * 17);
            *b = (uint8_t)(bv * 17);
            *a = (uint8_t)(av * 17);
            return true;
        } else if (len == 6) {
            // #RRGGBB
            unsigned int rv, gv, bv;
            if (sscanf(hex, "%2x%2x%2x", &rv, &gv, &bv) != 3) return false;
            *r = (uint8_t)rv;
            *g = (uint8_t)gv;
            *b = (uint8_t)bv;
            return true;
        } else if (len == 8) {
            // #RRGGBBAA
            unsigned int rv, gv, bv, av;
            if (sscanf(hex, "%2x%2x%2x%2x", &rv, &gv, &bv, &av) != 4) return false;
            *r = (uint8_t)rv;
            *g = (uint8_t)gv;
            *b = (uint8_t)bv;
            *a = (uint8_t)av;
            return true;
        }
    }

    // Handle named colors
    if (parser_str_equal_ignore_case(color_str, "transparent")) {
        *r = *g = *b = *a = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "white")) {
        *r = *g = *b = 255;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "black")) {
        *r = *g = *b = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "red")) {
        *r = 255; *g = *b = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "green")) {
        *r = 0; *g = 128; *b = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "blue")) {
        *r = *g = 0; *b = 255;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "yellow")) {
        *r = *g = 255; *b = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "cyan")) {
        *r = 0; *g = *b = 255;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "magenta")) {
        *r = *b = 255; *g = 0;
        return true;
    }
    if (parser_str_equal_ignore_case(color_str, "gray") ||
        parser_str_equal_ignore_case(color_str, "grey")) {
        *r = *g = *b = 128;
        return true;
    }

    return false;
}

bool parser_parse_dimension(const char* dim_str, IRDimensionType* type, float* value) {
    if (!dim_str || !type || !value) {
        return false;
    }

    // Handle "auto"
    if (parser_str_equal_ignore_case(dim_str, "auto")) {
        *type = IR_DIMENSION_AUTO;
        *value = 0.0f;
        return true;
    }

    // Try to parse numeric value
    char* end = NULL;
    float val = strtof(dim_str, &end);

    if (end == dim_str) {
        // No numeric value found
        return false;
    }

    *value = val;

    // Check unit suffix
    if (*end == '\0' || parser_str_equal_ignore_case(end, "px")) {
        *type = IR_DIMENSION_PX;
        return true;
    }
    if (*end == '%') {
        *type = IR_DIMENSION_PERCENT;
        return true;
    }
    if (parser_str_equal_ignore_case(end, "em")) {
        *type = IR_DIMENSION_EM;
        return true;
    }
    if (parser_str_equal_ignore_case(end, "rem")) {
        *type = IR_DIMENSION_REM;
        return true;
    }
    if (parser_str_equal_ignore_case(end, "vw")) {
        *type = IR_DIMENSION_VW;
        return true;
    }
    if (parser_str_equal_ignore_case(end, "vh")) {
        *type = IR_DIMENSION_VH;
        return true;
    }

    // Default to pixels if no unit
    *type = IR_DIMENSION_PX;
    return true;
}

float parser_parse_px_value(const char* str) {
    if (!str) return 0.0f;
    float value = 0.0f;
    sscanf(str, "%f", &value);
    return value;
}

bool parser_is_transparent_color(const char* color) {
    if (!color) return false;
    return strcmp(color, "#00000000") == 0 ||
           parser_str_equal_ignore_case(color, "transparent");
}

// ============================================================================
// String Utilities
// ============================================================================

bool parser_str_equal_ignore_case(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;  // Both should be null terminators
}

char* parser_trim_whitespace(const char* str) {
    if (!str) return NULL;

    // Find start (skip leading whitespace)
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        // String was all whitespace
        return strdup("");
    }

    // Find end (skip trailing whitespace)
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    // Allocate and copy trimmed string
    size_t len = (size_t)(end - str + 1);
    char* result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

bool parser_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool parser_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}
