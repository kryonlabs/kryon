/**
 * React Code Generation Common Utilities
 * Shared logic for TSX and JSX generation
 */

#include "react_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

// =============================================================================
// String Builder Implementation
// =============================================================================

StringBuilder* sb_create(size_t initial_capacity) {
    StringBuilder* sb = malloc(sizeof(StringBuilder));
    if (!sb) return NULL;

    sb->capacity = initial_capacity;
    sb->size = 0;
    sb->buffer = malloc(sb->capacity);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }
    sb->buffer[0] = '\0';
    return sb;
}

bool sb_append(StringBuilder* sb, const char* str) {
    if (!sb || !str) return false;

    size_t len = strlen(str);
    while (sb->size + len >= sb->capacity) {
        sb->capacity *= 2;
        char* new_buffer = realloc(sb->buffer, sb->capacity);
        if (!new_buffer) return false;
        sb->buffer = new_buffer;
    }

    strcpy(sb->buffer + sb->size, str);
    sb->size += len;
    return true;
}

bool sb_append_fmt(StringBuilder* sb, const char* fmt, ...) {
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    return sb_append(sb, temp);
}

const char* sb_get(StringBuilder* sb) {
    return sb ? sb->buffer : NULL;
}

void sb_free(StringBuilder* sb) {
    if (sb) {
        free(sb->buffer);
        free(sb);
    }
}

// =============================================================================
// Mode-Aware Helper Functions
// =============================================================================

const char* react_type_annotation(ReactOutputMode mode, const char* typ) {
    if (mode != REACT_MODE_TYPESCRIPT) return "";

    if (strcmp(typ, "int") == 0 || strcmp(typ, "float") == 0) return ": number";
    if (strcmp(typ, "string") == 0) return ": string";
    if (strcmp(typ, "bool") == 0) return ": boolean";
    return ": any";
}

bool react_should_generate_interface(ReactOutputMode mode) {
    return mode == REACT_MODE_TYPESCRIPT;
}

const char* react_file_extension(ReactOutputMode mode) {
    return mode == REACT_MODE_TYPESCRIPT ? ".tsx" : ".jsx";
}

const char* react_generate_imports(ReactOutputMode mode) {
    (void)mode; // Same for both modes
    return "import { kryonApp, Column, Row, Button, Text, Input, useState, useEffect, useCallback, useMemo, useReducer } from '@kryon/react';";
}

// =============================================================================
// Property Mapping
// =============================================================================

/**
 * Convert event type to React camelCase event handler name
 * Examples: "click" -> "onClick", "keydown" -> "onKeyDown", "mouseenter" -> "onMouseEnter"
 */
static void react_event_type_to_handler_name(const char* evt_type, char* output, size_t output_size) {
    // Common compound event names that need special handling
    struct {
        const char* event;
        const char* react_name;
    } event_map[] = {
        {"keydown", "onKeyDown"},
        {"keyup", "onKeyUp"},
        {"keypress", "onKeyPress"},
        {"mousedown", "onMouseDown"},
        {"mouseup", "onMouseUp"},
        {"mousemove", "onMouseMove"},
        {"mouseenter", "onMouseEnter"},
        {"mouseleave", "onMouseLeave"},
        {"mouseover", "onMouseOver"},
        {"mouseout", "onMouseOut"},
        {"doubleclick", "onDoubleClick"},
        {"contextmenu", "onContextMenu"},
        {"touchstart", "onTouchStart"},
        {"touchend", "onTouchEnd"},
        {"touchmove", "onTouchMove"},
        {"touchcancel", "onTouchCancel"},
        {NULL, NULL}
    };

    // Check if it's a known compound event
    for (int i = 0; event_map[i].event != NULL; i++) {
        if (strcmp(evt_type, event_map[i].event) == 0) {
            snprintf(output, output_size, "%s", event_map[i].react_name);
            return;
        }
    }

    // For simple events (click, change, focus, blur, submit, etc.),
    // just capitalize first letter and add "on"
    snprintf(output, output_size, "on%c%s", toupper(evt_type[0]), evt_type + 1);
}

const char* react_map_kir_property(const char* key) {
    // Simple mapping - just return the key as-is for now
    // Could expand with specific mappings if needed
    if (strcmp(key, "backgroundColor") == 0) return "background";
    if (strcmp(key, "windowTitle") == 0) return "title";
    return key;
}

static bool ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

char* react_format_value(const char* key, cJSON* val) {
    (void)key; // May use for context-specific formatting
    char buffer[1024];

    if (cJSON_IsString(val)) {
        const char* s = cJSON_GetStringValue(val);

        // Check for pixel values
        if (ends_with(s, "px")) {
            char num_str[256];
            size_t len = strlen(s) - 2;
            strncpy(num_str, s, len);
            num_str[len] = '\0';

            double f = atof(num_str);
            if (f == (int)f) {
                snprintf(buffer, sizeof(buffer), "{%d}", (int)f);
            } else {
                snprintf(buffer, sizeof(buffer), "{%s}", num_str);
            }
            return strdup(buffer);
        }

        // Check for percentage values
        if (ends_with(s, "%")) {
            snprintf(buffer, sizeof(buffer), "\"%s\"", s);
            return strdup(buffer);
        }

        // Check for color values
        if (starts_with(s, "#")) {
            snprintf(buffer, sizeof(buffer), "\"%s\"", s);
            return strdup(buffer);
        }

        // Check for template expressions {{value}}
        if (starts_with(s, "{{") && ends_with(s, "}}")) {
            char var_name[256];
            size_t len = strlen(s) - 4;
            strncpy(var_name, s + 2, len);
            var_name[len] = '\0';
            snprintf(buffer, sizeof(buffer), "{String(%s)}", var_name);
            return strdup(buffer);
        }

        // Regular string
        snprintf(buffer, sizeof(buffer), "\"%s\"", s);
        return strdup(buffer);
    }

    if (cJSON_IsNumber(val)) {
        double d = cJSON_GetNumberValue(val);
        if (d == (int)d) {
            snprintf(buffer, sizeof(buffer), "{%d}", (int)d);
        } else {
            snprintf(buffer, sizeof(buffer), "{%g}", d);
        }
        return strdup(buffer);
    }

    if (cJSON_IsBool(val)) {
        snprintf(buffer, sizeof(buffer), "{%s}", cJSON_IsTrue(val) ? "true" : "false");
        return strdup(buffer);
    }

    if (cJSON_IsArray(val)) {
        StringBuilder* sb = sb_create(256);
        sb_append(sb, "{[");

        cJSON* item = NULL;
        int first = 1;
        cJSON_ArrayForEach(item, val) {
            if (!first) sb_append(sb, ", ");
            first = 0;

            if (cJSON_IsString(item)) {
                sb_append_fmt(sb, "\"%s\"", cJSON_GetStringValue(item));
            } else {
                char* formatted = react_format_value(key, item);
                sb_append(sb, formatted);
                free(formatted);
            }
        }

        sb_append(sb, "]}");
        char* result = strdup(sb_get(sb));
        sb_free(sb);
        return result;
    }

    return strdup("\"\"");
}

bool react_is_default_value(const char* key, cJSON* val) {
    if (strcmp(key, "direction") == 0 && cJSON_IsString(val)) {
        return strcmp(cJSON_GetStringValue(val), "auto") == 0;
    }

    if ((strcmp(key, "flexShrink") == 0 || strcmp(key, "flexGrow") == 0) && cJSON_IsNumber(val)) {
        return cJSON_GetNumberValue(val) == 0;
    }

    if ((strcmp(key, "color") == 0 || strcmp(key, "background") == 0) && cJSON_IsString(val)) {
        return strcmp(cJSON_GetStringValue(val), "#00000000") == 0;
    }

    return false;
}

// =============================================================================
// Expression Generation
// =============================================================================

char* react_generate_expression(cJSON* expr) {
    char buffer[1024];

    if (cJSON_IsNumber(expr)) {
        double d = cJSON_GetNumberValue(expr);
        if (d == (int)d) {
            snprintf(buffer, sizeof(buffer), "%d", (int)d);
        } else {
            snprintf(buffer, sizeof(buffer), "%g", d);
        }
        return strdup(buffer);
    }

    if (cJSON_IsString(expr)) {
        snprintf(buffer, sizeof(buffer), "\"%s\"", cJSON_GetStringValue(expr));
        return strdup(buffer);
    }

    if (cJSON_IsBool(expr)) {
        return strdup(cJSON_IsTrue(expr) ? "true" : "false");
    }

    if (!cJSON_IsObject(expr)) {
        return strdup("null");
    }

    // Variable reference: {"var": "name"}
    cJSON* var_node = cJSON_GetObjectItem(expr, "var");
    if (var_node && cJSON_IsString(var_node)) {
        return strdup(cJSON_GetStringValue(var_node));
    }

    // Binary operations
    cJSON* op_node = cJSON_GetObjectItem(expr, "op");
    if (!op_node || !cJSON_IsString(op_node)) {
        return strdup("null");
    }

    const char* op = cJSON_GetStringValue(op_node);

    if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0 ||
        strcmp(op, "mul") == 0 || strcmp(op, "div") == 0) {
        char* left = react_generate_expression(cJSON_GetObjectItem(expr, "left"));
        char* right = react_generate_expression(cJSON_GetObjectItem(expr, "right"));

        char op_char;
        if (strcmp(op, "add") == 0) op_char = '+';
        else if (strcmp(op, "sub") == 0) op_char = '-';
        else if (strcmp(op, "mul") == 0) op_char = '*';
        else op_char = '/';

        snprintf(buffer, sizeof(buffer), "(%s %c %s)", left, op_char, right);
        free(left);
        free(right);
        return strdup(buffer);
    }

    if (strcmp(op, "not") == 0) {
        cJSON* operand = cJSON_GetObjectItem(expr, "operand");
        if (!operand) operand = cJSON_GetObjectItem(expr, "expr");
        char* operand_str = react_generate_expression(operand);
        snprintf(buffer, sizeof(buffer), "(!%s)", operand_str);
        free(operand_str);
        return strdup(buffer);
    }

    return strdup("null");
}

/**
 * Generate handler body from logic_block sources[] format
 * Looks for TypeScript/JavaScript source, or translates from kry
 */
static char* react_generate_handler_from_sources(cJSON* functions, const char* handler_id) {
    if (!functions || !cJSON_IsArray(functions)) return NULL;

    // Find the function by name
    cJSON* func = NULL;
    cJSON_ArrayForEach(func, functions) {
        cJSON* name_node = cJSON_GetObjectItem(func, "name");
        if (name_node && cJSON_IsString(name_node) &&
            strcmp(cJSON_GetStringValue(name_node), handler_id) == 0) {
            break;
        }
    }
    if (!func) return NULL;

    // Get sources array
    cJSON* sources = cJSON_GetObjectItem(func, "sources");
    if (!sources || !cJSON_IsArray(sources)) return NULL;

    // Look for TypeScript or JavaScript source first
    const char* preferred_langs[] = {"typescript", "javascript", "jsx", "tsx", NULL};
    for (int i = 0; preferred_langs[i]; i++) {
        cJSON* source = NULL;
        cJSON_ArrayForEach(source, sources) {
            cJSON* lang = cJSON_GetObjectItem(source, "language");
            if (lang && cJSON_IsString(lang) &&
                strcmp(cJSON_GetStringValue(lang), preferred_langs[i]) == 0) {
                cJSON* source_code = cJSON_GetObjectItem(source, "source");
                if (source_code && cJSON_IsString(source_code)) {
                    return strdup(cJSON_GetStringValue(source_code));
                }
            }
        }
    }

    // Fall back to kry source and translate simple cases
    cJSON* source = cJSON_GetArrayItem(sources, 0);
    if (source) {
        cJSON* lang = cJSON_GetObjectItem(source, "language");
        cJSON* source_code = cJSON_GetObjectItem(source, "source");

        if (source_code && cJSON_IsString(source_code)) {
            const char* code = cJSON_GetStringValue(source_code);
            char buffer[1024];

            // Strip leading/trailing whitespace and braces
            while (*code && (*code == ' ' || *code == '\t' || *code == '\n' || *code == '{')) {
                code++;
            }

            // Find end of actual code (before closing brace if present)
            const char* end = code + strlen(code) - 1;
            while (end > code && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '}')) {
                end--;
            }

            size_t len = end - code + 1;
            char cleaned[1024];
            strncpy(cleaned, code, len);
            cleaned[len] = '\0';

            // Simple translation: print(...) -> console.log(...)
            if (strstr(cleaned, "print(") != NULL) {
                const char* start = strstr(cleaned, "print(");
                const char* args_start = start + 6; // After "print("
                const char* args_end = strchr(args_start, ')');

                if (args_end) {
                    size_t args_len = args_end - args_start;
                    char args[512];
                    strncpy(args, args_start, args_len);
                    args[args_len] = '\0';

                    snprintf(buffer, sizeof(buffer), "console.log(%s)", args);
                    return strdup(buffer);
                }
            }

            // Return cleaned code
            return strdup(cleaned);
        }
    }

    return NULL;
}

char* react_generate_handler_body(cJSON* functions, const char* handler_id) {
    if (!functions) return NULL;

    // Try new sources[] format first (for logic_block)
    if (cJSON_IsArray(functions)) {
        char* result = react_generate_handler_from_sources(functions, handler_id);
        if (result) return result;
    }

    // Fall back to old universal format (for reactive manifest)
    if (!cJSON_IsObject(functions)) return NULL;

    cJSON* func_def = cJSON_GetObjectItem(functions, handler_id);
    if (!func_def) return NULL;

    cJSON* universal = cJSON_GetObjectItem(func_def, "universal");
    if (!universal) return NULL;

    cJSON* statements = cJSON_GetObjectItem(universal, "statements");
    if (!statements || !cJSON_IsArray(statements) || cJSON_GetArraySize(statements) == 0) {
        return NULL;
    }

    // Process first statement (simplified)
    cJSON* stmt = cJSON_GetArrayItem(statements, 0);
    if (!stmt) return NULL;

    cJSON* op = cJSON_GetObjectItem(stmt, "op");
    if (!op || strcmp(cJSON_GetStringValue(op), "assign") != 0) return NULL;

    const char* target = cJSON_GetStringValue(cJSON_GetObjectItem(stmt, "target"));
    cJSON* expr = cJSON_GetObjectItem(stmt, "expr");

    // Generate setter name: counter -> setCounter
    char setter_name[256];
    if (target && strlen(target) > 0) {
        snprintf(setter_name, sizeof(setter_name), "set%c%s",
                 toupper(target[0]), target + 1);
    } else {
        return NULL;
    }

    // Check for increment/decrement patterns
    cJSON* expr_op = cJSON_GetObjectItem(expr, "op");
    if (expr_op && cJSON_IsString(expr_op)) {
        const char* expr_op_str = cJSON_GetStringValue(expr_op);

        if (strcmp(expr_op_str, "add") == 0 || strcmp(expr_op_str, "sub") == 0) {
            cJSON* left = cJSON_GetObjectItem(expr, "left");
            cJSON* right = cJSON_GetObjectItem(expr, "right");

            cJSON* left_var = left ? cJSON_GetObjectItem(left, "var") : NULL;
            if (left_var && strcmp(cJSON_GetStringValue(left_var), target) == 0 && cJSON_IsNumber(right)) {
                char op_char = (strcmp(expr_op_str, "add") == 0) ? '+' : '-';
                char buffer[512];
                snprintf(buffer, sizeof(buffer), "%s(%s %c %d)",
                         setter_name, target, op_char, (int)cJSON_GetNumberValue(right));
                return strdup(buffer);
            }
        }
    }

    // Generic assignment
    char* expr_str = react_generate_expression(expr);
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s(%s)", setter_name, expr_str);
    free(expr_str);
    return strdup(buffer);
}

// =============================================================================
// Element Generation (Forward declaration and helpers)
// =============================================================================

static char* generate_children(cJSON* node, ReactContext* ctx, int indent);

StringBuilder* react_generate_props(cJSON* node, ReactContext* ctx, bool has_text_expression) {
    StringBuilder* sb = sb_create(512);

    const char* skip_props[] = {
        "id", "type", "children", "events", "template", "loop_var",
        "iterable", "direction", "flexShrink", "flexGrow",
        "component_ref", "text_expression", "windowTitle", "dropdown_state",
        NULL
    };

    // Skip text if we have text_expression
    bool skip_text = has_text_expression;

    cJSON* item = NULL;
    cJSON_ArrayForEach(item, node) {
        const char* key = item->string;
        if (!key) continue;

        // Check skip list
        bool should_skip = false;
        for (int i = 0; skip_props[i]; i++) {
            if (strcmp(key, skip_props[i]) == 0) {
                should_skip = true;
                break;
            }
        }
        if (should_skip) continue;
        if (skip_text && strcmp(key, "text") == 0) continue;

        // Check default values
        if (react_is_default_value(key, item)) continue;

        const char* react_key = react_map_kir_property(key);
        char* react_val = react_format_value(key, item);

        // Skip empty values (except selectedIndex)
        if ((strcmp(react_val, "\"\"") == 0 || strcmp(react_val, "null") == 0 ||
             strcmp(react_val, "{0}") == 0) && strcmp(key, "selectedIndex") != 0) {
            free(react_val);
            continue;
        }

        if (sb->size > 0) sb_append(sb, " ");
        sb_append_fmt(sb, "%s=%s", react_key, react_val);
        free(react_val);
    }

    // Handle ALL event types (onClick, onChange, onSubmit, onFocus, onBlur, etc.)
    cJSON* events = cJSON_GetObjectItem(node, "events");
    if (events && cJSON_IsArray(events)) {
        cJSON* event = NULL;
        cJSON_ArrayForEach(event, events) {
            cJSON* event_type = cJSON_GetObjectItem(event, "type");
            if (!event_type || !cJSON_IsString(event_type)) continue;

            const char* evt_type = cJSON_GetStringValue(event_type);
            char* handler_body = NULL;

            // Try to generate from logic_functions first
            cJSON* logic_id = cJSON_GetObjectItem(event, "logic_id");
            if (logic_id && cJSON_IsString(logic_id) && ctx->logic_functions) {
                handler_body = react_generate_handler_body(
                    ctx->logic_functions, cJSON_GetStringValue(logic_id));
            }

            // Fall back to handler_data if available
            if (!handler_body) {
                cJSON* handler_data = cJSON_GetObjectItem(event, "handler_data");
                if (handler_data && cJSON_IsString(handler_data)) {
                    const char* code = cJSON_GetStringValue(handler_data);

                    // Clean up and translate
                    char cleaned[1024];
                    const char* start = code;
                    while (*start && (*start == ' ' || *start == '\t' || *start == '{')) start++;

                    const char* end = start + strlen(start) - 1;
                    while (end > start && (*end == ' ' || *end == '\t' || *end == '}')) end--;

                    size_t len = end - start + 1;
                    strncpy(cleaned, start, len);
                    cleaned[len] = '\0';

                    // Simple translation: print(...) -> console.log(...)
                    if (strstr(cleaned, "print(") != NULL) {
                        const char* print_start = strstr(cleaned, "print(");
                        const char* args_start = print_start + 6;
                        const char* args_end = strchr(args_start, ')');
                        if (args_end) {
                            size_t args_len = args_end - args_start;
                            char args[512];
                            strncpy(args, args_start, args_len);
                            args[args_len] = '\0';

                            char buffer[1024];
                            snprintf(buffer, sizeof(buffer), "console.log(%s)", args);
                            handler_body = strdup(buffer);
                        }
                    }

                    if (!handler_body) {
                        handler_body = strdup(cleaned);
                    }
                }
            }

            if (handler_body) {
                // Map event type to React prop name: "click" -> "onClick", "keydown" -> "onKeyDown"
                char react_event_name[64];
                react_event_type_to_handler_name(evt_type, react_event_name, sizeof(react_event_name));

                if (sb->size > 0) sb_append(sb, " ");

                // Check if handler_body already starts with arrow function or has event parameter
                const char* trimmed = handler_body;
                while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

                bool is_function = (
                    strncmp(trimmed, "() =>", 5) == 0 ||
                    strncmp(trimmed, "()=>", 4) == 0 ||
                    strncmp(trimmed, "(e) =>", 6) == 0 ||
                    strncmp(trimmed, "(event) =>", 10) == 0 ||
                    strstr(trimmed, "function") == trimmed
                );

                if (is_function) {
                    // Already a function, use directly
                    sb_append_fmt(sb, "%s={%s}", react_event_name, handler_body);
                } else {
                    // Wrap in arrow function
                    sb_append_fmt(sb, "%s={() => %s}", react_event_name, handler_body);
                }
                free(handler_body);
            }
        }
    }

    // text_expression is now handled in generate_children, not here

    return sb;
}

char* react_generate_element(cJSON* node, ReactContext* ctx, int indent) {
    if (!cJSON_IsObject(node)) return strdup("");

    StringBuilder* sb = sb_create(1024);

    // Generate indentation
    for (int i = 0; i < indent; i++) {
        sb_append(sb, "  ");
    }

    const char* comp_type = "div";
    cJSON* type_node = cJSON_GetObjectItem(node, "type");
    if (type_node && cJSON_IsString(type_node)) {
        comp_type = cJSON_GetStringValue(type_node);
    }

    // Check for text_expression
    bool has_text_expr = cJSON_HasObjectItem(node, "text_expression");

    // Generate properties
    StringBuilder* props_sb = react_generate_props(node, ctx, has_text_expr);
    const char* props = sb_get(props_sb);

    // Generate children
    char* children = generate_children(node, ctx, indent + 1);

    // Check if children is inline text (no newlines = flattened Text children)
    bool is_inline_text = (strlen(children) > 0 && strchr(children, '\n') == NULL);

    // Build element
    if (strlen(children) == 0) {
        // Self-closing
        if (strlen(props) == 0) {
            sb_append_fmt(sb, "<%s />", comp_type);
        } else {
            sb_append_fmt(sb, "<%s %s />", comp_type, props);
        }
    } else if (is_inline_text) {
        // Inline text content (Text component with flattened children)
        if (strlen(props) == 0) {
            sb_append_fmt(sb, "<%s>%s</%s>", comp_type, children, comp_type);
        } else {
            sb_append_fmt(sb, "<%s %s>%s</%s>", comp_type, props, children, comp_type);
        }
    } else {
        // With children (multiline)
        if (strlen(props) == 0) {
            sb_append_fmt(sb, "<%s>%s\n", comp_type, children);
        } else {
            sb_append_fmt(sb, "<%s %s>%s\n", comp_type, props, children);
        }

        for (int i = 0; i < indent; i++) {
            sb_append(sb, "  ");
        }
        sb_append_fmt(sb, "</%s>", comp_type);
    }

    sb_free(props_sb);
    free(children);

    char* result = strdup(sb_get(sb));
    sb_free(sb);
    return result;
}

static char* generate_children(cJSON* node, ReactContext* ctx, int indent) {
    // Check if this is a Text component with text_expression (even without children array)
    cJSON* type_node = cJSON_GetObjectItem(node, "type");
    const char* parent_type = type_node && cJSON_IsString(type_node) ? cJSON_GetStringValue(type_node) : "";

    if (strcmp(parent_type, "Text") == 0) {
        cJSON* text_expr = cJSON_GetObjectItem(node, "text_expression");
        if (text_expr && cJSON_IsString(text_expr)) {
            // Generate JSX expression like {variable}
            StringBuilder* sb = sb_create(256);
            const char* expr = cJSON_GetStringValue(text_expr);

            // Check if there's a scope attribute to determine scoped variable name
            cJSON* scope_attr = cJSON_GetObjectItem(node, "scope");
            const char* scope = scope_attr && cJSON_IsString(scope_attr) ? cJSON_GetStringValue(scope_attr) : NULL;

            // Generate scoped variable name if needed
            char scoped_var[256];
            if (scope && strcmp(scope, "component") != 0) {
                snprintf(scoped_var, sizeof(scoped_var), "%s_%s", expr, scope);
                // Replace # with _ for valid JavaScript identifiers
                for (char* p = scoped_var; *p; p++) {
                    if (*p == '#') *p = '_';
                }
                sb_append_fmt(sb, "{%s}", scoped_var);
            } else {
                sb_append_fmt(sb, "{%s}", expr);
            }

            char* result = strdup(sb_get(sb));
            sb_free(sb);
            return result;
        }
    }

    cJSON* children = cJSON_GetObjectItem(node, "children");
    if (!children || !cJSON_IsArray(children) || cJSON_GetArraySize(children) == 0) {
        return strdup("");
    }

    // Check if this is a Text component with Text children - handle specially
    if (strcmp(parent_type, "Text") == 0) {
        // Check if this has a text_expression with children (complex pattern)
        cJSON* text_expr = cJSON_GetObjectItem(node, "text_expression");
        if (text_expr && cJSON_IsString(text_expr)) {
            // Reconstruct with JSX expression
            // Pattern: first child is static text, last child is the evaluated value to replace
            StringBuilder* sb = sb_create(256);
            const char* expr = cJSON_GetStringValue(text_expr);

            int child_count = cJSON_GetArraySize(children);
            int idx = 0;

            cJSON* child = NULL;
            cJSON_ArrayForEach(child, children) {
                cJSON* text_node = cJSON_GetObjectItem(child, "text");
                if (text_node && cJSON_IsString(text_node)) {
                    // Replace the last child (evaluated value) with the expression
                    if (idx == child_count - 1) {
                        sb_append(sb, "{");
                        sb_append(sb, expr);
                        sb_append(sb, "}");
                    } else {
                        sb_append(sb, cJSON_GetStringValue(text_node));
                    }
                }
                idx++;
            }

            char* result = strdup(sb_get(sb));
            sb_free(sb);
            return result;
        }

        // Check if all children are Text nodes with text property
        bool all_text_children = true;
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, children) {
            cJSON* child_type = cJSON_GetObjectItem(child, "type");
            const char* child_type_str = child_type && cJSON_IsString(child_type) ? cJSON_GetStringValue(child_type) : "";
            if (strcmp(child_type_str, "Text") != 0 || !cJSON_HasObjectItem(child, "text")) {
                all_text_children = false;
                break;
            }
        }

        // If all children are simple Text nodes, flatten them into inline content
        if (all_text_children) {
            StringBuilder* sb = sb_create(256);
            cJSON_ArrayForEach(child, children) {
                cJSON* text_node = cJSON_GetObjectItem(child, "text");
                if (text_node && cJSON_IsString(text_node)) {
                    sb_append(sb, cJSON_GetStringValue(text_node));
                }
            }
            char* result = strdup(sb_get(sb));
            sb_free(sb);
            return result;
        }
    }

    StringBuilder* sb = sb_create(2048);

    cJSON* child = NULL;
    cJSON_ArrayForEach(child, children) {
        sb_append(sb, "\n");
        char* elem = react_generate_element(child, ctx, indent);
        sb_append(sb, elem);
        free(elem);
    }

    char* result = strdup(sb_get(sb));
    sb_free(sb);
    return result;
}

// =============================================================================
// Window Configuration
// =============================================================================

WindowConfig react_extract_window_config(cJSON* component) {
    WindowConfig config = {
        .width = 800,
        .height = 600,
        .title = strdup("Kryon App"),
        .background = strdup("#1E1E1E")
    };

    if (!component) return config;

    // Handle width (supports windowWidth, width, strings with "px", and numbers)
    cJSON* width = cJSON_GetObjectItem(component, "windowWidth");
    if (!width) width = cJSON_GetObjectItem(component, "width");
    if (width) {
        if (cJSON_IsNumber(width)) {
            config.width = cJSON_GetNumberValue(width);
        } else if (cJSON_IsString(width)) {
            const char* w = cJSON_GetStringValue(width);
            if (ends_with(w, "px")) {
                config.width = atoi(w);
            } else {
                config.width = atoi(w);
            }
        }
    }

    // Handle height (supports windowHeight, height, strings with "px", and numbers)
    cJSON* height = cJSON_GetObjectItem(component, "windowHeight");
    if (!height) height = cJSON_GetObjectItem(component, "height");
    if (height) {
        if (cJSON_IsNumber(height)) {
            config.height = cJSON_GetNumberValue(height);
        } else if (cJSON_IsString(height)) {
            const char* h = cJSON_GetStringValue(height);
            if (ends_with(h, "px")) {
                config.height = atoi(h);
            } else {
                config.height = atoi(h);
            }
        }
    }

    cJSON* title = cJSON_GetObjectItem(component, "windowTitle");
    if (!title) title = cJSON_GetObjectItem(component, "title");
    if (title && cJSON_IsString(title)) {
        free(config.title);
        config.title = strdup(cJSON_GetStringValue(title));
    }

    cJSON* background = cJSON_GetObjectItem(component, "background");
    if (background && cJSON_IsString(background)) {
        free(config.background);
        config.background = strdup(cJSON_GetStringValue(background));
    }

    return config;
}

void react_free_window_config(WindowConfig* config) {
    if (config) {
        free(config->title);
        free(config->background);
    }
}

// =============================================================================
// State Hooks Generation
// =============================================================================

char* react_generate_state_hooks(cJSON* manifest, ReactContext* ctx) {
    if (!manifest || !cJSON_IsObject(manifest)) return strdup("");

    StringBuilder* sb = sb_create(2048);

    // Generate useState hooks
    cJSON* variables = cJSON_GetObjectItem(manifest, "variables");
    if (variables && cJSON_IsArray(variables) && cJSON_GetArraySize(variables) > 0) {
        cJSON* var = NULL;
        cJSON_ArrayForEach(var, variables) {
            cJSON* name_node = cJSON_GetObjectItem(var, "name");
            cJSON* type_node = cJSON_GetObjectItem(var, "type");
            cJSON* initial_node = cJSON_GetObjectItem(var, "initial_value");
            cJSON* setter_node = cJSON_GetObjectItem(var, "setter_name");
            cJSON* scope_node = cJSON_GetObjectItem(var, "scope");

            if (!name_node || !cJSON_IsString(name_node)) continue;

            const char* name = cJSON_GetStringValue(name_node);
            const char* type = type_node ? cJSON_GetStringValue(type_node) : "any";
            const char* initial = initial_node ? cJSON_GetStringValue(initial_node) : "undefined";
            const char* setter = setter_node ? cJSON_GetStringValue(setter_node) : NULL;
            const char* scope = scope_node ? cJSON_GetStringValue(scope_node) : NULL;

            // Skip variables with unresolved parameter references
            if (strcmp(initial, "initialValue") == 0) {
                continue;
            }

            // Generate unique names based on scope to avoid collisions
            char scoped_name[256];
            char scoped_setter[256];

            if (scope && strcmp(scope, "component") != 0) {
                // Replace # with _ for valid JavaScript identifiers
                snprintf(scoped_name, sizeof(scoped_name), "%s_%s", name, scope);
                // Replace # with _ in the scoped name
                for (char* p = scoped_name; *p; p++) {
                    if (*p == '#') *p = '_';
                }

                // Generate setter name from scoped name
                snprintf(scoped_setter, sizeof(scoped_setter), "set%c%s",
                         toupper(scoped_name[0]), scoped_name + 1);

                name = scoped_name;
                setter = scoped_setter;
            } else {
                // Auto-generate setter name if not provided
                char auto_setter[256];
                if (!setter) {
                    snprintf(auto_setter, sizeof(auto_setter), "set%c%s",
                             toupper(name[0]), name + 1);
                    setter = auto_setter;
                }
            }

            // Map KIR types to TypeScript types
            const char* ts_type = type;
            char ts_type_buf[64];
            if (strcmp(type, "int") == 0 || strcmp(type, "float") == 0) {
                ts_type = "number";
            } else if (strcmp(type, "bool") == 0) {
                ts_type = "boolean";
            } else if (strcmp(type, "array") == 0) {
                ts_type = "any[]";
            } else if (strcmp(type, "object") == 0) {
                ts_type = "Record<string, any>";
            }

            // Generate with TypeScript type annotation if in TypeScript mode
            if (ctx->mode == REACT_MODE_TYPESCRIPT && strcmp(type, "any") != 0) {
                sb_append_fmt(sb, "  const [%s, %s] = useState<%s>(%s);\n",
                              name, setter, ts_type, initial);
            } else {
                sb_append_fmt(sb, "  const [%s, %s] = useState(%s);\n",
                              name, setter, initial);
            }
        }
    }

    // Generate useEffect, useCallback, and useMemo hooks
    cJSON* hooks = cJSON_GetObjectItem(manifest, "hooks");
    if (hooks && cJSON_IsArray(hooks) && cJSON_GetArraySize(hooks) > 0) {
        cJSON* hook = NULL;
        cJSON_ArrayForEach(hook, hooks) {
            cJSON* type_node = cJSON_GetObjectItem(hook, "type");
            if (!type_node || !cJSON_IsString(type_node)) continue;

            const char* hook_type = cJSON_GetStringValue(type_node);

            if (strcmp(hook_type, "useEffect") == 0) {
                cJSON* callback = cJSON_GetObjectItem(hook, "callback");
                cJSON* deps = cJSON_GetObjectItem(hook, "dependencies");

                if (callback && cJSON_IsString(callback)) {
                    const char* cb_str = cJSON_GetStringValue(callback);
                    const char* deps_str = (deps && cJSON_IsString(deps)) ?
                        cJSON_GetStringValue(deps) : "[]";

                    sb_append_fmt(sb, "  useEffect(%s, %s);\n", cb_str, deps_str);
                }
            }
            else if (strcmp(hook_type, "useCallback") == 0) {
                cJSON* callback = cJSON_GetObjectItem(hook, "callback");
                cJSON* deps = cJSON_GetObjectItem(hook, "dependencies");
                cJSON* name_node = cJSON_GetObjectItem(hook, "name");

                if (callback && cJSON_IsString(callback)) {
                    const char* cb_str = cJSON_GetStringValue(callback);
                    const char* deps_str = (deps && cJSON_IsString(deps)) ?
                        cJSON_GetStringValue(deps) : "[]";
                    const char* var_name = (name_node && cJSON_IsString(name_node)) ?
                        cJSON_GetStringValue(name_node) : "memoizedCallback";

                    sb_append_fmt(sb, "  const %s = useCallback(%s, %s);\n",
                                  var_name, cb_str, deps_str);
                }
            }
            else if (strcmp(hook_type, "useMemo") == 0) {
                cJSON* callback = cJSON_GetObjectItem(hook, "callback");
                cJSON* deps = cJSON_GetObjectItem(hook, "dependencies");
                cJSON* name_node = cJSON_GetObjectItem(hook, "name");

                if (callback && cJSON_IsString(callback)) {
                    const char* cb_str = cJSON_GetStringValue(callback);
                    const char* deps_str = (deps && cJSON_IsString(deps)) ?
                        cJSON_GetStringValue(deps) : "[]";
                    const char* var_name = (name_node && cJSON_IsString(name_node)) ?
                        cJSON_GetStringValue(name_node) : "memoizedValue";

                    sb_append_fmt(sb, "  const %s = useMemo(%s, %s);\n",
                                  var_name, cb_str, deps_str);
                }
            }
            else if (strcmp(hook_type, "useReducer") == 0) {
                cJSON* callback = cJSON_GetObjectItem(hook, "callback");
                cJSON* deps = cJSON_GetObjectItem(hook, "dependencies");

                if (callback && cJSON_IsString(callback)) {
                    const char* reducer_str = cJSON_GetStringValue(callback);
                    const char* initial_str = (deps && cJSON_IsString(deps)) ?
                        cJSON_GetStringValue(deps) : "{}";

                    sb_append_fmt(sb, "  const [state, dispatch] = useReducer(%s, %s);\n",
                                  reducer_str, initial_str);
                }
            }
        }
    }

    char* result = strdup(sb_get(sb));
    sb_free(sb);
    return result;
}
