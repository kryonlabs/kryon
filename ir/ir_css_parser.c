#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "ir_css_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

// Parse CSS color (rgba, rgb, hex, named)
bool ir_css_parse_color(const char* value, IRColor* out_color) {
    if (!value || !out_color) return false;

    // Handle "transparent"
    if (strcmp(value, "transparent") == 0) {
        out_color->type = IR_COLOR_TRANSPARENT;
        return true;
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

// Parse CSS spacing (margin/padding: top right bottom left)
bool ir_css_parse_spacing(const char* value, IRSpacing* out_spacing) {
    if (!value || !out_spacing) return false;

    float top, right, bottom, left;

    // Parse "10px 20px 10px 20px" format
    if (sscanf(value, "%fpx %fpx %fpx %fpx", &top, &right, &bottom, &left) == 4) {
        out_spacing->top = top;
        out_spacing->right = right;
        out_spacing->bottom = bottom;
        out_spacing->left = left;
        return true;
    }

    // Parse "10px" (all sides same)
    if (sscanf(value, "%fpx", &top) == 1) {
        out_spacing->top = top;
        out_spacing->right = top;
        out_spacing->bottom = top;
        out_spacing->left = top;
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
        else if (strcmp(prop, "background-color") == 0 || strcmp(prop, "background") == 0) {
            ir_css_parse_color(val, &style->background);
        } else if (strcmp(prop, "color") == 0) {
            ir_css_parse_color(val, &style->font.color);
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

        // Typography
        else if (strcmp(prop, "font-size") == 0) {
            float size;
            if (sscanf(val, "%fpx", &size) == 1) {
                style->font.size = size;
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
    }
}
