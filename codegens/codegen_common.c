/**
 * Kryon Codegen - Common Utilities Implementation
 *
 * Shared utilities for all code generators to reduce code duplication.
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen_common.h"
#include "../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Thread-local error prefix for error reporting
static __thread char g_error_prefix[32] = "Codegen";

// Static buffer for color formatting (6+1 for # and null terminator, +2 for alpha)
static char g_color_buffer[10];

// ============================================================================
// File I/O Utilities
// ============================================================================

char* codegen_read_kir_file(const char* path, size_t* out_size) {
    if (!path) {
        codegen_error("NULL path provided");
        return NULL;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        codegen_error("Could not open KIR file: %s (%s)", path, strerror(errno));
        return NULL;
    }

    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) {
        codegen_error("Could not seek in file: %s", path);
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0) {
        codegen_error("Could not determine file size: %s", path);
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        codegen_error("Could not seek to start of file: %s", path);
        fclose(f);
        return NULL;
    }

    // Allocate buffer (+1 for null terminator)
    char* buffer = malloc(size + 1);
    if (!buffer) {
        codegen_error("Memory allocation failed for file: %s", path);
        fclose(f);
        return NULL;
    }

    // Read file content
    size_t bytes_read = fread(buffer, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) {
        codegen_error("Failed to read complete file: %s (got %zu of %ld bytes)",
                      path, bytes_read, size);
        free(buffer);
        fclose(f);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(f);

    if (out_size) {
        *out_size = (size_t)size;
    }

    return buffer;
}

bool codegen_write_output_file(const char* path, const char* content) {
    if (!path) {
        codegen_error("NULL output path provided");
        return false;
    }

    if (!content) {
        codegen_error("NULL content provided for output: %s", path);
        return false;
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        codegen_error("Could not write output file: %s (%s)", path, strerror(errno));
        return false;
    }

    fputs(content, f);
    fclose(f);

    return true;
}

// ============================================================================
// JSON Parsing Utilities
// ============================================================================

cJSON* codegen_parse_kir_json(const char* kir_json) {
    if (!kir_json) {
        codegen_error("NULL JSON string provided");
        return NULL;
    }

    cJSON* root = cJSON_Parse(kir_json);
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            codegen_error("JSON parse error at: %s", error_ptr);
        } else {
            codegen_error("Failed to parse JSON (unknown error)");
        }
        return NULL;
    }

    return root;
}

cJSON* codegen_extract_root_component(cJSON* root) {
    if (!root) return NULL;

    // KIR JSON structure: { "component": { ... } }
    cJSON* component = cJSON_GetObjectItem(root, "component");
    return component;
}

cJSON* codegen_extract_logic_block(cJSON* root) {
    if (!root) return NULL;

    // KIR JSON structure: { "logic_block": { ... } }
    cJSON* logic_block = cJSON_GetObjectItem(root, "logic_block");
    return logic_block;
}

const char* codegen_get_component_type(cJSON* component) {
    if (!component) return NULL;

    cJSON* type_item = cJSON_GetObjectItem(component, "type");
    if (type_item && cJSON_IsString(type_item)) {
        return type_item->valuestring;
    }

    return NULL;
}

cJSON* codegen_get_property(cJSON* component, const char* key) {
    if (!component || !key) return NULL;

    // Check in style properties
    cJSON* style = cJSON_GetObjectItem(component, "style");
    if (style) {
        cJSON* prop = cJSON_GetObjectItem(style, key);
        if (prop) return prop;
    }

    // Check in layout properties
    cJSON* layout = cJSON_GetObjectItem(component, "layout");
    if (layout) {
        cJSON* prop = cJSON_GetObjectItem(layout, key);
        if (prop) return prop;
    }

    // Check directly on component (for id, text, etc.)
    cJSON* prop = cJSON_GetObjectItem(component, key);
    return prop;
}

// ============================================================================
// Property Parsing Utilities
// ============================================================================

float codegen_parse_px_value(const char* str) {
    if (!str) return 0.0f;

    float value = 0.0f;
    if (sscanf(str, "%f", &value) == 1) {
        return value;
    }

    return 0.0f;
}

bool codegen_parse_percentage(const char* str, float* out_value) {
    if (!str || !out_value) return false;

    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '%') {
        // Extract percentage value
        if (sscanf(str, "%f", out_value) == 1) {
            return true;
        }
    }

    return false;
}

bool codegen_is_transparent_color(const char* color) {
    if (!color) return false;

    // Check for hex transparent
    if (strcmp(color, "#00000000") == 0) return true;
    if (strcmp(color, "#0000000") == 0) return true;  // Missing leading zero

    // Check for named transparent
    if (strcmp(color, "transparent") == 0) return true;

    return false;
}

bool codegen_parse_color_rgba(const char* color, uint8_t* out_r, uint8_t* out_g,
                              uint8_t* out_b, uint8_t* out_a) {
    if (!color || !out_r || !out_g || !out_b) return false;

    // Skip leading # if present
    const char* start = color;
    if (start[0] == '#') start++;

    // Check for 0x prefix
    if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
        start += 2;
    }

    size_t len = strlen(start);

    // Parse based on length
    if (len == 6) {
        // #RRGGBB format
        if (sscanf(start, "%02hhx%02hhx%02hhx", out_r, out_g, out_b) == 3) {
            if (out_a) *out_a = 255;
            return true;
        }
    } else if (len == 8) {
        // #RRGGBBAA format
        if (sscanf(start, "%02hhx%02hhx%02hhx%02hhx", out_r, out_g, out_b, out_a) == 4) {
            return true;
        }
    } else if (len == 3) {
        // #RGB shorthand format - expand each digit
        char expanded[7];
        expanded[0] = start[0]; expanded[1] = start[0];
        expanded[2] = start[1]; expanded[3] = start[1];
        expanded[4] = start[2]; expanded[5] = start[2];
        expanded[6] = '\0';
        if (sscanf(expanded, "%02hhx%02hhx%02hhx", out_r, out_g, out_b) == 3) {
            if (out_a) *out_a = 255;
            return true;
        }
    }

    return false;
}

const char* codegen_format_rgba_hex(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                     bool include_alpha) {
    if (include_alpha) {
        snprintf(g_color_buffer, sizeof(g_color_buffer), "#%02x%02x%02x%02x", r, g, b, a);
    } else {
        snprintf(g_color_buffer, sizeof(g_color_buffer), "#%02x%02x%02x", r, g, b);
    }
    return g_color_buffer;
}

// ============================================================================
// Error Reporting Utilities
// ============================================================================

void codegen_set_error_prefix(const char* prefix) {
    if (prefix) {
        snprintf(g_error_prefix, sizeof(g_error_prefix), "%s", prefix);
    } else {
        snprintf(g_error_prefix, sizeof(g_error_prefix), "Codegen");
    }
}

void codegen_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[%s Codegen] Error: ", g_error_prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

void codegen_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[%s Codegen] Warning: ", g_error_prefix);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

// ============================================================================
// StringBuilder Implementation
// ============================================================================

struct StringBuilder {
    char* buffer;
    size_t capacity;
    size_t length;
};

StringBuilder* sb_create(size_t initial_capacity) {
    StringBuilder* sb = (StringBuilder*)malloc(sizeof(StringBuilder));
    if (!sb) return NULL;
    
    sb->buffer = (char*)malloc(initial_capacity);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }
    
    sb->capacity = initial_capacity;
    sb->length = 0;
    sb->buffer[0] = '\0';
    return sb;
}

void sb_append(StringBuilder* sb, const char* str) {
    if (!sb || !str) return;
    
    size_t str_len = strlen(str);
    size_t needed = sb->length + str_len + 1;
    
    if (needed > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < needed) new_capacity *= 2;
        
        char* new_buffer = (char*)realloc(sb->buffer, new_capacity);
        if (!new_buffer) return;
        
        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
    }
    
    strcpy(sb->buffer + sb->length, str);
    sb->length += str_len;
}

void sb_append_fmt(StringBuilder* sb, const char* fmt, ...) {
    if (!sb || !fmt) return;
    
    va_list args;
    va_start(args, fmt);
    
    // Try with current buffer
    size_t available = sb->capacity - sb->length;
    int needed = vsnprintf(sb->buffer + sb->length, available, fmt, args);
    va_end(args);
    
    if (needed < 0) return;
    
    if ((size_t)needed >= available) {
        // Need to resize
        size_t new_capacity = sb->capacity + needed + 1;
        char* new_buffer = (char*)realloc(sb->buffer, new_capacity);
        if (!new_buffer) return;
        
        sb->buffer = new_buffer;
        sb->capacity = new_capacity;
        
        // Try again with new buffer
        va_start(args, fmt);
        vsnprintf(sb->buffer + sb->length, sb->capacity - sb->length, fmt, args);
        va_end(args);
    }
    
    sb->length += needed;
}

char* sb_get(StringBuilder* sb) {
    if (!sb) return NULL;
    return strdup(sb->buffer ? sb->buffer : "");
}

void sb_free(StringBuilder* sb) {
    if (!sb) return;
    free(sb->buffer);
    free(sb);
}

// ============================================================================
// TSX/React Helper Implementation
// ============================================================================

WindowConfig react_extract_window_config(cJSON* root) {
    WindowConfig config;

    // Initialize with defaults
    config.width = 800;
    config.height = 600;
    config.title = strdup("Kryon App");
    config.background = strdup("#1E1E1E");

    if (!root) return config;

    // Try to get from app section
    cJSON* app = cJSON_GetObjectItem(root, "app");
    if (app && cJSON_IsObject(app)) {
        cJSON* width = cJSON_GetObjectItem(app, "width");
        cJSON* height = cJSON_GetObjectItem(app, "height");
        cJSON* title = cJSON_GetObjectItem(app, "title");
        cJSON* background = cJSON_GetObjectItem(app, "background");

        if (width && cJSON_IsNumber(width)) config.width = width->valueint;
        if (height && cJSON_IsNumber(height)) config.height = height->valueint;
        if (title && cJSON_IsString(title)) {
            free(config.title);
            config.title = strdup(title->valuestring);
        }
        if (background && cJSON_IsString(background)) {
            free(config.background);
            config.background = strdup(background->valuestring);
        }
    } else {
        // Try to get directly from root
        cJSON* width = cJSON_GetObjectItem(root, "width");
        cJSON* height = cJSON_GetObjectItem(root, "height");
        cJSON* title = cJSON_GetObjectItem(root, "title");
        cJSON* background = cJSON_GetObjectItem(root, "background");

        if (width && cJSON_IsNumber(width)) config.width = width->valueint;
        if (height && cJSON_IsNumber(height)) config.height = height->valueint;
        if (title && cJSON_IsString(title)) {
            free(config.title);
            config.title = strdup(title->valuestring);
        }
        if (background && cJSON_IsString(background)) {
            free(config.background);
            config.background = strdup(background->valuestring);
        }
    }

    return config;
}

void react_free_window_config(WindowConfig* config) {
    if (!config) return;
    free(config->title);
    free(config->background);
    config->title = NULL;
    config->background = NULL;
}

char* react_generate_imports(int mode) {
    return strdup("import React from 'react';\nimport { kryonApp } from '@kryon/react-runtime';\n");
}

char* react_generate_state_hooks(cJSON* manifest, ReactContext* ctx) {
    if (!manifest || !cJSON_IsObject(manifest)) {
        return strdup("");
    }

    StringBuilder* sb = sb_create(1024);
    if (!sb) return strdup("");

    // Iterate through manifest and generate useState hooks
    cJSON* vars = cJSON_GetObjectItem(manifest, "variables");
    if (vars && cJSON_IsArray(vars)) {
        cJSON* var = NULL;
        cJSON_ArrayForEach(var, vars) {
            const char* name = cJSON_GetStringValue(var);
            if (name) {
                sb_append_fmt(sb, "    const [%s, set%s] = React.useState(initial%s);\n",
                             name, name, name);
            }
        }
    }

    char* result = sb_get(sb);
    sb_free(sb);
    return result;
}

char* react_generate_element(cJSON* component, ReactContext* ctx, int indent) {
    if (!component) return strdup("null");

    const char* type = codegen_get_component_type(component);
    if (!type) return strdup("null");

    StringBuilder* sb = sb_create(4096);
    if (!sb) return strdup("null");

    // Add indentation
    for (int i = 0; i < indent; i++) sb_append(sb, " ");

    // Generate element opening
    sb_append_fmt(sb, "<%s", type);

    // Add properties (simplified - would need full implementation)
    sb_append(sb, ">");

    // Add children (simplified)
    cJSON* children = cJSON_GetObjectItem(component, "children");
    if (children && cJSON_IsArray(children) && cJSON_GetArraySize(children) > 0) {
        sb_append(sb, "\n");
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            char* child_str = react_generate_element(child, ctx, indent + 2);
            sb_append(sb, child_str);
            free(child_str);
        }
        for (int i = 0; i < indent; i++) sb_append(sb, " ");
    }

    // Close element
    sb_append_fmt(sb, "</%s>\n", type);

    char* result = sb_get(sb);
    sb_free(sb);
    return result;
}

