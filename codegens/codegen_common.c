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
#include <sys/stat.h>

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

    // KIR JSON structure can be:
    // - Old format: { "component": { ... } }
    // - New format: { "root": { ... } }
    cJSON* component = cJSON_GetObjectItem(root, "component");
    if (!component) {
        component = cJSON_GetObjectItem(root, "root");
    }
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
// Tk-Based Codegen Utilities Implementation
// ============================================================================

/**
 * Map KIR element type to Tk widget type.
 *
 * *** DEPRECATED ***
 *
 * This function is Tk-biased and should NOT be used for non-Tk targets.
 * Please use toolkit-specific profiles instead:
 *   - Tk targets: Use toolkit_map_widget_type(tk_profile, ...)
 *   - Draw targets: Use drawir_type_from_kir() in draw_ir_builder.c
 *   - Other targets: Use the appropriate toolkit profile
 *
 * This function only works for Tk toolkit and will emit a deprecation warning.
 * It will be removed in a future version.
 *
 * @param element_type KIR element type (e.g., "Button", "Text")
 * @return Tk widget type string (e.g., "button", "label")
 */
const char* codegen_map_kir_to_tk_widget(const char* element_type) {
    static bool warned = false;

    // Emit deprecation warning only once
    if (!warned) {
        fprintf(stderr,
                "\n"
                "╌──────────────────────────────────────────────────────────────────────────────╌\n"
                "  DEPRECATION WARNING: codegen_map_kir_to_tk_widget() is deprecated!\n"
                "╌──────────────────────────────────────────────────────────────────────────────╌\n"
                "  This function is Tk-biased and should NOT be used for non-Tk targets.\n\n"
                "  For NEW code, please use toolkit-specific profiles:\n"
                "    • Tk targets:     toolkit_map_widget_type(tk_profile, ...)\n"
                "    • Draw targets:   drawir_type_from_kir(...)\n"
                "    • DOM targets:    toolkit_map_widget_type(dom_profile, ...)\n\n"
                "  This function will be removed in Kryon v2.0.\n"
                "╌──────────────────────────────────────────────────────────────────────────────╌\n\n");
        warned = true;
    }

    if (!element_type) return "frame";

    // Container widgets
    if (strcmp(element_type, "Container") == 0) return "frame";
    if (strcmp(element_type, "Row") == 0) return "frame";
    if (strcmp(element_type, "Column") == 0) return "frame";
    if (strcmp(element_type, "Center") == 0) return "frame";
    if (strcmp(element_type, "Frame") == 0) return "frame";

    // Basic widgets
    if (strcmp(element_type, "Button") == 0) return "button";
    if (strcmp(element_type, "Text") == 0) return "label";
    if (strcmp(element_type, "Label") == 0) return "label";
    if (strcmp(element_type, "Input") == 0) return "entry";
    if (strcmp(element_type, "TextInput") == 0) return "entry";

    // Complex widgets
    if (strcmp(element_type, "Checkbox") == 0) return "checkbutton";
    if (strcmp(element_type, "Radio") == 0) return "radiobutton";
    if (strcmp(element_type, "Dropdown") == 0) return "ttk::combobox";
    if (strcmp(element_type, "Canvas") == 0) return "canvas";
    if (strcmp(element_type, "Image") == 0) return "label";  // Use label with image
    if (strcmp(element_type, "ListBox") == 0) return "listbox";
    if (strcmp(element_type, "Menu") == 0) return "menu";
    if (strcmp(element_type, "Progress") == 0) return "ttk::progressbar";
    if (strcmp(element_type, "Slider") == 0) return "scale";
    if (strcmp(element_type, "Table") == 0) return "ttk::treeview";
    if (strcmp(element_type, "Tabs") == 0) return "ttk::notebook";

    // Default fallback
    return "frame";
}

void codegen_extract_widget_props(cJSON* component,
                                  const char** out_text,
                                  const char** out_width,
                                  const char** out_height,
                                  const char** out_background,
                                  const char** out_foreground,
                                  const char** out_font) {
    if (!component) return;

    // Extract text
    if (out_text) {
        cJSON* text = cJSON_GetObjectItem(component, "text");
        *out_text = (text && cJSON_IsString(text)) ? text->valuestring : NULL;
    }

    // Extract width
    if (out_width) {
        cJSON* width = cJSON_GetObjectItem(component, "width");
        if (width && cJSON_IsString(width)) {
            *out_width = width->valuestring;
        } else if (width && cJSON_IsNumber(width)) {
            // Static buffer for number conversion
            static char width_buf[32];
            snprintf(width_buf, sizeof(width_buf), "%d", width->valueint);
            *out_width = width_buf;
        } else {
            *out_width = NULL;
        }
    }

    // Extract height
    if (out_height) {
        cJSON* height = cJSON_GetObjectItem(component, "height");
        if (height && cJSON_IsString(height)) {
            *out_height = height->valuestring;
        } else if (height && cJSON_IsNumber(height)) {
            static char height_buf[32];
            snprintf(height_buf, sizeof(height_buf), "%d", height->valueint);
            *out_height = height_buf;
        } else {
            *out_height = NULL;
        }
    }

    // Extract background
    if (out_background) {
        cJSON* background = cJSON_GetObjectItem(component, "background");
        *out_background = (background && cJSON_IsString(background)) ? background->valuestring : NULL;
    }

    // Extract foreground (color)
    if (out_foreground) {
        cJSON* color = cJSON_GetObjectItem(component, "color");
        *out_foreground = (color && cJSON_IsString(color)) ? color->valuestring : NULL;
    }

    // Extract font
    if (out_font) {
        cJSON* font = cJSON_GetObjectItem(component, "font");
        *out_font = (font && cJSON_IsString(font)) ? font->valuestring : NULL;
    }
}

int codegen_parse_size_value(const char* value) {
    if (!value) return 0;

    // Parse as float first to handle "200.0px" format
    float result = 0.0f;
    if (sscanf(value, "%f", &result) == 1) {
        return (int)result;
    }

    return 0;
}

bool codegen_is_percentage_value(const char* value) {
    if (!value) return false;
    return strchr(value, '%') != NULL;
}

bool codegen_is_layout_container(const char* type) {
    if (!type) return false;

    return strcmp(type, "Row") == 0 ||
           strcmp(type, "Column") == 0 ||
           strcmp(type, "Container") == 0 ||
           strcmp(type, "Center") == 0 ||
           strcmp(type, "Canvas") == 0 ||
           strcmp(type, "Table") == 0 ||
           strcmp(type, "Tabs") == 0 ||
           strcmp(type, "Frame") == 0;
}

CodegenLayoutType codegen_detect_layout_type(cJSON* component) {
    if (!component) return CODEGEN_LAYOUT_AUTO;

    // Check for grid layout (row property)
    cJSON* row = cJSON_GetObjectItem(component, "row");
    if (row && cJSON_IsNumber(row)) {
        return CODEGEN_LAYOUT_GRID;
    }

    // Check for absolute positioning (left AND top properties)
    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");
    if (left && cJSON_IsNumber(left) && top && cJSON_IsNumber(top)) {
        return CODEGEN_LAYOUT_PLACE;
    }

    // Default to pack layout
    return CODEGEN_LAYOUT_PACK;
}

void codegen_extract_layout_options(cJSON* component,
                                    const char* parent_type,
                                    CodegenLayoutOptions* out_options) {
    if (!component || !out_options) return;

    memset(out_options, 0, sizeof(CodegenLayoutOptions));

    // Store parent type
    out_options->parent_type = parent_type;

    // Check for explicit size
    cJSON* width = cJSON_GetObjectItem(component, "width");
    cJSON* height = cJSON_GetObjectItem(component, "height");
    out_options->has_explicit_size = (width != NULL) || (height != NULL);

    // Check for absolute position
    cJSON* left = cJSON_GetObjectItem(component, "left");
    cJSON* top = cJSON_GetObjectItem(component, "top");
    if (left && cJSON_IsNumber(left) && top && cJSON_IsNumber(top)) {
        out_options->has_position = true;
        out_options->left = left->valueint;
        out_options->top = top->valueint;
    } else {
        out_options->has_position = false;
    }
}

CodegenHandlerTracker* codegen_handler_tracker_create(void) {
    CodegenHandlerTracker* tracker = calloc(1, sizeof(CodegenHandlerTracker));
    if (!tracker) return NULL;

    tracker->count = 0;
    return tracker;
}

bool codegen_handler_tracker_contains(CodegenHandlerTracker* tracker,
                                      const char* handler_name) {
    if (!tracker || !handler_name) return false;

    for (int i = 0; i < tracker->count; i++) {
        if (tracker->handlers[i] && strcmp(tracker->handlers[i], handler_name) == 0) {
            return true;
        }
    }
    return false;
}

bool codegen_handler_tracker_mark(CodegenHandlerTracker* tracker,
                                  const char* handler_name) {
    if (!tracker || !handler_name) return false;
    if (tracker->count >= 256) return false;  // Full

    tracker->handlers[tracker->count++] = strdup(handler_name);
    return true;
}

void codegen_handler_tracker_clear(CodegenHandlerTracker* tracker) {
    if (!tracker) return;

    for (int i = 0; i < tracker->count; i++) {
        if (tracker->handlers[i]) {
            free(tracker->handlers[i]);
            tracker->handlers[i] = NULL;
        }
    }
    tracker->count = 0;
}

void codegen_handler_tracker_free(CodegenHandlerTracker* tracker) {
    if (!tracker) return;

    codegen_handler_tracker_clear(tracker);
    free(tracker);
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

// ============================================================================
// Multi-File Codegen Utilities
// ============================================================================

bool codegen_processed_modules_contains(CodegenProcessedModules* pm, const char* module_id) {
    if (!pm || !module_id) return false;

    for (int i = 0; i < pm->count; i++) {
        if (pm->modules[i] && strcmp(pm->modules[i], module_id) == 0) {
            return true;
        }
    }
    return false;
}

void codegen_processed_modules_add(CodegenProcessedModules* pm, const char* module_id) {
    if (!pm || !module_id) return;

    if (pm->count >= CODEGEN_MAX_PROCESSED_MODULES) {
        codegen_warn("Processed modules limit reached (%d), cannot add: %s",
                     CODEGEN_MAX_PROCESSED_MODULES, module_id);
        return;
    }

    pm->modules[pm->count++] = strdup(module_id);
}

void codegen_processed_modules_free(CodegenProcessedModules* pm) {
    if (!pm) return;

    for (int i = 0; i < pm->count; i++) {
        free(pm->modules[i]);
        pm->modules[i] = NULL;
    }
    pm->count = 0;
}

bool codegen_is_internal_module(const char* module_id) {
    if (!module_id) return false;

    // Check for kryon/ prefix
    if (strncmp(module_id, "kryon/", 6) == 0) return true;

    // Check for known internal module names
    if (strcmp(module_id, "dsl") == 0) return true;
    if (strcmp(module_id, "ffi") == 0) return true;
    if (strcmp(module_id, "runtime") == 0) return true;
    if (strcmp(module_id, "reactive") == 0) return true;
    if (strcmp(module_id, "runtime_web") == 0) return true;
    if (strcmp(module_id, "kryon") == 0) return true;

    return false;
}

// ============================================================================
// Plugin Manifest Loading
// ============================================================================

// Static storage for discovered plugins from manifest
static char* g_discovered_plugins[64];
static int g_discovered_plugin_count = 0;
static bool g_plugins_loaded = false;

void codegen_load_plugin_manifest(const char* build_dir) {
    if (g_plugins_loaded) return;

    char manifest_path[2048];
    snprintf(manifest_path, sizeof(manifest_path), "%s/plugins.txt",
             build_dir ? build_dir : "build");

    FILE* f = fopen(manifest_path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && g_discovered_plugin_count < 64) {
            // Strip newline
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (len > 1 && line[len-2] == '\r') line[len-2] = '\0';
            if (strlen(line) > 0) {
                g_discovered_plugins[g_discovered_plugin_count++] = strdup(line);
            }
        }
        fclose(f);
    }
    g_plugins_loaded = true;
}

bool codegen_is_external_plugin(const char* module_id) {
    if (!module_id) return false;

    // Load manifest if not already loaded
    if (!g_plugins_loaded) {
        codegen_load_plugin_manifest(NULL);
    }

    // Check against discovered plugins from manifest
    for (int i = 0; i < g_discovered_plugin_count; i++) {
        if (strcmp(module_id, g_discovered_plugins[i]) == 0) {
            return true;
        }
    }

    // Fallback: known external plugins for backwards compatibility
    // (covers case where manifest doesn't exist or wasn't written)
    if (strcmp(module_id, "datetime") == 0) return true;
    if (strcmp(module_id, "storage") == 0) return true;
    if (strcmp(module_id, "animations") == 0) return true;
    if (strcmp(module_id, "http") == 0) return true;
    if (strcmp(module_id, "audio") == 0) return true;
    if (strcmp(module_id, "filesystem") == 0) return true;
    if (strcmp(module_id, "database") == 0) return true;
    if (strcmp(module_id, "math") == 0) return true;
    if (strcmp(module_id, "uuid") == 0) return true;

    return false;
}

void codegen_get_parent_dir(const char* path, char* parent, size_t parent_size) {
    if (!path || !parent || parent_size == 0) return;

    strncpy(parent, path, parent_size - 1);
    parent[parent_size - 1] = '\0';

    char* last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        parent[0] = '.';
        parent[1] = '\0';
    }
}

bool codegen_mkdir_p(const char* path) {
    if (!path) return false;

    struct stat st = {0};
    if (stat(path, &st) == 0) {
        // Path exists
        return S_ISDIR(st.st_mode);
    }

    // Need to create - use mkdir -p (portable)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    int result = system(cmd);

    return result == 0;
}

bool codegen_write_file_with_mkdir(const char* path, const char* content) {
    if (!path || !content) return false;

    // Create a mutable copy for directory extraction
    char dir_path[2048];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // Find and create parent directory
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (!codegen_mkdir_p(dir_path)) {
            codegen_warn("Could not create directory: %s", dir_path);
        }
    }

    // Write the file
    FILE* f = fopen(path, "w");
    if (!f) {
        codegen_error("Could not write file: %s (%s)", path, strerror(errno));
        return false;
    }
    fputs(content, f);
    fclose(f);
    return true;
}

bool codegen_file_exists(const char* path) {
    if (!path) return false;

    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int codegen_copy_file(const char* src, const char* dst) {
    if (!src || !dst) return -1;

    FILE* in = fopen(src, "rb");
    if (!in) return -1;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

