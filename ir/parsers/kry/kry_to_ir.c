/**
 * Kryon .kry AST to IR Converter
 *
 * Converts the parsed .kry AST into IR component trees.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_parser.h"
#include "kry_ast.h"
#include "../../ir_builder.h"
#include "../../ir_serialization.h"
#include "../../ir_logic.h"
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
        ctx->params[ctx->param_count].name = param_name;
        ctx->params[ctx->param_count].value = param_value;
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
                    ctx->params[ctx->param_count].name = child->name;
                    ctx->params[ctx->param_count].value = param_value;
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

static KryNode* find_component_definition(ConversionContext* ctx, const char* name) {
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

    // Default to container for unknown types
    return IR_COMPONENT_CONTAINER;
}

// ============================================================================
// Color Parsing
// ============================================================================

static uint32_t parse_color(const char* color_str) {
    if (!color_str) return 0x000000FF;

    // Handle hex colors (#RRGGBB or #RRGGBBAA)
    if (color_str[0] == '#') {
        unsigned int r, g, b, a = 255;
        int len = strlen(color_str);

        if (len == 7) {  // #RRGGBB
            sscanf(color_str + 1, "%02x%02x%02x", &r, &g, &b);
        } else if (len == 9) {  // #RRGGBBAA
            sscanf(color_str + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
        } else {
            return 0x000000FF;
        }

        return (r << 24) | (g << 16) | (b << 8) | a;
    }

    // Handle named colors
    if (strcmp(color_str, "red") == 0) return 0xFF0000FF;
    if (strcmp(color_str, "green") == 0) return 0x00FF00FF;
    if (strcmp(color_str, "blue") == 0) return 0x0000FFFF;
    if (strcmp(color_str, "yellow") == 0) return 0xFFFF00FF;
    if (strcmp(color_str, "cyan") == 0) return 0x00FFFFFF;
    if (strcmp(color_str, "magenta") == 0) return 0xFF00FFFF;
    if (strcmp(color_str, "pink") == 0) return 0xFFC0CBFF;
    if (strcmp(color_str, "white") == 0) return 0xFFFFFFFF;
    if (strcmp(color_str, "black") == 0) return 0x000000FF;
    if (strcmp(color_str, "gray") == 0 || strcmp(color_str, "grey") == 0) return 0x808080FF;

    return 0x000000FF;
}

// ============================================================================
// Property Application
// ============================================================================

static void apply_property(ConversionContext* ctx, IRComponent* component, const char* name, KryValue* value) {
    if (!component || !name || !value) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
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

            // Create logic function with .kry source code
            IRLogicFunction* func = ir_logic_function_create(handler_name);
            if (func) {
                // Add the source code as .kry language
                ir_logic_function_add_source(func, "kry", value->expression);

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
            uint32_t color = parse_color(color_str);
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
            uint32_t color = parse_color(color_str);
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
            uint32_t color = parse_color(color_str);
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
            if (layout) {
                layout->flex.cross_axis = alignment;
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
        if (value->type == KRY_VALUE_STRING) {
            // Get or create metadata
            IRContext* ctx = g_ir_context;
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
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
                    }
                }
            }
        }
        return;
    }

    if (strcmp(name, "windowWidth") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRContext* ctx = g_ir_context;
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
                }
                if (ctx->metadata) {
                    ctx->metadata->window_width = (int)value->number_value;
                }
            }
        }
        return;
    }

    if (strcmp(name, "windowHeight") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRContext* ctx = g_ir_context;
            if (ctx) {
                if (!ctx->metadata) {
                    ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
                }
                if (ctx->metadata) {
                    ctx->metadata->window_height = (int)value->number_value;
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
        fprintf(stderr, "[DEBUG_FORLOOP] Early return: for_node=%p, type=%d\n",
                (void*)for_node, for_node ? for_node->type : -1);
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
    fprintf(stderr, "[DEBUG] expand_for_loop: compile_mode=%d, current_static_block_id=%s\n",
            ctx->compile_mode, ctx->current_static_block_id ? ctx->current_static_block_id : "NULL");

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
            fprintf(stderr, "[DEBUG] Preserving for loop template!\n");
            // Get collection reference as string
            const char* collection_ref = NULL;
            // Try to find the original identifier name from the parent call
            // For now, use a generic name - this will be improved later
            collection_ref = "items";  // FIXME: Pass this from parent call

            // Create template component (convert first child WITHOUT parameter substitution)
            IRComponent* template_comp = NULL;
            fprintf(stderr, "[DEBUG] for_node->first_child=%p\n", (void*)for_node->first_child);
            if (for_node->first_child) {
                fprintf(stderr, "[DEBUG] for_node->first_child->type=%u (COMPONENT=%u)\n",
                        for_node->first_child->type, KRY_NODE_COMPONENT);
            }
            if (for_node->first_child && for_node->first_child->type == KRY_NODE_COMPONENT) {
                fprintf(stderr, "[DEBUG] Creating template component from first child\n");
                template_comp = convert_node(ctx, for_node->first_child);
                if (template_comp) {
                    fprintf(stderr, "[DEBUG] Template component created: tag=%s, id=%u\n",
                            template_comp->tag ? template_comp->tag : "NULL", template_comp->id);
                }
            } else {
                fprintf(stderr, "[DEBUG] Skipping template creation - no component child found\n");
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
        for (size_t i = 0; i < collection->array.count; i++) {
            KryValue* element = collection->array.elements[i];

            // Create new context with iterator variable bound
            ConversionContext loop_ctx = *ctx;

            // Track malloc'd strings for cleanup after iteration
            char* allocated_strings[MAX_PARAMS * 2];
            int alloc_count = 0;

            // For object elements, we need to make properties accessible
            // For now, store the element value
            // TODO: Implement proper object property access

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
            while (loop_body_child) {
                if (loop_body_child->type == KRY_NODE_COMPONENT) {
                    IRComponent* child_component = convert_node(&loop_ctx, loop_body_child);
                    if (child_component) {
                        ir_add_child(parent, child_component);

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

        // Look up the variable in context
        for (int i = 0; i < ctx->param_count; i++) {
            if (strcmp(ctx->params[i].name, collection_name) == 0) {
                // Found the variable - check if it's a KryValue
                if (ctx->params[i].kry_value != NULL) {
                    // Recursively call expand_for_loop with the resolved KryValue
                    KryNode temp_for_node = *for_node;
                    temp_for_node.value = ctx->params[i].kry_value;
                    expand_for_loop(ctx, parent, &temp_for_node);
                    return;
                }
                break;
            }
        }
    }

    // Update loop_data with expanded component IDs
    if (loop_data && expanded_ids && expanded_count > 0) {
        loop_data->expanded_component_ids = expanded_ids;
        loop_data->expanded_count = (uint32_t)expanded_count;
        fprintf(stderr, "[DEBUG] Updated loop_data with %zu expanded component IDs\n", expanded_count);
        for (size_t i = 0; i < expanded_count; i++) {
            fprintf(stderr, "[DEBUG]   ID[%zu] = %u\n", i, expanded_ids[i]);
        }
    } else if (expanded_ids) {
        // Clean up if we didn't use the array
        fprintf(stderr, "[DEBUG] Cleaning up expanded_ids (loop_data=%p, expanded_count=%zu)\n",
                (void*)loop_data, expanded_count);
        free(expanded_ids);
    }
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
                printf("[CONVERT_NODE] Applying property '%s', value=%p, value->type=%d\n",
                       child->name, (void*)child->value, child->value ? child->value->type : -1);
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
                IRReactiveValue react_value = {.as_int = 0};  // Default int value
                if (init_value) {
                    react_value.as_int = atoi(init_value);
                }

                uint32_t var_id = ir_reactive_manifest_add_var(
                    ctx->manifest,
                    var_name,
                    IR_REACTIVE_TYPE_INT,  // TODO: Parse actual type from declaration
                    react_value
                );

                // Set metadata for proper KIR serialization
                if (var_id > 0) {
                    ir_reactive_manifest_set_var_metadata(
                        ctx->manifest,
                        var_id,
                        "int",  // TODO: Use actual type from state declaration
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
                    } else if (static_child->type == KRY_NODE_VAR_DECL) {
                        // Process variable declaration in static context
                        const char* var_name = static_child->name;
                        const char* var_value = NULL;

                        // HYBRID MODE: Preserve variable declaration
                        if (ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                            // Determine variable type
                            const char* var_type = "const";  // Default
                            // TODO: Parse actual var type from AST (const/let/var)

                            // Convert value to JSON string for preservation
                            const char* value_json = NULL;
                            static char json_buffer[4096];  // Static buffer for JSON serialization
                            if (static_child->value) {
                                if (static_child->value->type == KRY_VALUE_STRING) {
                                    // Quote string values
                                    snprintf(json_buffer, sizeof(json_buffer), "\"%s\"", static_child->value->string_value);
                                    value_json = json_buffer;
                                } else if (static_child->value->type == KRY_VALUE_ARRAY) {
                                    // Serialize array to JSON
                                    size_t pos = 0;
                                    pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, "[");
                                    for (size_t i = 0; i < static_child->value->array.count; i++) {
                                        if (i > 0) pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, ", ");
                                        KryValue* elem = static_child->value->array.elements[i];
                                        if (elem->type == KRY_VALUE_STRING) {
                                            pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, "\"%s\"", elem->string_value);
                                        } else if (elem->type == KRY_VALUE_NUMBER) {
                                            pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, "%g", elem->number_value);
                                        }
                                    }
                                    pos += snprintf(json_buffer + pos, sizeof(json_buffer) - pos, "]");
                                    value_json = json_buffer;
                                } else if (static_child->value->type == KRY_VALUE_OBJECT) {
                                    // TODO: Serialize object to JSON string
                                    value_json = "{...}";  // Placeholder
                                } else if (static_child->value->type == KRY_VALUE_NUMBER) {
                                    snprintf(json_buffer, sizeof(json_buffer), "%g", static_child->value->number_value);
                                    value_json = json_buffer;
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
            } else if (child->type == KRY_NODE_FOR_LOOP) {
                // For loop outside static block - expand/unroll
                expand_for_loop(ctx, component, child);
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

    // Convert AST to IR
    IRComponent* root = convert_node(&ctx, root_node);

    // Free parser (includes AST)
    kry_parser_free(parser);

    // TODO: Return manifest and logic_block along with root (needs API change)
    // For now, just destroy them since we can't return them
    if (ctx.manifest) {
        ir_reactive_manifest_destroy(ctx.manifest);
    }
    if (ctx.logic_block) {
        ir_logic_block_free(ctx.logic_block);
    }

    return root;
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
        IRComponentStateVar* state_var = &def->state_vars[i];
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
            // Skip component definitions and variable declarations
            if (!child->is_component_definition && child->type != KRY_NODE_VAR_DECL) {
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

    // Create IR context for component ID generation
    IRContext* ir_ctx = ir_create_context();
    ir_set_context(ir_ctx);

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
    metadata.source_file = "stdin";  // TODO: Pass actual filename
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
