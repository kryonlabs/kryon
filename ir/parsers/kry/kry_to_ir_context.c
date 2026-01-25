/**
 * Kryon .kry to IR Converter - Context Functions
 *
 * Parameter substitution and value resolution utilities.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_to_ir_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Forward declaration
static char* evaluate_string_concat(const char* expr);

// ============================================================================
// Parameter Substitution
// ============================================================================

void kry_ctx_add_param(ConversionContext* ctx, const char* name, const char* value) {
    if (!ctx || !name || !value || ctx->param_count >= MAX_PARAMS) return;

    ctx->params[ctx->param_count].name = (char*)name;
    ctx->params[ctx->param_count].value = (char*)value;
    ctx->params[ctx->param_count].kry_value = NULL;  // Simple string value
    ctx->param_count++;
}

void kry_ctx_add_param_value(ConversionContext* ctx, const char* name, KryValue* value) {
    if (!ctx || !name || !value || ctx->param_count >= MAX_PARAMS) return;

    ctx->params[ctx->param_count].name = (char*)name;
    ctx->params[ctx->param_count].value = NULL;      // No string representation
    ctx->params[ctx->param_count].kry_value = value;  // Store full KryValue
    ctx->param_count++;
}

const char* kry_ctx_substitute_param(ConversionContext* ctx, const char* expr) {
    if (!ctx || !expr) return expr;

    // First try exact match (fast path for simple identifiers)
    for (int i = 0; i < ctx->param_count; i++) {
        if (strcmp(expr, ctx->params[i].name) == 0) {
            return ctx->params[i].value;
        }
    }

    // Check if any param name appears in the expression (compound expression case)
    bool needs_substitution = false;
    for (int i = 0; i < ctx->param_count; i++) {
        if (ctx->params[i].value && strstr(expr, ctx->params[i].name) != NULL) {
            needs_substitution = true;
            break;
        }
    }

    if (!needs_substitution) {
        return expr;
    }

    // Build substituted expression
    static char result_buf[4096];
    char* result = result_buf;
    const char* p = expr;

    while (*p && (result - result_buf) < 4090) {
        bool matched = false;

        // Try to match any param at current position
        for (int i = 0; i < ctx->param_count; i++) {
            if (!ctx->params[i].value) continue;

            size_t name_len = strlen(ctx->params[i].name);

            // Check for identifier match (must not be part of larger identifier)
            if (strncmp(p, ctx->params[i].name, name_len) == 0) {
                char prev = (p > expr) ? *(p - 1) : ' ';
                char next = *(p + name_len);

                // Verify it's a standalone identifier (not part of larger word)
                bool prev_ok = !isalnum((unsigned char)prev) && prev != '_';
                bool next_ok = !isalnum((unsigned char)next) && next != '_';

                if (prev_ok && next_ok) {
                    // Substitute: wrap value in quotes if it's a string value
                    *result++ = '"';
                    const char* val = ctx->params[i].value;
                    while (*val && (result - result_buf) < 4090) {
                        *result++ = *val++;
                    }
                    *result++ = '"';

                    p += name_len;
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            *result++ = *p++;
        }
    }
    *result = '\0';

    // Try to evaluate the substituted expression if it's a simple string concatenation
    char* evaluated = evaluate_string_concat(result_buf);
    if (evaluated) {
        return evaluated;
    }

    return result_buf;
}

bool kry_ctx_is_unresolved_expr(ConversionContext* ctx, const char* expr) {
    if (!ctx || !expr) return false;

    // If no params registered, everything is unresolved
    if (ctx->param_count == 0) return true;

    // Check if this expression matches any param name or contains a param reference
    for (int i = 0; i < ctx->param_count; i++) {
        // Direct match (e.g., "item")
        if (strcmp(expr, ctx->params[i].name) == 0) {
            // Check if this is a self-referencing param (state variable marker)
            // State variables are added with name=value to mark them as known
            // but still need to be treated as unresolved for text_expression
            if (ctx->params[i].value && strcmp(expr, ctx->params[i].value) == 0) {
                // Self-reference: this is a state variable, treat as unresolved
                return true;
            }
            return false;  // Resolved!
        }

        // Property access (e.g., "item.value" or "item.colors[0]")
        size_t param_len = strlen(ctx->params[i].name);
        if (strncmp(expr, ctx->params[i].name, param_len) == 0 &&
            (expr[param_len] == '.' || expr[param_len] == '[')) {
            return false;  // Resolved - it's a property/index access on a param
        }

        // Check if param appears anywhere in compound expression (e.g., "- " + todo)
        const char* found = strstr(expr, ctx->params[i].name);
        if (found) {
            // Verify it's a standalone identifier
            char prev = (found > expr) ? *(found - 1) : ' ';
            char next = *(found + param_len);
            bool prev_ok = !isalnum((unsigned char)prev) && prev != '_';
            bool next_ok = !isalnum((unsigned char)next) && next != '_';

            if (prev_ok && next_ok) {
                // Check if self-reference (for reactive/state vars)
                if (ctx->params[i].value &&
                    strcmp(ctx->params[i].name, ctx->params[i].value) == 0) {
                    return true;  // Unresolved - reactive var
                }
                return false;  // Resolved - loop var with actual value
            }
        }
    }

    return true;  // Unresolved
}

// ============================================================================
// Simple Expression Evaluation (String Concatenation)
// ============================================================================

/**
 * Evaluate simple string concatenation expressions.
 * Handles patterns like: "str1" + "str2" + "str3"
 * Returns a newly allocated string, or NULL if not evaluable.
 */
static char* evaluate_string_concat(const char* expr) {
    if (!expr) return NULL;

    static char result[4096];
    char* out = result;
    const char* p = expr;

    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) p++;

    while (*p) {
        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        // Expect a quoted string
        if (*p == '"') {
            p++;  // Skip opening quote
            // Copy string content until closing quote
            while (*p && *p != '"' && (out - result) < 4090) {
                if (*p == '\\' && *(p+1)) {
                    // Handle escape sequences
                    p++;
                    switch (*p) {
                        case 'n': *out++ = '\n'; break;
                        case 't': *out++ = '\t'; break;
                        case 'r': *out++ = '\r'; break;
                        case '\\': *out++ = '\\'; break;
                        case '"': *out++ = '"'; break;
                        default: *out++ = *p; break;
                    }
                    p++;
                } else {
                    *out++ = *p++;
                }
            }
            if (*p == '"') p++;  // Skip closing quote
        } else {
            // Not a string literal - can't evaluate
            return NULL;
        }

        // Skip whitespace
        while (*p && isspace((unsigned char)*p)) p++;

        // Check for concatenation operator
        if (*p == '+') {
            p++;  // Skip the +
            // Continue to next string
        } else if (*p) {
            // Unexpected character - can't evaluate
            return NULL;
        }
    }

    *out = '\0';
    return result[0] ? result : NULL;
}

// ============================================================================
// KryValue to JSON Serialization
// ============================================================================

char* kry_value_to_json(KryValue* value) {
    if (!value) return strdup("null");

    switch (value->type) {
        case KRY_VALUE_STRING: {
            char* result = (char*)malloc(strlen(value->string_value) + 3);
            if (result) {
                sprintf(result, "\"%s\"", value->string_value);
            }
            return result;
        }

        case KRY_VALUE_NUMBER: {
            char* result = (char*)malloc(64);
            if (result) {
                snprintf(result, 64, "%g", value->number_value);
            }
            return result;
        }

        case KRY_VALUE_IDENTIFIER: {
            return strdup(value->identifier);
        }

        case KRY_VALUE_EXPRESSION: {
            return strdup(value->expression);
        }

        case KRY_VALUE_ARRAY: {
            // Calculate required buffer size
            size_t buffer_size = 2;  // '[' and ']'
            for (size_t i = 0; i < value->array.count; i++) {
                if (i > 0) buffer_size += 2;  // ", "
                char* elem_json = kry_value_to_json(value->array.elements[i]);
                if (elem_json) {
                    buffer_size += strlen(elem_json);
                    free(elem_json);
                }
            }

            char* result = (char*)malloc(buffer_size + 1);
            if (!result) return strdup("[]");

            char* pos = result;
            *pos++ = '[';

            for (size_t i = 0; i < value->array.count; i++) {
                if (i > 0) {
                    *pos++ = ',';
                    *pos++ = ' ';
                }
                char* elem_json = kry_value_to_json(value->array.elements[i]);
                if (elem_json) {
                    strcpy(pos, elem_json);
                    pos += strlen(elem_json);
                    free(elem_json);
                }
            }

            *pos++ = ']';
            *pos = '\0';
            return result;
        }

        case KRY_VALUE_OBJECT: {
            // Calculate required buffer size
            size_t buffer_size = 2;  // '{' and '}'
            for (size_t i = 0; i < value->object.count; i++) {
                if (i > 0) buffer_size += 2;  // ", "
                buffer_size += strlen(value->object.keys[i]) + 4;  // key and quotes
                char* val_json = kry_value_to_json(value->object.values[i]);
                if (val_json) {
                    buffer_size += strlen(val_json);
                    free(val_json);
                }
            }

            char* result = (char*)malloc(buffer_size + 1);
            if (!result) return strdup("{}");

            char* pos = result;
            *pos++ = '{';

            for (size_t i = 0; i < value->object.count; i++) {
                if (i > 0) {
                    *pos++ = ',';
                    *pos++ = ' ';
                }
                *pos++ = '"';
                strcpy(pos, value->object.keys[i]);
                pos += strlen(value->object.keys[i]);
                strcpy(pos, "\": ");
                pos += 3;

                char* val_json = kry_value_to_json(value->object.values[i]);
                if (val_json) {
                    strcpy(pos, val_json);
                    pos += strlen(val_json);
                    free(val_json);
                }
            }

            *pos++ = '}';
            *pos = '\0';
            return result;
        }

        default:
            return strdup("null");
    }
}
