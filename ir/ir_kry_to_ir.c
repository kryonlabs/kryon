/**
 * Kryon .kry AST to IR Converter
 *
 * Converts the parsed .kry AST into IR component trees.
 */

#include "ir_kry_parser.h"
#include "ir_kry_ast.h"
#include "ir_builder.h"
#include "ir_serialization.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Conversion Context
// ============================================================================

#define MAX_PARAMS 16

typedef struct {
    char* name;
    char* value;  // String representation of the value
} ParamSubstitution;

typedef struct {
    KryNode* ast_root;  // Root of AST (for finding component definitions)
    ParamSubstitution params[MAX_PARAMS];  // Parameter substitutions
    int param_count;
    IRReactiveManifest* manifest;  // Reactive manifest for state variables
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

// ============================================================================
// Parameter Substitution
// ============================================================================

static const char* substitute_param(ConversionContext* ctx, const char* expr) {
    if (!ctx || !expr) return expr;

    // Check if expression is a parameter reference
    for (int i = 0; i < ctx->param_count; i++) {
        if (strcmp(expr, ctx->params[i].name) == 0) {
            return ctx->params[i].value;
        }
    }

    return expr;  // No substitution found
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
        if (value->type == KRY_VALUE_STRING) {
            ir_set_text_content(component, value->string_value);
            return;
        } else if (value->type == KRY_VALUE_EXPRESSION) {
            // Handle expressions - apply parameter substitution
            const char* substituted = substitute_param(ctx, value->expression);
            ir_set_text_content(component, substituted);
            return;
        }
    }

    // Event handlers
    if (strcmp(name, "onClick") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION) {
            // Create click event with handler code
            IREvent* event = ir_create_event(IR_EVENT_CLICK, NULL, value->expression);
            if (event) {
                ir_add_event(component, event);
            }
            return;
        }
    }

    if (strcmp(name, "onHover") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION) {
            IREvent* event = ir_create_event(IR_EVENT_HOVER, NULL, value->expression);
            if (event) {
                ir_add_event(component, event);
            }
            return;
        }
    }

    if (strcmp(name, "onChange") == 0) {
        if (value->type == KRY_VALUE_EXPRESSION) {
            IREvent* event = ir_create_event(IR_EVENT_TEXT_CHANGE, NULL, value->expression);
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
        }
        return;
    }

    if (strcmp(name, "height") == 0) {
        if (value->type == KRY_VALUE_NUMBER) {
            IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
            ir_set_height(component, dim_type, (float)value->number_value);
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
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* color_str = value->type == KRY_VALUE_STRING ?
                                    value->string_value : value->identifier;
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
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* color_str = value->type == KRY_VALUE_STRING ?
                                    value->string_value : value->identifier;
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
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* color_str = value->type == KRY_VALUE_STRING ?
                                    value->string_value : value->identifier;
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
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* align = value->type == KRY_VALUE_STRING ?
                               value->string_value : value->identifier;
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
        if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
            const char* justify = value->type == KRY_VALUE_STRING ?
                                 value->string_value : value->identifier;
            IRAlignment alignment = IR_ALIGNMENT_START;
            if (strcmp(justify, "center") == 0) {
                alignment = IR_ALIGNMENT_CENTER;
            } else if (strcmp(justify, "start") == 0) {
                alignment = IR_ALIGNMENT_START;
            } else if (strcmp(justify, "end") == 0) {
                alignment = IR_ALIGNMENT_END;
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

    // Window properties (for App component)
    if (strcmp(name, "windowTitle") == 0 || strcmp(name, "windowWidth") == 0 ||
        strcmp(name, "windowHeight") == 0) {
        // These are metadata properties - handle separately if needed
        return;
    }

    // Default: ignore unknown properties
}

// ============================================================================
// AST to IR Conversion
// ============================================================================

static IRComponent* convert_node(ConversionContext* ctx, KryNode* node) {
    if (!node) return NULL;

    if (node->type == KRY_NODE_COMPONENT) {
        // Check if this is a custom component instantiation
        IRComponentType type = get_component_type(node->name);

        // If unknown type, check if it's a custom component
        if (type == IR_COMPONENT_CONTAINER) {
            KryNode* def = find_component_definition(ctx, node->name);
            if (def) {
                // Found custom component definition - expand it inline
                // Create a new context with parameter substitutions
                ConversionContext expanded_ctx = *ctx;  // Copy parent context

                // Parse arguments and set up parameter substitutions
                parse_arguments(&expanded_ctx, node->arguments ? node->arguments : "", def);

                if (def->first_child) {
                    // Find the actual content (skip state declarations)
                    KryNode* body_child = def->first_child;
                    while (body_child) {
                        if (body_child->type == KRY_NODE_COMPONENT) {
                            // Found the actual component content - expand with substitutions
                            return convert_node(&expanded_ctx, body_child);
                        }
                        body_child = body_child->next_sibling;
                    }
                }
            }
        }

        // Create component (built-in or fallback container)
        IRComponent* component = ir_create_component(type);
        if (!component) return NULL;

        // Process children
        KryNode* child = node->first_child;
        while (child) {
            if (child->type == KRY_NODE_PROPERTY) {
                // Apply property
                apply_property(ctx, component, child->name, child->value);
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

    // Convert AST to IR
    IRComponent* root = convert_node(&ctx, root_node);

    // Free parser (includes AST)
    kry_parser_free(parser);

    // TODO: Return manifest along with root (needs API change)
    // For now, just destroy it since we can't return it
    if (ctx.manifest) {
        ir_reactive_manifest_destroy(ctx.manifest);
    }

    return root;
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

    // Create conversion context with manifest
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.param_count = 0;
    ctx.manifest = ir_reactive_manifest_create();

    // Track all component definitions in the manifest
    KryNode* def_node = ast;
    while (def_node) {
        if (def_node->is_component_definition && def_node->name) {
            // Convert the component definition template
            IRComponent* template_comp = convert_node(&ctx, def_node);

            if (template_comp) {
                // Add to manifest (will be serialized as component_definitions in KIR)
                ir_reactive_manifest_add_component_def(
                    ctx.manifest,
                    def_node->name,
                    NULL, 0,  // TODO: Extract props from component definition
                    NULL, 0,  // TODO: Extract state vars from component definition
                    template_comp
                );
            }
        }
        def_node = def_node->next_sibling;
    }

    // Convert AST to IR
    IRComponent* root = convert_node(&ctx, root_node);

    // Free parser (includes AST)
    kry_parser_free(parser);

    if (!root) {
        if (ctx.manifest) {
            ir_reactive_manifest_destroy(ctx.manifest);
        }
        return NULL;
    }

    // Serialize with manifest
    char* json = ir_serialize_json(root, ctx.manifest);

    // Clean up
    ir_destroy_component(root);
    if (ctx.manifest) {
        ir_reactive_manifest_destroy(ctx.manifest);
    }

    return json;
}
