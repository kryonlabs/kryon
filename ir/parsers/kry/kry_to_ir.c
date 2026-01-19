/**
 * Kryon .kry AST to IR Converter
 *
 * Converts the parsed .kry AST into IR component trees.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_parser.h"
#include "kry_ast.h"
#include "kry_expression.h"
#include "../include/ir_builder.h"
#include "../include/ir_serialization.h"
#include "../include/ir_logic.h"
#include "../include/ir_style.h"
#include "../src/style/ir_stylesheet.h"
#include "../include/ir_animation_builder.h"
#include "../src/logic/ir_foreach.h"
#include "../html/css_parser.h"  // For ir_css_parse_color
#include "../parser_utils.h"     // For parser_parse_color_packed
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Compilation Modes
// ============================================================================

typedef enum {
    IR_COMPILE_MODE_RUNTIME,  // Expand only (default, current behavior)
    IR_COMPILE_MODE_CODEGEN,  // Preserve only (templates for codegen)
    IR_COMPILE_MODE_HYBRID    // Both expand AND preserve (recommended for round-trip)
} IRCompileMode;

// ============================================================================
// Conversion Context
// ============================================================================

#define MAX_PARAMS 16

typedef struct {
    char* name;
    char* value;       // String representation of the value (for simple types)
    KryValue* kry_value;  // Full KryValue for arrays/objects (NULL for simple types)
} ParamSubstitution;

typedef struct {
    KryNode* ast_root;  // Root of AST (for finding component definitions)
    ParamSubstitution params[MAX_PARAMS];  // Parameter substitutions
    int param_count;
    IRReactiveManifest* manifest;  // Reactive manifest for state variables
    IRLogicBlock* logic_block;     // Logic block for event handlers
    uint32_t next_handler_id;      // Counter for generating unique handler names
    IRCompileMode compile_mode;    // Compilation mode (RUNTIME/CODEGEN/HYBRID)
    IRSourceStructures* source_structures;  // Source preservation (for round-trip codegen)
    uint32_t static_block_counter; // Counter for generating unique static block IDs
    const char* current_static_block_id;  // Current static block context (NULL if not in static block)
    KryExprTarget target_platform; // Target platform for expression transpilation (Lua or JavaScript)
} ConversionContext;

// ============================================================================
// Argument Parsing
// ============================================================================

static void parse_arguments(ConversionContext* ctx, const char* args, KryNode* definition) {
    if (!ctx || !args || !definition) return;

    // Skip whitespace
    while (*args == ' ' || *args == '\t' || *args == '\n') args++;

    const char* param_name = "initialValue";  // Default parameter name
    const char* param_value = NULL;

    // Empty arguments - use defaults
    if (*args == '\0') {
        // Set default value (0 for initialValue)
        param_value = "0";
    } else {
        // Simple parsing: check if it's a positional argument (just a number)
        // or a named argument (name=value)
        const char* equals = strchr(args, '=');

        if (equals == NULL) {
            // Positional argument - map to first parameter (initialValue)
            size_t len = strlen(args);
            char* value = (char*)malloc(len + 1);
            if (value) {
                strncpy(value, args, len);
                value[len] = '\0';
                // Trim whitespace from end
                while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t')) {
                    value[--len] = '\0';
                }
                param_value = value;
            }
        } else {
            // Named argument: initialValue=10
            size_t name_len = equals - args;
            char* pname = (char*)malloc(name_len + 1);
            if (pname) {
                strncpy(pname, args, name_len);
                pname[name_len] = '\0';

                // Trim whitespace from parameter name
                while (name_len > 0 && (pname[name_len-1] == ' ' || pname[name_len-1] == '\t')) {
                    pname[--name_len] = '\0';
                }
                param_name = pname;

                const char* value_start = equals + 1;
                while (*value_start == ' ' || *value_start == '\t') value_start++;
                param_value = value_start;
            }
        }
    }

    // Add parameter substitution: "initialValue" -> "5" (or "0", "10", etc.)
    if (param_value && ctx->param_count < MAX_PARAMS) {
        ctx->params[ctx->param_count].name = (char*)param_name;
        ctx->params[ctx->param_count].value = (char*)param_value;
        ctx->param_count++;
    }

    // Now find state declarations that use this parameter and create substitutions for them
    // Example: state value: int = initialValue
    // This creates substitution: "value" -> param_value (e.g., "5")
    KryNode* child = definition->first_child;
    while (child && ctx->param_count < MAX_PARAMS) {
        if (child->type == KRY_NODE_STATE && child->value) {
            // State declaration with initializer
            // Check if the initializer expression references our parameter
            if (child->value->type == KRY_VALUE_IDENTIFIER ||
                child->value->type == KRY_VALUE_EXPRESSION) {

                const char* init_expr = (child->value->type == KRY_VALUE_IDENTIFIER)
                    ? child->value->identifier
                    : child->value->expression;

                // If the initializer matches our parameter name, create state variable substitution
                if (init_expr && strcmp(init_expr, param_name) == 0) {
                    // Map state variable name to the resolved parameter value
                    ctx->params[ctx->param_count].name = (char*)child->name;
                    ctx->params[ctx->param_count].value = (char*)param_value;
                    ctx->param_count++;
                }
            }
        }
        child = child->next_sibling;
    }
}

// Helper function to add a parameter substitution (string value)
static void add_param(ConversionContext* ctx, const char* name, const char* value) {
    if (!ctx || !name || !value || ctx->param_count >= MAX_PARAMS) return;

    ctx->params[ctx->param_count].name = (char*)name;
    ctx->params[ctx->param_count].value = (char*)value;
    ctx->params[ctx->param_count].kry_value = NULL;  // Simple string value
    ctx->param_count++;
}

// Helper function to add a parameter substitution (KryValue for arrays/objects)
static void add_param_value(ConversionContext* ctx, const char* name, KryValue* value) {
    if (!ctx || !name || !value || ctx->param_count >= MAX_PARAMS) return;

    ctx->params[ctx->param_count].name = (char*)name;
    ctx->params[ctx->param_count].value = NULL;      // No string representation
    ctx->params[ctx->param_count].kry_value = value;  // Store full KryValue
    ctx->param_count++;
}

// ============================================================================
// KryValue to JSON Serialization
// ============================================================================

/**
 * Convert a KryValue to a JSON string representation.
 * Returns a dynamically allocated string that must be freed by the caller.
 */
static char* kry_value_to_json(KryValue* value) {
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

/**
 * Transpile a KRY value expression to the target platform (Lua or JavaScript).
 * Returns a dynamically allocated string that must be freed by the caller.
 * If the value is not an expression or transpilation fails, returns NULL.
 */
static char* kry_value_transpile_expression(KryValue* value, KryExprTarget target) {
    if (!value || value->type != KRY_VALUE_EXPRESSION) {
        return NULL;
    }

    // Try to parse and transpile the expression
    char* error = NULL;
    KryExprNode* node = kry_expr_parse(value->expression, &error);
    if (!node) {
        // If parsing fails, fall back to raw expression (for compatibility)
        printf("[TRANSPILE] Failed to parse expression '%s': %s\n", value->expression, error ? error : "unknown");
        free(error);
        return NULL;
    }

    // Transpile to target platform
    KryExprOptions options = {target, false, 0};
    size_t output_length = 0;
    char* result = kry_expr_transpile(node, &options, &output_length);

    kry_expr_free(node);

    if (result) {
        printf("[TRANSPILE] Successfully transpiled '%s' to '%s' (target: %s)\n",
               value->expression, result, target == KRY_TARGET_LUA ? "Lua" : "JavaScript");
    }

    return result;
}

/**
 * Convert a KryValue to the target platform code (Lua or JavaScript).
 * This handles expression transpilation for cross-platform support.
 * Returns a dynamically allocated string that must be freed by the caller.
 */
static char* kry_value_to_target_code(KryValue* value, KryExprTarget target) {
    if (!value) return strdup("null");

    // If it's an expression, try to transpile it
    if (value->type == KRY_VALUE_EXPRESSION) {
        char* transpiled = kry_value_transpile_expression(value, target);
        if (transpiled) {
            return transpiled;  // Successfully transpiled
        }
        // Fall through to raw expression if transpilation fails
    }

    // Fall back to JSON conversion for non-expressions or failed transpilation
    return kry_value_to_json(value);
}

// ============================================================================
// Parameter Substitution
// ============================================================================

static const char* substitute_param(ConversionContext* ctx, const char* expr) {
    if (!ctx || !expr) return expr;

    printf("[SUBSTITUTE_PARAM] Looking for '%s' in %d params\n", expr, ctx->param_count);
    // Check if expression is a parameter reference
    for (int i = 0; i < ctx->param_count; i++) {
        printf("[SUBSTITUTE_PARAM]   param[%d]: name='%s', value='%s'\n",
               i, ctx->params[i].name, ctx->params[i].value ? ctx->params[i].value : "NULL");
        if (strcmp(expr, ctx->params[i].name) == 0) {
            printf("[SUBSTITUTE_PARAM]   MATCH! Returning '%s'\n", ctx->params[i].value);
            return ctx->params[i].value;
        }
    }

    printf("[SUBSTITUTE_PARAM] No match found, returning original\n");
    return expr;  // No substitution found
}

// Check if an expression is unresolved (no parameter substitution found)
// This is used to detect property expressions that should be preserved as bindings
static bool is_unresolved_expr(ConversionContext* ctx, const char* expr) {
    if (!ctx || !expr) return false;

    // If no params registered, everything is unresolved
    if (ctx->param_count == 0) return true;

    // Check if this expression matches any param name or starts with a param
    for (int i = 0; i < ctx->param_count; i++) {
        // Direct match (e.g., "item")
        if (strcmp(expr, ctx->params[i].name) == 0) {
            return false;  // Resolved!
        }

        // Property access (e.g., "item.value" or "item.colors[0]")
        size_t param_len = strlen(ctx->params[i].name);
        if (strncmp(expr, ctx->params[i].name, param_len) == 0 &&
            (expr[param_len] == '.' || expr[param_len] == '[')) {
            return false;  // Resolved - it's a property/index access on a param
        }
    }

    return true;  // Unresolved
}

// ============================================================================
// Component Definition Lookup
// ============================================================================

__attribute__((unused)) static KryNode* find_component_definition(ConversionContext* ctx, const char* name) {
    if (!ctx || !ctx->ast_root || !name) return NULL;

    // Search through top-level siblings for component definitions
    KryNode* node = ctx->ast_root;
    while (node) {
        if (node->is_component_definition && node->name && strcmp(node->name, name) == 0) {
            return node;
        }
        node = node->next_sibling;
    }

    return NULL;
}

// ============================================================================
// Component Name Mapping
// ============================================================================

static IRComponentType get_component_type(const char* name) {
    if (!name) return IR_COMPONENT_CONTAINER;

    // Normalize to lowercase for comparison
    char lower[256];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && name[i]; i++) {
        lower[i] = tolower(name[i]);
    }
    lower[i] = '\0';

    if (strcmp(lower, "app") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(lower, "container") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(lower, "row") == 0) return IR_COMPONENT_ROW;
    if (strcmp(lower, "column") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(lower, "text") == 0) return IR_COMPONENT_TEXT;
    if (strcmp(lower, "button") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(lower, "input") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(lower, "checkbox") == 0) return IR_COMPONENT_CHECKBOX;
    if (strcmp(lower, "dropdown") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(lower, "center") == 0) return IR_COMPONENT_CENTER;
    if (strcmp(lower, "canvas") == 0) return IR_COMPONENT_CANVAS;

    // Table components
    if (strcmp(lower, "table") == 0) return IR_COMPONENT_TABLE;
    if (strcmp(lower, "tablehead") == 0) return IR_COMPONENT_TABLE_HEAD;
    if (strcmp(lower, "tablebody") == 0) return IR_COMPONENT_TABLE_BODY;
    if (strcmp(lower, "tablefoot") == 0) return IR_COMPONENT_TABLE_FOOT;
    if (strcmp(lower, "tablerow") == 0 || strcmp(lower, "tr") == 0) return IR_COMPONENT_TABLE_ROW;
    if (strcmp(lower, "tablecell") == 0 || strcmp(lower, "td") == 0) return IR_COMPONENT_TABLE_CELL;
    if (strcmp(lower, "tableheadercell") == 0 || strcmp(lower, "th") == 0) return IR_COMPONENT_TABLE_HEADER_CELL;

    // Inline semantic components (for rich text)
    if (strcmp(lower, "span") == 0) return IR_COMPONENT_SPAN;
    if (strcmp(lower, "strong") == 0) return IR_COMPONENT_STRONG;
    if (strcmp(lower, "em") == 0) return IR_COMPONENT_EM;
    if (strcmp(lower, "codeinline") == 0 || strcmp(lower, "code") == 0) return IR_COMPONENT_CODE_INLINE;
    if (strcmp(lower, "small") == 0) return IR_COMPONENT_SMALL;
    if (strcmp(lower, "mark") == 0) return IR_COMPONENT_MARK;

    // Markdown document components
    if (strcmp(lower, "paragraph") == 0 || strcmp(lower, "p") == 0) return IR_COMPONENT_PARAGRAPH;
    if (strcmp(lower, "heading") == 0) return IR_COMPONENT_HEADING;
    if (strcmp(lower, "blockquote") == 0) return IR_COMPONENT_BLOCKQUOTE;
    if (strcmp(lower, "codeblock") == 0) return IR_COMPONENT_CODE_BLOCK;
    if (strcmp(lower, "list") == 0) return IR_COMPONENT_LIST;
    if (strcmp(lower, "listitem") == 0) return IR_COMPONENT_LIST_ITEM;
    if (strcmp(lower, "link") == 0 || strcmp(lower, "a") == 0) return IR_COMPONENT_LINK;

    // Default to container for unknown types
    return IR_COMPONENT_CONTAINER;
}

// Color parsing now uses parser_parse_color_packed() from parser_utils.h

// ============================================================================
// Property Application
// ============================================================================

static void apply_property(ConversionContext* ctx, IRComponent* component, const char* name, KryValue* value) {
    fprintf(stderr, "[APPLY_PROP_ENTRY] name='%s', component=%p\n", name ? name : "NULL", (void*)component);
    if (!component || !name || !value) {
        fprintf(stderr, "[APPLY_PROP_ENTRY]   Early return: component=%p, name=%p, value=%p\n",
                (void*)component, (void*)name, (void*)value);
        return;
    }

    // Property aliases for App component (root container)
    // Map user-friendly names to internal window properties
    fprintf(stderr, "[PROP_ALIAS] component->id=%u, name='%s'\n", component->id, name);
    if (component->id == 1) {  // Root component (App)
        fprintf(stderr, "[PROP_ALIAS]   Is root component!\n");
        if (strcmp(name, "title") == 0) {
            fprintf(stderr, "[PROP_ALIAS]   Aliasing 'title' -> 'windowTitle'\n");
            name = "windowTitle";
        } else if (strcmp(name, "width") == 0 && value->type == KRY_VALUE_NUMBER) {
            fprintf(stderr, "[PROP_ALIAS]   Calling windowWidth for width\n");
            // For App, width sets window width, but also fall through to set container width
            apply_property(ctx, component, "windowWidth", value);
        } else if (strcmp(name, "height") == 0 && value->type == KRY_VALUE_NUMBER) {
            fprintf(stderr, "[PROP_ALIAS]   Calling windowHeight for height\n");
            // For App, height sets window height, but also fall through to set container height
            apply_property(ctx, component, "windowHeight", value);
        }
    }

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // CSS class name
    if (strcmp(name, "className") == 0 || strcmp(name, "class") == 0) {
        if (value->type == KRY_VALUE_STRING) {
            if (component->css_class) {
                free(component->css_class);
            }
            component->css_class = strdup(value->string_value);
            fprintf(stderr, "[APPLY_PROPERTY] Set css_class = '%s'\n", component->css_class);
        }
        return;
    }

    // Text content
    if (strcmp(name, "text") == 0) {
        printf("[APPLY_PROPERTY] text property, value type=%d\n", value->type);
        printf("[APPLY_PROPERTY]   STRING=%d, IDENTIFIER=%d, EXPRESSION=%d\n",
               KRY_VALUE_STRING, KRY_VALUE_IDENTIFIER, KRY_VALUE_EXPRESSION);
        if (value->type == KRY_VALUE_STRING) {
            printf("[APPLY_PROPERTY]   Setting text to string: %s\n", value->string_value);
            ir_set_text_content(component, value->string_value);
            return;
        } else if (value->type == KRY_VALUE_IDENTIFIER) {
            printf("[APPLY_PROPERTY]   Identifier: %s\n", value->identifier);
            // Handle identifiers - apply parameter substitution (for variables like item.name)
            const char* substituted = substitute_param(ctx, value->identifier);
            printf("[APPLY_PROPERTY]   Substituted to: %s\n", substituted);

            // Detect and preserve unresolved identifiers as property bindings
            if (is_unresolved_expr(ctx, value->identifier) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                printf("[APPLY_PROPERTY]   Unresolved! Creating property binding for '%s'\n", value->identifier);
                ir_component_add_property_binding(component, name, value->identifier, substituted, "static_template");

                // Set as text expression for runtime evaluation instead of static content
                printf("[APPLY_PROPERTY]   Setting text_expression = '%s'\n", value->identifier);
                if (component->text_expression) {
                    free(component->text_expression);
                }
                component->text_expression = strdup(value->identifier);
                return;
            }

            ir_set_text_content(component, substituted);
            return;
        } else if (value->type == KRY_VALUE_EXPRESSION) {
            printf("[APPLY_PROPERTY]   Expression: %s\n", value->expression);
            // Handle expressions - apply parameter substitution
            const char* substituted = substitute_param(ctx, value->expression);
            printf("[APPLY_PROPERTY]   Substituted to: %s\n", substituted);

            // Detect and preserve unresolved expressions as property bindings
            if (is_unresolved_expr(ctx, value->expression) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                printf("[APPLY_PROPERTY]   Unresolved! Creating property binding for '%s'\n", value->expression);
                ir_component_add_property_binding(component, name, value->expression, substituted, "static_template");

                // Set as text expression for runtime evaluation instead of static content
                printf("[APPLY_PROPERTY]   Setting text_expression = '%s'\n", value->expression);
                if (component->text_expression) {
                    free(component->text_expression);
                }
                component->text_expression = strdup(value->expression);
                return;
            }

            ir_set_text_content(component, substituted);
            return;
        } else {
            printf("[APPLY_PROPERTY]   Unknown type %d\n", value->type);
        }
    }

    // Checkbox label
    if (strcmp(name, "label") == 0 && value->type == KRY_VALUE_STRING) {
        ir_set_text_content(component, value->string_value);
        return;
    }

    // Checkbox checked state
    if (strcmp(name, "checked") == 0) {
        bool checked = false;
        if (value->type == KRY_VALUE_IDENTIFIER) {
            if (strcmp(value->identifier, "true") == 0) {
                checked = true;
            } else if (strcmp(value->identifier, "false") == 0) {
                checked = false;
            }
        } else if (value->type == KRY_VALUE_NUMBER) {
            checked = (value->number_value != 0);
        }
        ir_set_checkbox_state(component, checked);
        return;
    }

    // Event handlers - create logic functions and event bindings
    if (strcmp(name, "onClick") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION && ctx->logic_block) {
            // Generate unique handler name
            char handler_name[64];
            snprintf(handler_name, sizeof(handler_name), "handler_%u_click", ctx->next_handler_id++);

            IRLogicFunction* func = NULL;

            // Try to detect simple patterns and convert to universal logic
            // Pattern: "var = !var" (toggle)
            const char* expr = value->expression;
            char var_name[64] = {0};

            // Strip trailing whitespace and braces
            const char* expr_end = expr + strlen(expr);
            while (expr_end > expr && (isspace(*(expr_end - 1)) || *(expr_end - 1) == '}')) {
                expr_end--;
            }

            // Simple pattern matching for "varName = !varName"
            const char* p = expr;
            while (*p && isspace(*p)) p++;  // Skip leading whitespace

            const char* var_start = p;
            while (*p && p < expr_end && (isalnum(*p) || *p == '_')) p++;  // Parse identifier
            size_t var_len = p - var_start;

            if (var_len > 0 && var_len < sizeof(var_name)) {
                while (*p && p < expr_end && isspace(*p)) p++;  // Skip whitespace
                if (*p == '=' && p < expr_end) {
                    p++;
                    while (*p && p < expr_end && isspace(*p)) p++;  // Skip whitespace
                    if (*p == '!' && p < expr_end) {
                        p++;
                        while (*p && p < expr_end && isspace(*p)) p++;  // Skip whitespace

                        // Check if same variable name follows
                        if (p + var_len <= expr_end &&
                            strncmp(p, var_start, var_len) == 0 &&
                            (p + var_len == expr_end || (!isalnum(p[var_len]) && p[var_len] != '_'))) {
                            // It's a toggle! Extract variable name
                            strncpy(var_name, var_start, var_len);
                            var_name[var_len] = '\0';

                            // Create toggle handler with universal logic
                            func = ir_logic_create_toggle(handler_name, var_name);
                        }
                    }
                }
            }

            // Pattern: "var += number" (increment) or "var -= number" (decrement)
            if (!func) {
                const char* p2 = expr;
                while (*p2 && isspace(*p2)) p2++;  // Skip leading whitespace

                const char* var_start2 = p2;
                while (*p2 && p2 < expr_end && (isalnum(*p2) || *p2 == '_')) p2++;
                size_t var_len2 = p2 - var_start2;

                if (var_len2 > 0 && var_len2 < sizeof(var_name)) {
                    while (*p2 && p2 < expr_end && isspace(*p2)) p2++;  // Skip whitespace

                    char op = 0;
                    if (p2 + 1 < expr_end && *p2 == '+' && *(p2+1) == '=') {
                        op = '+';
                        p2 += 2;
                    } else if (p2 + 1 < expr_end && *p2 == '-' && *(p2+1) == '=') {
                        op = '-';
                        p2 += 2;
                    }

                    if (op) {
                        while (*p2 && p2 < expr_end && isspace(*p2)) p2++;  // Skip whitespace

                        // Parse number
                        char* num_end = NULL;
                        int64_t delta = strtoll(p2, &num_end, 10);

                        // Check if we parsed a valid number
                        if (num_end > p2) {
                            // Extract variable name
                            strncpy(var_name, var_start2, var_len2);
                            var_name[var_len2] = '\0';

                            // For simple +=1 or -=1, use existing helpers
                            if (delta == 1) {
                                if (op == '+') {
                                    func = ir_logic_create_increment(handler_name, var_name);
                                } else {
                                    func = ir_logic_create_decrement(handler_name, var_name);
                                }
                            } else {
                                // For other values, create STMT_ASSIGN_OP manually
                                func = ir_logic_function_create(handler_name);
                                if (func) {
                                    IRExpression* delta_expr = ir_expr_int(delta);
                                    IRStatement* stmt = ir_stmt_assign_op(
                                        var_name,
                                        op == '+' ? ASSIGN_OP_ADD : ASSIGN_OP_SUB,
                                        delta_expr
                                    );
                                    ir_logic_function_add_stmt(func, stmt);
                                }
                            }
                        }
                    }
                }
            }

            // If pattern detection failed, create function with .kry source code
            if (!func) {
                func = ir_logic_function_create(handler_name);
                if (func) {
                    // Add the source code as .kry language
                    ir_logic_function_add_source(func, "kry", value->expression);
                }
            }

            if (func) {
                // Add function to logic block
                ir_logic_block_add_function(ctx->logic_block, func);

                // Create event binding
                IREventBinding* binding = ir_event_binding_create(
                    component->id, "click", handler_name
                );
                if (binding) {
                    ir_logic_block_add_binding(ctx->logic_block, binding);
                }
            }

            // Also create legacy IREvent for backwards compatibility
            IREvent* event = ir_create_event(IR_EVENT_CLICK, handler_name, value->expression);
            if (event) {
                ir_add_event(component, event);
            }
            return;
        }
    }

    if (strcmp(name, "onHover") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION && ctx->logic_block) {
            char handler_name[64];
            snprintf(handler_name, sizeof(handler_name), "handler_%u_hover", ctx->next_handler_id++);

            IRLogicFunction* func = ir_logic_function_create(handler_name);
            if (func) {
                ir_logic_function_add_source(func, "kry", value->expression);
                ir_logic_block_add_function(ctx->logic_block, func);

                IREventBinding* binding = ir_event_binding_create(
                    component->id, "hover", handler_name
                );
                if (binding) {
                    ir_logic_block_add_binding(ctx->logic_block, binding);
                }
            }

            IREvent* event = ir_create_event(IR_EVENT_HOVER, handler_name, value->expression);
            if (event) {
                ir_add_event(component, event);
            }
            return;
        }
    }

    if (strcmp(name, "onChange") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION && ctx->logic_block) {
            char handler_name[64];
            snprintf(handler_name, sizeof(handler_name), "handler_%u_change", ctx->next_handler_id++);

            IRLogicFunction* func = ir_logic_function_create(handler_name);
            if (func) {
                ir_logic_function_add_source(func, "kry", value->expression);
                ir_logic_block_add_function(ctx->logic_block, func);

                IREventBinding* binding = ir_event_binding_create(
                    component->id, "change", handler_name
                );
                if (binding) {
                    ir_logic_block_add_binding(ctx->logic_block, binding);
                }
            }

            IREvent* event = ir_create_event(IR_EVENT_TEXT_CHANGE, handler_name, value->expression);
            if (event) {
                ir_add_event(component, event);
            }
            return;
        }
    }

    // Handle string values for other properties
    if (value->type == KRY_VALUE_STRING) {
        // Original string handling continues below...
    }

    // Dimensions
    if (strcmp(name, "width") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
            ir_set_width(component, dim_type, (float)value->number_value);
        } else if (value->type == KRY_VALUE_STRING) {
            // Parse string like "100px", "50%", "auto"
            const char* str = value->string_value;
            if (strcmp(str, "auto") == 0) {
                ir_set_width(component, IR_DIMENSION_AUTO, 0);
            } else if (strstr(str, "%")) {
                float num = (float)atof(str);
                ir_set_width(component, IR_DIMENSION_PERCENT, num);
            } else {
                // Parse px value (strip "px" suffix if present)
                float num = (float)atof(str);
                ir_set_width(component, IR_DIMENSION_PX, num);
            }
        }
        return;
    }

    if (strcmp(name, "height") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
            ir_set_height(component, dim_type, (float)value->number_value);
        } else if (value->type == KRY_VALUE_STRING) {
            // Parse string like "100px", "50%", "auto"
            const char* str = value->string_value;
            if (strcmp(str, "auto") == 0) {
                ir_set_height(component, IR_DIMENSION_AUTO, 0);
            } else if (strstr(str, "%")) {
                float num = (float)atof(str);
                ir_set_height(component, IR_DIMENSION_PERCENT, num);
            } else {
                // Parse px value (strip "px" suffix if present)
                float num = (float)atof(str);
                ir_set_height(component, IR_DIMENSION_PX, num);
            }
        }
        return;
    }

    // Position
    if (strcmp(name, "posX") == 0 || strcmp(name, "left") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->position_mode = IR_POSITION_ABSOLUTE;
            style->absolute_x = (float)value->number_value;
        }
        return;
    }

    if (strcmp(name, "posY") == 0 || strcmp(name, "top") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->position_mode = IR_POSITION_ABSOLUTE;
            style->absolute_y = (float)value->number_value;
        }
        return;
    }

    // Colors
    if (strcmp(name, "backgroundColor") == 0 || strcmp(name, "background") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            const char* color_str = (value->type == KRY_VALUE_STRING) ? value->string_value :
                                    (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                                    value->expression;
            // Apply parameter substitution for identifiers and expressions (e.g., item.colors[0])
            if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
                const char* original_expr = color_str;
                color_str = substitute_param(ctx, color_str);

                // Detect and preserve unresolved identifiers as property bindings
                printf("[APPLY_PROPERTY]   Checking if '%s' is unresolved for background (compile_mode=%d)\n", original_expr, ctx->compile_mode);
                bool unresolved = is_unresolved_expr(ctx, original_expr);
                printf("[APPLY_PROPERTY]   is_unresolved=%d\n", unresolved);
                if (unresolved && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    printf("[APPLY_PROPERTY]   Unresolved! Creating property binding for background '%s'\n", original_expr);
                    ir_component_add_property_binding(component, name, original_expr, "#00000000", "static_template");
                }
            }
            uint32_t color = parser_parse_color_packed(color_str);
            ir_set_background_color(style,
                (color >> 24) & 0xFF,
                (color >> 16) & 0xFF,
                (color >> 8) & 0xFF,
                color & 0xFF);
        }
        return;
    }

    if (strcmp(name, "color") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            const char* color_str = (value->type == KRY_VALUE_STRING) ? value->string_value :
                                    (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                                    value->expression;
            // Apply parameter substitution for identifiers and expressions
            if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
                const char* original_expr = color_str;
                color_str = substitute_param(ctx, color_str);

                // Detect and preserve unresolved identifiers as property bindings
                if (is_unresolved_expr(ctx, original_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    ir_component_add_property_binding(component, name, original_expr, "#00000000", "static_template");
                }
            }
            uint32_t color = parser_parse_color_packed(color_str);
            style->font.color.type = IR_COLOR_SOLID;
            style->font.color.data.r = (color >> 24) & 0xFF;
            style->font.color.data.g = (color >> 16) & 0xFF;
            style->font.color.data.b = (color >> 8) & 0xFF;
            style->font.color.data.a = color & 0xFF;
        }
        return;
    }

    if (strcmp(name, "borderColor") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            const char* color_str = (value->type == KRY_VALUE_STRING) ? value->string_value :
                                    (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                                    value->expression;
            // Apply parameter substitution for identifiers and expressions
            if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
                const char* original_expr = color_str;
                color_str = substitute_param(ctx, color_str);

                // Detect and preserve unresolved identifiers as property bindings
                if (is_unresolved_expr(ctx, original_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    ir_component_add_property_binding(component, name, original_expr, "#00000000", "static_template");
                }
            }
            uint32_t color = parser_parse_color_packed(color_str);
            style->border.color.type = IR_COLOR_SOLID;
            style->border.color.data.r = (color >> 24) & 0xFF;
            style->border.color.data.g = (color >> 16) & 0xFF;
            style->border.color.data.b = (color >> 8) & 0xFF;
            style->border.color.data.a = color & 0xFF;
        }
        return;
    }

    // Border
    if (strcmp(name, "borderWidth") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->border.width = (float)value->number_value;
        }
        return;
    }

    if (strcmp(name, "borderRadius") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->border.radius = (uint8_t)value->number_value;
        }
        return;
    }

    // Layout alignment
    if (strcmp(name, "contentAlignment") == 0 || strcmp(name, "alignItems") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            const char* align = (value->type == KRY_VALUE_STRING) ? value->string_value :
                               (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                               value->expression;
            // Apply parameter substitution for identifiers/expressions
            if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
                const char* original_expr = align;
                align = substitute_param(ctx, align);

                // Detect and preserve unresolved identifiers as property bindings
                if (is_unresolved_expr(ctx, original_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    ir_component_add_property_binding(component, name, original_expr, "start", "static_template");
                }
            }

            IRAlignment alignment = IR_ALIGNMENT_START;
            if (strcmp(align, "center") == 0) {
                alignment = IR_ALIGNMENT_CENTER;
            } else if (strcmp(align, "start") == 0) {
                alignment = IR_ALIGNMENT_START;
            } else if (strcmp(align, "end") == 0) {
                alignment = IR_ALIGNMENT_END;
            }

            // Ensure layout exists
            IRLayout* layout = ir_get_layout(component);
            if (getenv("KRYON_DEBUG_LAYOUT")) {
                fprintf(stderr, "[KRY_PARSER] Component %u: %s='%s' -> alignment=%d, layout=%p\n",
                        component->id, name, align, alignment, (void*)layout);
            }
            if (layout) {
                layout->flex.cross_axis = alignment;
                // If contentAlignment, also set justify_content for both-axis centering
                if (strcmp(name, "contentAlignment") == 0) {
                    layout->flex.justify_content = alignment;
                    if (getenv("KRYON_DEBUG_LAYOUT")) {
                        fprintf(stderr, "[KRY_PARSER]   contentAlignment: set both cross_axis=%d and justify_content=%d\n",
                                alignment, alignment);
                    }
                } else {
                    if (getenv("KRYON_DEBUG_LAYOUT")) {
                        fprintf(stderr, "[KRY_PARSER]   alignItems: set cross_axis=%d only\n", alignment);
                    }
                }
            }
        }
        return;
    }

    if (strcmp(name, "justifyContent") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            const char* justify = (value->type == KRY_VALUE_STRING) ? value->string_value :
                                  (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                                  value->expression;
            // Apply parameter substitution for expressions and identifiers (e.g., item.value)
            if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
                const char* original_expr = justify;
                justify = substitute_param(ctx, justify);

                // Detect and preserve unresolved identifiers as property bindings
                printf("[APPLY_PROPERTY]   Checking if '%s' is unresolved (compile_mode=%d)\n", original_expr, ctx->compile_mode);
                bool unresolved = is_unresolved_expr(ctx, original_expr);
                printf("[APPLY_PROPERTY]   is_unresolved=%d\n", unresolved);
                if (unresolved && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    printf("[APPLY_PROPERTY]   Unresolved! Creating property binding for '%s'\n", original_expr);
                    ir_component_add_property_binding(component, name, original_expr, "start", "static_template");
                }
            }

            IRAlignment alignment = IR_ALIGNMENT_START;
            if (strcmp(justify, "center") == 0) {
                alignment = IR_ALIGNMENT_CENTER;
            } else if (strcmp(justify, "start") == 0) {
                alignment = IR_ALIGNMENT_START;
            } else if (strcmp(justify, "end") == 0) {
                alignment = IR_ALIGNMENT_END;
            } else if (strcmp(justify, "flex-start") == 0) {
                alignment = IR_ALIGNMENT_START;
            } else if (strcmp(justify, "flex-end") == 0) {
                alignment = IR_ALIGNMENT_END;
            } else if (strcmp(justify, "spaceEvenly") == 0 || strcmp(justify, "space-evenly") == 0) {
                alignment = IR_ALIGNMENT_SPACE_EVENLY;
            } else if (strcmp(justify, "spaceAround") == 0 || strcmp(justify, "space-around") == 0) {
                alignment = IR_ALIGNMENT_SPACE_AROUND;
            } else if (strcmp(justify, "spaceBetween") == 0 || strcmp(justify, "space-between") == 0) {
                alignment = IR_ALIGNMENT_SPACE_BETWEEN;
            }

            // Ensure layout exists
            IRLayout* layout = ir_get_layout(component);
            if (layout) {
                layout->flex.justify_content = alignment;
            }
        }
        return;
    }

    // Padding
    if (strcmp(name, "padding") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            float p = (float)value->number_value;
            ir_set_padding(component, p, p, p, p);
        }
        return;
    }

    // Gap (for flex layouts)
    if (strcmp(name, "gap") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRLayout* layout = ir_get_layout(component);
            if (layout) {
                layout->flex.gap = (uint32_t)value->number_value;
            }
        } else if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            // Handle identifiers and expressions (e.g., item.gap)
            const char* gap_expr = (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier : value->expression;
            const char* gap_str = substitute_param(ctx, gap_expr);

            // Detect and preserve unresolved identifiers as property bindings
            if (is_unresolved_expr(ctx, gap_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                ir_component_add_property_binding(component, name, gap_expr, "0", "static_template");
            }

            float gap_value = (float)atof(gap_str);
            IRLayout* layout = ir_get_layout(component);
            if (layout) {
                layout->flex.gap = (uint32_t)gap_value;
            }
        }
        return;
    }

    // Font properties
    if (strcmp(name, "fontSize") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->font.size = (float)value->number_value;
        } else if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            // Handle identifiers and expressions (e.g., item.fontSize)
            const char* size_expr = (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier : value->expression;
            const char* size_str = substitute_param(ctx, size_expr);

            // Detect and preserve unresolved identifiers as property bindings
            if (is_unresolved_expr(ctx, size_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                ir_component_add_property_binding(component, name, size_expr, "16", "static_template");
            }

            style->font.size = (float)atof(size_str);
        }
        return;
    }

    if (strcmp(name, "fontWeight") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            style->font.weight = (uint16_t)value->number_value;
        }
        return;
    }

    if (strcmp(name, "fontFamily") == 0) {
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* family = value->type == KRY_VALUE_STRING ?
                                value->string_value : value->identifier;
            strncpy(style->font.family, family, sizeof(style->font.family) - 1);
        }
        return;
    }

    // Window properties (for App component)
    if (strcmp(name, "windowTitle") == 0) {
        fprintf(stderr, "[WINDOW_PROP] Setting windowTitle\n");
        if (value->type == KRY_VALUE_STRING) {
            // Get or create metadata
            IRContext* ctx = g_ir_context;
            fprintf(stderr, "[WINDOW_PROP]   g_ir_context=%p\n", (void*)ctx);
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
                    fprintf(stderr, "[WINDOW_PROP]   Created metadata=%p\n", (void*)ctx->metadata);
                }
                if (ctx->metadata) {
                    // Free old title if exists
                    if (ctx->metadata->window_title) {
                        free(ctx->metadata->window_title);
                    }
                    // Allocate and copy new title
                    size_t len = strlen(value->string_value);
                    ctx->metadata->window_title = (char*)malloc(len + 1);
                    if (ctx->metadata->window_title) {
                        strcpy(ctx->metadata->window_title, value->string_value);
                        fprintf(stderr, "[WINDOW_PROP]   Set title='%s'\n", ctx->metadata->window_title);
                    }
                }
            }
        }
        return;
    }

    if (strcmp(name, "windowWidth") == 0) {
        fprintf(stderr, "[WINDOW_PROP] Setting windowWidth\n");
        if (value->type == KRY_VALUE_NUMBER) {
            IRContext* ctx = g_ir_context;
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
                }
                if (ctx->metadata) {
                    ctx->metadata->window_width = (int)value->number_value;
                    fprintf(stderr, "[WINDOW_PROP]   Set width=%d\n", ctx->metadata->window_width);
                }
            }
        }
        return;
    }

    if (strcmp(name, "windowHeight") == 0) {
        fprintf(stderr, "[WINDOW_PROP] Setting windowHeight\n");
        if (value->type == KRY_VALUE_NUMBER) {
            IRContext* ctx = g_ir_context;
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
                }
                if (ctx->metadata) {
                    ctx->metadata->window_height = (int)value->number_value;
                    fprintf(stderr, "[WINDOW_PROP]   Set height=%d\n", ctx->metadata->window_height);
                }
            }
        }
        return;
    }

    // Animation property
    if (strcmp(name, "animation") == 0) {
        if (value->type == KRY_VALUE_STRING) {
            // Parse preset animation string (e.g., "pulse(2.0, -1)")
            const char* anim_str = value->string_value;

            // Extract animation name and parameters
            char anim_name[64] = {0};
            float duration = 1.0f;
            int iterations = 1;

            // Simple parser for "name(duration, iterations)" format
            if (sscanf(anim_str, "%63[^(](%f, %d)", anim_name, &duration, &iterations) >= 1) {
                IRAnimation* anim = NULL;

                // Create preset animation based on name
                if (strcmp(anim_name, "pulse") == 0) {
                    anim = ir_animation_pulse(duration);
                } else if (strcmp(anim_name, "fadeInOut") == 0) {
                    anim = ir_animation_fade_in_out(duration);
                } else if (strcmp(anim_name, "slideInLeft") == 0) {
                    anim = ir_animation_slide_in_left(duration);
                }

                if (anim) {
                    anim->iteration_count = iterations;

                    // Ensure style exists
                    if (!component->style) {
                        component->style = (IRStyle*)calloc(1, sizeof(IRStyle));
                    }

                    // Add animation to component
                    ir_component_add_animation(component, anim);
                }
            }
        }
        return;
    }

    // Transition property
    if (strcmp(name, "transition") == 0) {
        // Handle both string and array format
        // String format: "opacity 0.3 ease-out"
        // Array format: [{property: "opacity", duration: 0.3, easing: "ease-out"}]

        if (value->type == KRY_VALUE_STRING) {
            // Parse simple transition string: "property duration easing"
            char prop_name[64] = {0};
            float duration = 0.3f;
            char easing_name[32] = "ease-in-out";

            int parsed = sscanf(value->string_value, "%63s %f %31s", prop_name, &duration, easing_name);
            if (parsed >= 2) {
                // Map property name to IRAnimationProperty
                IRAnimationProperty prop = IR_ANIM_PROP_CUSTOM;
                if (strcmp(prop_name, "opacity") == 0) prop = IR_ANIM_PROP_OPACITY;
                else if (strcmp(prop_name, "transform") == 0) prop = IR_ANIM_PROP_ROTATE;  // Generic transform
                else if (strcmp(prop_name, "translateX") == 0) prop = IR_ANIM_PROP_TRANSLATE_X;
                else if (strcmp(prop_name, "translateY") == 0) prop = IR_ANIM_PROP_TRANSLATE_Y;
                else if (strcmp(prop_name, "scaleX") == 0) prop = IR_ANIM_PROP_SCALE_X;
                else if (strcmp(prop_name, "scaleY") == 0) prop = IR_ANIM_PROP_SCALE_Y;
                else if (strcmp(prop_name, "rotate") == 0) prop = IR_ANIM_PROP_ROTATE;
                else if (strcmp(prop_name, "background") == 0) prop = IR_ANIM_PROP_BACKGROUND_COLOR;

                // Map easing name to IREasingType
                IREasingType easing = IR_EASING_EASE_IN_OUT;
                if (strcmp(easing_name, "linear") == 0) easing = IR_EASING_LINEAR;
                else if (strcmp(easing_name, "ease-in") == 0 || strcmp(easing_name, "easeIn") == 0) easing = IR_EASING_EASE_IN;
                else if (strcmp(easing_name, "ease-out") == 0 || strcmp(easing_name, "easeOut") == 0) easing = IR_EASING_EASE_OUT;
                else if (strcmp(easing_name, "ease-in-out") == 0 || strcmp(easing_name, "easeInOut") == 0) easing = IR_EASING_EASE_IN_OUT;
                else if (strcmp(easing_name, "ease-in-quad") == 0) easing = IR_EASING_EASE_IN_QUAD;
                else if (strcmp(easing_name, "ease-out-quad") == 0) easing = IR_EASING_EASE_OUT_QUAD;
                else if (strcmp(easing_name, "ease-in-out-quad") == 0) easing = IR_EASING_EASE_IN_OUT_QUAD;
                else if (strcmp(easing_name, "ease-in-cubic") == 0) easing = IR_EASING_EASE_IN_CUBIC;
                else if (strcmp(easing_name, "ease-out-cubic") == 0) easing = IR_EASING_EASE_OUT_CUBIC;
                else if (strcmp(easing_name, "ease-in-out-cubic") == 0) easing = IR_EASING_EASE_IN_OUT_CUBIC;

                IRTransition* trans = ir_transition_create(prop, duration);
                ir_transition_set_easing(trans, easing);

                // Ensure style exists
                if (!component->style) {
                    component->style = (IRStyle*)calloc(1, sizeof(IRStyle));
                }

                ir_component_add_transition(component, trans);
            }
        }
        else if (value->type == KRY_VALUE_ARRAY) {
            // Array of transition definitions
            for (size_t i = 0; i < value->array.count; i++) {
                KryValue* item = value->array.elements[i];
                if (!item || item->type != KRY_VALUE_OBJECT) continue;

                // Extract properties from object
                KryValue* prop_val = NULL;
                KryValue* dur_val = NULL;
                KryValue* easing_val = NULL;
                KryValue* delay_val = NULL;

                for (size_t j = 0; j < item->object.count; j++) {
                    const char* key = item->object.keys[j];
                    KryValue* val = item->object.values[j];

                    if (strcmp(key, "property") == 0) prop_val = val;
                    else if (strcmp(key, "duration") == 0) dur_val = val;
                    else if (strcmp(key, "easing") == 0) easing_val = val;
                    else if (strcmp(key, "delay") == 0) delay_val = val;
                }

                if (!prop_val || prop_val->type != KRY_VALUE_STRING) continue;
                if (!dur_val || dur_val->type != KRY_VALUE_NUMBER) continue;

                // Map property name
                IRAnimationProperty prop = IR_ANIM_PROP_CUSTOM;
                const char* prop_name = prop_val->string_value;
                if (strcmp(prop_name, "opacity") == 0) prop = IR_ANIM_PROP_OPACITY;
                else if (strcmp(prop_name, "transform") == 0) prop = IR_ANIM_PROP_ROTATE;
                else if (strcmp(prop_name, "translateX") == 0) prop = IR_ANIM_PROP_TRANSLATE_X;
                else if (strcmp(prop_name, "translateY") == 0) prop = IR_ANIM_PROP_TRANSLATE_Y;
                else if (strcmp(prop_name, "scaleX") == 0) prop = IR_ANIM_PROP_SCALE_X;
                else if (strcmp(prop_name, "scaleY") == 0) prop = IR_ANIM_PROP_SCALE_Y;
                else if (strcmp(prop_name, "rotate") == 0) prop = IR_ANIM_PROP_ROTATE;
                else if (strcmp(prop_name, "background") == 0) prop = IR_ANIM_PROP_BACKGROUND_COLOR;

                IRTransition* trans = ir_transition_create(prop, (float)dur_val->number_value);

                // Set easing if provided
                if (easing_val && easing_val->type == KRY_VALUE_STRING) {
                    const char* easing_name = easing_val->string_value;
                    IREasingType easing = IR_EASING_EASE_IN_OUT;
                    if (strcmp(easing_name, "linear") == 0) easing = IR_EASING_LINEAR;
                    else if (strcmp(easing_name, "ease-in") == 0 || strcmp(easing_name, "easeIn") == 0) easing = IR_EASING_EASE_IN;
                    else if (strcmp(easing_name, "ease-out") == 0 || strcmp(easing_name, "easeOut") == 0) easing = IR_EASING_EASE_OUT;
                    else if (strcmp(easing_name, "ease-in-out") == 0 || strcmp(easing_name, "easeInOut") == 0) easing = IR_EASING_EASE_IN_OUT;
                    else if (strcmp(easing_name, "ease-in-quad") == 0) easing = IR_EASING_EASE_IN_QUAD;
                    else if (strcmp(easing_name, "ease-out-quad") == 0) easing = IR_EASING_EASE_OUT_QUAD;
                    else if (strcmp(easing_name, "ease-in-out-quad") == 0) easing = IR_EASING_EASE_IN_OUT_QUAD;
                    else if (strcmp(easing_name, "ease-in-cubic") == 0) easing = IR_EASING_EASE_IN_CUBIC;
                    else if (strcmp(easing_name, "ease-out-cubic") == 0) easing = IR_EASING_EASE_OUT_CUBIC;
                    else if (strcmp(easing_name, "ease-in-out-cubic") == 0) easing = IR_EASING_EASE_IN_OUT_CUBIC;

                    ir_transition_set_easing(trans, easing);
                }

                // Set delay if provided
                if (delay_val && delay_val->type == KRY_VALUE_NUMBER) {
                    ir_transition_set_delay(trans, (float)delay_val->number_value);
                }

                // Ensure style exists
                if (!component->style) {
                    component->style = (IRStyle*)calloc(1, sizeof(IRStyle));
                }

                ir_component_add_transition(component, trans);
            }
        }
        return;
    }

    // Dropdown-specific properties
    if (component->type == IR_COMPONENT_DROPDOWN) {
        if (strcmp(name, "placeholder") == 0 && value->type == KRY_VALUE_STRING) {
            // Initialize dropdown state if needed
            if (!component->custom_data) {
                IRDropdownState* state = (IRDropdownState*)calloc(1, sizeof(IRDropdownState));
                state->selected_index = -1;
                state->is_open = false;
                state->hovered_index = -1;
                component->custom_data = (char*)state;
            }
            IRDropdownState* state = (IRDropdownState*)component->custom_data;
            if (state->placeholder) free(state->placeholder);
            state->placeholder = strdup(value->string_value);
            return;
        }

        if (strcmp(name, "selectedIndex") == 0 && value->type == KRY_VALUE_NUMBER) {
            // Initialize dropdown state if needed
            if (!component->custom_data) {
                IRDropdownState* state = (IRDropdownState*)calloc(1, sizeof(IRDropdownState));
                state->selected_index = -1;
                state->is_open = false;
                state->hovered_index = -1;
                component->custom_data = (char*)state;
            }
            IRDropdownState* state = (IRDropdownState*)component->custom_data;
            state->selected_index = (int32_t)value->number_value;
            return;
        }

        if (strcmp(name, "options") == 0 && value->type == KRY_VALUE_ARRAY) {
            // Initialize dropdown state if needed
            if (!component->custom_data) {
                IRDropdownState* state = (IRDropdownState*)calloc(1, sizeof(IRDropdownState));
                state->selected_index = -1;
                state->is_open = false;
                state->hovered_index = -1;
                component->custom_data = (char*)state;
            }
            IRDropdownState* state = (IRDropdownState*)component->custom_data;

            // Free old options if they exist
            if (state->options) {
                for (uint32_t i = 0; i < state->option_count; i++) {
                    free(state->options[i]);
                }
                free(state->options);
            }

            // Copy new options
            state->option_count = value->array.count;
            state->options = (char**)malloc(sizeof(char*) * value->array.count);
            for (size_t i = 0; i < value->array.count; i++) {
                KryValue* elem = value->array.elements[i];
                if (elem->type == KRY_VALUE_STRING) {
                    state->options[i] = strdup(elem->string_value);
                } else {
                    state->options[i] = strdup(""); // Default to empty string
                }
            }
            return;
        }
    }

    // Default: ignore unknown properties
}

// ============================================================================
// AST to IR Conversion
// ============================================================================

// Forward declarations
static IRComponent* convert_node(ConversionContext* ctx, KryNode* node);
static bool is_custom_component(const char* name, IRReactiveManifest* manifest);
static IRComponent* expand_component_template(const char* comp_name,
                                                IRReactiveManifest* manifest,
                                                const char* instance_scope);
static IRComponent* ir_component_clone_tree(IRComponent* source);

// Expand for loop by unrolling at compile time
static void expand_for_loop(ConversionContext* ctx, IRComponent* parent, KryNode* for_node) {
    fprintf(stderr, "[DEBUG_FORLOOP] Called expand_for_loop\n");
    if (!for_node || for_node->type != KRY_NODE_FOR_LOOP) {
        fprintf(stderr, "[DEBUG_FORLOOP] Early return: for_node=%p, type=%u\n",
                (void*)for_node, for_node ? (uint32_t)for_node->type : 0xFFFFFFFFu);
        return;
    }

    const char* iter_name = for_node->name;  // Iterator variable name
    KryValue* collection = for_node->value;  // Collection to iterate over

    if (!collection || !iter_name) {
        fprintf(stderr, "[DEBUG_FORLOOP] Early return: collection=%p, iter_name=%s\n",
                (void*)collection, iter_name ? iter_name : "NULL");
        return;
    }

    // HYBRID MODE: Preserve for loop template (ONLY if inside static block)
    // NOTE: For loops outside static blocks are runtime reactive (IRReactiveForLoop)

    // Track expanded component IDs for source structures
    IRForLoopData* loop_data = NULL;
    uint32_t* expanded_ids = NULL;
    size_t expanded_count = 0;
    size_t expanded_capacity = 0;

    // ALSO expand for runtime execution (current behavior)
    // Handle different collection types
    if (collection->type == KRY_VALUE_ARRAY) {
        // HYBRID MODE: Create loop template metadata (only when we have the actual array)
        if (ctx->compile_mode == IR_COMPILE_MODE_HYBRID && ctx->current_static_block_id != NULL) {
            // Get collection reference as string
            const char* collection_ref = NULL;
            // If collection is an identifier, use its name; otherwise use a generic name
            if (collection->type == KRY_VALUE_IDENTIFIER) {
                collection_ref = collection->identifier;
            } else {
                collection_ref = "items";  // Fallback for array literals
            }

            // Create template component (convert first child WITHOUT parameter substitution)
            IRComponent* template_comp = NULL;
            if (for_node->first_child && for_node->first_child->type == KRY_NODE_COMPONENT) {
                template_comp = convert_node(ctx, for_node->first_child);
            }

            loop_data = ir_source_structures_add_for_loop(
                ctx->source_structures,
                ctx->current_static_block_id,
                iter_name,
                collection_ref,
                template_comp  // Store template component
            );

            // Initialize expanded IDs array
            expanded_capacity = 16;  // Initial capacity
            expanded_ids = (uint32_t*)malloc(expanded_capacity * sizeof(uint32_t));
        }
        // Iterate over array elements
        fprintf(stderr, "[DEBUG_FORLOOP] Starting array iteration: count=%zu\n", collection->array.count);
        for (size_t i = 0; i < collection->array.count; i++) {
            fprintf(stderr, "[DEBUG_FORLOOP]   Iteration %zu/%zu\n", i + 1, collection->array.count);
            KryValue* element = collection->array.elements[i];

            // Create new context with iterator variable bound
            ConversionContext loop_ctx = *ctx;

            // Track malloc'd strings for cleanup after iteration
            char* allocated_strings[MAX_PARAMS * 2];
            int alloc_count = 0;

            // For object elements, make properties accessible via dot notation
            // e.g., if iterator is "item" and object has "name" property,
            // store "item.name" → value

            // Convert element to string for simple cases
            if (element->type == KRY_VALUE_STRING) {
                add_param(&loop_ctx, iter_name, element->string_value);
            } else if (element->type == KRY_VALUE_NUMBER) {
                // FIX: Allocate on heap instead of stack
                char* num_buf = (char*)malloc(32);
                allocated_strings[alloc_count++] = num_buf;
                snprintf(num_buf, 32, "%g", element->number_value);
                add_param(&loop_ctx, iter_name, num_buf);
            } else if (element->type == KRY_VALUE_IDENTIFIER) {
                add_param(&loop_ctx, iter_name, element->identifier);
            } else if (element->type == KRY_VALUE_OBJECT) {
                // Store object properties as nested parameters
                // e.g., if iterator is "item" and object has "name" property,
                // store "item.name" → value
                for (size_t j = 0; j < element->object.count; j++) {
                    char param_name_buf[256];
                    snprintf(param_name_buf, sizeof(param_name_buf), "%s.%s", iter_name, element->object.keys[j]);
                    char* param_name = (char*)malloc(strlen(param_name_buf) + 1);
                    allocated_strings[alloc_count++] = param_name;  // Track for cleanup
                    strcpy(param_name, param_name_buf);

                    KryValue* prop_value = element->object.values[j];
                    if (prop_value->type == KRY_VALUE_STRING) {
                        add_param(&loop_ctx, param_name, prop_value->string_value);
                    } else if (prop_value->type == KRY_VALUE_NUMBER) {
                        char num_buf_tmp[32];
                        snprintf(num_buf_tmp, sizeof(num_buf_tmp), "%g", prop_value->number_value);
                        char* num_buf = (char*)malloc(strlen(num_buf_tmp) + 1);
                        allocated_strings[alloc_count++] = num_buf;  // Track for cleanup
                        strcpy(num_buf, num_buf_tmp);
                        add_param(&loop_ctx, param_name, num_buf);
                    } else if (prop_value->type == KRY_VALUE_IDENTIFIER) {
                        add_param(&loop_ctx, param_name, prop_value->identifier);
                    } else if (prop_value->type == KRY_VALUE_ARRAY) {
                        // Handle nested arrays (e.g., item.colors[0])
                        for (size_t k = 0; k < prop_value->array.count; k++) {
                            char array_param_name_buf[256];
                            snprintf(array_param_name_buf, sizeof(array_param_name_buf), "%s.%s[%zu]",
                                     iter_name, element->object.keys[j], k);
                            char* array_param_name = (char*)malloc(strlen(array_param_name_buf) + 1);
                            allocated_strings[alloc_count++] = array_param_name;  // Track for cleanup
                            strcpy(array_param_name, array_param_name_buf);

                            KryValue* array_elem = prop_value->array.elements[k];
                            if (array_elem->type == KRY_VALUE_STRING) {
                                add_param(&loop_ctx, array_param_name, array_elem->string_value);
                            } else if (array_elem->type == KRY_VALUE_NUMBER) {
                                char num_buf_tmp[32];
                                snprintf(num_buf_tmp, sizeof(num_buf_tmp), "%g", array_elem->number_value);
                                char* num_buf = (char*)malloc(strlen(num_buf_tmp) + 1);
                                allocated_strings[alloc_count++] = num_buf;  // Track for cleanup
                                strcpy(num_buf, num_buf_tmp);
                                add_param(&loop_ctx, array_param_name, num_buf);
                            } else if (array_elem->type == KRY_VALUE_IDENTIFIER) {
                                add_param(&loop_ctx, array_param_name, array_elem->identifier);
                            }
                        }
                    }
                }
            }

            // Convert loop body children with the loop context
            KryNode* loop_body_child = for_node->first_child;
            fprintf(stderr, "[DEBUG_FORLOOP]   Converting loop body (first_child=%p)\n", (void*)loop_body_child);
            while (loop_body_child) {
                fprintf(stderr, "[DEBUG_FORLOOP]     loop_body_child type=%d (COMPONENT=%d)\n",
                        loop_body_child->type, KRY_NODE_COMPONENT);
                if (loop_body_child->type == KRY_NODE_COMPONENT) {
                    IRComponent* child_component = convert_node(&loop_ctx, loop_body_child);
                    fprintf(stderr, "[DEBUG_FORLOOP]     child_component=%p\n", (void*)child_component);
                    if (child_component) {
                        fprintf(stderr, "[DEBUG_FORLOOP]     Adding child (id=%u) to parent (id=%u, child_count=%d)\n",
                                child_component->id, parent->id, parent->child_count);
                        ir_add_child(parent, child_component);
                        fprintf(stderr, "[DEBUG_FORLOOP]     After add: parent->child_count=%d\n", parent->child_count);

                        // Track component ID for source structures
                        if (loop_data && expanded_ids) {
                            if (expanded_count >= expanded_capacity) {
                                expanded_capacity *= 2;
                                expanded_ids = (uint32_t*)realloc(expanded_ids, expanded_capacity * sizeof(uint32_t));
                            }
                            expanded_ids[expanded_count++] = child_component->id;
                        }
                    }
                }
                loop_body_child = loop_body_child->next_sibling;
            }

            // Clean up allocated strings for this iteration
            for (int j = 0; j < alloc_count; j++) {
                free(allocated_strings[j]);
            }
        }
    } else if (collection->type == KRY_VALUE_IDENTIFIER || collection->type == KRY_VALUE_EXPRESSION) {
        // Try to resolve the collection from context
        const char* collection_name = (collection->type == KRY_VALUE_IDENTIFIER)
            ? collection->identifier
            : collection->expression;

        fprintf(stderr, "[DEBUG_FORLOOP] Collection is IDENTIFIER/EXPRESSION: '%s'\n", collection_name);
        fprintf(stderr, "[DEBUG_FORLOOP] Searching in %d params...\n", ctx->param_count);

        // Look up the variable in context
        for (int i = 0; i < ctx->param_count; i++) {
            fprintf(stderr, "[DEBUG_FORLOOP]   param[%d]: name='%s', kry_value=%p\n",
                    i, ctx->params[i].name, (void*)ctx->params[i].kry_value);
            if (strcmp(ctx->params[i].name, collection_name) == 0) {
                fprintf(stderr, "[DEBUG_FORLOOP]   FOUND match!\n");
                // Found the variable - check if it's a KryValue
                if (ctx->params[i].kry_value != NULL) {
                    fprintf(stderr, "[DEBUG_FORLOOP]   kry_value type=%d (ARRAY=%d)\n",
                            ctx->params[i].kry_value->type, KRY_VALUE_ARRAY);
                    // Recursively call expand_for_loop with the resolved KryValue
                    KryNode temp_for_node = *for_node;
                    temp_for_node.value = ctx->params[i].kry_value;
                    expand_for_loop(ctx, parent, &temp_for_node);
                    return;
                } else {
                    fprintf(stderr, "[DEBUG_FORLOOP]   ERROR: kry_value is NULL!\n");
                }
                break;
            }
        }
        fprintf(stderr, "[DEBUG_FORLOOP] Variable '%s' NOT FOUND in context\n", collection_name);
    }

    // Update loop_data with expanded component IDs
    if (loop_data && expanded_ids && expanded_count > 0) {
        loop_data->expanded_component_ids = expanded_ids;
        loop_data->expanded_count = (uint32_t)expanded_count;
    } else if (expanded_ids) {
        // Clean up if we didn't use the array
        free(expanded_ids);
    }
}

// Convert "for each" loop to ForEach IR component
// This creates an IR_COMPONENT_FOR_EACH with foreach_def metadata
// Unlike "for" loops, "for each" does NOT expand at compile time
static IRComponent* convert_for_each_node(ConversionContext* ctx, KryNode* for_each_node) {
    fprintf(stderr, "[DEBUG_FOR_EACH] Called convert_for_each_node\n");
    if (!for_each_node || for_each_node->type != KRY_NODE_FOR_EACH) {
        fprintf(stderr, "[DEBUG_FOR_EACH] Early return: invalid node type\n");
        return NULL;
    }

    const char* item_name = for_each_node->name;  // Iterator variable name
    KryValue* collection = for_each_node->value;  // Collection to iterate over

    if (!collection || !item_name) {
        fprintf(stderr, "[DEBUG_FOR_EACH] Early return: missing collection or item_name\n");
        return NULL;
    }

    // Get collection reference string
    const char* collection_ref = NULL;
    if (collection->type == KRY_VALUE_IDENTIFIER) {
        collection_ref = collection->identifier;
    } else if (collection->type == KRY_VALUE_EXPRESSION) {
        collection_ref = collection->expression;
    } else {
        kry_parser_error(NULL, "for each collection must be an identifier or expression");
        return NULL;
    }

    // Create ForEach component
    IRComponent* for_each_comp = ir_create_component(IR_COMPONENT_FOR_EACH);
    if (!for_each_comp) {
        fprintf(stderr, "[DEBUG_FOR_EACH] Failed to create ForEach component\n");
        return NULL;
    }

    // Create ForEach definition
    struct IRForEachDef* def = ir_foreach_def_create(item_name, "index");
    if (!def) {
        fprintf(stderr, "[DEBUG_FOR_EACH] Failed to create ForEachDef\n");
        // Note: ir_component_destroy may not be available, leak is acceptable for error case
        return NULL;
    }

    // Set data source
    ir_foreach_set_source_variable(def, collection_ref);

    // Convert the loop body (first child should be a component)
    // This becomes the template for each iteration
    if (for_each_node->first_child && for_each_node->first_child->type == KRY_NODE_COMPONENT) {
        KryNode* template_node = for_each_node->first_child;

        // Create a child conversion context without parameter substitution
        // We want to preserve the template properties as expressions
        ConversionContext template_ctx = *ctx;
        template_ctx.param_count = 0;  // Clear params for template

        IRComponent* template_comp = convert_node(&template_ctx, template_node);
        if (template_comp) {
            ir_foreach_set_template(def, template_comp);

            // Extract property bindings from the template
            // Scan for expressions that reference the item variable
            if (template_comp->text_expression &&
                strstr(template_comp->text_expression, item_name) != NULL) {
                // Text content references item
                ir_foreach_add_binding(def, "text", template_comp->text_expression, true);
            }
        }
    }

    // Attach foreach_def to the component
    for_each_comp->foreach_def = def;

    fprintf(stderr, "[DEBUG_FOR_EACH] Created ForEach component: item=%s, source=%s\n",
            item_name, collection_ref);

    return for_each_comp;
}

static IRComponent* convert_node(ConversionContext* ctx, KryNode* node) {
    if (!node) return NULL;

    if (node->type == KRY_NODE_COMPONENT) {
        // Check if this is a custom component instantiation
        if (ctx->manifest && is_custom_component(node->name, ctx->manifest)) {
            fprintf(stderr, "[CUSTOM_COMPONENT] Found custom component instantiation: %s\n", node->name);
            fflush(stderr);

            // Generate unique instance ID
            static uint32_t instance_counter = 0;
            char instance_scope[128];
            snprintf(instance_scope, sizeof(instance_scope),
                     "%s#%u", node->name, instance_counter++);

            // Expand component template
            IRComponent* instance = expand_component_template(
                node->name,
                ctx->manifest,
                instance_scope
            );

            if (instance) {
                fprintf(stderr, "[CUSTOM_COMPONENT] Successfully expanded %s (instance %d)\n",
                        node->name, instance_counter - 1);
                fflush(stderr);

                // Apply instance arguments to initialize state variables
                // Find the component definition
                IRComponentDefinition* def = NULL;
                for (uint32_t i = 0; i < ctx->manifest->component_def_count; i++) {
                    if (strcmp(ctx->manifest->component_defs[i].name, node->name) == 0) {
                        def = &ctx->manifest->component_defs[i];
                        break;
                    }
                }

                if (node->arguments && def) {
                    fprintf(stderr, "[CUSTOM_COMPONENT] Applying arguments: '%s'\n", node->arguments);
                    fflush(stderr);
                    {
                        // Parse argument - for now support simple positional or named syntax
                        // "5" -> first positional arg
                        // "initialValue = 10" -> named arg
                        char* arg_copy = strdup(node->arguments);
                        char* equals = strchr(arg_copy, '=');

                        char* arg_value = NULL;
                        if (equals) {
                            // Named argument: "initialValue = 10"
                            arg_value = equals + 1;
                            // Trim whitespace
                            while (*arg_value == ' ') arg_value++;
                        } else {
                            // Positional argument: "5"
                            arg_value = arg_copy;
                            // Trim whitespace
                            while (*arg_value == ' ') arg_value++;
                        }

                        // Apply to state variables that reference this prop
                        for (uint32_t i = 0; i < def->state_var_count; i++) {
                            IRComponentStateVar* state_var = &def->state_vars[i];
                            // If the initial_expr is the prop name, substitute with actual value
                            if (state_var->initial_expr && equals) {
                                // Named arg: check if initial_expr matches prop name
                                char* prop_name_start = arg_copy;
                                *equals = '\0'; // Temporarily terminate
                                // Trim whitespace from prop name
                                char* prop_name_end = equals - 1;
                                while (prop_name_end > prop_name_start && *prop_name_end == ' ') {
                                    *prop_name_end = '\0';
                                    prop_name_end--;
                                }

                                if (strcmp(state_var->initial_expr, prop_name_start) == 0) {
                                    fprintf(stderr, "[CUSTOM_COMPONENT] Registering %s.%s with initial value %s\n",
                                            instance_scope, state_var->name, arg_value);
                                    fflush(stderr);

                                    // Register state variable in manifest with the actual value
                                    IRReactiveValue initial_value;
                                    initial_value.as_int = atoi(arg_value);

                                    uint32_t var_id = ir_reactive_manifest_add_var(
                                        ctx->manifest,
                                        state_var->name,
                                        IR_REACTIVE_TYPE_INT,
                                        initial_value
                                    );

                                    // Set metadata including scope
                                    ir_reactive_manifest_set_var_metadata(
                                        ctx->manifest,
                                        var_id,
                                        "int",
                                        arg_value,
                                        instance_scope
                                    );
                                }
                                *equals = '='; // Restore
                            } else if (state_var->initial_expr) {
                                // Positional arg - just use the value directly
                                fprintf(stderr, "[CUSTOM_COMPONENT] Registering %s.%s with initial value %s (positional)\n",
                                        instance_scope, state_var->name, arg_value);
                                fflush(stderr);

                                IRReactiveValue initial_value;
                                initial_value.as_int = atoi(arg_value);

                                uint32_t var_id = ir_reactive_manifest_add_var(
                                    ctx->manifest,
                                    state_var->name,
                                    IR_REACTIVE_TYPE_INT,
                                    initial_value
                                );

                                ir_reactive_manifest_set_var_metadata(
                                    ctx->manifest,
                                    var_id,
                                    "int",
                                    arg_value,
                                    instance_scope
                                );
                            }
                        }

                        free(arg_copy);
                    }
                } else if (def) {
                    // No arguments provided - use default values
                    fprintf(stderr, "[CUSTOM_COMPONENT] No arguments, using defaults\n");
                    fflush(stderr);

                    for (uint32_t i = 0; i < def->state_var_count; i++) {
                        IRComponentStateVar* state_var = &def->state_vars[i];
                        // For Counter with no args, default is 0
                        fprintf(stderr, "[CUSTOM_COMPONENT] Registering %s.%s with default value 0\n",
                                instance_scope, state_var->name);
                        fflush(stderr);

                        IRReactiveValue initial_value;
                        initial_value.as_int = 0;

                        uint32_t var_id = ir_reactive_manifest_add_var(
                            ctx->manifest,
                            state_var->name,
                            IR_REACTIVE_TYPE_INT,
                            initial_value
                        );

                        ir_reactive_manifest_set_var_metadata(
                            ctx->manifest,
                            var_id,
                            "int",
                            "0",
                            instance_scope
                        );
                    }
                }

                return instance;
            }

            fprintf(stderr, "[CUSTOM_COMPONENT] Failed to expand %s\n", node->name);
            fflush(stderr);
            // If expansion failed, fall through to create regular container
        }

        IRComponentType type = get_component_type(node->name);

        // Create component (built-in or fallback container)
        IRComponent* component = ir_create_component(type);
        if (!component) return NULL;

        // Process children
        KryNode* child = node->first_child;
        while (child) {
            if (child->type == KRY_NODE_PROPERTY) {
                // Apply property
                printf("[CONVERT_NODE] Applying property '%s', value=%p, value->type=%u\n",
                       child->name, (void*)child->value, child->value ? (uint32_t)child->value->type : 0xFFFFFFFFu);
                fflush(stdout);
                apply_property(ctx, component, child->name, child->value);
                printf("[CONVERT_NODE] After apply_property for '%s'\n", child->name);
                fflush(stdout);
            } else if (child->type == KRY_NODE_COMPONENT) {
                // Recursively convert child component
                IRComponent* child_component = convert_node(ctx, child);
                if (child_component) {
                    ir_add_child(component, child_component);
                }
            } else if (child->type == KRY_NODE_STATE && ctx->manifest) {
                // Handle state declaration: state value: int = initialValue
                // Add to reactive manifest
                const char* var_name = child->name;
                const char* init_value = "0";  // Default value

                // Get initial value from the substitution context or parse it
                if (child->value) {
                    if (child->value->type == KRY_VALUE_NUMBER) {
                        // Direct number value
                        static char num_buf[32];
                        snprintf(num_buf, sizeof(num_buf), "%g", child->value->number_value);
                        init_value = num_buf;
                    } else if (child->value->type == KRY_VALUE_STRING) {
                        init_value = child->value->string_value;
                    } else if (child->value->type == KRY_VALUE_IDENTIFIER ||
                               child->value->type == KRY_VALUE_EXPRESSION) {
                        // Try to resolve from parameter substitutions
                        const char* expr = (child->value->type == KRY_VALUE_IDENTIFIER)
                            ? child->value->identifier
                            : child->value->expression;
                        const char* resolved = substitute_param(ctx, expr);
                        init_value = resolved;
                    }
                }

                // Add variable to reactive manifest
                // Parse type from state declaration (child->state_type)
                IRReactiveVarType ir_type = IR_REACTIVE_TYPE_INT;  // Default
                const char* type_str = "int";

                if (child->state_type) {
                    if (strcmp(child->state_type, "string") == 0) {
                        ir_type = IR_REACTIVE_TYPE_STRING;
                        type_str = "string";
                    } else if (strcmp(child->state_type, "float") == 0 ||
                               strcmp(child->state_type, "double") == 0 ||
                               strcmp(child->state_type, "number") == 0) {
                        ir_type = IR_REACTIVE_TYPE_FLOAT;
                        type_str = child->state_type;
                    } else if (strcmp(child->state_type, "bool") == 0 ||
                               strcmp(child->state_type, "boolean") == 0) {
                        ir_type = IR_REACTIVE_TYPE_BOOL;
                        type_str = child->state_type;
                    } else {
                        // Default to int for unknown types
                        type_str = child->state_type;
                    }
                }

                IRReactiveValue react_value = {.as_int = 0};  // Default value
                if (init_value) {
                    switch (ir_type) {
                        case IR_REACTIVE_TYPE_INT:
                            react_value.as_int = atoi(init_value);
                            break;
                        case IR_REACTIVE_TYPE_FLOAT:
                            react_value.as_float = atof(init_value);
                            break;
                        case IR_REACTIVE_TYPE_BOOL:
                            react_value.as_bool = (strcmp(init_value, "true") == 0);
                            break;
                        case IR_REACTIVE_TYPE_STRING:
                            react_value.as_string = (char*)init_value;
                            break;
                        default:
                            react_value.as_int = atoi(init_value);
                            break;
                    }
                }

                uint32_t var_id = ir_reactive_manifest_add_var(
                    ctx->manifest,
                    var_name,
                    ir_type,
                    react_value
                );

                // Set metadata for proper KIR serialization
                if (var_id > 0) {
                    ir_reactive_manifest_set_var_metadata(
                        ctx->manifest,
                        var_id,
                        type_str,
                        init_value,
                        "component"  // State variables are component-scoped
                    );
                }
            } else if (child->type == KRY_NODE_VAR_DECL) {
                // Handle variable declaration (const/let/var)
                // Store in context for later substitution
                const char* var_name = child->name;
                const char* var_value = NULL;

                // Get value as string for substitution
                if (child->value) {
                    if (child->value->type == KRY_VALUE_STRING) {
                        var_value = child->value->string_value;
                    } else if (child->value->type == KRY_VALUE_NUMBER) {
                        static char num_buf[32];
                        snprintf(num_buf, sizeof(num_buf), "%g", child->value->number_value);
                        var_value = num_buf;
                    } else if (child->value->type == KRY_VALUE_IDENTIFIER) {
                        var_value = child->value->identifier;
                    } else if (child->value->type == KRY_VALUE_EXPRESSION) {
                        var_value = child->value->expression;
                    } else if (child->value->type == KRY_VALUE_ARRAY || child->value->type == KRY_VALUE_OBJECT) {
                        // For arrays and objects, store the full KryValue in the context
                        add_param_value(ctx, var_name, child->value);
                    }
                }

                if (var_value) {
                    add_param(ctx, var_name, var_value);
                }
            } else if (child->type == KRY_NODE_STATIC_BLOCK) {
                // Static block: expand children at compile time

                // HYBRID MODE: Create static block metadata and set context
                char* static_block_id = NULL;
                if (ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    // Generate unique static block ID
                    static char id_buf[64];
                    snprintf(id_buf, sizeof(id_buf), "static_%u", ctx->static_block_counter++);
                    static_block_id = strdup(id_buf);

                    // Create static block data
                    IRStaticBlockData* static_data = ir_source_structures_add_static_block(
                        ctx->source_structures,
                        component->id  // Parent component ID
                    );

                    if (static_data) {
                        static_data->id = static_block_id;
                    }

                    // Set current static block context
                    ctx->current_static_block_id = static_block_id;
                }

                KryNode* static_child = child->first_child;
                while (static_child) {
                    if (static_child->type == KRY_NODE_COMPONENT) {
                        IRComponent* child_component = convert_node(ctx, static_child);
                        if (child_component) {
                            ir_add_child(component, child_component);
                        }
                    } else if (static_child->type == KRY_NODE_FOR_LOOP) {
                        // Handle for loop expansion (unrolling)
                        expand_for_loop(ctx, component, static_child);
                    } else if (static_child->type == KRY_NODE_FOR_EACH) {
                        // Handle for each - convert to ForEach IR component
                        IRComponent* for_each_comp = convert_for_each_node(ctx, static_child);
                        if (for_each_comp) {
                            ir_add_child(component, for_each_comp);
                        }
                    } else if (static_child->type == KRY_NODE_VAR_DECL) {
                        // Process variable declaration in static context
                        const char* var_name = static_child->name;
                        const char* var_value = NULL;

                        // HYBRID MODE: Preserve variable declaration
                        if (ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                            // Parse var type from AST (const/let/var)
                            const char* var_type = static_child->var_type ? static_child->var_type : "var";

                            // Convert value to JSON string for preservation
                            const char* value_json = NULL;
                            static char* json_buffer_ptr = NULL;  // For dynamically allocated JSON
                            if (static_child->value) {
                                // Use the kry_value_to_json helper for all complex types
                                if (static_child->value->type == KRY_VALUE_ARRAY ||
                                    static_child->value->type == KRY_VALUE_OBJECT) {
                                    json_buffer_ptr = kry_value_to_json(static_child->value);
                                    value_json = json_buffer_ptr;
                                } else if (static_child->value->type == KRY_VALUE_STRING) {
                                    // Simple string values can use a static buffer
                                    static char str_buf[2048];
                                    snprintf(str_buf, sizeof(str_buf), "\"%s\"", static_child->value->string_value);
                                    value_json = str_buf;
                                } else if (static_child->value->type == KRY_VALUE_NUMBER) {
                                    static char num_buf[64];
                                    snprintf(num_buf, sizeof(num_buf), "%g", static_child->value->number_value);
                                    value_json = num_buf;
                                } else if (static_child->value->type == KRY_VALUE_IDENTIFIER) {
                                    value_json = static_child->value->identifier;
                                } else if (static_child->value->type == KRY_VALUE_EXPRESSION) {
                                    value_json = static_child->value->expression;
                                }
                            }

                            // Add variable declaration to source structures
                            ir_source_structures_add_var_decl(
                                ctx->source_structures,
                                var_name,
                                var_type,
                                value_json,
                                static_block_id  // Scope: this static block
                            );

                            // Clean up dynamically allocated JSON
                            if (json_buffer_ptr) {
                                free(json_buffer_ptr);
                            }
                        }

                        if (static_child->value) {
                            if (static_child->value->type == KRY_VALUE_STRING) {
                                var_value = static_child->value->string_value;
                            } else if (static_child->value->type == KRY_VALUE_NUMBER) {
                                static char num_buf[32];
                                snprintf(num_buf, sizeof(num_buf), "%g", static_child->value->number_value);
                                var_value = num_buf;
                            } else if (static_child->value->type == KRY_VALUE_IDENTIFIER) {
                                var_value = static_child->value->identifier;
                            } else if (static_child->value->type == KRY_VALUE_EXPRESSION) {
                                var_value = static_child->value->expression;
                            } else if (static_child->value->type == KRY_VALUE_ARRAY ||
                                       static_child->value->type == KRY_VALUE_OBJECT) {
                                // For arrays and objects, store the full KryValue
                                add_param_value(ctx, var_name, static_child->value);
                                var_value = NULL;  // Don't add string value
                            }
                        }

                        if (var_value) {
                            add_param(ctx, var_name, var_value);
                        }
                    }

                    static_child = static_child->next_sibling;
                }

                // HYBRID MODE: Clear static block context
                if (ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                    ctx->current_static_block_id = NULL;
                    // NOTE: Don't free static_block_id here - ownership transferred to IRStaticBlockData
                    // It will be freed when source_structures is destroyed
                }
            } else if (child->type == KRY_NODE_STYLE_BLOCK) {
                // Style block: add rule to global stylesheet
                const char* selector = child->name;  // Selector is stored in name
                if (!selector) {
                    child = child->next_sibling;
                    continue;
                }

                // Ensure stylesheet exists in global context
                if (g_ir_context && !g_ir_context->stylesheet) {
                    g_ir_context->stylesheet = ir_stylesheet_create();
                }

                if (g_ir_context && g_ir_context->stylesheet) {
                    // Build IRStyleProperties from child properties
                    IRStyleProperties props;
                    ir_style_properties_init(&props);

                    // Iterate through property children
                    KryNode* prop_child = child->first_child;
                    while (prop_child) {
                        if (prop_child->type == KRY_NODE_PROPERTY && prop_child->name && prop_child->value) {
                            const char* prop_name = prop_child->name;
                            KryValue* val = prop_child->value;

                            // Convert property to IRStyleProperties
                            if (strcmp(prop_name, "backgroundColor") == 0 || strcmp(prop_name, "background") == 0) {
                                if (val->type == KRY_VALUE_STRING) {
                                    IRColor color;
                                    if (ir_css_parse_color(val->string_value, &color)) {
                                        props.background = color;
                                        props.set_flags |= IR_PROP_BACKGROUND;
                                    }
                                }
                            } else if (strcmp(prop_name, "color") == 0) {
                                if (val->type == KRY_VALUE_STRING) {
                                    IRColor color;
                                    if (ir_css_parse_color(val->string_value, &color)) {
                                        props.color = color;
                                        props.set_flags |= IR_PROP_COLOR;
                                    }
                                }
                            } else if (strcmp(prop_name, "display") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* display_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(display_val, "flex") == 0) {
                                        props.display = IR_LAYOUT_MODE_FLEX;
                                    } else if (strcmp(display_val, "grid") == 0) {
                                        props.display = IR_LAYOUT_MODE_GRID;
                                    } else if (strcmp(display_val, "block") == 0) {
                                        props.display = IR_LAYOUT_MODE_BLOCK;
                                    }
                                    // Note: "none" is handled via visible property, not layout mode
                                    props.display_explicit = true;
                                    props.set_flags |= IR_PROP_DISPLAY;
                                }
                            } else if (strcmp(prop_name, "flexDirection") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* dir_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(dir_val, "row") == 0) {
                                        props.flex_direction = 1;
                                    } else {
                                        props.flex_direction = 0;  // column
                                    }
                                    props.set_flags |= IR_PROP_FLEX_DIRECTION;
                                }
                            } else if (strcmp(prop_name, "justifyContent") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* jc_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(jc_val, "center") == 0) {
                                        props.justify_content = IR_ALIGNMENT_CENTER;
                                    } else if (strcmp(jc_val, "spaceBetween") == 0 || strcmp(jc_val, "space-between") == 0) {
                                        props.justify_content = IR_ALIGNMENT_SPACE_BETWEEN;
                                    } else if (strcmp(jc_val, "spaceAround") == 0 || strcmp(jc_val, "space-around") == 0) {
                                        props.justify_content = IR_ALIGNMENT_SPACE_AROUND;
                                    } else if (strcmp(jc_val, "flexEnd") == 0 || strcmp(jc_val, "flex-end") == 0) {
                                        props.justify_content = IR_ALIGNMENT_END;
                                    } else {
                                        props.justify_content = IR_ALIGNMENT_START;
                                    }
                                    props.set_flags |= IR_PROP_JUSTIFY_CONTENT;
                                }
                            } else if (strcmp(prop_name, "alignItems") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* ai_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(ai_val, "center") == 0) {
                                        props.align_items = IR_ALIGNMENT_CENTER;
                                    } else if (strcmp(ai_val, "flexEnd") == 0 || strcmp(ai_val, "flex-end") == 0) {
                                        props.align_items = IR_ALIGNMENT_END;
                                    } else if (strcmp(ai_val, "stretch") == 0) {
                                        props.align_items = IR_ALIGNMENT_STRETCH;
                                    } else {
                                        props.align_items = IR_ALIGNMENT_START;
                                    }
                                    props.set_flags |= IR_PROP_ALIGN_ITEMS;
                                }
                            } else if (strcmp(prop_name, "padding") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    float p = (float)val->number_value;
                                    props.padding.top = props.padding.right = props.padding.bottom = props.padding.left = p;
                                    props.set_flags |= IR_PROP_PADDING;
                                }
                            } else if (strcmp(prop_name, "margin") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    float m = (float)val->number_value;
                                    props.margin.top = props.margin.right = props.margin.bottom = props.margin.left = m;
                                    props.set_flags |= IR_PROP_MARGIN;
                                }
                            } else if (strcmp(prop_name, "gap") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.gap = (float)val->number_value;
                                    props.set_flags |= IR_PROP_GAP;
                                }
                            } else if (strcmp(prop_name, "fontSize") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.font_size.type = IR_DIMENSION_PX;
                                    props.font_size.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_FONT_SIZE;
                                }
                            } else if (strcmp(prop_name, "borderRadius") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.border_radius = (uint8_t)val->number_value;
                                    props.set_flags |= IR_PROP_BORDER_RADIUS;
                                }
                            } else if (strcmp(prop_name, "width") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.width.type = val->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
                                    props.width.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_WIDTH;
                                }
                            } else if (strcmp(prop_name, "height") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.height.type = val->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
                                    props.height.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_HEIGHT;
                                }
                            } else if (strcmp(prop_name, "opacity") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.opacity = (float)val->number_value;
                                    props.set_flags |= IR_PROP_OPACITY;
                                }
                            }
                        }
                        prop_child = prop_child->next_sibling;
                    }

                    // Add rule to stylesheet
                    if (props.set_flags != 0) {
                        ir_stylesheet_add_rule(g_ir_context->stylesheet, selector, &props);
                    }

                    ir_style_properties_cleanup(&props);
                }
            } else if (child->type == KRY_NODE_FOR_LOOP) {
                // For loop outside static block - expand/unroll
                expand_for_loop(ctx, component, child);
            } else if (child->type == KRY_NODE_FOR_EACH) {
                // For each - convert to ForEach IR component
                IRComponent* for_each_comp = convert_for_each_node(ctx, child);
                if (for_each_comp) {
                    ir_add_child(component, for_each_comp);
                }
            } else if (child->type == KRY_NODE_IF) {
                // Runtime conditional rendering support
                // Render BOTH branches and mark each with visibility condition

                const char* condition_var = NULL;

                // Extract condition variable name
                if (child->value && child->value->type == KRY_VALUE_IDENTIFIER) {
                    condition_var = child->value->identifier;
                }

                // Render then branch (visible when condition is true)
                KryNode* then_child = child->first_child;
                while (then_child) {
                    if (then_child->type == KRY_NODE_COMPONENT) {
                        IRComponent* child_component = convert_node(ctx, then_child);
                        if (child_component && condition_var) {
                            // Mark component as conditionally visible
                            child_component->visible_condition = strdup(condition_var);
                            child_component->visible_when_true = true;
                            ir_add_child(component, child_component);
                        } else if (child_component) {
                            // No condition variable, always visible
                            ir_add_child(component, child_component);
                        }
                    }
                    then_child = then_child->next_sibling;
                }

                // Render else branch (visible when condition is false)
                if (child->else_branch) {
                    KryNode* else_child = child->else_branch->first_child;
                    while (else_child) {
                        if (else_child->type == KRY_NODE_COMPONENT) {
                            IRComponent* child_component = convert_node(ctx, else_child);
                            if (child_component && condition_var) {
                                // Mark component as conditionally visible (opposite condition)
                                child_component->visible_condition = strdup(condition_var);
                                child_component->visible_when_true = false;  // Visible when FALSE
                                ir_add_child(component, child_component);
                            } else if (child_component) {
                                // No condition variable, always visible
                                ir_add_child(component, child_component);
                            }
                        }
                        else_child = else_child->next_sibling;
                    }
                }
            } else if (child->type == KRY_NODE_CODE_BLOCK) {
                // Code block (@lua, @js) - add to logic block
                if (ctx->logic_block && child->code_language && child->code_source) {
                    // Generate a unique function name for this code block
                    char func_name[256];
                    static int code_block_counter = 0;
                    snprintf(func_name, sizeof(func_name), "_code_block_%d", code_block_counter++);

                    // Create logic function to hold the source code
                    IRLogicFunction* func = ir_logic_function_create(func_name);
                    if (func) {
                        // Direct @lua or @js block - just add the source
                        ir_logic_function_add_source(func, child->code_language, child->code_source);
                        printf("[KRY_TO_IR] Added code block to logic: language='%s', func='%s'\n",
                               child->code_language, func_name);

                        // Add function to logic block
                        ir_logic_block_add_function(ctx->logic_block, func);
                    }
                }
            }

            child = child->next_sibling;
        }

        return component;
    }

    return NULL;
}

// ============================================================================
// Public API
// ============================================================================

IRComponent* ir_kry_parse(const char* source, size_t length) {
    if (!source) return NULL;

    // Create parser
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) return NULL;

    // Parse to AST
    KryNode* ast = kry_parse(parser);

    if (!ast || parser->has_error) {
        if (parser->has_error) {
            fprintf(stderr, "Parse error at %u:%u: %s\n",
                    parser->error_line, parser->error_column,
                    parser->error_message ? parser->error_message : "Unknown error");
        }
        kry_parser_free(parser);
        return NULL;
    }

    // Find the root application (skip component definitions)
    KryNode* root_node = ast;
    while (root_node) {
        if (!root_node->is_component_definition) {
            // Found the actual root application
            break;
        }
        root_node = root_node->next_sibling;
    }

    if (!root_node) {
        // No root application found, only component definitions
        fprintf(stderr, "Error: No root application found (only component definitions)\n");
        kry_parser_free(parser);
        return NULL;
    }

    // Create conversion context
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.param_count = 0;  // Initialize with no parameters
    ctx.manifest = ir_reactive_manifest_create();  // Create reactive manifest for state tracking
    ctx.logic_block = ir_logic_block_create();     // Create logic block for event handlers
    ctx.next_handler_id = 1;                       // Start handler ID counter
    ctx.target_platform = KRY_TARGET_LUA;          // Default to Lua target

    // Convert AST to IR
    IRComponent* root = convert_node(&ctx, root_node);

    // Free parser (includes AST)
    kry_parser_free(parser);

    // Original API only returns root - destroy manifest and logic_block
    // For extended result, use ir_kry_parse_ex()
    if (ctx.manifest) {
        ir_reactive_manifest_destroy(ctx.manifest);
    }
    if (ctx.logic_block) {
        ir_logic_block_free(ctx.logic_block);
    }

    return root;
}

// ============================================================================
// Extended Parse API (returns manifest and logic_block)
// ============================================================================

/**
 * Extended parse function that returns root, manifest, and logic_block
 * Allows callers to access state variables and event handlers from parsing
 */
IRKryParseResult ir_kry_parse_ex(const char* source, size_t length) {
    IRKryParseResult result = {NULL, NULL, NULL};

    if (!source) {
        fprintf(stderr, "Error: NULL source passed to ir_kry_parse_ex\n");
        return result;
    }

    if (length == 0) {
        length = strlen(source);
    }

    // Parse source to AST
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) {
        fprintf(stderr, "Error: Failed to create parser\n");
        return result;
    }

    if (parser->has_error) {
        fprintf(stderr, "Parse error at line %u: %s\n",
                parser->error_line, parser->error_message ? parser->error_message : "Unknown error");
        kry_parser_free(parser);
        return result;
    }

    KryNode* ast = parser->root;
    if (!ast) {
        fprintf(stderr, "Error: No AST generated\n");
        kry_parser_free(parser);
        return result;
    }

    // Find the root application (skip component definitions)
    KryNode* root_node = ast;
    while (root_node) {
        if (!root_node->is_component_definition) {
            break;
        }
        root_node = root_node->next_sibling;
    }

    if (!root_node) {
        fprintf(stderr, "Error: No root application found (only component definitions)\n");
        kry_parser_free(parser);
        return result;
    }

    // Create conversion context
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.param_count = 0;
    ctx.manifest = ir_reactive_manifest_create();
    ctx.logic_block = ir_logic_block_create();
    ctx.next_handler_id = 1;
    ctx.compile_mode = IR_COMPILE_MODE_HYBRID;
    ctx.source_structures = ir_source_structures_create();
    ctx.static_block_counter = 0;
    ctx.current_static_block_id = NULL;
    ctx.target_platform = KRY_TARGET_LUA;          // Default to Lua target

    // Convert AST to IR
    result.root = convert_node(&ctx, root_node);
    result.manifest = ctx.manifest;
    result.logic_block = ctx.logic_block;

    // Free parser (includes AST)
    kry_parser_free(parser);

    // Note: caller is responsible for freeing source_structures
    // For now, we destroy it to avoid memory leak
    if (ctx.source_structures) {
        ir_source_structures_destroy(ctx.source_structures);
    }

    return result;
}

// ============================================================================
// Helper Functions for Component Definitions
// ============================================================================

// Deep clone a component tree (for component instantiation)
static IRComponent* ir_component_clone_tree(IRComponent* source) {
    if (!source) return NULL;

    // Create new component with same type
    IRComponent* clone = ir_create_component(source->type);
    if (!clone) return NULL;

    // Copy basic properties
    clone->text_content = source->text_content ? strdup(source->text_content) : NULL;
    clone->text_expression = source->text_expression ? strdup(source->text_expression) : NULL;
    clone->tag = source->tag ? strdup(source->tag) : NULL;
    clone->custom_data = source->custom_data ? strdup(source->custom_data) : NULL;

    // Copy style (deep copy)
    if (source->style) {
        clone->style = malloc(sizeof(IRStyle));
        if (clone->style) {
            memcpy(clone->style, source->style, sizeof(IRStyle));
        }
    }

    // Copy layout (deep copy) - contains gap, alignItems, etc.
    if (source->layout) {
        clone->layout = malloc(sizeof(IRLayout));
        if (clone->layout) {
            memcpy(clone->layout, source->layout, sizeof(IRLayout));
        }
    }

    // Clone children recursively
    for (uint32_t i = 0; i < source->child_count; i++) {
        IRComponent* child_clone = ir_component_clone_tree(source->children[i]);
        if (child_clone) {
            ir_add_child(clone, child_clone);
        }
    }

    // Copy event handlers
    if (source->events) {
        IREvent* src_event = source->events;
        IREvent* prev_event = NULL;

        while (src_event) {
            // Create new event
            IREvent* event_clone = (IREvent*)calloc(1, sizeof(IREvent));
            event_clone->type = src_event->type;
            event_clone->event_name = src_event->event_name ? strdup(src_event->event_name) : NULL;
            event_clone->logic_id = src_event->logic_id ? strdup(src_event->logic_id) : NULL;
            event_clone->handler_data = src_event->handler_data ? strdup(src_event->handler_data) : NULL;
            event_clone->bytecode_function_id = src_event->bytecode_function_id;

            // Link to list
            if (prev_event) {
                prev_event->next = event_clone;
            } else {
                clone->events = event_clone;
            }
            prev_event = event_clone;

            src_event = src_event->next;
        }
    }

    return clone;
}

// Check if a component name refers to a custom component definition
static bool is_custom_component(const char* name, IRReactiveManifest* manifest) {
    if (!name || !manifest) {
        fprintf(stderr, "[is_custom_component] NULL check failed: name=%p manifest=%p\n",
                (void*)name, (void*)manifest);
        return false;
    }

    fprintf(stderr, "[is_custom_component] Checking '%s' against %u component defs\n",
            name, manifest->component_def_count);
    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        fprintf(stderr, "[is_custom_component]   [%u] %s\n", i, manifest->component_defs[i].name);
        if (strcmp(manifest->component_defs[i].name, name) == 0) {
            fprintf(stderr, "[is_custom_component] MATCH FOUND for %s!\n", name);
            return true;
        }
    }
    fprintf(stderr, "[is_custom_component] No match for %s\n", name);
    return false;
}

// Expand a component template into an instance
// Helper: Recursively set scope on component and all children
static void set_component_scope_recursive(IRComponent* comp, const char* scope) {
    if (!comp) return;

    // Set scope on this component
    if (comp->scope) {
        free(comp->scope);
    }
    comp->scope = scope ? strdup(scope) : NULL;

    // Recursively set scope on all children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        set_component_scope_recursive(comp->children[i], scope);
    }
}

static IRComponent* expand_component_template(
    const char* comp_name,
    IRReactiveManifest* manifest,
    const char* instance_scope
) {
    if (!comp_name || !manifest) return NULL;

    // Find component definition
    IRComponentDefinition* def = NULL;
    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        if (strcmp(manifest->component_defs[i].name, comp_name) == 0) {
            def = &manifest->component_defs[i];
            break;
        }
    }

    if (!def || !def->template_root) return NULL;

    // Clone the template tree
    IRComponent* instance = ir_component_clone_tree(def->template_root);
    if (!instance) return NULL;

    // Set scope on all components in the cloned tree
    set_component_scope_recursive(instance, instance_scope);
    fprintf(stderr, "[CUSTOM_COMPONENT] Set scope '%s' on component tree\n", instance_scope);

    // Initialize state variables with instance scope
    // For MVP, we just store them in the manifest
    // Full implementation would need runtime state management
    for (uint32_t i = 0; i < def->state_var_count; i++) {
        IRComponentStateVar* state_var __attribute__((unused)) = &def->state_vars[i];
        // State variables are already registered in the manifest during argument application
    }

    return instance;
}

// Extract state variable declarations from component body
static IRComponentStateVar* extract_component_state_vars(
    KryNode* component_body,
    uint32_t* out_count
) {
    if (!component_body || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // Count state declarations
    uint32_t count = 0;
    KryNode* child = component_body->first_child;
    while (child) {
        if (child->type == KRY_NODE_STATE) {
            count++;
        }
        child = child->next_sibling;
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    // Allocate array
    IRComponentStateVar* state_vars = calloc(count, sizeof(IRComponentStateVar));
    if (!state_vars) {
        *out_count = 0;
        return NULL;
    }

    // Extract each state variable
    uint32_t idx = 0;
    child = component_body->first_child;
    while (child && idx < count) {
        if (child->type != KRY_NODE_STATE) {
            child = child->next_sibling;
            continue;
        }

        // Parse state declaration: "state value: int = initialValue"
        // child->name = "value"
        // child->state_type = "int"
        // child->value = initial expression (KryValue)

        state_vars[idx].name = child->name ? strdup(child->name) : strdup("unknown");
        state_vars[idx].type = child->state_type ? strdup(child->state_type) : strdup("any");

        // Convert initial expression to string
        if (child->value) {
            switch (child->value->type) {
                case KRY_VALUE_STRING:
                    state_vars[idx].initial_expr = child->value->string_value ?
                        strdup(child->value->string_value) : NULL;
                    break;
                case KRY_VALUE_NUMBER: {
                    char num_str[64];
                    snprintf(num_str, sizeof(num_str), "%.10g", child->value->number_value);
                    state_vars[idx].initial_expr = strdup(num_str);
                    break;
                }
                case KRY_VALUE_IDENTIFIER:
                    state_vars[idx].initial_expr = child->value->identifier ?
                        strdup(child->value->identifier) : NULL;
                    break;
                case KRY_VALUE_EXPRESSION:
                    state_vars[idx].initial_expr = child->value->expression ?
                        strdup(child->value->expression) : NULL;
                    break;
                default:
                    state_vars[idx].initial_expr = NULL;
                    break;
            }
        } else {
            state_vars[idx].initial_expr = NULL;
        }

        idx++;
        child = child->next_sibling;
    }

    *out_count = count;
    return state_vars;
}

// ============================================================================
// KIR Generation (with manifest serialization)
// ============================================================================

char* ir_kry_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Create parser
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) return NULL;

    // Parse to AST
    KryNode* ast = kry_parse(parser);

    if (!ast || parser->has_error) {
        if (parser->has_error) {
            fprintf(stderr, "Parse error at %u:%u: %s\n",
                    parser->error_line, parser->error_column,
                    parser->error_message ? parser->error_message : "Unknown error");
        }
        kry_parser_free(parser);
        return NULL;
    }

    // Check if ast is a Root wrapper with children or a single component
    KryNode* root_node = NULL;
    fprintf(stderr, "[ir_kry_to_kir] ast->name='%s', type=%d, is_component_definition=%d\n",
            ast->name ? ast->name : "(null)", ast->type, ast->is_component_definition);
    fflush(stderr);

    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        // Root wrapper with children - find the application node
        fprintf(stderr, "[ir_kry_to_kir] Root wrapper detected, scanning children...\n");
        fflush(stderr);
        KryNode* child = ast->first_child;
        while (child) {
            fprintf(stderr, "[ir_kry_to_kir]   Child: name='%s', is_component_definition=%d, type=%d\n",
                    child->name ? child->name : "(null)", child->is_component_definition, child->type);
            fflush(stderr);
            // Skip component definitions, variable declarations, and style blocks
            if (!child->is_component_definition &&
                child->type != KRY_NODE_VAR_DECL &&
                child->type != KRY_NODE_STYLE_BLOCK) {
                root_node = child;
                fprintf(stderr, "[ir_kry_to_kir]   Found root application: %s\n", root_node->name);
                fflush(stderr);
                break;
            }
            child = child->next_sibling;
        }
    } else {
        // Single component (old behavior for compatibility)
        fprintf(stderr, "[ir_kry_to_kir] Single component (no Root wrapper)\n");
        fflush(stderr);
        root_node = ast;
        while (root_node && root_node->is_component_definition) {
            root_node = root_node->next_sibling;
        }
    }

    if (!root_node) {
        // No root application found, only component definitions
        fprintf(stderr, "Error: No root application found (only component definitions)\n");
        kry_parser_free(parser);
        return NULL;
    }

    // Create conversion context with manifest and logic block
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.param_count = 0;
    ctx.manifest = ir_reactive_manifest_create();
    ctx.logic_block = ir_logic_block_create();  // NEW: Create logic block
    ctx.next_handler_id = 1;                    // NEW: Initialize handler counter
    ctx.compile_mode = IR_COMPILE_MODE_HYBRID;  // Use HYBRID mode for round-trip codegen
    ctx.source_structures = ir_source_structures_create();  // Create source structures
    ctx.static_block_counter = 0;               // Initialize static block counter
    ctx.current_static_block_id = NULL;         // Not in static block initially
    ctx.target_platform = KRY_TARGET_LUA;       // Default to Lua target

    // Create IR context for component ID generation
    IRContext* ir_ctx = ir_create_context();
    ir_set_context(ir_ctx);

    // Process top-level variable declarations (const/let/var)
    fprintf(stderr, "[VAR_DECL] Processing top-level variable declarations...\n");
    fflush(stderr);
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* var_node = ast->first_child;
        while (var_node) {
            if (var_node->type == KRY_NODE_VAR_DECL && var_node != root_node) {
                fprintf(stderr, "[VAR_DECL] Found top-level var: %s\n", var_node->name ? var_node->name : "(null)");
                fflush(stderr);

                const char* var_type = var_node->var_type ? var_node->var_type : "const";
                const char* var_name = var_node->name;

                // Store the variable in ctx->params so for-loop expansion can access it
                if (ctx.param_count < MAX_PARAMS && var_node->value) {
                    ctx.params[ctx.param_count].name = (char*)var_name;
                    ctx.params[ctx.param_count].value = NULL;  // String representation
                    ctx.params[ctx.param_count].kry_value = var_node->value;  // Original KryValue
                    ctx.param_count++;
                    fprintf(stderr, "[VAR_DECL]   Added to ctx->params, count=%d\n", ctx.param_count);
                    fflush(stderr);
                }

                // Add variable declaration to source_structures for serialization
                if (ctx.compile_mode == IR_COMPILE_MODE_HYBRID) {
                    // Get value as JSON string
                    char* var_value_json = NULL;
                    if (var_node->value) {
                        // Convert KryValue to JSON string
                        var_value_json = kry_value_to_json(var_node->value);
                    } else {
                        var_value_json = strdup("null");
                    }

                    fprintf(stderr, "[VAR_DECL]   Variable type=%s, name=%s, value_json=%s\n",
                            var_type, var_name, var_value_json ? var_value_json : "(null)");
                    fflush(stderr);

                    // Store the variable declaration
                    ir_source_structures_add_var_decl(ctx.source_structures,
                                                     var_name, var_type, var_value_json, "global");
                }

                // Add variable to reactive manifest with initial value
                if (ctx.manifest && var_node->value) {
                    IRReactiveValue init_value;
                    IRReactiveVarType react_type;
                    bool has_init_value = false;

                    // Convert KryValue to IRReactiveValue
                    if (var_node->value->type == KRY_VALUE_IDENTIFIER) {
                        const char* ident = var_node->value->identifier;
                        if (strcmp(ident, "true") == 0) {
                            react_type = IR_REACTIVE_TYPE_BOOL;
                            init_value.as_bool = true;
                            has_init_value = true;
                        } else if (strcmp(ident, "false") == 0) {
                            react_type = IR_REACTIVE_TYPE_BOOL;
                            init_value.as_bool = false;
                            has_init_value = true;
                        }
                    } else if (var_node->value->type == KRY_VALUE_NUMBER) {
                        // Check if it's a float or int
                        if (strchr(var_node->value->string_value, '.')) {
                            react_type = IR_REACTIVE_TYPE_FLOAT;
                            init_value.as_float = atof(var_node->value->string_value);
                        } else {
                            react_type = IR_REACTIVE_TYPE_INT;
                            init_value.as_int = atoll(var_node->value->string_value);
                        }
                        has_init_value = true;
                    } else if (var_node->value->type == KRY_VALUE_STRING) {
                        react_type = IR_REACTIVE_TYPE_STRING;
                        init_value.as_string = var_node->value->string_value;
                        has_init_value = true;
                    }

                    if (has_init_value) {
                        uint32_t var_id = ir_reactive_manifest_add_var(ctx.manifest,
                                                                       var_name, react_type, init_value);

                        // Set metadata including initial value as JSON
                        char* init_json = NULL;
                        if (react_type == IR_REACTIVE_TYPE_BOOL) {
                            init_json = init_value.as_bool ? "true" : "false";
                        } else if (react_type == IR_REACTIVE_TYPE_INT) {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%lld", (long long)init_value.as_int);
                            init_json = strdup(buf);
                        } else if (react_type == IR_REACTIVE_TYPE_FLOAT) {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%g", init_value.as_float);
                            init_json = strdup(buf);
                        } else if (react_type == IR_REACTIVE_TYPE_STRING) {
                            // Quote the string value
                            size_t len = strlen(init_value.as_string);
                            init_json = malloc(len + 3);  // quotes + null
                            snprintf(init_json, len + 3, "\"%s\"", init_value.as_string);
                        }

                        if (init_json) {
                            const char* type_str = react_type == IR_REACTIVE_TYPE_BOOL ? "bool" :
                                                 react_type == IR_REACTIVE_TYPE_INT ? "int" :
                                                 react_type == IR_REACTIVE_TYPE_FLOAT ? "float" : "string";
                            ir_reactive_manifest_set_var_metadata(ctx.manifest, var_id,
                                                                 type_str, init_json, "global");

                            // Free allocated strings (bool literals don't need freeing)
                            if (react_type != IR_REACTIVE_TYPE_BOOL) {
                                free(init_json);
                            }
                        }

                        fprintf(stderr, "[VAR_DECL]   Added to reactive manifest: %s\n", var_name);
                        fflush(stderr);
                    }
                }
            }
            var_node = var_node->next_sibling;
        }
    }
    fprintf(stderr, "[VAR_DECL] Finished processing top-level variables\n");
    fflush(stderr);

    // Process top-level style blocks
    fprintf(stderr, "[STYLE_BLOCKS] Processing top-level style blocks...\n");
    fflush(stderr);
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* style_node = ast->first_child;
        while (style_node) {
            if (style_node->type == KRY_NODE_STYLE_BLOCK && style_node->name) {
                fprintf(stderr, "[STYLE_BLOCKS] Found style block: %s\n", style_node->name);
                fflush(stderr);

                // Ensure stylesheet exists in global context
                if (g_ir_context && !g_ir_context->stylesheet) {
                    g_ir_context->stylesheet = ir_stylesheet_create();
                }

                if (g_ir_context && g_ir_context->stylesheet) {
                    // Build IRStyleProperties from child properties
                    IRStyleProperties props;
                    ir_style_properties_init(&props);

                    // Iterate through property children
                    KryNode* prop_child = style_node->first_child;
                    while (prop_child) {
                        if (prop_child->type == KRY_NODE_PROPERTY && prop_child->name && prop_child->value) {
                            const char* prop_name = prop_child->name;
                            KryValue* val = prop_child->value;

                            // Convert property to IRStyleProperties
                            if (strcmp(prop_name, "backgroundColor") == 0 || strcmp(prop_name, "background") == 0) {
                                if (val->type == KRY_VALUE_STRING) {
                                    IRColor color;
                                    if (ir_css_parse_color(val->string_value, &color)) {
                                        props.background = color;
                                        props.set_flags |= IR_PROP_BACKGROUND;
                                    }
                                }
                            } else if (strcmp(prop_name, "color") == 0) {
                                if (val->type == KRY_VALUE_STRING) {
                                    IRColor color;
                                    if (ir_css_parse_color(val->string_value, &color)) {
                                        props.color = color;
                                        props.set_flags |= IR_PROP_COLOR;
                                    }
                                }
                            } else if (strcmp(prop_name, "display") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* display_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(display_val, "flex") == 0) {
                                        props.display = IR_LAYOUT_MODE_FLEX;
                                    } else if (strcmp(display_val, "grid") == 0) {
                                        props.display = IR_LAYOUT_MODE_GRID;
                                    } else if (strcmp(display_val, "block") == 0) {
                                        props.display = IR_LAYOUT_MODE_BLOCK;
                                    }
                                    props.display_explicit = true;
                                    props.set_flags |= IR_PROP_DISPLAY;
                                }
                            } else if (strcmp(prop_name, "flexDirection") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* dir_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(dir_val, "row") == 0) {
                                        props.flex_direction = 1;
                                    } else {
                                        props.flex_direction = 0;
                                    }
                                    props.set_flags |= IR_PROP_FLEX_DIRECTION;
                                }
                            } else if (strcmp(prop_name, "justifyContent") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* jc_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(jc_val, "center") == 0) {
                                        props.justify_content = IR_ALIGNMENT_CENTER;
                                    } else if (strcmp(jc_val, "spaceBetween") == 0 || strcmp(jc_val, "space-between") == 0) {
                                        props.justify_content = IR_ALIGNMENT_SPACE_BETWEEN;
                                    } else if (strcmp(jc_val, "spaceAround") == 0 || strcmp(jc_val, "space-around") == 0) {
                                        props.justify_content = IR_ALIGNMENT_SPACE_AROUND;
                                    } else if (strcmp(jc_val, "flexEnd") == 0 || strcmp(jc_val, "flex-end") == 0) {
                                        props.justify_content = IR_ALIGNMENT_END;
                                    } else {
                                        props.justify_content = IR_ALIGNMENT_START;
                                    }
                                    props.set_flags |= IR_PROP_JUSTIFY_CONTENT;
                                }
                            } else if (strcmp(prop_name, "alignItems") == 0) {
                                if (val->type == KRY_VALUE_IDENTIFIER || val->type == KRY_VALUE_STRING) {
                                    const char* ai_val = (val->type == KRY_VALUE_IDENTIFIER) ? val->identifier : val->string_value;
                                    if (strcmp(ai_val, "center") == 0) {
                                        props.align_items = IR_ALIGNMENT_CENTER;
                                    } else if (strcmp(ai_val, "flexEnd") == 0 || strcmp(ai_val, "flex-end") == 0) {
                                        props.align_items = IR_ALIGNMENT_END;
                                    } else if (strcmp(ai_val, "stretch") == 0) {
                                        props.align_items = IR_ALIGNMENT_STRETCH;
                                    } else {
                                        props.align_items = IR_ALIGNMENT_START;
                                    }
                                    props.set_flags |= IR_PROP_ALIGN_ITEMS;
                                }
                            } else if (strcmp(prop_name, "padding") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    float p = (float)val->number_value;
                                    props.padding.top = props.padding.right = props.padding.bottom = props.padding.left = p;
                                    props.set_flags |= IR_PROP_PADDING;
                                }
                            } else if (strcmp(prop_name, "margin") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    float m = (float)val->number_value;
                                    props.margin.top = props.margin.right = props.margin.bottom = props.margin.left = m;
                                    props.set_flags |= IR_PROP_MARGIN;
                                }
                            } else if (strcmp(prop_name, "gap") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.gap = (float)val->number_value;
                                    props.set_flags |= IR_PROP_GAP;
                                }
                            } else if (strcmp(prop_name, "fontSize") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.font_size.type = IR_DIMENSION_PX;
                                    props.font_size.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_FONT_SIZE;
                                }
                            } else if (strcmp(prop_name, "borderRadius") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.border_radius = (uint8_t)val->number_value;
                                    props.set_flags |= IR_PROP_BORDER_RADIUS;
                                }
                            } else if (strcmp(prop_name, "width") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.width.type = val->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
                                    props.width.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_WIDTH;
                                }
                            } else if (strcmp(prop_name, "height") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.height.type = val->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
                                    props.height.value = (float)val->number_value;
                                    props.set_flags |= IR_PROP_HEIGHT;
                                }
                            } else if (strcmp(prop_name, "opacity") == 0) {
                                if (val->type == KRY_VALUE_NUMBER) {
                                    props.opacity = (float)val->number_value;
                                    props.set_flags |= IR_PROP_OPACITY;
                                }
                            }
                        }
                        prop_child = prop_child->next_sibling;
                    }

                    // Add rule to stylesheet
                    if (props.set_flags != 0) {
                        ir_stylesheet_add_rule(g_ir_context->stylesheet, style_node->name, &props);
                        fprintf(stderr, "[STYLE_BLOCKS]   Added rule for selector: %s\n", style_node->name);
                        fflush(stderr);
                    }

                    ir_style_properties_cleanup(&props);
                }
            }
            style_node = style_node->next_sibling;
        }
    }
    fprintf(stderr, "[STYLE_BLOCKS] Finished processing top-level style blocks\n");
    fflush(stderr);

    // Process top-level code blocks (@lua, @js)
    fprintf(stderr, "[CODE_BLOCKS] Processing top-level code blocks...\n");
    fflush(stderr);
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* code_node = ast->first_child;
        while (code_node) {
            if (code_node->type == KRY_NODE_CODE_BLOCK && code_node->code_language && code_node->code_source) {
                fprintf(stderr, "[CODE_BLOCKS] Found code block: language='%s'\n", code_node->code_language);
                fflush(stderr);

                // Generate a unique function name for this code block
                char func_name[256];
                static int top_level_code_block_counter = 0;
                snprintf(func_name, sizeof(func_name), "_top_level_code_block_%d", top_level_code_block_counter++);

                // Create logic function to hold the source code
                IRLogicFunction* func = ir_logic_function_create(func_name);
                if (func) {
                    // Direct @lua or @js block - just add the source
                    ir_logic_function_add_source(func, code_node->code_language, code_node->code_source);
                    fprintf(stderr, "[CODE_BLOCKS]   Added to logic block: func='%s'\n", func_name);

                    // Add function to logic block
                    ir_logic_block_add_function(ctx.logic_block, func);
                }
            }
            code_node = code_node->next_sibling;
        }
    }
    fprintf(stderr, "[CODE_BLOCKS] Finished processing top-level code blocks\n");
    fflush(stderr);

    // Track all component definitions in the manifest
    fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Starting component definition scan...\n");
    fflush(stderr);

    // If ast is a Root wrapper, scan its children; otherwise scan ast and siblings
    KryNode* def_node = NULL;
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Scanning children of Root wrapper...\n");
        fflush(stderr);
        def_node = ast->first_child;
    } else {
        fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Scanning ast and siblings...\n");
        fflush(stderr);
        def_node = ast;
    }

    uint32_t node_count = 0;
    while (def_node) {
        node_count++;
        fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Node %u: name='%s', is_component_definition=%d\n",
                node_count, def_node->name ? def_node->name : "(null)", def_node->is_component_definition);
        fflush(stderr);

        if (def_node->is_component_definition && def_node->name) {
            fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Found component definition: %s\n", def_node->name);
            fflush(stderr);

            // Convert the component definition template
            IRComponent* template_comp = convert_node(&ctx, def_node);
            fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] template_comp = %p\n", (void*)template_comp);
            fflush(stderr);

            if (template_comp) {
                // Extract state variables from component definition
                uint32_t state_var_count = 0;
                IRComponentStateVar* state_vars = extract_component_state_vars(
                    def_node,
                    &state_var_count
                );
                fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Extracted %u state vars\n", state_var_count);
                fflush(stderr);

                // Add to manifest (will be serialized as component_definitions in KIR)
                fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Calling ir_reactive_manifest_add_component_def...\n");
                fflush(stderr);
                ir_reactive_manifest_add_component_def(
                    ctx.manifest,
                    def_node->name,
                    NULL, 0,  // TODO: Extract props from component definition
                    state_vars,
                    state_var_count,
                    template_comp
                );
                fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] After registration, component_def_count = %u\n",
                        ctx.manifest->component_def_count);
                fflush(stderr);
            } else {
                fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Failed to convert component template!\n");
                fflush(stderr);
            }
        }
        def_node = def_node->next_sibling;
    }
    fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Scan complete. Total nodes scanned: %u\n", node_count);
    fprintf(stderr, "[COMPONENT_DEF_REGISTRATION] Final component_def_count: %u\n", ctx.manifest->component_def_count);
    fflush(stderr);

    // Convert AST to IR
    IRComponent* root = convert_node(&ctx, root_node);

    // Free parser (includes AST)
    kry_parser_free(parser);

    if (!root) {
        if (ctx.manifest) {
            ir_reactive_manifest_destroy(ctx.manifest);
        }
        if (ctx.logic_block) {
            ir_logic_block_free(ctx.logic_block);
        }
        return NULL;
    }

    // Create source metadata
    IRSourceMetadata metadata;
    metadata.source_language = "kry";
    metadata.compiler_version = "kryon-1.0.0";
    metadata.timestamp = NULL;  // TODO: Add timestamp

    // Serialize with complete KIR format (manifest + logic_block + metadata + source_structures)
    char* json = ir_serialize_json_complete(root, ctx.manifest, ctx.logic_block, &metadata, ctx.source_structures);

    // Clean up
    ir_destroy_component(root);
    if (ctx.manifest) {
        ir_reactive_manifest_destroy(ctx.manifest);
    }
    if (ctx.logic_block) {
        ir_logic_block_free(ctx.logic_block);
    }
    if (ctx.source_structures) {
        ir_source_structures_destroy(ctx.source_structures);
    }
    ir_destroy_context(ir_ctx);  // Clean up IR context

    return json;
}
