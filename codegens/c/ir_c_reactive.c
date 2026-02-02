/**
 * C Code Generator - Reactive Signal Implementation
 */

#define _DEFAULT_SOURCE
#include "ir_c_reactive.h"
#include "ir_c_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool c_is_reactive_variable(CCodegenContext* ctx, const char* name) {
    if (!ctx->reactive_vars || !cJSON_IsArray(ctx->reactive_vars) || !name) {
        return false;
    }

    int var_count = cJSON_GetArraySize(ctx->reactive_vars);
    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->reactive_vars, i);
        cJSON* var_name = cJSON_GetObjectItem(var, "name");
        if (var_name && var_name->valuestring && strcmp(name, var_name->valuestring) == 0) {
            return true;
        }
    }
    return false;
}

char* c_get_scoped_var_name(CCodegenContext* ctx, const char* name) {
    if (!name) return NULL;

    // If no current scope or scope is "component" (global), use base name
    if (!ctx->current_scope || strcmp(ctx->current_scope, "component") == 0) {
        return strdup(name);
    }

    // Look for a variable that matches both name and current scope
    if (ctx->reactive_vars && cJSON_IsArray(ctx->reactive_vars)) {
        int var_count = cJSON_GetArraySize(ctx->reactive_vars);
        for (int i = 0; i < var_count; i++) {
            cJSON* var = cJSON_GetArrayItem(ctx->reactive_vars, i);
            cJSON* var_name = cJSON_GetObjectItem(var, "name");
            cJSON* scope = cJSON_GetObjectItem(var, "scope");

            if (var_name && var_name->valuestring &&
                strcmp(var_name->valuestring, name) == 0 &&
                scope && scope->valuestring &&
                strcmp(scope->valuestring, ctx->current_scope) == 0) {
                // Found a variable with matching name and scope
                return c_generate_scoped_var_name(name, scope->valuestring);
            }
        }
    }

    // Not found with current scope, fall back to base name
    return strdup(name);
}

const char* c_get_signal_creator(const char* kir_type) {
    if (!kir_type) return "kryon_signal_create";

    if (strcmp(kir_type, "string") == 0) {
        return "kryon_signal_create_string";
    }
    if (strcmp(kir_type, "bool") == 0 || strcmp(kir_type, "boolean") == 0) {
        return "kryon_signal_create_bool";
    }
    return "kryon_signal_create";  // Default: float signal
}

char* c_generate_scoped_var_name(const char* name, const char* scope) {
    if (!name) return NULL;

    // If no scope or scope is "component" (global), use base name
    if (!scope || strcmp(scope, "component") == 0) {
        return strdup(name);
    }

    // Calculate length: name + "_" + sanitized_scope
    size_t len = strlen(name) + 1;
    const char* s = scope;
    while (*s) {
        // Replace special chars with underscore
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
            (*s >= '0' && *s <= '9')) {
            len++;
        } else {
            len++;  // underscore for special chars
        }
        s++;
    }

    char* result = calloc(1, len + 1);  // +1 for null terminator
    if (!result) return NULL;

    // Copy name
    strcpy(result, name);
    strcat(result, "_");

    // Sanitize scope (replace special chars with underscore)
    s = scope;
    char* dest = result + strlen(result);
    while (*s) {
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
            (*s >= '0' && *s <= '9')) {
            *dest++ = *s;
        } else {
            *dest++ = '_';  // Replace #, -, etc with _
        }
        s++;
    }
    *dest = '\0';

    return result;
}

void c_generate_reactive_signal_declarations(CCodegenContext* ctx) {
    if (!ctx->reactive_vars || !cJSON_IsArray(ctx->reactive_vars)) return;

    int var_count = cJSON_GetArraySize(ctx->reactive_vars);
    if (var_count == 0) return;

    fprintf(ctx->output, "// Reactive state signals\n");

    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->reactive_vars, i);
        cJSON* name = cJSON_GetObjectItem(var, "name");
        cJSON* scope = cJSON_GetObjectItem(var, "scope");

        if (!name || !name->valuestring) continue;

        char* scoped_name = c_generate_scoped_var_name(
            name->valuestring,
            (scope && scope->valuestring) ? scope->valuestring : NULL
        );

        fprintf(ctx->output, "KryonSignal* %s_signal;\n", scoped_name);
        free(scoped_name);
    }
    fprintf(ctx->output, "\n");
}

void c_generate_reactive_signal_initialization(CCodegenContext* ctx) {
    if (!ctx->reactive_vars || !cJSON_IsArray(ctx->reactive_vars)) return;

    int var_count = cJSON_GetArraySize(ctx->reactive_vars);
    if (var_count == 0) return;

    fprintf(ctx->output, "    // Initialize reactive state\n");

    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->reactive_vars, i);
        cJSON* name = cJSON_GetObjectItem(var, "name");
        cJSON* scope = cJSON_GetObjectItem(var, "scope");
        cJSON* type = cJSON_GetObjectItem(var, "type");
        cJSON* init = cJSON_GetObjectItem(var, "initial_value");

        if (!name || !name->valuestring) continue;

        char* scoped_name = c_generate_scoped_var_name(
            name->valuestring,
            (scope && scope->valuestring) ? scope->valuestring : NULL
        );

        const char* init_val_raw = (init && init->valuestring) ? init->valuestring : "0";
        const char* var_type = (type && type->valuestring) ? type->valuestring : "int";

        const char* signal_creator = c_get_signal_creator(var_type);

        if (strcmp(signal_creator, "kryon_signal_create_string") == 0) {
            // For strings, initial_value is a JSON string like "\"actual value\""
            // Strip the outer quotes if present
            const char* str_val = init_val_raw;
            size_t len = strlen(str_val);
            char* unquoted = NULL;
            if (len >= 2 && str_val[0] == '"' && str_val[len-1] == '"') {
                // Strip outer quotes
                unquoted = malloc(len - 1);
                if (unquoted) {
                    strncpy(unquoted, str_val + 1, len - 2);
                    unquoted[len - 2] = '\0';
                    str_val = unquoted;
                }
            }
            fprintf(ctx->output, "    %s_signal = %s(\"%s\");\n",
                    scoped_name, signal_creator, str_val);
            free(unquoted);
        } else if (strcmp(signal_creator, "kryon_signal_create_bool") == 0) {
            bool bool_val = (strcmp(init_val_raw, "true") == 0);
            fprintf(ctx->output, "    %s_signal = %s(%s);\n",
                    scoped_name, signal_creator, bool_val ? "true" : "false");
        } else {
            // For numeric values, use as-is or default to 0 if non-numeric
            const char* init_val = is_numeric(init_val_raw) ? init_val_raw : "0";
            fprintf(ctx->output, "    %s_signal = %s(%s);\n",
                    scoped_name, signal_creator, init_val);
        }

        free(scoped_name);
    }
    fprintf(ctx->output, "\n");
}

void c_generate_reactive_signal_cleanup(CCodegenContext* ctx) {
    if (!ctx->reactive_vars || !cJSON_IsArray(ctx->reactive_vars)) return;

    int var_count = cJSON_GetArraySize(ctx->reactive_vars);
    if (var_count == 0) return;

    fprintf(ctx->output, "    // Cleanup reactive state\n");

    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->reactive_vars, i);
        cJSON* name = cJSON_GetObjectItem(var, "name");
        cJSON* scope = cJSON_GetObjectItem(var, "scope");

        if (!name || !name->valuestring) continue;

        char* scoped_name = c_generate_scoped_var_name(
            name->valuestring,
            (scope && scope->valuestring) ? scope->valuestring : NULL
        );

        fprintf(ctx->output, "    kryon_signal_destroy(%s_signal);\n", scoped_name);
        free(scoped_name);
    }
    fprintf(ctx->output, "\n");
}
