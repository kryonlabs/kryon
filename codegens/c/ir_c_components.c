/**
 * C Code Generator - Component Tree Generation
 *
 * Functions for generating C code from KIR component trees.
 */

#include "ir_c_components.h"
#include "ir_c_output.h"
#include "ir_c_types.h"
#include "ir_c_reactive.h"
#include "ir_c_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

// Parse color string (e.g., "#1a1a2e" or "0x1a1a2e") to uint32_t
static uint32_t parse_color_string(const char* str) {
    if (!str) return 0;
    // Skip # or 0x prefix
    if (str[0] == '#') str++;
    else if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) str += 2;
    return (uint32_t)strtoul(str, NULL, 16);
}

// Internal aliases for shorter function names
#define write_indent(ctx) c_write_indent(ctx)
#define writeln(ctx, str) c_writeln(ctx, str)
#define write_raw(ctx, str) c_write_raw(ctx, str)
#define is_reactive_variable(ctx, name) c_is_reactive_variable(ctx, name)
#define get_scoped_var_name(ctx, name) c_get_scoped_var_name(ctx, name)
#define generate_scoped_var_name(name, scope) c_generate_scoped_var_name(name, scope)
#define expr_to_c(expr) c_expr_to_c(expr)
// Note: get_component_macro and kir_type_to_c are from ir_c_types.h (no c_ prefix)

/**
 * Transform expression strings with dot access to arrow access for pointer types
 * e.g., "habit.name" -> "habit->name"
 * Returns newly allocated string that must be freed.
 */
static char* transform_dot_to_arrow(const char* expr) {
    if (!expr) return strdup("NULL");

    // Find first dot that represents member access (not method call - no parentheses after)
    char* result = strdup(expr);
    if (!result) return strdup(expr);

    char* dot = strchr(result, '.');
    while (dot) {
        // Check if this looks like object.property (no parentheses before the dot)
        // and the left side looks like a simple identifier
        char* left_start = result;
        char* p = dot - 1;

        // Find start of the identifier before the dot
        while (p >= result && (isalnum((unsigned char)*p) || *p == '_')) {
            p--;
        }
        char* id_start = p + 1;

        // Check if it's a simple identifier (not a module call like Storage.load)
        // Modules that should use dot notation, not arrow
        bool is_module = false;
        size_t id_len = dot - id_start;
        if (id_len == 7 && strncmp(id_start, "Storage", 7) == 0) is_module = true;
        if (id_len == 8 && strncmp(id_start, "DateTime", 8) == 0) is_module = true;
        if (id_len == 4 && strncmp(id_start, "Math", 4) == 0) is_module = true;
        if (id_len == 4 && strncmp(id_start, "UUID", 4) == 0) is_module = true;

        if (!is_module && id_start < dot) {
            // Replace dot with arrow
            // Need to reallocate to fit the extra character
            size_t pos = dot - result;
            size_t len = strlen(result);
            char* new_result = malloc(len + 2);
            if (new_result) {
                strncpy(new_result, result, pos);
                new_result[pos] = '-';
                new_result[pos + 1] = '>';
                strcpy(new_result + pos + 2, dot + 1);
                free(result);
                result = new_result;
                dot = strchr(result + pos + 2, '.');  // Continue from after the arrow
            } else {
                break;
            }
        } else {
            dot = strchr(dot + 1, '.');  // Skip this dot
        }
    }

    return result;
}

// Get variable name for a component ID (internal utility)
static const char* get_variable_for_component_id(CCodegenContext* ctx, int component_id) {
    if (!ctx->variables) return NULL;

    int var_count = cJSON_GetArraySize(ctx->variables);
    for (int i = 0; i < var_count; i++) {
        cJSON* var = cJSON_GetArrayItem(ctx->variables, i);
        cJSON* id_item = cJSON_GetObjectItem(var, "component_id");
        if (id_item && id_item->valueint == component_id) {
            cJSON* name = cJSON_GetObjectItem(var, "name");
            if (name && name->valuestring) {
                return name->valuestring;
            }
        }
    }
    return NULL;
}

/**
 * Parse a string template expression like "\"prefix\" + varname" or "varname + \"suffix\""
 * Returns true if successfully parsed, with out parameters filled in.
 * The prefix/suffix will include the quotes and be ready for use in a format string.
 * Sets out_is_reactive to indicate if the variable is a reactive signal or a plain variable.
 */
static bool parse_string_template_expr_ex(CCodegenContext* ctx, const char* expr,
                                          char* out_var_name, size_t var_size,
                                          char* out_format, size_t format_size,
                                          bool* out_is_reactive) {
    if (!expr) return false;

    *out_is_reactive = false;

    // Look for pattern: "string" + varname or varname + "string"
    const char* plus = strstr(expr, " + ");
    if (!plus) return false;

    // Get the parts before and after the +
    size_t left_len = plus - expr;
    const char* right = plus + 3;

    // Check which side is the string literal
    bool left_is_string = (expr[0] == '"');
    bool right_is_string = (right[0] == '"');

    if (!left_is_string && !right_is_string) return false;
    if (left_is_string && right_is_string) return false; // Both strings, not a simple template

    const char* var_part;
    size_t var_part_len;
    const char* str_part;
    size_t str_part_len;
    bool prefix_mode;

    if (left_is_string) {
        // "string" + varname
        str_part = expr;
        str_part_len = left_len;
        var_part = right;
        var_part_len = strlen(right);
        prefix_mode = true;
    } else {
        // varname + "string"
        var_part = expr;
        var_part_len = left_len;
        str_part = right;
        str_part_len = strlen(right);
        prefix_mode = false;
    }

    // Trim whitespace from var_part
    while (var_part_len > 0 && isspace((unsigned char)var_part[var_part_len - 1])) var_part_len--;
    while (var_part_len > 0 && isspace((unsigned char)*var_part)) { var_part++; var_part_len--; }

    // Check if variable is valid identifier (alphanumeric or underscore)
    bool valid_identifier = var_part_len > 0;
    for (size_t i = 0; i < var_part_len && valid_identifier; i++) {
        char c = var_part[i];
        if (i == 0) {
            valid_identifier = isalpha((unsigned char)c) || c == '_';
        } else {
            valid_identifier = isalnum((unsigned char)c) || c == '_';
        }
    }
    if (!valid_identifier) return false;

    // Copy the variable name
    size_t copy_len = var_part_len < var_size - 1 ? var_part_len : var_size - 1;
    strncpy(out_var_name, var_part, copy_len);
    out_var_name[copy_len] = '\0';

    // Check if it's a reactive variable
    if (ctx->reactive_vars) {
        cJSON* var;
        cJSON_ArrayForEach(var, ctx->reactive_vars) {
            cJSON* vname = cJSON_GetObjectItem(var, "name");
            cJSON* vscope = cJSON_GetObjectItem(var, "scope");

            if (!vname || !vname->valuestring) continue;

            const char* var_name = vname->valuestring;
            if (strlen(var_name) != var_part_len || strncmp(var_name, var_part, var_part_len) != 0) continue;

            // Found as reactive variable! Build the signal name
            *out_is_reactive = true;
            const char* scope = (vscope && vscope->valuestring) ? vscope->valuestring : "component";
            if (strcmp(scope, "global") == 0) {
                snprintf(out_var_name, var_size, "%s_global_signal", var_name);
            } else if (strcmp(scope, "component") != 0) {
                snprintf(out_var_name, var_size, "%s_%s_signal", var_name, scope);
            } else {
                snprintf(out_var_name, var_size, "%s_signal", var_name);
            }
            break;
        }
    }

    // Build the format string: extract content between quotes and add %s
    // str_part is like "\"prefix\"" - we need "prefix%s"
    if (str_part_len >= 2 && str_part[0] == '"') {
        // Find closing quote
        const char* end_quote = strrchr(str_part, '"');
        if (end_quote && end_quote > str_part) {
            size_t content_len = end_quote - str_part - 1;
            if (prefix_mode) {
                // "prefix" + var -> "prefix%s"
                snprintf(out_format, format_size, "\"%.*s%%s\"", (int)content_len, str_part + 1);
            } else {
                // var + "suffix" -> "%ssuffix"
                snprintf(out_format, format_size, "\"%%s%.*s\"", (int)content_len, str_part + 1);
            }
            return true;
        }
    }

    return false;
}

// Wrapper for backward compatibility
static bool parse_string_template_expr(CCodegenContext* ctx, const char* expr,
                                       char* out_signal_name, size_t signal_size,
                                       char* out_format, size_t format_size) {
    bool is_reactive = false;
    if (parse_string_template_expr_ex(ctx, expr, out_signal_name, signal_size, out_format, format_size, &is_reactive)) {
        return is_reactive; // Only return true for reactive variables (backward compatibility)
    }
    return false;
}

/**
 * Transform an expression by replacing variable names with their scoped signal names.
 * E.g., "\"You typed: \" + textValue" -> "\"You typed: \" + textValue_global_signal"
 * Returns newly allocated string that must be freed.
 */
static char* transform_expr_to_signal_refs(CCodegenContext* ctx, const char* expr) {
    if (!expr || !ctx->reactive_vars) return strdup(expr);

    // Start with a copy of the expression
    size_t result_size = strlen(expr) * 2 + 256; // Allow room for signal name expansion
    char* result = malloc(result_size);
    if (!result) return strdup(expr);
    strcpy(result, expr);

    // For each reactive variable, replace occurrences of its name with the signal name
    cJSON* var;
    cJSON_ArrayForEach(var, ctx->reactive_vars) {
        cJSON* vname = cJSON_GetObjectItem(var, "name");
        cJSON* vscope = cJSON_GetObjectItem(var, "scope");

        if (!vname || !vname->valuestring) continue;

        const char* var_name = vname->valuestring;
        const char* scope = (vscope && vscope->valuestring) ? vscope->valuestring : "component";

        // Build the signal name
        char signal_name[256];
        if (strcmp(scope, "global") == 0) {
            snprintf(signal_name, sizeof(signal_name), "%s_global_signal", var_name);
        } else if (strcmp(scope, "component") != 0) {
            snprintf(signal_name, sizeof(signal_name), "%s_%s_signal", var_name, scope);
        } else {
            snprintf(signal_name, sizeof(signal_name), "%s_signal", var_name);
        }

        // Replace all occurrences of var_name with signal_name
        // (only when var_name is a complete identifier, not part of another word)
        size_t var_len = strlen(var_name);
        size_t sig_len = strlen(signal_name);
        char* pos = result;

        while ((pos = strstr(pos, var_name)) != NULL) {
            // Check that this is a complete identifier (not part of a larger word)
            bool is_start = (pos == result) || !isalnum((unsigned char)*(pos - 1)) && *(pos - 1) != '_';
            bool is_end = !isalnum((unsigned char)*(pos + var_len)) && *(pos + var_len) != '_';

            if (is_start && is_end) {
                // Replace: shift the rest and insert signal name
                size_t tail_len = strlen(pos + var_len);
                size_t new_len = (pos - result) + sig_len + tail_len;

                if (new_len >= result_size) {
                    // Need to reallocate
                    result_size = new_len + 256;
                    size_t offset = pos - result;
                    result = realloc(result, result_size);
                    if (!result) return strdup(expr);
                    pos = result + offset;
                }

                memmove(pos + sig_len, pos + var_len, tail_len + 1);
                memcpy(pos, signal_name, sig_len);
                pos += sig_len;
            } else {
                pos++;
            }
        }
    }

    return result;
}

// Forward declaration
static bool generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop);

/**
 * Check if an expression references the current loop variable.
 * Handles simple variable names and member access (e.g., "habit" or "habit.name").
 * Returns true if the base variable matches the current loop variable.
 */
static bool expr_references_loop_var(CCodegenContext* ctx, const char* expr) {
    if (!expr || !ctx->current_loop_var) return false;

    // Extract the base variable (before any . or -> access)
    char base_var[256];
    const char* dot = strchr(expr, '.');
    const char* arrow = strstr(expr, "->");

    size_t len;
    if (dot && (!arrow || dot < arrow)) {
        len = dot - expr;
    } else if (arrow) {
        len = arrow - expr;
    } else {
        len = strlen(expr);
    }

    if (len >= sizeof(base_var)) len = sizeof(base_var) - 1;
    strncpy(base_var, expr, len);
    base_var[len] = '\0';

    // Check if this matches the loop variable
    return strcmp(base_var, ctx->current_loop_var) == 0;
}

/**
 * Check if a component has a non-reactive text template binding (e.g., "- " + todo in a for-loop).
 * Also handles plain variable references (e.g., text = todo).
 * If so, extracts the format string and variable name.
 * Returns true if found, with format_out and var_out filled in.
 */
static bool check_nonreactive_text_template(CCodegenContext* ctx, cJSON* component,
                                            char* format_out, size_t format_size,
                                            char* var_out, size_t var_size) {
    if (!ctx->current_loop_var) return false;  // Only relevant in for-loop context

    cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
    if (!property_bindings) return false;

    cJSON* text_binding = cJSON_GetObjectItem(property_bindings, "text");
    if (!text_binding) return false;

    cJSON* binding_type = cJSON_GetObjectItem(text_binding, "binding_type");
    if (!binding_type || !binding_type->valuestring ||
        strcmp(binding_type->valuestring, "static_template") != 0) {
        return false;
    }

    cJSON* source_expr = cJSON_GetObjectItem(text_binding, "source_expr");
    if (!source_expr || !source_expr->valuestring) return false;

    // First, check if this is a plain variable reference that matches the loop variable
    // (no string concatenation, just "text = item")
    if (strcmp(source_expr->valuestring, ctx->current_loop_var) == 0) {
        // Check it's not a reactive variable
        bool is_reactive_var = false;
        if (ctx->reactive_vars) {
            cJSON* var;
            cJSON_ArrayForEach(var, ctx->reactive_vars) {
                cJSON* vname = cJSON_GetObjectItem(var, "name");
                if (vname && vname->valuestring &&
                    strcmp(vname->valuestring, ctx->current_loop_var) == 0) {
                    is_reactive_var = true;
                    break;
                }
            }
        }
        if (!is_reactive_var) {
            // Plain loop variable reference - use "%s" format
            snprintf(format_out, format_size, "\"%%s\"");
            strncpy(var_out, ctx->current_loop_var, var_size - 1);
            var_out[var_size - 1] = '\0';
            return true;
        }
    }

    // Try to parse as string template (e.g., "- " + item)
    char var_name[256];
    char format_str[256];
    bool is_reactive = false;

    if (!parse_string_template_expr_ex(ctx, source_expr->valuestring,
                                        var_name, sizeof(var_name),
                                        format_str, sizeof(format_str),
                                        &is_reactive)) {
        return false;
    }

    // Only handle non-reactive variables that match the loop variable
    if (is_reactive) return false;
    if (strcmp(var_name, ctx->current_loop_var) != 0) return false;

    // Found a non-reactive loop variable text template
    strncpy(format_out, format_str, format_size - 1);
    format_out[format_size - 1] = '\0';
    strncpy(var_out, var_name, var_size - 1);
    var_out[var_size - 1] = '\0';

    return true;
}

void c_generate_component_recursive(CCodegenContext* ctx, cJSON* component, bool is_root) {
    cJSON* type_obj = cJSON_GetObjectItem(component, "type");
    cJSON* id_obj = cJSON_GetObjectItem(component, "id");
    cJSON* children_obj = cJSON_GetObjectItem(component, "children");
    cJSON* text_obj = cJSON_GetObjectItem(component, "text");
    cJSON* scope_obj = cJSON_GetObjectItem(component, "scope");

    if (!type_obj || !type_obj->valuestring) return;

    const char* type = type_obj->valuestring;

    // Save and set current scope for scoped components
    const char* prev_scope = ctx->current_scope;
    if (scope_obj && scope_obj->valuestring) {
        ctx->current_scope = scope_obj->valuestring;
    }

    // Special handling for For loops - emit FOR_EACH macro
    if (strcmp(type, "For") == 0) {
        cJSON* for_def = cJSON_GetObjectItem(component, "for_def");
        if (for_def) {
            cJSON* item_name = cJSON_GetObjectItem(for_def, "item_name");
            cJSON* source = cJSON_GetObjectItem(for_def, "source");
            cJSON* source_expr = source ? cJSON_GetObjectItem(source, "expression") : NULL;

            const char* item_var = item_name ? item_name->valuestring : "item";
            const char* source_var = source_expr ? source_expr->valuestring : "items";

            // Check if source is a KryonStringArray (from const_declarations)
            bool is_string_array = false;
            cJSON* source_structures = cJSON_GetObjectItem(ctx->root_json, "source_structures");
            if (source_structures) {
                cJSON* const_decls = cJSON_GetObjectItem(source_structures, "const_declarations");
                if (const_decls && cJSON_IsArray(const_decls)) {
                    cJSON* decl;
                    cJSON_ArrayForEach(decl, const_decls) {
                        cJSON* decl_name = cJSON_GetObjectItem(decl, "name");
                        cJSON* value_type = cJSON_GetObjectItem(decl, "value_type");
                        if (decl_name && decl_name->valuestring &&
                            strcmp(decl_name->valuestring, source_var) == 0 &&
                            value_type && value_type->valuestring &&
                            strcmp(value_type->valuestring, "array") == 0) {
                            is_string_array = true;
                            break;
                        }
                    }
                }
            }

            // Generate debug print and dynamic list registration wrapper
            write_indent(ctx);
            fprintf(ctx->output, "({\n");
            ctx->indent_level++;

            // Capture parent container for dynamic list registration
            write_indent(ctx);
            fprintf(ctx->output, "IRComponent* _for_each_parent = _kryon_get_current_parent();\n");

            // Generate FOR_EACH macro call
            write_indent(ctx);
            if (is_string_array) {
                // Use KryonStringArray struct access - typed array, can use FOR_EACH
                fprintf(ctx->output, "FOR_EACH(%s, %s_array.items, %s_array.count,\n",
                        item_var, source_var, source_var);
            } else {
                // Use FOR_EACH_TYPED for void* arrays (function results, state variables)
                // This avoids the typeof(void) compilation error
                // Try to infer the element type from the item variable name
                // e.g., "habit" -> "Habit*" (capitalize first letter and add pointer)
                char inferred_type[128];
                if (item_var && item_var[0] >= 'a' && item_var[0] <= 'z') {
                    // Capitalize first letter and make it a pointer type
                    snprintf(inferred_type, sizeof(inferred_type), "%c%s*",
                             item_var[0] - 32, item_var + 1);
                } else {
                    strcpy(inferred_type, "void*");
                }
                fprintf(ctx->output, "FOR_EACH_TYPED(%s, %s, %s, %s_count,\n",
                        inferred_type, item_var, source_var, source_var);
            }

            ctx->indent_level++;

            // Save and set for-loop context
            const char* prev_loop_var = ctx->current_loop_var;
            ctx->current_loop_var = item_var;

            // Generate template children as the body
            if (children_obj && cJSON_IsArray(children_obj)) {
                int child_count = cJSON_GetArraySize(children_obj);
                for (int i = 0; i < child_count; i++) {
                    cJSON* child = cJSON_GetArrayItem(children_obj, i);
                    c_generate_component_recursive(ctx, child, false);
                    if (i < child_count - 1) {
                        fprintf(ctx->output, ",\n");
                    }
                }
            }

            // Restore for-loop context
            ctx->current_loop_var = prev_loop_var;

            ctx->indent_level--;
            fprintf(ctx->output, "\n");
            write_indent(ctx);
            fprintf(ctx->output, ");\n");

            // Register dynamic list for runtime updates
            write_indent(ctx);
            if (is_string_array) {
                fprintf(ctx->output, "kryon_register_dynamic_list(_for_each_parent, &%s_array, \"- %%s\");\n",
                        source_var);
            }

            ctx->indent_level--;
            write_indent(ctx);
            fprintf(ctx->output, "(void)0; })");
            return;
        }
    }

    // Special handling for Custom components (component references)
    cJSON* component_ref = cJSON_GetObjectItem(component, "component_ref");
    if (component_ref && component_ref->valuestring) {
        write_indent(ctx);
        cJSON* arg_obj = cJSON_GetObjectItem(component, "arg");
        if (arg_obj && arg_obj->valuestring) {
            fprintf(ctx->output, "%s(%s)", component_ref->valuestring, arg_obj->valuestring);
        } else {
            fprintf(ctx->output, "%s()", component_ref->valuestring);
        }
        return;
    }

    // Check if this is a custom component (type is PascalCase and not a builtin)
    // Custom components have their type set to the component name (e.g., "HabitPanel")
    const char* macro_check = get_component_macro(type);
    bool is_custom_component = (strcmp(macro_check, "CONTAINER") == 0 &&
                                type[0] >= 'A' && type[0] <= 'Z' &&
                                strcmp(type, "Container") != 0);
    if (is_custom_component) {
        write_indent(ctx);
        cJSON* arg_obj = cJSON_GetObjectItem(component, "arg");
        if (arg_obj && arg_obj->valuestring) {
            fprintf(ctx->output, "%s(%s)", type, arg_obj->valuestring);
        } else {
            fprintf(ctx->output, "%s()", type);
        }
        return;
    }

    const char* macro = get_component_macro(type);

    // Check if this component has a variable assignment
    const char* var_name = NULL;
    if (id_obj) {
        var_name = get_variable_for_component_id(ctx, id_obj->valueint);
    }

    // Check for non-reactive text template (e.g., "- " + todo in a for-loop)
    // For TEXT components with such templates, we need to wrap with snprintf
    char nr_format[256] = {0};
    char nr_var[256] = {0};
    bool has_nonreactive_text_template = false;

    if (strcmp(type, "Text") == 0) {
        has_nonreactive_text_template = check_nonreactive_text_template(
            ctx, component, nr_format, sizeof(nr_format), nr_var, sizeof(nr_var));
    }

    // Write indentation
    write_indent(ctx);

    // For non-reactive text templates, generate snprintf wrapper
    if (has_nonreactive_text_template) {
        fprintf(ctx->output, "({\n");
        ctx->indent_level++;
        write_indent(ctx);
        fprintf(ctx->output, "char _fmt_buf[256];\n");
        write_indent(ctx);
        fprintf(ctx->output, "snprintf(_fmt_buf, sizeof(_fmt_buf), %s, %s);\n", nr_format, nr_var);
        write_indent(ctx);
    }

    // Variable assignment if needed
    if (var_name && !is_root) {
        fprintf(ctx->output, "%s = ", var_name);
    }

    // Component macro
    fprintf(ctx->output, "%s(", macro);

    // Note: BIND_TEXT for reactive text should be handled specially
    // It should be output immediately after the TEXT component, not as a separate child

    // Output text parameter
    cJSON* text_expr_obj = cJSON_GetObjectItem(component, "text_expression");

    // Check for reactive text binding via property_bindings
    cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
    bool has_property_binding_bind_text = false;

    if (property_bindings) {
        cJSON* text_binding = cJSON_GetObjectItem(property_bindings, "text");
        if (text_binding) {
            cJSON* binding_type = cJSON_GetObjectItem(text_binding, "binding_type");
            if (binding_type && binding_type->valuestring &&
                strcmp(binding_type->valuestring, "static_template") == 0) {
                has_property_binding_bind_text = true;
            }
        }
    }

    // Fallback: check if text_expression references a reactive variable
    // (property_bindings may not be present in expanded component instances)
    bool has_fallback_bind_text = !has_property_binding_bind_text &&
                                     text_expr_obj && text_expr_obj->valuestring &&
                                     is_reactive_variable(ctx, text_expr_obj->valuestring);

    // Output text parameter
    bool has_text = (text_obj && text_obj->valuestring) || (text_expr_obj && text_expr_obj->valuestring);

    if (has_nonreactive_text_template) {
        // Non-reactive loop variable template - use the formatted buffer
        fprintf(ctx->output, "_fmt_buf");
        has_text = true;  // Treat as having text for comma handling
    } else if (has_property_binding_bind_text || has_fallback_bind_text) {
        // Reactive binding - use empty string, actual value comes from signal
        fprintf(ctx->output, "\"\"");
    } else if (text_obj && text_obj->valuestring) {
        fprintf(ctx->output, "\"%s\"", text_obj->valuestring);
    } else if (text_expr_obj && text_expr_obj->valuestring) {
        fprintf(ctx->output, "\"\"");
    }

    // Special handling for Checkbox: output 'checked' parameter after label
    if (strcmp(type, "Checkbox") == 0) {
        cJSON* checked_obj = cJSON_GetObjectItem(component, "checked");
        bool checked_val = checked_obj && cJSON_IsTrue(checked_obj);
        fprintf(ctx->output, ", %s", checked_val ? "true" : "false");
    }

    // Special handling for Dropdown/Input: output 'placeholder' parameter
    if (strcmp(type, "Dropdown") == 0 || strcmp(type, "Input") == 0) {
        // For Dropdown/Input with no text, output placeholder as the first argument
        if (!has_text) {
            cJSON* placeholder_obj = cJSON_GetObjectItem(component, "placeholder");
            const char* placeholder_val = placeholder_obj && placeholder_obj->valuestring ?
                                          placeholder_obj->valuestring : "";
            fprintf(ctx->output, "\"%s\"", placeholder_val);
        }
    }

    // Special handling for Tab: output 'title' as first argument
    if (strcmp(type, "Tab") == 0) {
        cJSON* title_obj = cJSON_GetObjectItem(component, "title");
        const char* title_val = title_obj && title_obj->valuestring ?
                                title_obj->valuestring : "Tab";
        fprintf(ctx->output, "\"%s\"", title_val);
    }

    // Check for FULL_SIZE pattern (width=100.0px AND height=100.0px)
    cJSON* width_prop = cJSON_GetObjectItem(component, "width");
    cJSON* height_prop = cJSON_GetObjectItem(component, "height");
    bool is_full_size = (width_prop && width_prop->valuestring &&
                         height_prop && height_prop->valuestring &&
                         (strcmp(width_prop->valuestring, "100.0px") == 0 ||
                          strcmp(width_prop->valuestring, "100.0%") == 0) &&
                         (strcmp(height_prop->valuestring, "100.0px") == 0 ||
                          strcmp(height_prop->valuestring, "100.0%") == 0));

    // Check if we have properties or children
    bool has_properties = false;
    cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, component) {
        const char* key = prop->string;
        if (!key) continue;
        if (strcmp(key, "id") != 0 && strcmp(key, "type") != 0 &&
            strcmp(key, "text") != 0 && strcmp(key, "children") != 0 &&
            strcmp(key, "TEST_MARKER") != 0 && strcmp(key, "direction") != 0 &&
            strcmp(key, "background") != 0 && strcmp(key, "color") != 0 &&
            strcmp(key, "placeholder") != 0 &&  // Skip placeholder for Dropdown/Input
            strcmp(key, "checked") != 0 &&      // Skip checked for Checkbox
            strcmp(key, "text_expression") != 0) { // Skip text_expression (handled separately)
            // Skip property_bindings for non-reactive text templates (handled via snprintf wrapper)
            if (has_nonreactive_text_template && strcmp(key, "property_bindings") == 0) {
                continue;
            }
            has_properties = true;
            break;
        }
        // Check if background/color are non-default
        if (strcmp(key, "background") == 0 && prop->valuestring && strcmp(prop->valuestring, "#00000000") != 0) {
            has_properties = true;
            break;
        }
        if (strcmp(key, "color") == 0 && prop->valuestring && strcmp(prop->valuestring, "#00000000") != 0) {
            has_properties = true;
            break;
        }
    }

    bool has_children = (children_obj && cJSON_GetArraySize(children_obj) > 0);

    // Check if this component has a first argument (Dropdown/Input placeholder, Tab title)
    // For Dropdown/Input, we always output a placeholder (even if empty), so check based on type and !has_text
    // For Tab, we always output the title
    bool has_first_arg = ((strcmp(type, "Dropdown") == 0 || strcmp(type, "Input") == 0) && !has_text) ||
                         (strcmp(type, "Tab") == 0);

    // Add comma after text/first_arg if there are properties or children
    if ((has_text || has_first_arg) && (has_properties || has_children)) {
        fprintf(ctx->output, ",\n");
    } else if (!has_text && !has_first_arg) {
        fprintf(ctx->output, "\n");
    } else if (!has_properties && !has_children) {
        fprintf(ctx->output, "\n");
    }

    ctx->indent_level++;

    // Generate FULL_SIZE if applicable
    bool first_prop = true;
    if (is_full_size) {
        if (!first_prop) {
            fprintf(ctx->output, ",\n");
        }
        write_indent(ctx);
        fprintf(ctx->output, "FULL_SIZE");
        first_prop = false;
    }

    // Special handling for dropdown_state (Dropdown components)
    cJSON* dropdown_state = cJSON_GetObjectItem(component, "dropdown_state");
    if (dropdown_state && cJSON_IsObject(dropdown_state) && strcmp(type, "Dropdown") == 0) {
        // Handle options array
        cJSON* options_array = cJSON_GetObjectItem(dropdown_state, "options");
        if (options_array && cJSON_IsArray(options_array)) {
            int count = cJSON_GetArraySize(options_array);
            if (count > 0) {
                if (!first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);

                // Generate OPTIONS macro call with array elements
                fprintf(ctx->output, "OPTIONS(%d", count);
                for (int i = 0; i < count; i++) {
                    cJSON* item = cJSON_GetArrayItem(options_array, i);
                    if (item && cJSON_IsString(item)) {
                        fprintf(ctx->output, ", \"%s\"", item->valuestring);
                    }
                }
                fprintf(ctx->output, ")");
                first_prop = false;
            }
        }

        // Handle selectedIndex
        cJSON* selected_index = cJSON_GetObjectItem(dropdown_state, "selectedIndex");
        if (selected_index && cJSON_IsNumber(selected_index)) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "SELECTED_INDEX(%d)", selected_index->valueint);
            first_prop = false;
        }
    }

    // Special handling for TEXT components with reactive text_expression
    // Generate BIND_TEXT_EXPR as a property for fallback case (no property_bindings)
    if (strcmp(type, "Text") == 0) {
        cJSON* text_expr_obj = cJSON_GetObjectItem(component, "text_expression");
        if (text_expr_obj && text_expr_obj->valuestring && is_reactive_variable(ctx, text_expr_obj->valuestring)) {
            // Check that there's no property_bindings (which would be handled elsewhere)
            cJSON* property_bindings = cJSON_GetObjectItem(component, "property_bindings");
            bool has_property_binding = false;
            if (property_bindings) {
                cJSON* text_binding = cJSON_GetObjectItem(property_bindings, "text");
                if (text_binding) {
                    cJSON* binding_type = cJSON_GetObjectItem(text_binding, "binding_type");
                    if (binding_type && binding_type->valuestring &&
                        strcmp(binding_type->valuestring, "static_template") == 0) {
                        has_property_binding = true;
                    }
                }
            }

            // Only generate BIND_TEXT_EXPR for fallback case (no property_bindings)
            if (!has_property_binding) {
                if (!first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);

                // Try to parse as string template first (e.g., "prefix" + varname)
                char signal_name[256];
                char format_str[256];
                if (parse_string_template_expr(ctx, text_expr_obj->valuestring, signal_name, sizeof(signal_name), format_str, sizeof(format_str))) {
                    fprintf(ctx->output, "BIND_TEXT_FMT_EXPR(%s, %s)", signal_name, format_str);
                } else {
                    // Fallback: transform the expression to replace variable names with signal names
                    char* signal_transformed = transform_expr_to_signal_refs(ctx, text_expr_obj->valuestring);
                    // Also transform dot access to arrow for pointer types (e.g., habit.name -> habit->name)
                    char* transformed_expr = transform_dot_to_arrow(signal_transformed);
                    free(signal_transformed);
                    fprintf(ctx->output, "BIND_TEXT_EXPR(%s)", transformed_expr);
                    free(transformed_expr);
                }
                first_prop = false;
            }
        }
    }

    // Check for conditional visibility (if/else)
    cJSON* visible_cond = cJSON_GetObjectItem(component, "visible_condition");
    cJSON* visible_when = cJSON_GetObjectItem(component, "visible_when_true");

    if (visible_cond && visible_cond->valuestring && visible_cond->valuestring[0]) {
        bool when_true = visible_when ? cJSON_IsTrue(visible_when) : true;

        // Build signal name from condition variable
        char signal_name[256];
        const char* var_name = visible_cond->valuestring;

        // Check scope from reactive_vars
        const char* scope = "component";
        if (ctx->reactive_vars && cJSON_IsArray(ctx->reactive_vars)) {
            cJSON* var;
            cJSON_ArrayForEach(var, ctx->reactive_vars) {
                cJSON* vname = cJSON_GetObjectItem(var, "name");
                cJSON* vscope = cJSON_GetObjectItem(var, "scope");
                if (vname && vname->valuestring && strcmp(vname->valuestring, var_name) == 0) {
                    if (vscope && vscope->valuestring) {
                        scope = vscope->valuestring;
                    }
                    break;
                }
            }
        }

        if (strcmp(scope, "global") == 0) {
            snprintf(signal_name, sizeof(signal_name), "%s_global_signal", var_name);
        } else if (strcmp(scope, "component") != 0) {
            snprintf(signal_name, sizeof(signal_name), "%s_%s_signal", var_name, scope);
        } else {
            snprintf(signal_name, sizeof(signal_name), "%s_signal", var_name);
        }

        // Generate binding (use _EXPR versions for macro argument context)
        if (!first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (when_true) {
            fprintf(ctx->output, "BIND_VISIBLE_EXPR(%s)", signal_name);
        } else {
            fprintf(ctx->output, "BIND_VISIBLE_NOT_EXPR(%s)", signal_name);
        }
        first_prop = false;
    }

    // Handle table_config for Table components
    cJSON* table_config = cJSON_GetObjectItem(component, "table_config");
    if (table_config && cJSON_IsObject(table_config)) {
        // Cell padding
        cJSON* cell_padding = cJSON_GetObjectItem(table_config, "cellPadding");
        if (cell_padding && cJSON_IsNumber(cell_padding)) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "CELL_PADDING(%.0f)", cell_padding->valuedouble);
            first_prop = false;
        }

        // Striped rows
        cJSON* striped = cJSON_GetObjectItem(table_config, "striped");
        if (striped && cJSON_IsTrue(striped)) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "TABLE_STRIPED");
            first_prop = false;
        }

        // Show borders
        cJSON* show_borders = cJSON_GetObjectItem(table_config, "showBorders");
        if (show_borders && cJSON_IsTrue(show_borders)) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "TABLE_BORDERS");
            first_prop = false;
        }

        // Header background
        cJSON* header_bg = cJSON_GetObjectItem(table_config, "headerBackground");
        if (header_bg && header_bg->valuestring) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            uint32_t color = parse_color_string(header_bg->valuestring);
            fprintf(ctx->output, "HEADER_BG(0x%06x)", color);
            first_prop = false;
        }

        // Even row background
        cJSON* even_row_bg = cJSON_GetObjectItem(table_config, "evenRowBackground");
        if (even_row_bg && even_row_bg->valuestring) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            uint32_t color = parse_color_string(even_row_bg->valuestring);
            fprintf(ctx->output, "EVEN_ROW_BG(0x%06x)", color);
            first_prop = false;
        }

        // Odd row background
        cJSON* odd_row_bg = cJSON_GetObjectItem(table_config, "oddRowBackground");
        if (odd_row_bg && odd_row_bg->valuestring) {
            if (!first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            uint32_t color = parse_color_string(odd_row_bg->valuestring);
            fprintf(ctx->output, "ODD_ROW_BG(0x%06x)", color);
            first_prop = false;
        }
    }

    // Generate properties
    cJSON* prop2 = NULL;
    cJSON_ArrayForEach(prop2, component) {
        const char* key = prop2->string;
        if (!key) continue;

        // Skip internal fields
        if (strcmp(key, "id") == 0 || strcmp(key, "type") == 0 ||
            strcmp(key, "text") == 0 || strcmp(key, "children") == 0 ||
            strcmp(key, "TEST_MARKER") == 0 || strcmp(key, "direction") == 0 ||
            strcmp(key, "placeholder") == 0 ||  // Skip placeholder for Dropdown/Input (handled as argument)
            strcmp(key, "checked") == 0 ||      // Skip checked for Checkbox (handled as argument)
            strcmp(key, "title") == 0 ||        // Skip title for Tab (handled as argument)
            strcmp(key, "dropdown_state") == 0 || // Skip dropdown_state (handled separately below)
            strcmp(key, "visible_condition") == 0 ||  // Skip (handled separately for if/else)
            strcmp(key, "visible_when_true") == 0 ||  // Skip (handled separately for if/else)
            strcmp(key, "table_config") == 0) {  // Skip table_config (handled above)
            continue;
        }

        // Skip width/height if we already generated FULL_SIZE
        if (is_full_size && (strcmp(key, "width") == 0 || strcmp(key, "height") == 0)) {
            continue;
        }

        // Skip property_bindings for TEXT with non-reactive text template (already handled via snprintf wrapper)
        if (has_nonreactive_text_template && strcmp(key, "property_bindings") == 0) {
            continue;
        }

        // Skip transparent/default colors
        if ((strcmp(key, "background") == 0 || strcmp(key, "color") == 0) &&
            prop2->valuestring && strcmp(prop2->valuestring, "#00000000") == 0) {
            continue;
        }

        // Skip events property if there are no event handlers in metadata OR logic_block
        // KRY handlers are in logic_block.functions, C handlers are in c_metadata.event_handlers
        if (strcmp(key, "events") == 0) {
            bool has_kry_handlers = false;
            // Check if we have logic_block with functions (KRY handlers)
            cJSON* logic_block = cJSON_GetObjectItem(ctx->root_json, "logic_block");
            if (logic_block) {
                cJSON* functions = cJSON_GetObjectItem(logic_block, "functions");
                if (functions && cJSON_IsArray(functions) && cJSON_GetArraySize(functions) > 0) {
                    has_kry_handlers = true;
                }
            }
            bool has_c_handlers = (ctx->event_handlers && cJSON_GetArraySize(ctx->event_handlers) > 0);
            if (!has_kry_handlers && !has_c_handlers) {
                continue;
            }
        }

        // Generate the property (comma handling is internal now)
        c_generate_property_macro(ctx, key, prop2, &first_prop);
    }

    // Generate children
    if (children_obj && cJSON_IsArray(children_obj)) {
        int child_count = cJSON_GetArraySize(children_obj);
        if (child_count > 0) {
            // Print comma before children if we had properties
            if (!first_prop) {
                fprintf(ctx->output, ",\n");
                fprintf(ctx->output, "\n");  // Blank line for readability
            }

            for (int i = 0; i < child_count; i++) {
                cJSON* child = cJSON_GetArrayItem(children_obj, i);
                c_generate_component_recursive(ctx, child, false);
                if (i < child_count - 1) {
                    fprintf(ctx->output, ",\n");
                } else {
                    fprintf(ctx->output, "\n");
                }
            }
        }
    }

    ctx->indent_level--;
    write_indent(ctx);
    fprintf(ctx->output, ")");

    // Close snprintf wrapper if we opened one
    if (has_nonreactive_text_template) {
        fprintf(ctx->output, ";\n");
        ctx->indent_level--;
        write_indent(ctx);
        fprintf(ctx->output, "})");
    }

    // Restore previous scope
    ctx->current_scope = prev_scope;
}
bool c_generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop) {
    // NOTE: Each case that generates output must:
    // 1. Print comma if not first_prop
    // 2. Set *first_prop = false after printing

    // Width/Height
    if (strcmp(key, "width") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Only 100% is FULL_WIDTH, not 100px
        if (strcmp(value->valuestring, "100.0%") == 0) {
            fprintf(ctx->output, "FULL_WIDTH");
        } else {
            // Parse numeric value (e.g., "200.0px" → 200, "100.0px" → 100)
            int width_val = 0;
            if (sscanf(value->valuestring, "%d", &width_val) == 1 && width_val > 0) {
                fprintf(ctx->output, "WIDTH(%d)", width_val);
            } else {
                fprintf(ctx->output, "WIDTH(\"%s\")", value->valuestring);
            }
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "height") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Only 100% is FULL_HEIGHT, not 100px
        if (strcmp(value->valuestring, "100.0%") == 0) {
            fprintf(ctx->output, "FULL_HEIGHT");
        } else {
            // Parse numeric value (e.g., "60.0px" → 60, "100.0px" → 100)
            int height_val = 0;
            if (sscanf(value->valuestring, "%d", &height_val) == 1 && height_val > 0) {
                fprintf(ctx->output, "HEIGHT(%d)", height_val);
            } else {
                fprintf(ctx->output, "HEIGHT(\"%s\")", value->valuestring);
            }
        }
        *first_prop = false;
        return true;
    }

    // Min/Max Width
    if (strcmp(key, "minWidth") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MIN_WIDTH(%d)", val);
        } else {
            fprintf(ctx->output, "MIN_WIDTH(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "maxWidth") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MAX_WIDTH(%d)", val);
        } else {
            fprintf(ctx->output, "MAX_WIDTH(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    // Min/Max Height
    if (strcmp(key, "minHeight") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MIN_HEIGHT(%d)", val);
        } else {
            fprintf(ctx->output, "MIN_HEIGHT(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "maxHeight") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        int val = 0;
        if (sscanf(value->valuestring, "%d", &val) == 1 && val > 0) {
            fprintf(ctx->output, "MAX_HEIGHT(%d)", val);
        } else {
            fprintf(ctx->output, "MAX_HEIGHT(\"%s\")", value->valuestring);
        }
        *first_prop = false;
        return true;
    }

    // Positioning - "left" and "top" are used together for absolute positioning
    if (strcmp(key, "left") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "POS_X(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "top") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "POS_Y(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }
    // Skip "position" key as we handle it via left/top
    if (strcmp(key, "position") == 0) {
        return false;  // Don't generate anything, handled by left/top
    }

    // Border (object with width and color)
    if (strcmp(key, "border") == 0 && cJSON_IsObject(value)) {
        cJSON* border_width = cJSON_GetObjectItem(value, "width");
        cJSON* border_color = cJSON_GetObjectItem(value, "color");

        if (border_width && cJSON_IsNumber(border_width)) {
            if (!*first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "BORDER_WIDTH(%.0f)", border_width->valuedouble);
            *first_prop = false;
        }

        if (border_color && border_color->valuestring) {
            if (!*first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            const char* color_str = border_color->valuestring;
            if (color_str[0] == '#' && strlen(color_str) >= 7) {
                fprintf(ctx->output, "BORDER_COLOR(0x%s)", color_str + 1);
            } else {
                fprintf(ctx->output, "BORDER_COLOR(0x%s)", color_str);
            }
            *first_prop = false;
        }
        return true;
    }

    // Colors
    if (strcmp(key, "background") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Convert color string "#RRGGBB" to hex 0xRRGGBB
        const char* color_str = value->valuestring;
        if (color_str[0] == '#' && strlen(color_str) >= 7) {
            fprintf(ctx->output, "BG_COLOR(0x%s)", color_str + 1);  // Skip '#'
        } else {
            // Fallback for non-hex colors
            fprintf(ctx->output, "BG_COLOR(0x%s)", color_str);
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "color") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        // Check for named colors
        if (strcmp(value->valuestring, "#ffffff") == 0) {
            fprintf(ctx->output, "COLOR_WHITE");
        } else if (strcmp(value->valuestring, "#000000") == 0) {
            fprintf(ctx->output, "COLOR_BLACK");
        } else if (strcmp(value->valuestring, "#ff0000") == 0) {
            fprintf(ctx->output, "COLOR_RED");
        } else if (strcmp(value->valuestring, "#00ff00") == 0) {
            fprintf(ctx->output, "COLOR_GREEN");
        } else if (strcmp(value->valuestring, "#0000ff") == 0) {
            fprintf(ctx->output, "COLOR_BLUE");
        } else if (strcmp(value->valuestring, "#ffff00") == 0) {
            fprintf(ctx->output, "COLOR_YELLOW");
        } else if (strcmp(value->valuestring, "#00ffff") == 0) {
            fprintf(ctx->output, "COLOR_CYAN");
        } else if (strcmp(value->valuestring, "#ff00ff") == 0) {
            fprintf(ctx->output, "COLOR_MAGENTA");
        } else if (strcmp(value->valuestring, "#808080") == 0) {
            fprintf(ctx->output, "COLOR_GRAY");
        } else if (strcmp(value->valuestring, "#ffa500") == 0) {
            fprintf(ctx->output, "COLOR_ORANGE");
        } else if (strcmp(value->valuestring, "#800080") == 0) {
            fprintf(ctx->output, "COLOR_PURPLE");
        } else {
            // Convert color string "#RRGGBB" to hex 0xRRGGBB
            const char* color_str = value->valuestring;
            if (color_str[0] == '#' && strlen(color_str) >= 7) {
                fprintf(ctx->output, "TEXT_COLOR(0x%s)", color_str + 1);  // Skip '#'
            } else {
                fprintf(ctx->output, "TEXT_COLOR(0x%s)", color_str);
            }
        }
        *first_prop = false;
        return true;
    }

    // Padding/Margin
    if (strcmp(key, "padding") == 0) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (cJSON_IsNumber(value)) {
            fprintf(ctx->output, "PADDING(%.0f)", value->valuedouble);
        } else if (cJSON_IsArray(value)) {
            int size = cJSON_GetArraySize(value);
            if (size == 4) {
                fprintf(ctx->output, "PADDING_SIDES(%.0f, %.0f, %.0f, %.0f)",
                        cJSON_GetArrayItem(value, 0)->valuedouble,
                        cJSON_GetArrayItem(value, 1)->valuedouble,
                        cJSON_GetArrayItem(value, 2)->valuedouble,
                        cJSON_GetArrayItem(value, 3)->valuedouble);
            }
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "gap") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "GAP(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }

    // Typography
    if (strcmp(key, "fontSize") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "FONT_SIZE(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "fontBold") == 0 && cJSON_IsBool(value)) {
        if (cJSON_IsTrue(value)) {
            if (!*first_prop) fprintf(ctx->output, ",\n");
            write_indent(ctx);
            fprintf(ctx->output, "FONT_BOLD");
            *first_prop = false;
            return true;
        }
        return false;  // fontBold=false generates no output, so no comma
    }

    // Layout
    if (strcmp(key, "justifyContent") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "center") == 0) {
            fprintf(ctx->output, "JUSTIFY_CENTER");
        } else if (strcmp(value->valuestring, "flex-start") == 0 || strcmp(value->valuestring, "start") == 0) {
            fprintf(ctx->output, "JUSTIFY_START");
        } else if (strcmp(value->valuestring, "flex-end") == 0 || strcmp(value->valuestring, "end") == 0) {
            fprintf(ctx->output, "JUSTIFY_END");
        } else if (strcmp(value->valuestring, "space-between") == 0) {
            fprintf(ctx->output, "JUSTIFY_SPACE_BETWEEN");
        } else if (strcmp(value->valuestring, "space-around") == 0) {
            fprintf(ctx->output, "JUSTIFY_SPACE_AROUND");
        } else if (strcmp(value->valuestring, "space-evenly") == 0) {
            fprintf(ctx->output, "JUSTIFY_EVENLY");
        } else {
            fprintf(ctx->output, "JUSTIFY_START");  // Default to start, not center
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "alignItems") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        if (strcmp(value->valuestring, "center") == 0) {
            fprintf(ctx->output, "ALIGN_CENTER");
        } else if (strcmp(value->valuestring, "flex-start") == 0 || strcmp(value->valuestring, "start") == 0) {
            fprintf(ctx->output, "ALIGN_START");
        } else if (strcmp(value->valuestring, "flex-end") == 0 || strcmp(value->valuestring, "end") == 0) {
            fprintf(ctx->output, "ALIGN_END");
        } else if (strcmp(value->valuestring, "stretch") == 0) {
            fprintf(ctx->output, "ALIGN_STRETCH");
        } else {
            fprintf(ctx->output, "ALIGN_CENTER");
        }
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "flexShrink") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "FLEX_SHRINK(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }

    // Table properties
    if (strcmp(key, "cellPadding") == 0 && cJSON_IsNumber(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "CELL_PADDING(%.0f)", value->valuedouble);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "striped") == 0 && cJSON_IsTrue(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "TABLE_STRIPED");
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "showBorders") == 0 && cJSON_IsTrue(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "TABLE_BORDERS");
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "headerBackground") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        uint32_t color = parse_color_string(value->valuestring);
        fprintf(ctx->output, "HEADER_BG(0x%06x)", color);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "evenRowBackground") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        uint32_t color = parse_color_string(value->valuestring);
        fprintf(ctx->output, "EVEN_ROW_BG(0x%06x)", color);
        *first_prop = false;
        return true;
    }
    if (strcmp(key, "oddRowBackground") == 0 && value->valuestring) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        uint32_t color = parse_color_string(value->valuestring);
        fprintf(ctx->output, "ODD_ROW_BG(0x%06x)", color);
        *first_prop = false;
        return true;
    }

    // Visibility
    if (strcmp(key, "visible") == 0 && cJSON_IsBool(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);
        fprintf(ctx->output, "VISIBLE(%s)", cJSON_IsTrue(value) ? "true" : "false");
        *first_prop = false;
        return true;
    }

    // Events
    if (strcmp(key, "events") == 0 && cJSON_IsArray(value)) {
        int event_count = cJSON_GetArraySize(value);
        bool printed = false;
        for (int i = 0; i < event_count; i++) {
            cJSON* event = cJSON_GetArrayItem(value, i);
            cJSON* event_type = cJSON_GetObjectItem(event, "type");
            cJSON* logic_id = cJSON_GetObjectItem(event, "logic_id");

            if (event_type && event_type->valuestring && logic_id && logic_id->valuestring) {
                // For KRY handlers, logic_id is the function name directly (e.g., "handler_1_click")
                // For C metadata handlers, we need to look up the function name
                const char* func_name = logic_id->valuestring;  // Default: use logic_id as function name
                bool is_c_handler = false;  // Track if this is a C metadata handler

                // Try to find function name in c_metadata.event_handlers (legacy C handlers)
                if (ctx->event_handlers) {
                    int handler_count = cJSON_GetArraySize(ctx->event_handlers);
                    for (int j = 0; j < handler_count; j++) {
                        cJSON* handler = cJSON_GetArrayItem(ctx->event_handlers, j);
                        cJSON* h_logic_id = cJSON_GetObjectItem(handler, "logic_id");
                        cJSON* h_func_name = cJSON_GetObjectItem(handler, "function_name");

                        if (h_logic_id && h_logic_id->valuestring &&
                            h_func_name && h_func_name->valuestring &&
                            strcmp(h_logic_id->valuestring, logic_id->valuestring) == 0) {
                            func_name = h_func_name->valuestring;
                            is_c_handler = true;  // This is a C metadata handler
                            break;
                        }
                    }
                }

                // For KRY handlers, scope the function name based on current_scope
                // C handlers are already defined by the user, so don't scope them
                char* scoped_func_name = NULL;
                if (!is_c_handler && ctx->current_scope && strcmp(ctx->current_scope, "component") != 0) {
                    // Generate scoped handler name (e.g., "handler_1_click_Counter_0")
                    scoped_func_name = generate_scoped_var_name(func_name, ctx->current_scope);
                    func_name = scoped_func_name;
                }

                if (strcmp(event_type->valuestring, "click") == 0) {
                    if (!*first_prop) fprintf(ctx->output, ",\n");
                    write_indent(ctx);
                    fprintf(ctx->output, "ON_CLICK(%s)", func_name);
                    *first_prop = false;
                    printed = true;
                }

                if (scoped_func_name) {
                    free(scoped_func_name);
                }
            }
        }
        return printed;
    }

    // Dropdown options array
    if (strcmp(key, "options") == 0 && cJSON_IsArray(value)) {
        if (!*first_prop) fprintf(ctx->output, ",\n");
        write_indent(ctx);

        // Generate OPTIONS macro call with array elements
        int count = cJSON_GetArraySize(value);
        fprintf(ctx->output, "OPTIONS(%d", count);

        // Output each option string
        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(value, i);
            if (item && cJSON_IsString(item)) {
                fprintf(ctx->output, ", \"%s\"", item->valuestring);
            }
        }
        fprintf(ctx->output, ")");

        *first_prop = false;
        return true;
    }

    // Handle reactive property bindings
    if (strcmp(key, "property_bindings") == 0 && cJSON_IsObject(value)) {
        bool printed = false;
        cJSON* binding = NULL;

        cJSON_ArrayForEach(binding, value) {
            const char* prop_name = binding->string;
            if (!prop_name) continue;

            cJSON* source_expr = cJSON_GetObjectItem(binding, "source_expr");
            cJSON* binding_type = cJSON_GetObjectItem(binding, "binding_type");

            if (!source_expr || !source_expr->valuestring || !binding_type || !binding_type->valuestring) {
                continue;
            }

            // Handle two-way bindings (e.g., INPUT value)
            if (strcmp(binding_type->valuestring, "two_way") == 0) {
                if (!*first_prop) fprintf(ctx->output, ",\n");
                write_indent(ctx);

                // Transform variable to signal name
                char* transformed = transform_expr_to_signal_refs(ctx, source_expr->valuestring);
                fprintf(ctx->output, "BIND_INPUT_VALUE(%s)", transformed);
                free(transformed);

                *first_prop = false;
                printed = true;
                continue;
            }

            if (strcmp(binding_type->valuestring, "static_template") == 0) {

                // Generate binding macro call (use _EXPR versions for macro argument context)
                if (strcmp(prop_name, "text") == 0) {
                    // Try to parse as string template first (e.g., "prefix" + varname)
                    char var_name[256];
                    char format_str[256];
                    bool is_reactive = false;
                    if (parse_string_template_expr_ex(ctx, source_expr->valuestring, var_name, sizeof(var_name), format_str, sizeof(format_str), &is_reactive)) {
                        if (is_reactive) {
                            // Reactive variable - use BIND_TEXT_FMT_EXPR
                            if (!*first_prop) fprintf(ctx->output, ",\n");
                            write_indent(ctx);
                            fprintf(ctx->output, "BIND_TEXT_FMT_EXPR(%s, %s)", var_name, format_str);
                            *first_prop = false;
                            printed = true;
                        } else {
                            // Non-reactive variable (e.g., for-loop iterator)
                            // This should be handled by the snprintf wrapper in c_generate_component_recursive
                            // If we reach here, it means the wrapper wasn't applied - skip to avoid invalid code
                            // The TEXT content was already set to _fmt_buf by the wrapper
                            continue;
                        }
                    } else {
                        // Fallback: check if this is a loop variable expression
                        if (!*first_prop) fprintf(ctx->output, ",\n");
                        write_indent(ctx);
                        // Transform dot access to arrow for pointer types (e.g., habit.name -> habit->name)
                        char* transformed_expr = transform_dot_to_arrow(source_expr->valuestring);

                        // Check if this references the loop variable (non-reactive)
                        if (expr_references_loop_var(ctx, source_expr->valuestring)) {
                            // Use SET_TEXT_EXPR for loop variable member access
                            fprintf(ctx->output, "SET_TEXT_EXPR(%s)", transformed_expr);
                        } else {
                            // Reactive case: transform variable names to signal names
                            char* signal_transformed = transform_expr_to_signal_refs(ctx, transformed_expr);
                            fprintf(ctx->output, "BIND_TEXT_EXPR(%s)", signal_transformed);
                            free(signal_transformed);
                        }
                        free(transformed_expr);
                        *first_prop = false;
                        printed = true;
                    }
                    continue;
                } else {
                    // For non-text bindings, just transform variable names to signal names
                    if (!*first_prop) fprintf(ctx->output, ",\n");
                    write_indent(ctx);
                    char* signal_transformed = transform_expr_to_signal_refs(ctx, source_expr->valuestring);
                    // Also transform dot access to arrow for pointer types (e.g., habit.color -> habit->color)
                    char* transformed_expr = transform_dot_to_arrow(signal_transformed);
                    free(signal_transformed);
                    if (strcmp(prop_name, "visible") == 0) {
                        fprintf(ctx->output, "BIND_VISIBLE_EXPR(%s)", transformed_expr);
                    } else if (strcmp(prop_name, "background") == 0) {
                        fprintf(ctx->output, "BIND_BACKGROUND(%s)", transformed_expr);
                    } else if (strcmp(prop_name, "color") == 0) {
                        fprintf(ctx->output, "BIND_COLOR(%s)", transformed_expr);
                    } else if (strcmp(prop_name, "selectedIndex") == 0) {
                        fprintf(ctx->output, "SELECTED_INDEX(%s)", transformed_expr);
                    } else {
                        fprintf(ctx->output, "BIND(%s, %s)", prop_name, transformed_expr);
                    }
                    free(transformed_expr);
                }
                *first_prop = false;
                printed = true;
            }
        }
        return printed;
    }

    // Fallback: skip unknown properties (no comma printed)
    return false;
}
void c_generate_component_definitions(CCodegenContext* ctx, cJSON* component_defs) {
    if (!component_defs || !cJSON_IsArray(component_defs)) return;

    cJSON* def;
    cJSON_ArrayForEach(def, component_defs) {
        cJSON* name = cJSON_GetObjectItem(def, "name");
        cJSON* props = cJSON_GetObjectItem(def, "props");
        cJSON* template = cJSON_GetObjectItem(def, "template");

        if (!name || !cJSON_IsString(name)) continue;

        const char* comp_name = cJSON_GetStringValue(name);

        // Generate component function signature
        fprintf(ctx->output, "/**\n");
        fprintf(ctx->output, " * Component: %s\n", comp_name);

        // Document props
        if (props && cJSON_IsArray(props)) {
            cJSON* prop;
            cJSON_ArrayForEach(prop, props) {
                cJSON* prop_name = cJSON_GetObjectItem(prop, "name");
                cJSON* prop_type = cJSON_GetObjectItem(prop, "type");
                if (prop_name && cJSON_IsString(prop_name)) {
                    fprintf(ctx->output, " * @param %s %s\n",
                            cJSON_GetStringValue(prop_name),
                            prop_type && cJSON_IsString(prop_type) ?
                            cJSON_GetStringValue(prop_type) : "any");
                }
            }
        }
        fprintf(ctx->output, " */\n");

        // Generate KRYON_COMPONENT macro call
        fprintf(ctx->output, "KRYON_COMPONENT(%s", comp_name);

        // Add props as macro parameters
        if (props && cJSON_IsArray(props)) {
            cJSON* prop;
            cJSON_ArrayForEach(prop, props) {
                cJSON* prop_name = cJSON_GetObjectItem(prop, "name");
                if (prop_name && cJSON_IsString(prop_name)) {
                    fprintf(ctx->output, ", %s", cJSON_GetStringValue(prop_name));
                }
            }
        }
        fprintf(ctx->output, ",\n");

        // Generate template content
        ctx->indent_level++;
        if (template) {
            c_generate_component_recursive(ctx, template, true);
        }
        ctx->indent_level--;

        fprintf(ctx->output, "\n);\n\n");
    }
}

/**
 * Generate C struct definitions from source_structures.struct_types
 */

// Internal wrapper for forward declaration
static bool generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop) {
    return c_generate_property_macro(ctx, key, value, first_prop);
}
