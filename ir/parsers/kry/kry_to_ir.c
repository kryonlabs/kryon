/**
 * Kryon .kry AST to IR Converter
 *
 * Converts the parsed .kry AST into IR component trees.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_parser.h"
#include "kry_ast.h"
#include "kry_expression.h"
#include "kry_to_ir_internal.h"
#include "kry_to_ir_expressions.h"
#include "kry_struct_to_ir.h"
#include "kry_ir_validator.h"
#include "../include/ir_builder.h"
#include "../include/ir_serialization.h"
#include "../include/ir_logic.h"
#include "../include/ir_style.h"
#include "../src/style/ir_stylesheet.h"
// Animation system moved to plugin - no longer included here
#include "../src/logic/ir_foreach.h"
#include "../../include/kryon/capability.h"  // For kryon_property_parser_fn
#include "../include/ir_capability.h"  // For ir_capability_on_context_change
#include "../html/css_parser.h"  // For ir_css_parse_color
#include "../parser_utils.h"     // For parser_parse_color_packed
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Internal Aliases for Context Functions
// ============================================================================

// These aliases allow existing code to use the shorter names
#define add_param(ctx, name, value) kry_ctx_add_param(ctx, name, value)
#define add_param_value(ctx, name, value) kry_ctx_add_param_value(ctx, name, value)
#define substitute_param(ctx, expr) kry_ctx_substitute_param(ctx, expr)
#define is_unresolved_expr(ctx, expr) kry_ctx_is_unresolved_expr(ctx, expr)

// Expression conversion aliases (kry_to_ir_expressions.c)
#define convert_binary_op(op) kry_convert_binary_op(op)
#define convert_unary_op(op) kry_convert_unary_op(op)
#define convert_kry_expr_to_ir(node) kry_convert_expr_to_ir(node)

// Context functions (add_param, substitute_param, is_unresolved_expr, kry_value_to_json)
// are now implemented in kry_to_ir_context.c and declared via kry_to_ir_internal.h

// ============================================================================
// Component Name Mapping
// ============================================================================

// Component type mapping entry
typedef struct {
    const char* name;
    IRComponentType type;
} ComponentTypeMapping;

// Component type lookup table (sorted for potential binary search)
// Note: Multiple names can map to the same type
static const ComponentTypeMapping component_type_table[] = {
    // Core layout
    {"a", IR_COMPONENT_LINK},
    {"app", IR_COMPONENT_CONTAINER},
    {"blockquote", IR_COMPONENT_BLOCKQUOTE},
    {"button", IR_COMPONENT_BUTTON},
    {"canvas", IR_COMPONENT_CANVAS},
    {"center", IR_COMPONENT_CENTER},
    {"checkbox", IR_COMPONENT_CHECKBOX},
    {"code", IR_COMPONENT_CODE_INLINE},
    {"codeblock", IR_COMPONENT_CODE_BLOCK},
    {"codeinline", IR_COMPONENT_CODE_INLINE},
    {"column", IR_COMPONENT_COLUMN},
    {"container", IR_COMPONENT_CONTAINER},
    {"dropdown", IR_COMPONENT_DROPDOWN},
    {"em", IR_COMPONENT_EM},
    {"heading", IR_COMPONENT_HEADING},
    {"input", IR_COMPONENT_INPUT},
    {"link", IR_COMPONENT_LINK},
    {"list", IR_COMPONENT_LIST},
    {"listitem", IR_COMPONENT_LIST_ITEM},
    {"mark", IR_COMPONENT_MARK},
    {"p", IR_COMPONENT_PARAGRAPH},
    {"paragraph", IR_COMPONENT_PARAGRAPH},
    {"row", IR_COMPONENT_ROW},
    {"small", IR_COMPONENT_SMALL},
    {"span", IR_COMPONENT_SPAN},
    {"strong", IR_COMPONENT_STRONG},
    {"tab", IR_COMPONENT_TAB},
    {"tabbar", IR_COMPONENT_TAB_BAR},
    {"tabcontent", IR_COMPONENT_TAB_CONTENT},
    {"tabgroup", IR_COMPONENT_TAB_GROUP},
    {"tabpanel", IR_COMPONENT_TAB_PANEL},
    {"table", IR_COMPONENT_TABLE},
    {"tablebody", IR_COMPONENT_TABLE_BODY},
    {"tablecell", IR_COMPONENT_TABLE_CELL},
    {"tablefoot", IR_COMPONENT_TABLE_FOOT},
    {"tablehead", IR_COMPONENT_TABLE_HEAD},
    {"tableheadercell", IR_COMPONENT_TABLE_HEADER_CELL},
    {"tablerow", IR_COMPONENT_TABLE_ROW},
    {"td", IR_COMPONENT_TABLE_CELL},
    {"text", IR_COMPONENT_TEXT},
    {"th", IR_COMPONENT_TABLE_HEADER_CELL},
    {"tr", IR_COMPONENT_TABLE_ROW},
    {NULL, IR_COMPONENT_CONTAINER}  // Sentinel
};

static IRComponentType get_component_type(const char* name) {
    if (!name) return IR_COMPONENT_CONTAINER;

    // Normalize to lowercase for comparison
    char lower[256];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && name[i]; i++) {
        lower[i] = tolower(name[i]);
    }
    lower[i] = '\0';

    // Linear search through table (could be optimized to binary search)
    for (int j = 0; component_type_table[j].name != NULL; j++) {
        if (strcmp(lower, component_type_table[j].name) == 0) {
            return component_type_table[j].type;
        }
    }

    // Default to container for unknown types
    return IR_COMPONENT_CONTAINER;
}

// Color parsing now uses parser_parse_color_packed() from parser_utils.h

// ============================================================================
// Property Handler System
// ============================================================================

// Property handler function type
typedef bool (*PropertyHandler)(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
);

// Property dispatch entry
typedef struct {
    const char* name;
    PropertyHandler handler;
    bool requires_style;  // true if handler needs IRStyle* to exist
} PropertyDispatchEntry;

// ============================================================================
// Value Handling Helpers
// ============================================================================

// Handle string/identifier/expression value types with substitution
static const char* resolve_value_as_string(
    ConversionContext* ctx,
    KryValue* value,
    bool* is_unresolved
) {
    *is_unresolved = false;

    switch (value->type) {
        case KRY_VALUE_STRING:
            return value->string_value;

        case KRY_VALUE_IDENTIFIER: {
            const char* substituted = substitute_param(ctx, value->identifier);
            *is_unresolved = is_unresolved_expr(ctx, value->identifier) &&
                            ctx->compile_mode == IR_COMPILE_MODE_HYBRID;
            return substituted;
        }

        case KRY_VALUE_EXPRESSION: {
            const char* substituted = substitute_param(ctx, value->expression);
            *is_unresolved = is_unresolved_expr(ctx, value->expression) &&
                            ctx->compile_mode == IR_COMPILE_MODE_HYBRID;
            return substituted;
        }

        default:
            return NULL;
    }
}

// Get value as boolean (handles IDENTIFIER: true/false)
static bool get_value_as_bool(KryValue* value, bool* out) {
    if (value->type == KRY_VALUE_IDENTIFIER) {
        if (strcmp(value->identifier, "true") == 0) {
            *out = true;
            return true;
        } else if (strcmp(value->identifier, "false") == 0) {
            *out = false;
            return true;
        }
    }
    return false;
}

// ============================================================================
// Component Property Handlers
// ============================================================================

// Handle className/class property
static bool handle_className_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;  // Unused
    (void)name; // Unused
    if (value->type == KRY_VALUE_STRING) {
        if (component->css_class) {
            free(component->css_class);
        }
        component->css_class = strdup(value->string_value);
        return true;
    }
    return false;
}

// Handle text property (complex: string/identifier/expression)
static bool handle_text_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    bool is_unresolved;
    const char* text = resolve_value_as_string(ctx, value, &is_unresolved);

    if (!text) {
        // Hard error if we can't resolve at all
        const char* expr = (value->type == KRY_VALUE_IDENTIFIER)
                          ? value->identifier
                          : (value->type == KRY_VALUE_EXPRESSION)
                          ? value->expression
                          : "(unknown)";
        if (ctx->parser) {
            kry_parser_add_error_fmt(ctx->parser, KRY_ERROR_ERROR,
                                     KRY_ERROR_VALIDATION,
                                     "Cannot resolve text property value: %s", expr);
        }
        return false;
    }

    if (is_unresolved) {
        // Create property binding for runtime evaluation
        const char* original = (value->type == KRY_VALUE_IDENTIFIER)
                              ? value->identifier : value->expression;
        ir_component_add_property_binding(component, name, original, text, "static_template");

        if (component->text_expression) {
            free(component->text_expression);
        }
        component->text_expression = strdup(original);
        return true;
    }

    ir_set_text_content(component, text);
    return true;
}

// Handle label property
static bool handle_label_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;  // Unused
    (void)name; // Unused
    if (value->type == KRY_VALUE_STRING) {
        ir_set_text_content(component, value->string_value);
        return true;
    }
    return false;
}

// Handle checked property
static bool handle_checked_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;  // Unused
    (void)name; // Unused
    bool checked;
    if (get_value_as_bool(value, &checked)) {
        ir_set_checkbox_state(component, checked);
        return true;
    }
    return false;
}

// ============================================================================
// Event Property Handlers
// ============================================================================

// Generic event handler creation helper
static bool create_event_handler(
    ConversionContext* ctx,
    IRComponent* component,
    const char* event_type,
    KryValue* value
) {
    fprintf(stderr, "[DEBUG] create_event_handler called: event_type=%s, value_type=%d, logic_block=%p\n",
            event_type, value ? value->type : -1, ctx ? ctx->logic_block : NULL);

    // Accept both IDENTIFIER and EXPRESSION types
    // IDENTIFIER: onClick = handleButtonClick
    // EXPRESSION: onClick = () => { ... }
    if ((value->type != KRY_VALUE_EXPRESSION && value->type != KRY_VALUE_IDENTIFIER) || !ctx->logic_block) {
        fprintf(stderr, "[DEBUG] create_event_handler returning false: value_type=%d (need %d or %d), logic_block=%p\n",
                value ? value->type : -1, KRY_VALUE_EXPRESSION, KRY_VALUE_IDENTIFIER, ctx ? ctx->logic_block : NULL);
        return false;
    }

    const char* handler_name;
    char gen_handler_name[64];
    bool is_lambda_expression = false;
    IRLogicFunction* func = NULL;

    // Get the handler name from either identifier or expression
    const char* expr = (value->type == KRY_VALUE_IDENTIFIER)
        ? value->identifier
        : value->expression;

    // Check if the expression is a simple identifier (just a function name)
    // Simple identifiers are alphanumeric with underscores, no spaces or special chars
    bool is_simple_identifier = true;
    for (size_t i = 0; expr[i] != '\0'; i++) {
        char c = expr[i];
        if (!isalnum((unsigned char)c) && c != '_') {
            is_simple_identifier = false;
            break;
        }
    }

    if (is_simple_identifier) {
        // Use the function name directly (it's defined in @c, @lua, @js blocks)
        handler_name = expr;
    } else {
        // Lambda expression: generate a unique handler name
        snprintf(gen_handler_name, sizeof(gen_handler_name), "handler_%u_%s",
                 ctx->next_handler_id++, event_type);
        handler_name = gen_handler_name;
        is_lambda_expression = true;

        // Create handler function for lambda expression
        func = ir_logic_function_create(handler_name);
        if (!func) return false;

        // Add source code to function
        ir_logic_function_add_source(func, "kry", value->expression);

        // Add function to logic block
        ir_logic_block_add_function(ctx->logic_block, func);
    }

    // Create event binding
    IREventBinding* binding = ir_event_binding_create(
        component->id, event_type, handler_name
    );
    if (binding) {
        ir_logic_block_add_binding(ctx->logic_block, binding);
    }

    // Map event_type string to IREventType enum
    IREventType event_enum;
    if (strcmp(event_type, "click") == 0) {
        event_enum = IR_EVENT_CLICK;
    } else if (strcmp(event_type, "hover") == 0) {
        event_enum = IR_EVENT_HOVER;
    } else if (strcmp(event_type, "change") == 0) {
        event_enum = IR_EVENT_TEXT_CHANGE;  // Change event for Input components
    } else {
        event_enum = IR_EVENT_CLICK;  // Default
    }

    // Create legacy IREvent for backwards compatibility
    // For simple identifiers, logic_id is the function name; for lambdas, it's the generated name
    IREvent* event = ir_create_event(event_enum, handler_name,
                                     is_lambda_expression ? value->expression : NULL);
    if (event) {
        ir_add_event(component, event);
        fprintf(stderr, "[DEBUG] Created event for component %d: type=%s, logic_id=%s\n",
                component->id, event_type, handler_name);
    } else {
        fprintf(stderr, "[DEBUG] Failed to create event for component %d: type=%s\n",
                component->id, event_type);
    }

    return true;
}

// Handle onClick property
static bool handle_onClick_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)name; // Unused
    fprintf(stderr, "[DEBUG] handle_onClick_property called: value_type=%d\n", value ? value->type : -1);
    return create_event_handler(ctx, component, "click", value);
}

// Handle onHover property
static bool handle_onHover_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)name; // Unused
    return create_event_handler(ctx, component, "hover", value);
}

// Handle onChange property
static bool handle_onChange_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)name; // Unused
    return create_event_handler(ctx, component, "change", value);
}

// ============================================================================
// Layout Property Handlers
// ============================================================================

// Handle width property (supports numbers and strings like "100px", "50%", "auto")
static bool handle_width_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_width(component, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strcmp(str, "auto") == 0) {
            ir_set_width(component, IR_DIMENSION_AUTO, 0);
        } else if (strstr(str, "%")) {
            float num = (float)atof(str);
            ir_set_width(component, IR_DIMENSION_PERCENT, num);
        } else {
            float num = (float)atof(str);
            ir_set_width(component, IR_DIMENSION_PX, num);
        }
        return true;
    }

    return false;
}

// Handle height property (supports numbers and strings like "100px", "50%", "auto")
static bool handle_height_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_height(component, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strcmp(str, "auto") == 0) {
            ir_set_height(component, IR_DIMENSION_AUTO, 0);
        } else if (strstr(str, "%")) {
            float num = (float)atof(str);
            ir_set_height(component, IR_DIMENSION_PERCENT, num);
        } else {
            float num = (float)atof(str);
            ir_set_height(component, IR_DIMENSION_PX, num);
        }
        return true;
    }

    return false;
}

// Handle minWidth property
static bool handle_minWidth_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    IRLayout* layout = ir_get_layout(component);
    if (!layout) {
        layout = ir_create_layout();
        ir_set_layout(component, layout);
    }

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_min_width(layout, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strstr(str, "%")) {
            ir_set_min_width(layout, IR_DIMENSION_PERCENT, (float)atof(str));
        } else {
            ir_set_min_width(layout, IR_DIMENSION_PX, (float)atof(str));
        }
        return true;
    }
    return false;
}

// Handle maxWidth property
static bool handle_maxWidth_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    IRLayout* layout = ir_get_layout(component);
    if (!layout) {
        layout = ir_create_layout();
        ir_set_layout(component, layout);
    }

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_max_width(layout, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strstr(str, "%")) {
            ir_set_max_width(layout, IR_DIMENSION_PERCENT, (float)atof(str));
        } else {
            ir_set_max_width(layout, IR_DIMENSION_PX, (float)atof(str));
        }
        return true;
    }
    return false;
}

// Handle minHeight property
static bool handle_minHeight_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    IRLayout* layout = ir_get_layout(component);
    if (!layout) {
        layout = ir_create_layout();
        ir_set_layout(component, layout);
    }

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_min_height(layout, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strstr(str, "%")) {
            ir_set_min_height(layout, IR_DIMENSION_PERCENT, (float)atof(str));
        } else {
            ir_set_min_height(layout, IR_DIMENSION_PX, (float)atof(str));
        }
        return true;
    }
    return false;
}

// Handle maxHeight property
static bool handle_maxHeight_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    IRLayout* layout = ir_get_layout(component);
    if (!layout) {
        layout = ir_create_layout();
        ir_set_layout(component, layout);
    }

    if (value->type == KRY_VALUE_NUMBER) {
        IRDimensionType dim_type = value->is_percentage ? IR_DIMENSION_PERCENT : IR_DIMENSION_PX;
        ir_set_max_height(layout, dim_type, (float)value->number_value);
        return true;
    } else if (value->type == KRY_VALUE_STRING) {
        const char* str = value->string_value;
        if (strstr(str, "%")) {
            ir_set_max_height(layout, IR_DIMENSION_PERCENT, (float)atof(str));
        } else {
            ir_set_max_height(layout, IR_DIMENSION_PX, (float)atof(str));
        }
        return true;
    }
    return false;
}

// Handle posX/left property
static bool handle_posX_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }
        style->position_mode = IR_POSITION_ABSOLUTE;
        style->absolute_x = (float)value->number_value;
        return true;
    }

    return false;
}

// Handle posY/top property
static bool handle_posY_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }
        style->position_mode = IR_POSITION_ABSOLUTE;
        style->absolute_y = (float)value->number_value;
        return true;
    }

    return false;
}

// Handle padding property (uniform padding on all sides)
static bool handle_padding_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        float p = (float)value->number_value;
        ir_set_padding(component, p, p, p, p);
        return true;
    }

    return false;
}

// Handle gap property (for flex layouts)
static bool handle_gap_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.gap = (uint32_t)value->number_value;
            return true;
        }
    }

    return false;
}

// ============================================================================
// Style Property Handlers (Colors, Borders, etc.)
// ============================================================================

// Helper: Parse color from string/identifier/expression
static uint32_t parse_color_value(ConversionContext* ctx, KryValue* value, const char* property_name, IRComponent* component) {
    const char* color_str = NULL;
    const char* original_expr = NULL;

    if (value->type == KRY_VALUE_STRING) {
        color_str = value->string_value;
    } else if (value->type == KRY_VALUE_IDENTIFIER) {
        original_expr = value->identifier;
        color_str = substitute_param(ctx, original_expr);
    } else if (value->type == KRY_VALUE_EXPRESSION) {
        original_expr = value->expression;
        color_str = substitute_param(ctx, original_expr);
    } else {
        return 0x000000FF;  // Default black
    }

    // Handle unresolved identifiers/expressions for property bindings
    if (original_expr && is_unresolved_expr(ctx, original_expr) &&
        ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
        ir_component_add_property_binding(component, property_name, original_expr, "#00000000", "static_template");
    }

    return parser_parse_color_packed(color_str);
}

// Handle backgroundColor/background property
static bool handle_backgroundColor_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    uint32_t color = parse_color_value(ctx, value, name, component);
    ir_set_background_color(style,
        (color >> 24) & 0xFF,
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF);

    return true;
}

// Handle color (text color) property
static bool handle_color_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    uint32_t color = parse_color_value(ctx, value, name, component);
    ir_set_font_color(style,
        (color >> 24) & 0xFF,
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF);

    return true;
}

// Handle borderColor property
static bool handle_borderColor_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    uint32_t color = parse_color_value(ctx, value, name, component);
    style->border.color.type = IR_COLOR_SOLID;
    style->border.color.data.r = (color >> 24) & 0xFF;
    style->border.color.data.g = (color >> 16) & 0xFF;
    style->border.color.data.b = (color >> 8) & 0xFF;
    style->border.color.data.a = color & 0xFF;

    return true;
}

// Handle borderWidth property
static bool handle_borderWidth_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }
        style->border.width = (float)value->number_value;
        return true;
    }

    return false;
}

// Handle borderRadius property
static bool handle_borderRadius_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }
        style->border.radius = (float)value->number_value;
        return true;
    }

    return false;
}

// ============================================================================
// Font Property Handlers
// ============================================================================

// Handle fontSize property
static bool handle_fontSize_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    if (value->type == KRY_VALUE_NUMBER) {
        style->font.size = (float)value->number_value;
        return true;
    } else if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
        const char* size_expr = (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier : value->expression;
        const char* size_str = substitute_param(ctx, size_expr);

        // Handle unresolved expressions
        if (is_unresolved_expr(ctx, size_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
            ir_component_add_property_binding(component, name, size_expr, "16", "static_template");
        }

        style->font.size = (float)atof(size_str);
        return true;
    }

    return false;
}

// Handle fontWeight property
static bool handle_fontWeight_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }
        style->font.weight = (uint16_t)value->number_value;
        return true;
    }

    return false;
}

// Handle fontFamily property
static bool handle_fontFamily_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER) {
        IRStyle* style = ir_get_style(component);
        if (!style) {
            style = ir_create_style();
            ir_set_style(component, style);
        }

        const char* family = value->type == KRY_VALUE_STRING ?
                            value->string_value : value->identifier;
        strncpy(style->font.family, family, sizeof(style->font.family) - 1);
        style->font.family[sizeof(style->font.family) - 1] = '\0';
        return true;
    }

    return false;
}

// ============================================================================
// Window Property Handlers (for App component)
// ============================================================================

// External global context
extern IRContext* g_ir_context;

// Handle windowTitle property
static bool handle_windowTitle_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)component;
    (void)name;

    if (value->type == KRY_VALUE_STRING) {
        IRContext* ir_ctx = g_ir_context;
        if (ir_ctx) {
            if (!ir_ctx->metadata) {
                ir_ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
            }
            if (ir_ctx->metadata) {
                if (ir_ctx->metadata->window_title) {
                    free(ir_ctx->metadata->window_title);
                }
                size_t len = strlen(value->string_value);
                ir_ctx->metadata->window_title = (char*)malloc(len + 1);
                if (ir_ctx->metadata->window_title) {
                    strcpy(ir_ctx->metadata->window_title, value->string_value);
                }
            }
        }
        return true;
    }

    return false;
}

// Handle windowWidth property
static bool handle_windowWidth_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)component;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRContext* ir_ctx = g_ir_context;
        if (ir_ctx) {
            if (!ir_ctx->metadata) {
                ir_ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
            }
            if (ir_ctx->metadata) {
                ir_ctx->metadata->window_width = (int)value->number_value;
            }
        }
        return true;
    }

    return false;
}

// Handle windowHeight property
static bool handle_windowHeight_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)component;
    (void)name;

    if (value->type == KRY_VALUE_NUMBER) {
        IRContext* ir_ctx = g_ir_context;
        if (ir_ctx) {
            if (!ir_ctx->metadata) {
                ir_ctx->metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
            }
            if (ir_ctx->metadata) {
                ir_ctx->metadata->window_height = (int)value->number_value;
            }
        }
        return true;
    }

    return false;
}

// ============================================================================
// Tab Component Property Handlers
// ============================================================================

// Handle selectedIndex property (for TabGroup and Dropdown components)
static bool handle_selectedIndex_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    int32_t index_value = 0;

    // Get the numeric value
    if (value->type == KRY_VALUE_NUMBER) {
        index_value = (int32_t)value->number_value;
    } else if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
        const char* expr = (value->type == KRY_VALUE_IDENTIFIER)
                          ? value->identifier : value->expression;

        // Try to substitute if it's a known parameter
        const char* substituted = substitute_param(ctx, expr);

        // Try to parse as number if substituted
        if (substituted && substituted != expr) {
            char* endptr;
            long val = strtol(substituted, &endptr, 10);
            if (*endptr == '\0') {
                index_value = (int32_t)val;
            } else {
                // Variable reference or expression - create property binding for runtime evaluation
                ir_component_add_property_binding(component, name, expr, "0", "static_template");
                index_value = 0;  // Default value
            }
        } else {
            // Variable reference or expression - create property binding for runtime evaluation
            ir_component_add_property_binding(component, name, expr, "0", "static_template");
            index_value = 0;  // Default value
        }
    } else {
        return false;
    }

    // Handle Dropdown components
    if (component->type == IR_COMPONENT_DROPDOWN) {
        // Ensure dropdown state exists
        if (!component->custom_data) {
            component->custom_data = calloc(1, sizeof(IRDropdownState));
        }
        ir_set_dropdown_selected_index(component, index_value);
        return true;
    }

    // Handle TabGroup components
    if (component->type == IR_COMPONENT_TAB_GROUP) {
        // Ensure tab_data exists
        if (!component->tab_data) {
            component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
        }
        if (component->tab_data) {
            component->tab_data->selected_index = index_value;
        }
        return true;
    }

    return false;
}

// ============================================================================
// Dropdown Property Handlers
// ============================================================================

// Handle options property (for Dropdown components)
static bool handle_options_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    // Only handle array values for Dropdown components
    if (component->type != IR_COMPONENT_DROPDOWN) {
        return false;
    }

    if (value->type != KRY_VALUE_ARRAY) {
        if (ctx && ctx->parser) {
            kry_parser_add_error(ctx->parser, KRY_ERROR_ERROR, KRY_ERROR_CONVERSION,
                                "options property must be an array");
        }
        return false;
    }

    // Ensure dropdown state exists
    if (!component->custom_data) {
        component->custom_data = calloc(1, sizeof(IRDropdownState));
    }

    IRDropdownState* state = (IRDropdownState*)component->custom_data;
    if (!state) {
        return false;
    }

    // Convert KryValue array to C string array
    uint32_t count = (uint32_t)value->array.count;
    if (count > 0) {
        char** options = (char**)malloc(sizeof(char*) * count);
        for (uint32_t i = 0; i < count; i++) {
            KryValue* item = value->array.elements[i];
            if (item->type == KRY_VALUE_STRING) {
                options[i] = item->string_value;
            } else if (item->type == KRY_VALUE_IDENTIFIER) {
                options[i] = item->identifier;
            } else {
                options[i] = "";  // Empty string for non-string values
            }
        }

        // Call IR function to set options (takes ownership of strings)
        ir_set_dropdown_options(component, options, count);

        // Free the temporary array (strings are now owned by IRDropdownState)
        free(options);
    }

    return true;
}

// ============================================================================
// Alignment Property Handlers
// ============================================================================

// Helper: Parse alignment string to enum
static IRAlignment parse_alignment(const char* align_str) {
    if (strcmp(align_str, "center") == 0) return IR_ALIGNMENT_CENTER;
    else if (strcmp(align_str, "start") == 0) return IR_ALIGNMENT_START;
    else if (strcmp(align_str, "end") == 0) return IR_ALIGNMENT_END;
    else if (strcmp(align_str, "flex-start") == 0) return IR_ALIGNMENT_START;
    else if (strcmp(align_str, "flex-end") == 0) return IR_ALIGNMENT_END;
    else if (strcmp(align_str, "space-between") == 0) return IR_ALIGNMENT_SPACE_BETWEEN;
    else if (strcmp(align_str, "space-around") == 0) return IR_ALIGNMENT_SPACE_AROUND;
    else if (strcmp(align_str, "stretch") == 0) return IR_ALIGNMENT_STRETCH;
    return IR_ALIGNMENT_START;
}

// Handle contentAlignment/alignItems property
static bool handle_contentAlignment_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
        const char* align = (value->type == KRY_VALUE_STRING) ? value->string_value :
                           (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                           value->expression;

        // Handle parameter substitution and unresolved expressions
        const char* original_expr = align;
        if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            align = substitute_param(ctx, align);
            if (is_unresolved_expr(ctx, original_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                ir_component_add_property_binding(component, name, original_expr, "start", "static_template");
            }
        }

        IRAlignment alignment = parse_alignment(align);
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.cross_axis = alignment;
        }
        return true;
    }

    return false;
}

// Handle justifyContent property
static bool handle_justifyContent_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (value->type == KRY_VALUE_STRING || value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
        const char* justify = (value->type == KRY_VALUE_STRING) ? value->string_value :
                              (value->type == KRY_VALUE_IDENTIFIER) ? value->identifier :
                              value->expression;

        // Handle parameter substitution and unresolved expressions
        const char* original_expr = justify;
        if (value->type == KRY_VALUE_IDENTIFIER || value->type == KRY_VALUE_EXPRESSION) {
            justify = substitute_param(ctx, justify);
            if (is_unresolved_expr(ctx, original_expr) && ctx->compile_mode == IR_COMPILE_MODE_HYBRID) {
                ir_component_add_property_binding(component, name, original_expr, "start", "static_template");
            }
        }

        IRAlignment alignment = parse_alignment(justify);
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.justify_content = alignment;
        }
        return true;
    }

    return false;
}

// ============================================================================
// Property Dispatch Table
// ============================================================================

// Property dispatch table - maps property names to handler functions
static const PropertyDispatchEntry property_dispatch_table[] = {
    // Component properties
    {"class",            handle_className_property,      false},
    {"className",        handle_className_property,      false},
    {"text",             handle_text_property,           false},
    {"label",            handle_label_property,          false},
    {"checked",          handle_checked_property,        false},

    // Event properties
    {"onClick",          handle_onClick_property,        false},
    {"onHover",          handle_onHover_property,        false},
    {"onChange",         handle_onChange_property,       false},

    // Layout properties
    {"width",            handle_width_property,          true},
    {"height",           handle_height_property,         true},
    {"posX",             handle_posX_property,           false},
    {"posY",             handle_posY_property,           false},
    {"left",             handle_posX_property,           false},  // Alias for posX
    {"top",              handle_posY_property,           false},  // Alias for posY
    {"minWidth",         handle_minWidth_property,       false},
    {"maxWidth",         handle_maxWidth_property,       false},
    {"minHeight",        handle_minHeight_property,      false},
    {"maxHeight",        handle_maxHeight_property,      false},
    {"padding",          handle_padding_property,        false},
    {"gap",              handle_gap_property,            false},

    // Style properties
    {"backgroundColor",  handle_backgroundColor_property, true},
    {"background",       handle_backgroundColor_property, true},   // Alias
    {"color",            handle_color_property,          true},
    {"borderColor",      handle_borderColor_property,    true},
    {"borderWidth",      handle_borderWidth_property,    true},
    {"borderRadius",     handle_borderRadius_property,   true},

    // Font properties
    {"fontSize",         handle_fontSize_property,       true},
    {"fontWeight",       handle_fontWeight_property,     true},
    {"fontFamily",       handle_fontFamily_property,     true},

    // Window properties (for App component)
    {"windowTitle",      handle_windowTitle_property,    false},
    {"windowWidth",      handle_windowWidth_property,    false},
    {"windowHeight",     handle_windowHeight_property,   false},

    // Alignment properties
    {"contentAlignment", handle_contentAlignment_property, false},
    {"alignItems",       handle_contentAlignment_property, false},  // Alias
    {"justifyContent",   handle_justifyContent_property,   false},

    // Tab component properties
    {"selectedIndex",    handle_selectedIndex_property,    false},

    // Dropdown component properties
    {"options",          handle_options_property,          false},

    // Sentinel (marks end of table)
    {NULL,               NULL,                           false}
};

// Dispatch property to appropriate handler
static bool dispatch_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (!component || !name || !value) {
        if (ctx && ctx->parser) {
            kry_parser_add_error(ctx->parser, KRY_ERROR_ERROR, KRY_ERROR_CONVERSION,
                                "Invalid property assignment (NULL component, name, or value)");
        }
        return false;
    }

    // Search dispatch table for handler
    for (int i = 0; property_dispatch_table[i].name != NULL; i++) {
        if (strcmp(name, property_dispatch_table[i].name) == 0) {
            // Ensure style exists if handler requires it
            if (property_dispatch_table[i].requires_style) {
                IRStyle* style = ir_get_style(component);
                if (!style) {
                    style = ir_create_style();
                    ir_set_style(component, style);
                }
            }

            // Call handler
            return property_dispatch_table[i].handler(ctx, component, name, value);
        }
    }

    // Property not found in dispatch table - fall through to old apply_property
    return false;
}

// ============================================================================
// Property Application (Old - To Be Replaced)
// ============================================================================

static bool apply_property(ConversionContext* ctx, IRComponent* component, const char* name, KryValue* value) {
    if (!component || !name || !value) {
        if (ctx && ctx->parser) {
            kry_parser_add_error(ctx->parser, KRY_ERROR_ERROR, KRY_ERROR_CONVERSION,
                                 "Invalid property assignment (NULL component, name, or value)");
        }
        return false;
    }

    // Property aliases for App component (root container)
    // Map user-friendly names to internal window properties
    if (component->id == 1) {  // Root component (App)
        if (strcmp(name, "title") == 0) {
            name = "windowTitle";
        } else if (strcmp(name, "width") == 0 && value->type == KRY_VALUE_NUMBER) {
            // For App, width sets window width, but also fall through to set container width
            apply_property(ctx, component, "windowWidth", value);
        } else if (strcmp(name, "height") == 0 && value->type == KRY_VALUE_NUMBER) {
            // For App, height sets window height, but also fall through to set container height
            apply_property(ctx, component, "windowHeight", value);
        }
    }

    // Dispatch to property handler
    bool handled = dispatch_property(ctx, component, name, value);
    if (handled) {
        return true;
    }

    // Property not found in dispatch table - check if plugin registered a parser
    extern kryon_property_parser_fn ir_get_property_parser(const char* name);
    kryon_property_parser_fn parser = ir_get_property_parser(name);

    if (parser) {
        // Plugin has registered a parser for this property
        // TODO: Plugin parser interface should accept KryValue* instead of const char*
        const char* str_value = (value && value->type == KRY_VALUE_STRING) ? value->string_value : "";
        bool success = parser(component->id, name, str_value, &component->plugin_data);
        if (!success && ctx && ctx->parser) {
            kry_parser_add_error_fmt(ctx->parser, KRY_ERROR_ERROR, KRY_ERROR_CONVERSION,
                                     "Failed to parse property '%s'", name);
        }
        return success;
    }

    // No parser registered - HARD ERROR
    if (ctx && ctx->parser) {
        kry_parser_add_error_fmt(ctx->parser, KRY_ERROR_ERROR, KRY_ERROR_CONVERSION,
                                 "Unknown property '%s' - no plugin registered to handle this property.\n"
                                 "                 Check if required plugin is enabled in kryon.toml", name);
    }
    return false;
}
// ============================================================================
// AST to IR Conversion
// ============================================================================

// Forward declarations
static IRComponent* convert_node(ConversionContext* ctx, KryNode* node);
static bool is_custom_component(const char* name, IRReactiveManifest* manifest);
static IRComponent* expand_component_template(const char* comp_name,
                                                IRReactiveManifest* manifest,
                                                const char* instance_scope,
                                                const char** inheritance_chain,
                                                int chain_depth);
static IRComponent* ir_component_clone_tree(IRComponent* source);
static void merge_component_trees(IRComponent* parent, IRComponent* child_template);

// Expand for loop by unrolling at compile time
static void expand_for_loop(ConversionContext* ctx, IRComponent* parent, KryNode* for_node) {
    if (!for_node || for_node->type != KRY_NODE_FOR_LOOP) {
        return;
    }

    const char* iter_name = for_node->name;  // Iterator variable name
    KryValue* collection = for_node->value;  // Collection to iterate over

    if (!collection || !iter_name) {
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
        for (size_t i = 0; i < collection->array.count; i++) {
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
        bool resolved = false;
        for (int i = 0; i < ctx->param_count; i++) {
            if (strcmp(ctx->params[i].name, collection_name) == 0) {
                // Found the variable - check if it's a KryValue
                if (ctx->params[i].kry_value != NULL) {
                    // Recursively call expand_for_loop with the resolved KryValue
                    KryNode temp_for_node = *for_node;
                    temp_for_node.value = ctx->params[i].kry_value;
                    expand_for_loop(ctx, parent, &temp_for_node);
                    resolved = true;
                }
                break;
            }
        }

        // If collection couldn't be resolved at compile time, create a runtime ForEach component
        if (!resolved) {
            // Create ForEach component for runtime iteration
            IRComponent* for_each_comp = ir_create_component(IR_COMPONENT_FOR_EACH);
            if (for_each_comp) {
                // Create ForEach definition
                struct IRForEachDef* def = ir_foreach_def_create(iter_name, "index");
                if (def) {
                    // Set data source
                    ir_foreach_set_source_variable(def, collection_name);

                    // Convert the loop body template
                    if (for_node->first_child && for_node->first_child->type == KRY_NODE_COMPONENT) {
                        KryNode* template_node = for_node->first_child;

                        // Create a child conversion context without parameter substitution
                        ConversionContext template_ctx = *ctx;
                        template_ctx.param_count = 0;  // Clear params for template

                        IRComponent* template_comp = convert_node(&template_ctx, template_node);
                        if (template_comp) {
                            ir_foreach_set_template(def, template_comp);
                            // Add template as child[0] for serialization
                            ir_add_child(for_each_comp, template_comp);
                        }
                    }

                    // Attach foreach_def to the component
                    for_each_comp->foreach_def = def;
                }

                ir_add_child(parent, for_each_comp);
            }
        }
        return;
    } else if (collection->type == KRY_VALUE_RANGE) {
        // Range expression: 0..n
        KryValue* start = collection->range.start;
        KryValue* end = collection->range.end;

        // For compile-time expansion, both start and end must be numeric literals
        if (start && end && start->type == KRY_VALUE_NUMBER && end->type == KRY_VALUE_NUMBER) {
            int start_val = (int)start->number_value;
            int end_val = (int)end->number_value;

            for (int i = start_val; i < end_val; i++) {
                // Create context with iterator bound to current value
                ConversionContext loop_ctx = *ctx;
                char* num_buf = (char*)malloc(32);
                snprintf(num_buf, 32, "%d", i);
                add_param(&loop_ctx, iter_name, num_buf);

                // Convert loop body children
                KryNode* loop_body_child = for_node->first_child;
                while (loop_body_child) {
                    if (loop_body_child->type == KRY_NODE_COMPONENT) {
                        IRComponent* child_component = convert_node(&loop_ctx, loop_body_child);
                        if (child_component) {
                            ir_add_child(parent, child_component);
                        }
                    }
                    loop_body_child = loop_body_child->next_sibling;
                }
                free(num_buf);
            }
        }
        // Note: For ranges with expression bounds (e.g., 0..n where n is a variable),
        // these must be handled at runtime. Currently this is silently ignored in UI context.
        // In function context, the for loop is converted to statements which handle this.
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

// ============================================================================
// Function Statement Conversion Helpers
// ============================================================================

// Forward declaration for recursive statement conversion
static IRStatement* convert_function_stmt(ConversionContext* ctx, KryNode* stmt, IRLogicFunction* func);


// Convert a list of child statements to an array of IRStatement*
// Returns the count of converted statements
static int convert_child_statements(ConversionContext* ctx, KryNode* first_child,
                                    IRLogicFunction* func, IRStatement*** out_stmts) {
    if (!first_child || !out_stmts) {
        *out_stmts = NULL;
        return 0;
    }

    // First pass: count statements
    int count = 0;
    KryNode* child = first_child;
    while (child) {
        count++;
        child = child->next_sibling;
    }

    if (count == 0) {
        *out_stmts = NULL;
        return 0;
    }

    // Allocate array
    IRStatement** stmts = calloc(count, sizeof(IRStatement*));
    if (!stmts) {
        *out_stmts = NULL;
        return 0;
    }

    // Second pass: convert statements
    child = first_child;
    int actual_count = 0;
    for (int i = 0; i < count && child; i++) {
        IRStatement* ir_stmt = convert_function_stmt(ctx, child, func);
        if (ir_stmt) {
            stmts[actual_count++] = ir_stmt;
        }
        child = child->next_sibling;
    }

    *out_stmts = stmts;
    return actual_count;
}

// Convert a single function body statement to IRStatement
// This is the recursive helper for handling nested control flow
static IRStatement* convert_function_stmt(ConversionContext* ctx, KryNode* stmt, IRLogicFunction* func) {
    if (!stmt) return NULL;

    IRStatement* ir_stmt = NULL;

    switch (stmt->type) {
        case KRY_NODE_RETURN_STMT: {
            // Handle return statement
            IRExpression* return_value = NULL;
            if (stmt->return_expr && stmt->return_expr->value) {
                return_value = kry_value_to_ir_expr(stmt->return_expr->value);
            }
            ir_stmt = ir_stmt_return(return_value);
            break;
        }

        case KRY_NODE_DELETE_STMT: {
            // Delete statement: delete <expression>
            IRExpression* target_expr = NULL;
            if (stmt->delete_target && stmt->delete_target->value) {
                target_expr = kry_value_to_ir_expr(stmt->delete_target->value);
            }
            if (target_expr) {
                ir_stmt = ir_stmt_delete(target_expr);
            }
            break;
        }

        case KRY_NODE_VAR_DECL:
        case KRY_NODE_PROPERTY: {
            // Variable declaration (const/let/var name = value) or
            // Property assignment (name = value) - both have same structure
            if (stmt->name && stmt->value) {
                IRExpression* value_expr = kry_value_to_ir_expr(stmt->value);
                if (value_expr) {
                    ir_stmt = ir_stmt_assign(stmt->name, value_expr);
                }
            }
            break;
        }

        case KRY_NODE_IF: {
            // If statement: if condition { then_body } else { else_body }
            IRExpression* condition = kry_value_to_ir_expr(stmt->value);

            // Convert then-branch (children of the if node)
            IRStatement** then_stmts = NULL;
            int then_count = convert_child_statements(ctx, stmt->first_child, func, &then_stmts);

            // Convert else-branch (children of the else_branch node)
            IRStatement** else_stmts = NULL;
            int else_count = 0;
            if (stmt->else_branch) {
                else_count = convert_child_statements(ctx, stmt->else_branch->first_child, func, &else_stmts);
            }

            ir_stmt = ir_stmt_if(condition, then_stmts, then_count, else_stmts, else_count);

            // Free the arrays (ir_stmt_if makes copies)
            free(then_stmts);
            free(else_stmts);
            break;
        }

        case KRY_NODE_FOR_LOOP:
        case KRY_NODE_FOR_EACH: {
            // For loop: for item in collection { body }
            // Both FOR_LOOP and FOR_EACH have same structure in function context
            const char* item_name = stmt->name;
            IRExpression* iterable = kry_value_to_ir_expr(stmt->value);

            // Convert body statements
            IRStatement** body_stmts = NULL;
            int body_count = convert_child_statements(ctx, stmt->first_child, func, &body_stmts);

            ir_stmt = ir_stmt_for_each(item_name, iterable, body_stmts, body_count);

            // Free the array (ir_stmt_for_each makes copies)
            free(body_stmts);
            break;
        }

        case KRY_NODE_CODE_BLOCK: {
            // Platform-specific code block: @lua { ... } or @js { ... }
            // Add the embedded source to the function
            if (stmt->code_language && stmt->code_source) {
                ir_logic_function_add_source(func, stmt->code_language, stmt->code_source);
            }
            // CODE_BLOCK doesn't produce an IRStatement itself
            ir_stmt = NULL;
            break;
        }

        case KRY_NODE_COMPONENT: {
            // Nested components in function body - ignore for now
            // These could be embedded UI components in the future
            ir_stmt = NULL;
            break;
        }

        default:
            // Unknown statement type - skip
            ir_stmt = NULL;
            break;
    }

    return ir_stmt;
}

// Convert return statement to IRStatement
// Returns NULL if node is not a return statement
static IRStatement* convert_return_stmt(KryNode* node) {
    if (!node || node->type != KRY_NODE_RETURN_STMT) {
        return NULL;
    }

    // Create return statement with optional expression
    IRExpression* return_value = NULL;

    if (node->return_expr && node->return_expr->value) {
        // Convert the return expression to IRExpression
        // For now, we'll create a simple identifier or literal expression
        KryValue* value = node->return_expr->value;

        if (value->type == KRY_VALUE_IDENTIFIER) {
            return_value = ir_expr_var(value->identifier);
        } else if (value->type == KRY_VALUE_STRING) {
            return_value = ir_expr_string(value->string_value);
        } else if (value->type == KRY_VALUE_NUMBER) {
            return_value = ir_expr_float(value->number_value);
        } else if (value->type == KRY_VALUE_EXPRESSION) {
            // For expression blocks, create an identifier expression
            return_value = ir_expr_var(value->expression);
        }
    }

    return ir_stmt_return(return_value);
}

// Convert module-level return statement to IRModuleExport entries
// Returns true on success, false on failure
static bool convert_module_return(ConversionContext* ctx, KryNode* node) {
    if (!node || node->type != KRY_NODE_MODULE_RETURN) {
        return false;
    }


    if (!ctx->source_structures) {
        return false;
    }

    // For each export name, find the declaration and create IRModuleExport
    for (int i = 0; i < node->export_count; i++) {
        char* export_name = node->export_names[i];
        if (!export_name) continue;

        // Find the declaration in current scope
        // Search through children of the root/component for matching name
        KryNode* decl = NULL;
        KryNode* child = ctx->ast_root->first_child;

        while (child) {
            if ((child->type == KRY_NODE_VAR_DECL && strcmp(child->name, export_name) == 0) ||
                (child->type == KRY_NODE_FUNCTION_DECL && strcmp(child->func_name, export_name) == 0) ||
                (child->type == KRY_NODE_STRUCT_DECL && strcmp(child->struct_name, export_name) == 0)) {
                decl = child;
                break;
            }
            child = child->next_sibling;
        }

        if (!decl) {
            continue;
        }

        // Create IRModuleExport
        IRModuleExport* export = (IRModuleExport*)calloc(1, sizeof(IRModuleExport));
        if (!export) continue;

        export->name = strdup(export_name);
        export->line = node->line;

        if (decl->type == KRY_NODE_VAR_DECL) {
            // Export constant
            export->type = "constant";
            // Convert value to JSON string
            if (decl->value) {
                export->value_json = kry_value_to_json(decl->value);
            }
        } else if (decl->type == KRY_NODE_FUNCTION_DECL) {
            // Export function
            export->type = "function";
            export->function_name = strdup(decl->func_name);
        } else if (decl->type == KRY_NODE_STRUCT_DECL) {
            // Export struct type
            export->type = "struct";

            // Convert struct to IRStructType
            IRStructType* struct_type = kry_convert_struct_decl(ctx, decl);
            export->struct_def = struct_type;

        }

        // Add to source structures
        // Expand exports array if needed
        if (ctx->source_structures->export_count >= ctx->source_structures->export_capacity) {
            ctx->source_structures->export_capacity = (ctx->source_structures->export_capacity == 0) ? 16 : ctx->source_structures->export_capacity * 2;
            ctx->source_structures->exports = (IRModuleExport**)realloc(
                ctx->source_structures->exports,
                sizeof(IRModuleExport*) * ctx->source_structures->export_capacity
            );
        }

        ctx->source_structures->exports[ctx->source_structures->export_count++] = export;
    }

    return true;
}

// Convert function declaration to IRLogicFunction
// Returns NULL if node is not a function declaration
static IRLogicFunction* convert_function_decl(ConversionContext* ctx, KryNode* node, const char* scope) {
    if (!node || node->type != KRY_NODE_FUNCTION_DECL) {
        return NULL;
    }


    // Create function name with scope prefix
    char func_name[512];
    if (scope && scope[0] != '\0') {
        snprintf(func_name, sizeof(func_name), "%s:%s", scope, node->func_name);
    } else {
        snprintf(func_name, sizeof(func_name), "%s", node->func_name);
    }

    // Create IRLogicFunction
    IRLogicFunction* func = ir_logic_function_create(func_name);
    if (!func) {
        return NULL;
    }

    // Set return type
    if (node->func_return_type) {
        func->return_type = strdup(node->func_return_type);
    }

    // Add parameters
    for (int i = 0; i < node->param_count; i++) {
        KryNode* param = node->func_params[i];
        if (param && param->name) {
            // Use the type from var_type field if available, otherwise default to "any"
            const char* param_type = param->var_type ? param->var_type : "any";
            ir_logic_function_add_param(func, param->name, param_type);
        }
    }

    // Convert function body statements using the recursive helper
    if (node->func_body) {
        KryNode* stmt = node->func_body->first_child;
        while (stmt) {
            // Use the unified statement converter which handles:
            // - KRY_NODE_RETURN_STMT
            // - KRY_NODE_VAR_DECL
            // - KRY_NODE_IF (with nested then/else branches)
            // - KRY_NODE_FOR_LOOP (for i in range)
            // - KRY_NODE_FOR_EACH (for item in collection)
            // - KRY_NODE_CODE_BLOCK (@lua/@js blocks)
            // - KRY_NODE_COMPONENT (ignored)
            IRStatement* ir_stmt = convert_function_stmt(ctx, stmt, func);
            if (ir_stmt) {
                ir_logic_function_add_stmt(func, ir_stmt);
            }

            stmt = stmt->next_sibling;
        }
    }

    return func;
}

// Convert "for each" loop to ForEach IR component
// This creates an IR_COMPONENT_FOR_EACH with foreach_def metadata
// Unlike "for" loops, "for each" does NOT expand at compile time
static IRComponent* convert_for_each_node(ConversionContext* ctx, KryNode* for_each_node) {
    if (!for_each_node || for_each_node->type != KRY_NODE_FOR_EACH) {
        return NULL;
    }

    const char* item_name = for_each_node->name;  // Iterator variable name
    KryValue* collection = for_each_node->value;  // Collection to iterate over

    if (!collection || !item_name) {
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
        return NULL;
    }

    // Create ForEach definition
    struct IRForEachDef* def = ir_foreach_def_create(item_name, "index");
    if (!def) {
        // Note: ir_component_destroy may not be available, leak is acceptable for error case
        return NULL;
    }

    // Set loop type to explicit for_each
    def->loop_type = IR_LOOP_TYPE_FOR_EACH;

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

            // Add template as child[0] for serialization
            // The serializer expects the template to be in children array
            ir_add_child(for_each_comp, template_comp);

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

    return for_each_comp;
}

// Helper: Check if an argument string looks like a variable reference (not a literal)
static bool is_variable_reference(const char* arg) {
    if (!arg) return false;
    // Skip leading whitespace
    while (*arg == ' ') arg++;
    // Check if it starts with a digit or quote (literal)
    if (*arg >= '0' && *arg <= '9') return false;
    if (*arg == '"' || *arg == '\'') return false;
    if (*arg == '[' || *arg == '{') return false;  // Array/object literal
    if (strcmp(arg, "true") == 0 || strcmp(arg, "false") == 0) return false;
    if (strcmp(arg, "null") == 0) return false;
    // Looks like a variable reference (identifier)
    return (*arg >= 'a' && *arg <= 'z') || (*arg >= 'A' && *arg <= 'Z') || *arg == '_';
}

static IRComponent* convert_node(ConversionContext* ctx, KryNode* node) {
    if (!node) return NULL;

    if (node->type == KRY_NODE_COMPONENT) {
        // Check if this is a custom component instantiation
        if (ctx->manifest && is_custom_component(node->name, ctx->manifest)) {

            // SPECIAL CASE: When in template context (param_count == 0) and the component
            // has arguments that look like variable references, preserve it as a component
            // reference for runtime instantiation. This supports patterns like HabitPanel(habit)
            // inside for loops.
            if (ctx->param_count == 0 && node->arguments && is_variable_reference(node->arguments)) {
                // Create a placeholder component with component_ref and component_props
                IRComponent* ref_comp = ir_create_component(IR_COMPONENT_CUSTOM);
                if (ref_comp) {
                    ref_comp->component_ref = strdup(node->name);
                    // Store props as JSON for serialization
                    // Format: {"arg": "expression"}
                    char props_json[256];
                    snprintf(props_json, sizeof(props_json), "{\"arg\":\"%s\"}", node->arguments);
                    ref_comp->component_props = strdup(props_json);
                }
                return ref_comp;
            }

            // Generate unique instance ID
            static uint32_t instance_counter = 0;
            char instance_scope[128];
            snprintf(instance_scope, sizeof(instance_scope),
                     "%s#%u", node->name, instance_counter++);

            // Expand component template
            IRComponent* instance = expand_component_template(
                node->name,
                ctx->manifest,
                instance_scope,
                NULL,  // inheritance_chain
                0     // chain_depth
            );

            if (instance) {

                // Apply instance arguments to initialize state variables
                // Find the component definition
                IRComponentDefinition* def = NULL;
                for (uint32_t i = 0; i < ctx->manifest->component_def_count; i++) {
                    if (strcmp(ctx->manifest->component_defs[i].name, node->name) == 0) {
                        def = &ctx->manifest->component_defs[i];
                        break;
                    }
                }

                if (def) {
                    fprintf(stderr, "[DEBUG] Found component definition '%s' with %u state vars\n", node->name, def->state_var_count);
                } else {
                    fprintf(stderr, "[DEBUG] Component definition '%s' not found\n", node->name);
                }

                if (node->arguments && def) {
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
                            fprintf(stderr, "[DEBUG]   State var %u: name=%s, initial_expr=%s\n",
                                    i, state_var->name, state_var->initial_expr ? state_var->initial_expr : "(null)");
                            // If the initial_expr is the prop name, substitute with actual value
                            fprintf(stderr, "[DEBUG]     Checking: initial_expr=%s, equals=%s\n",
                                    state_var->initial_expr ? state_var->initial_expr : "(null)",
                                    equals ? "set" : "NULL");
                            if (state_var->initial_expr && equals) {
                                fprintf(stderr, "[DEBUG]     Taking NAMED arg path\n");
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
                                    fprintf(stderr, "[DEBUG]     MATCH! Adding variable with value %s, scope %s\n",
                                            arg_value, instance_scope);
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
                                fprintf(stderr, "[DEBUG]     Taking POSITIONAL arg path, value=%s, scope=%s\n",
                                        arg_value, instance_scope);
                                // Positional arg - just use the value directly
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

                    for (uint32_t i = 0; i < def->state_var_count; i++) {
                        IRComponentStateVar* state_var = &def->state_vars[i];
                        // For Counter with no args, default is 0

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
                fflush(stdout);
                if (!apply_property(ctx, component, child->name, child->value)) {
                    // Log warning but continue conversion
                    if (ctx->parser) {
                        kry_parser_add_error_fmt(ctx->parser, KRY_ERROR_WARNING,
                                                 KRY_ERROR_CONVERSION,
                                                 "Failed to apply property '%s'",
                                                 child->name ? child->name : "(unknown)");
                    }
                }
                fflush(stdout);
            } else if (child->type == KRY_NODE_COMPONENT) {
                // Recursively convert child component
                IRComponent* child_component = convert_node(ctx, child);
                if (!child_component && ctx->parser) {
                    // Log warning for failed child conversion
                    kry_parser_add_error_fmt(ctx->parser, KRY_ERROR_WARNING,
                                             KRY_ERROR_CONVERSION,
                                             "Failed to convert child component '%s'",
                                             child->name ? child->name : "(unknown)");
                } else if (child_component) {
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
                // NOTE: In component definitions, 'var' statements represent state variables
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

                // For 'var' declarations in component templates, we need special handling:
                // - State variables should NOT have their initial value substituted
                // - Instead, references like $value should become text_expression bindings
                // We detect state variables by checking if they have a type annotation (child->var_type)
                // For state variables, add a self-reference param to mark the variable name as known
                // but still trigger the unresolved expression path in handle_text_property
                bool is_state_var = (child->var_type != NULL);
                if (is_state_var) {
                    // State variable: add self-reference to param context
                    // This allows is_unresolved_expr() to recognize the variable name
                    add_param(ctx, var_name, var_name);
                    // Also store for reactive manifest (if we have one)
                    if (ctx->manifest) {
                        // Add to reactive manifest with proper initial value
                        IRReactiveVarType ir_type = IR_REACTIVE_TYPE_INT;
                        const char* type_str = "int";

                        if (child->var_type) {
                            if (strcmp(child->var_type, "string") == 0) {
                                ir_type = IR_REACTIVE_TYPE_STRING;
                                type_str = "string";
                            } else if (strcmp(child->var_type, "float") == 0 ||
                                       strcmp(child->var_type, "double") == 0 ||
                                       strcmp(child->var_type, "number") == 0) {
                                ir_type = IR_REACTIVE_TYPE_FLOAT;
                                type_str = child->var_type;
                            } else if (strcmp(child->var_type, "bool") == 0 ||
                                       strcmp(child->var_type, "boolean") == 0) {
                                ir_type = IR_REACTIVE_TYPE_BOOL;
                                type_str = child->var_type;
                            }
                        }

                        IRReactiveValue react_value = {.as_int = 0};
                        if (var_value) {
                            switch (ir_type) {
                                case IR_REACTIVE_TYPE_INT:
                                    react_value.as_int = atoi(var_value);
                                    break;
                                case IR_REACTIVE_TYPE_FLOAT:
                                    react_value.as_float = atof(var_value);
                                    break;
                                case IR_REACTIVE_TYPE_BOOL:
                                    react_value.as_bool = (strcmp(var_value, "true") == 0);
                                    break;
                                case IR_REACTIVE_TYPE_STRING:
                                    react_value.as_string = (char*)var_value;
                                    break;
                            }
                        }

                        uint32_t var_id = ir_reactive_manifest_add_var(
                            ctx->manifest,
                            var_name,
                            ir_type,
                            react_value
                        );

                        if (var_id > 0) {
                            ir_reactive_manifest_set_var_metadata(
                                ctx->manifest,
                                var_id,
                                type_str,
                                var_value,
                                "component"
                            );
                        }
                    }
                } else if (var_value) {
                    // Regular const/let variable: add with actual value for substitution
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
                                json_buffer_ptr = NULL;  // Reset to avoid use-after-free
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

                        // Add function to logic block
                        ir_logic_block_add_function(ctx->logic_block, func);
                    }
                }
            } else if (child->type == KRY_NODE_FUNCTION_DECL) {
                // Function declaration - convert to IRLogicFunction and add to logic block
                if (ctx->logic_block) {
                    IRLogicFunction* func = convert_function_decl(ctx, child, component->tag);
                    if (func) {
                        ir_logic_block_add_function(ctx->logic_block, func);
                    }
                }
            } else if (child->type == KRY_NODE_RETURN_STMT) {
                // Return statement - should only appear inside function bodies
                // This is handled in convert_function_decl
            } else if (child->type == KRY_NODE_MODULE_RETURN) {
                // Module-level return statement: return { exports }
                // Convert to IRModuleExport entries
                convert_module_return(ctx, child);
            }

            child = child->next_sibling;
        }

        return component;
    }

    // Import statements are processed at top-level in ir_kry_parse_with_manifest(),
    // not during node conversion. Return NULL to indicate no IR component is produced.
    if (node->type == KRY_NODE_IMPORT) {
        return NULL;
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
        kry_parser_free(parser);
        return NULL;
    }

    // Create conversion context
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.parser = parser;  // For error reporting during conversion
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
        return result;
    }

    if (length == 0) {
        length = strlen(source);
    }

    // Parse source to AST
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) {
        return result;
    }

    if (parser->has_error) {
        kry_parser_free(parser);
        return result;
    }

    KryNode* ast = parser->root;
    if (!ast) {
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
        kry_parser_free(parser);
        return result;
    }

    // Create conversion context
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.parser = parser;  // For error reporting during conversion
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

// Merge child template into parent component (for inheritance)
// Child properties override parent properties, child children are added to parent
static void merge_component_trees(IRComponent* parent, IRComponent* child_template) {
    if (!parent || !child_template) return;

    // 1. Merge style - carefully copy each field
    if (child_template->style) {
        if (!parent->style) {
            parent->style = calloc(1, sizeof(IRStyle));
        }
        if (parent->style) {
            // Copy value types (not pointers)
            parent->style->width = child_template->style->width;
            parent->style->height = child_template->style->height;
            parent->style->padding = child_template->style->padding;
            parent->style->margin = child_template->style->margin;
            parent->style->background = child_template->style->background;
            parent->style->text_fill_color = child_template->style->text_fill_color;
            parent->style->border_color = child_template->style->border_color;
            parent->style->border = child_template->style->border;
            parent->style->font = child_template->style->font;
            parent->style->visible = child_template->style->visible;
            parent->style->opacity = child_template->style->opacity;
            parent->style->z_index = child_template->style->z_index;
            parent->style->overflow_x = child_template->style->overflow_x;
            parent->style->overflow_y = child_template->style->overflow_y;
            parent->style->position_mode = child_template->style->position_mode;
            parent->style->absolute_x = child_template->style->absolute_x;
            parent->style->absolute_y = child_template->style->absolute_y;

            // Deep copy string pointers
            if (child_template->style->background_image) {
                if (parent->style->background_image) free(parent->style->background_image);
                parent->style->background_image = strdup(child_template->style->background_image);
            }
        }
    }

    // 2. Merge layout properties
    if (child_template->layout) {
        if (!parent->layout) {
            parent->layout = calloc(1, sizeof(IRLayout));
        }
        if (parent->layout) {
            // Copy value types
            parent->layout->mode = child_template->layout->mode;
            parent->layout->display_explicit = child_template->layout->display_explicit;
            parent->layout->min_width = child_template->layout->min_width;
            parent->layout->min_height = child_template->layout->min_height;
            parent->layout->max_width = child_template->layout->max_width;
            parent->layout->max_height = child_template->layout->max_height;
            parent->layout->flex = child_template->layout->flex;
            parent->layout->grid = child_template->layout->grid;
            parent->layout->margin = child_template->layout->margin;
            parent->layout->padding = child_template->layout->padding;
            parent->layout->aspect_ratio = child_template->layout->aspect_ratio;
        }
    }

    // 3. Copy other attributes
    if (child_template->tag && !parent->tag) {
        parent->tag = strdup(child_template->tag);
    }
    if (child_template->text_content && !parent->text_content) {
        parent->text_content = strdup(child_template->text_content);
    }
    if (child_template->text_expression && !parent->text_expression) {
        parent->text_expression = strdup(child_template->text_expression);
    }
    if (child_template->custom_data && !parent->custom_data) {
        parent->custom_data = strdup(child_template->custom_data);
    }

    // 4. Merge event handlers (append child's handlers to parent's)
    if (child_template->events) {
        IREvent* child_event = child_template->events;
        IREvent* parent_last_event = parent->events;

        // Find the last event in parent's list
        if (parent_last_event) {
            while (parent_last_event->next) {
                parent_last_event = parent_last_event->next;
            }
        }

        // Clone and append each child event
        while (child_event) {
            IREvent* event_clone = calloc(1, sizeof(IREvent));
            event_clone->type = child_event->type;
            event_clone->event_name = child_event->event_name ? strdup(child_event->event_name) : NULL;
            event_clone->logic_id = child_event->logic_id ? strdup(child_event->logic_id) : NULL;
            event_clone->handler_data = child_event->handler_data ? strdup(child_event->handler_data) : NULL;
            event_clone->bytecode_function_id = child_event->bytecode_function_id;

            if (parent_last_event) {
                parent_last_event->next = event_clone;
                parent_last_event = event_clone;
            } else {
                parent->events = event_clone;
                parent_last_event = event_clone;
            }

            child_event = child_event->next;
        }
    }

    // 5. Add child's children to parent's children
    if (child_template->child_count > 0) {
        for (uint32_t i = 0; i < child_template->child_count; i++) {
            IRComponent* child_clone = ir_component_clone_tree(child_template->children[i]);
            if (child_clone) {
                ir_add_child(parent, child_clone);
            }
        }
    }
}

// Check if a component name refers to a custom component definition
static bool is_custom_component(const char* name, IRReactiveManifest* manifest) {
    if (!name || !manifest) {
        return false;
    }

    for (uint32_t i = 0; i < manifest->component_def_count; i++) {
        if (strcmp(manifest->component_defs[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static IRComponent* expand_component_template(
    const char* comp_name,
    IRReactiveManifest* manifest,
    const char* instance_scope,
    const char** inheritance_chain,
    int chain_depth
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

    if (!def) {
        fprintf(stderr, "[EXPAND] ERROR: Component '%s' not found in manifest\n", comp_name);
        return NULL;
    }

    if (!def->template_root) {
        fprintf(stderr, "[EXPAND] ERROR: Component '%s' has no template_root\n", comp_name);
        return NULL;
    }

    // Check for circular inheritance - check if parent is already in the chain
    if (def->extends_parent && inheritance_chain && chain_depth > 0) {
        for (int i = 0; i < chain_depth; i++) {
            if (inheritance_chain[i] && strcmp(inheritance_chain[i], def->extends_parent) == 0) {
                // Circular dependency detected!
                fprintf(stderr, "[EXPAND] ERROR: Circular component inheritance detected!\n");
                fprintf(stderr, "[EXPAND] Chain: ");
                for (int j = 0; j < chain_depth; j++) {
                    if (inheritance_chain[j]) {
                        fprintf(stderr, "%s", inheritance_chain[j]);
                        if (j < chain_depth - 1) fprintf(stderr, " -> ");
                    }
                }
                fprintf(stderr, " -> %s\n", def->extends_parent);
                return NULL;
            }
        }
    }

    // Validate parent exists if specified
    if (def->extends_parent) {
        IRComponentType parent_type = ir_component_type_from_string(def->extends_parent);
        bool is_builtin = (strcmp(ir_component_type_to_string(parent_type), def->extends_parent) == 0);

        if (!is_builtin) {
            // Check if parent custom component exists
            bool parent_found = false;
            for (uint32_t i = 0; i < manifest->component_def_count; i++) {
                if (strcmp(manifest->component_defs[i].name, def->extends_parent) == 0) {
                    parent_found = true;
                    break;
                }
            }
            if (!parent_found) {
                fprintf(stderr, "[EXPAND] ERROR: Parent component '%s' not found for '%s'\n",
                        def->extends_parent, def->name);
                return NULL;
            }
        }
    }

    // Add current component to inheritance chain
    const char** new_chain = NULL;
    if (inheritance_chain) {
        new_chain = calloc(chain_depth + 1, sizeof(char*));
        for (int i = 0; i < chain_depth; i++) {
            new_chain[i] = inheritance_chain[i];
        }
        new_chain[chain_depth] = comp_name;
    }

    IRComponent* instance = NULL;

    // Handle inheritance
    if (def->extends_parent) {
        // Check if parent is a built-in component
        IRComponentType parent_type = ir_component_type_from_string(def->extends_parent);

        // Check if parent_type is actually a built-in component (not CUSTOM/CONTAINER default)
        bool is_builtin = (strcmp(ir_component_type_to_string(parent_type), def->extends_parent) == 0);

        if (is_builtin) {
            // Parent is a built-in component
            instance = ir_create_component(parent_type);
            if (!instance) {
                if (new_chain) free(new_chain);
                return NULL;
            }
        } else {
            // Parent is a custom component - recursive expansion
            instance = expand_component_template(def->extends_parent, manifest, instance_scope, new_chain, chain_depth + 1);
            if (!instance) {
                if (new_chain) free(new_chain);
                return NULL;
            }
        }

        // Merge child's template into parent
        merge_component_trees(instance, def->template_root);

    } else {
        // No inheritance - default to Container if no explicit extends
        instance = ir_create_component(IR_COMPONENT_CONTAINER);
        if (!instance) {
            if (new_chain) free(new_chain);
            return NULL;
        }

        // Merge child's template into Container
        merge_component_trees(instance, def->template_root);
    }

    if (new_chain) free(new_chain);

    if (!instance) return NULL;

    // Set scope only on the root component (not children)
    // This allows codegen to identify the custom component instance root
    // while rendering children normally
    if (instance->scope) free(instance->scope);
    instance->scope = strdup(instance_scope);

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
// Extract component props from a component definition's arguments field
// Arguments format: "propName: type, propName2: type2 = defaultValue"
static IRComponentProp* extract_component_props(
    KryNode* component_node,
    uint32_t* out_count
) {
    if (!component_node || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // Props can come from the component's arguments field
    // e.g., component HabitPanel(habitData: Habit, isActive: bool = false) { ... }
    const char* args = component_node->arguments;
    if (!args || strlen(args) == 0) {
        *out_count = 0;
        return NULL;
    }

    // First pass: count props (separated by commas)
    uint32_t count = 0;
    const char* p = args;
    while (*p) {
        // Skip whitespace
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p) break;

        // Found a prop name
        count++;

        // Skip to comma or end
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    // Allocate array
    IRComponentProp* props = calloc(count, sizeof(IRComponentProp));
    if (!props) {
        *out_count = 0;
        return NULL;
    }

    // Second pass: extract prop details
    uint32_t idx = 0;
    p = args;
    while (*p && idx < count) {
        // Skip whitespace
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p) break;

        // Extract prop name (until : or = or , or end)
        const char* name_start = p;
        while (*p && *p != ':' && *p != '=' && *p != ',') p++;
        size_t name_len = p - name_start;
        // Trim trailing whitespace from name
        while (name_len > 0 && (name_start[name_len-1] == ' ' || name_start[name_len-1] == '\t')) {
            name_len--;
        }
        props[idx].name = strndup(name_start, name_len);

        // Extract type if present
        if (*p == ':') {
            p++;  // Skip ':'
            while (*p && (*p == ' ' || *p == '\t')) p++;  // Skip whitespace

            const char* type_start = p;
            while (*p && *p != '=' && *p != ',') p++;
            size_t type_len = p - type_start;
            // Trim trailing whitespace from type
            while (type_len > 0 && (type_start[type_len-1] == ' ' || type_start[type_len-1] == '\t')) {
                type_len--;
            }
            props[idx].type = strndup(type_start, type_len);
        } else {
            props[idx].type = strdup("any");
        }

        // Extract default value if present
        if (*p == '=') {
            p++;  // Skip '='
            while (*p && (*p == ' ' || *p == '\t')) p++;  // Skip whitespace

            const char* value_start = p;
            while (*p && *p != ',') p++;
            size_t value_len = p - value_start;
            // Trim trailing whitespace from value
            while (value_len > 0 && (value_start[value_len-1] == ' ' || value_start[value_len-1] == '\t')) {
                value_len--;
            }
            props[idx].default_value = strndup(value_start, value_len);
        } else {
            props[idx].default_value = NULL;
        }

        // Skip comma
        if (*p == ',') p++;

        idx++;
    }

    *out_count = idx;
    return props;
}

static IRComponentStateVar* extract_component_state_vars(
    KryNode* component_body,
    uint32_t* out_count
) {
    if (!component_body || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // Count state declarations (both 'state' and 'var' inside components)
    uint32_t count = 0;
    KryNode* child = component_body->first_child;
    while (child) {
        // Include both KRY_NODE_STATE (state value: int = 0) and
        // KRY_NODE_VAR_DECL (var value: int = initialValue) as component state
        if (child->type == KRY_NODE_STATE || child->type == KRY_NODE_VAR_DECL) {
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
        if (child->type != KRY_NODE_STATE && child->type != KRY_NODE_VAR_DECL) {
            child = child->next_sibling;
            continue;
        }

        // Parse state declaration: "state value: int = initialValue" or "var value: int = initialValue"
        // child->name = "value"
        // child->state_type = "int" (for KRY_NODE_STATE)
        // child->value = initial expression (KryValue)

        state_vars[idx].name = child->name ? strdup(child->name) : strdup("unknown");
        // Use state_type if available (KRY_NODE_STATE), otherwise use var_type (KRY_NODE_VAR_DECL)
        const char* type_str = child->state_type ? child->state_type : child->var_type;
        state_vars[idx].type = type_str ? strdup(type_str) : strdup("any");

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
// Module Loading
// ============================================================================

/**
 * Resolve a module path to a file path
 * e.g., "components.habit_panel" -> "components/habit_panel.kry"
 */
static char* resolve_module_path(const char* module_path, const char* base_directory) {
    if (!module_path) return NULL;

    // Convert dots to slashes (e.g., "components.habit_panel" -> "components/habit_panel")
    char* resolved_path = strdup(module_path);
    for (char* p = resolved_path; *p; p++) {
        if (*p == '.') *p = '/';
    }

    // Add .kry extension
    size_t len = strlen(resolved_path);
    char* file_path = malloc(len + 5);  // +4 for ".kry" +1 for null
    if (!file_path) {
        free(resolved_path);
        return NULL;
    }
    sprintf(file_path, "%s.kry", resolved_path);
    free(resolved_path);

    // If base directory is provided, prepend it
    if (base_directory && strlen(base_directory) > 0) {
        char* full_path = malloc(strlen(base_directory) + strlen(file_path) + 2);
        if (full_path) {
            sprintf(full_path, "%s/%s", base_directory, file_path);
            free(file_path);
            file_path = full_path;
        }
    }

    return file_path;
}

// ============================================================================
// Circular Dependency Detection
// ============================================================================

// Track imports during module loading
typedef struct {
    char* module_path;      // Module being loaded
    int import_depth;       // Depth to detect deep imports
} ImportStack;

static ImportStack* g_import_stack = NULL;
static int g_import_depth = 0;
static int g_import_stack_capacity = 0;

static bool is_module_being_loaded(const char* module_path) {
    for (int i = 0; i < g_import_depth; i++) {
        if (strcmp(g_import_stack[i].module_path, module_path) == 0) {
            return true;
        }
    }
    return false;
}

static bool push_import_stack(const char* module_path) {
    if (is_module_being_loaded(module_path)) {
        // Circular dependency detected!
        for (int i = 0; i < g_import_depth; i++) {
        }
        return false;
    }

    // Add to stack
    if (g_import_depth >= g_import_stack_capacity) {
        g_import_stack_capacity = (g_import_stack_capacity == 0) ? 16 : g_import_stack_capacity * 2;
        g_import_stack = realloc(g_import_stack, sizeof(ImportStack) * g_import_stack_capacity);
    }

    g_import_stack[g_import_depth].module_path = strdup(module_path);
    g_import_stack[g_import_depth].import_depth = g_import_depth;
    g_import_depth++;
    return true;
}

static void pop_import_stack(void) {
    if (g_import_depth > 0) {
        g_import_depth--;
        if (g_import_stack[g_import_depth].module_path) {
            free(g_import_stack[g_import_depth].module_path);
            g_import_stack[g_import_depth].module_path = NULL;
        }
    }
}

// Reset the import stack for a fresh top-level parse
static void reset_import_stack(void) {
    // Free all remaining entries
    for (int i = 0; i < g_import_depth; i++) {
        if (g_import_stack[i].module_path) {
            free(g_import_stack[i].module_path);
            g_import_stack[i].module_path = NULL;
        }
    }
    g_import_depth = 0;
    // Keep the allocated capacity for reuse
}

// Register a module import with its exports for expression resolution
static void register_module_import(ConversionContext* ctx,
                                     const char* import_name,
                                     const char* module_path,
                                     IRModuleExport** exports,
                                     uint32_t export_count) {
    // Expand module_imports array if needed
    if (ctx->module_import_count >= ctx->module_import_capacity) {
        ctx->module_import_capacity = (ctx->module_import_capacity == 0) ? 8 : ctx->module_import_capacity * 2;
        ctx->module_imports = realloc(ctx->module_imports,
                                      sizeof(*ctx->module_imports) * ctx->module_import_capacity);
    }

    ctx->module_imports[ctx->module_import_count].import_name = strdup(import_name);
    ctx->module_imports[ctx->module_import_count].module_path = strdup(module_path);
    ctx->module_imports[ctx->module_import_count].exports = exports;
    ctx->module_imports[ctx->module_import_count].export_count = export_count;
    ctx->module_import_count++;
}

/**
 * Load and parse an imported module, registering its component definition
 */
static bool load_imported_module(ConversionContext* ctx, const char* import_name, const char* module_path) {
    if (!ctx || !import_name || !module_path) return false;

    // Check for circular dependencies
    if (!push_import_stack(module_path)) {
        return false;
    }

    // Resolve module path to file path
    char* file_path = resolve_module_path(module_path, ctx->base_directory);
    if (!file_path) {
        pop_import_stack();
        return false;
    }

    // Read the file
    FILE* f = fopen(file_path, "r");
    if (!f) {
        free(file_path);
        pop_import_stack();
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        free(file_path);
        fclose(f);
        pop_import_stack();
        return false;
    }

    char* source = malloc(size + 1);
    if (!source) {
        free(file_path);
        fclose(f);
        pop_import_stack();
        return false;
    }

    size_t bytes_read = fread(source, 1, (size_t)size, f);
    source[bytes_read] = '\0';
    fclose(f);


    // Parse the imported file
    KryParser* parser = kry_parser_create(source, bytes_read);
    if (!parser) {
        free(source);
        free(file_path);
        pop_import_stack();
        return false;
    }

    KryNode* import_ast = kry_parse(parser);
    if (!import_ast || parser->has_error) {
        kry_parser_free(parser);
        free(source);
        free(file_path);
        pop_import_stack();
        return false;
    }

    // NOTE: Do NOT free parser yet - the AST is chunk-allocated through the parser
    // and will be freed when parser is freed. We need to use import_ast first.
    free(source);

    // Find the root component (the component definition to import)
    // Look for components explicitly marked with 'component' keyword
    KryNode* import_root = NULL;
    if (import_ast->name && strcmp(import_ast->name, "Root") == 0 && import_ast->first_child) {
        KryNode* child = import_ast->first_child;
        while (child) {
            // Find components explicitly marked as component definitions
            if (child->type == KRY_NODE_COMPONENT &&
                child->is_component_definition &&
                child->name != NULL) {
                import_root = child;
                break;
            }
            child = child->next_sibling;
        }

        if (!import_root) {
            kry_parser_free(parser);  // Free parser (and AST) before returning
            free(file_path);
            pop_import_stack();
            return false;
        }
    } else if (import_ast->type == KRY_NODE_COMPONENT && import_ast->is_component_definition) {
        import_root = import_ast;
    }

    if (!import_root || !import_root->name) {
        kry_parser_free(parser);  // Free parser (and AST) before returning
        free(file_path);
        pop_import_stack();
        return false;
    }


    // Save the component name before calling convert_node
    // (convert_node may modify or free the AST node)
    char* component_name = import_root->name ? strdup(import_root->name) : NULL;
    if (!component_name) {
        kry_parser_free(parser);  // Free parser (and AST) before returning
        free(file_path);
        pop_import_stack();
        return false;
    }

    // Convert the component to IR
    IRComponent* template_comp = convert_node(ctx, import_root);

    if (!template_comp) {
        kry_parser_free(parser);  // Free parser (and AST) before returning
        free(component_name);
        free(file_path);
        pop_import_stack();
        return false;
    }

    // Extract state variables
    uint32_t state_var_count = 0;
    IRComponentStateVar* state_vars = extract_component_state_vars(import_root, &state_var_count);

    // Extract component props
    uint32_t prop_count = 0;
    IRComponentProp* props = extract_component_props(import_root, &prop_count);

    // Register the component definition in the manifest
    ir_reactive_manifest_add_component_def(
        ctx->manifest,
        component_name,              // Component name
        import_root->extends_parent, // Parent component for inheritance
        props, prop_count,           // Component props
        state_vars,
        state_var_count,
        template_comp
    );

    // Set module path fields
    if (ctx->manifest->component_def_count > 0) {
        IRComponentDefinition* def = &ctx->manifest->component_defs[ctx->manifest->component_def_count - 1];
        if (def) {
            def->module_path = file_path;  // Transfer ownership
            def->source_module = strdup(module_path);

        } else {
            free(file_path);
        }
    } else {
        free(file_path);
    }

    free(component_name);  // Free the saved name

    // Check for module exports (module-level return statement)
    // Look for KRY_NODE_MODULE_RETURN nodes in the imported AST
    IRModuleExport** module_exports = NULL;
    uint32_t module_export_count = 0;

    KryNode* child = import_ast->first_child;
    while (child) {
        if (child->type == KRY_NODE_MODULE_RETURN) {
            // Convert module return to IRModuleExport entries

            // Create a temporary conversion context for this module
            ConversionContext module_ctx = *ctx;  // Copy parent context
            module_ctx.ast_root = import_ast;
            module_ctx.source_structures = ir_source_structures_create();
            if (!module_ctx.source_structures) {
                kry_parser_free(parser);  // Free parser (and AST) before returning
                // Note: file_path was already transferred/freed earlier
                pop_import_stack();
                return false;
            }

            // Convert the module return to exports
            convert_module_return(&module_ctx, child);

            // Get the exports
            module_exports = module_ctx.source_structures->exports;
            module_export_count = module_ctx.source_structures->export_count;

            // Transfer ownership of exports array
            module_ctx.source_structures->exports = NULL;
            module_ctx.source_structures->export_count = 0;

            // Free the temporary source structures (but keep the exports)
            // Only free the structure itself, not the exports array
            free(module_ctx.source_structures);

            break;
        }
        child = child->next_sibling;
    }

    // Register the module import with its exports
    if (module_export_count > 0) {
        register_module_import(ctx, import_name, module_path, module_exports, module_export_count);
    }

    // Free the parser (and the imported AST which was chunk-allocated)
    kry_parser_free(parser);

    pop_import_stack();
    return true;
}

// ============================================================================
// KIR Generation (with manifest serialization)
// ============================================================================

// Internal implementation with base_directory support and import expansion control
static char* ir_kry_to_kir_internal_ex(const char* source, size_t length, const char* base_directory, bool skip_import_expansion) {
    if (!source) return NULL;

    if (length == 0) {
        length = strlen(source);
    }

    // Reset import stack for fresh parse (important when called multiple times)
    reset_import_stack();

    // Create parser
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) return NULL;

    // Parse to AST
    KryNode* ast = kry_parse(parser);

    if (!ast || parser->has_error) {
        if (parser->has_error) {
            // Print collected parse errors
            char* error_str = kry_error_list_format(&parser->errors);
            if (error_str) {
                fprintf(stderr, "%s", error_str);
                free(error_str);
            }
        }
        kry_parser_free(parser);
        return NULL;
    }

    // Check if ast is a Root wrapper with children or a single component
    KryNode* root_node = NULL;

    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        // Root wrapper with children - find the application node
        KryNode* child = ast->first_child;
        while (child) {
            // Skip component definitions, variable declarations, style blocks, imports,
            // function declarations, struct declarations, and module returns
            if (!child->is_component_definition &&
                child->type != KRY_NODE_VAR_DECL &&
                child->type != KRY_NODE_STYLE_BLOCK &&
                child->type != KRY_NODE_IMPORT &&
                child->type != KRY_NODE_FUNCTION_DECL &&
                child->type != KRY_NODE_STRUCT_DECL &&
                child->type != KRY_NODE_RETURN_STMT &&
                child->type != KRY_NODE_MODULE_RETURN) {
                root_node = child;
                break;
            }
            child = child->next_sibling;
        }
    } else {
        // Single component (old behavior for compatibility)
        root_node = ast;
        while (root_node && root_node->is_component_definition) {
            root_node = root_node->next_sibling;
        }
    }

    // Module-only files (no UI root) are valid - they contain just struct types and exports
    // We'll proceed with a NULL root_node and still process structs/exports below

    // Create conversion context with manifest and logic block
    ConversionContext ctx;
    ctx.ast_root = ast;
    ctx.parser = parser;  // For error reporting during conversion
    ctx.param_count = 0;
    ctx.manifest = ir_reactive_manifest_create();
    ctx.logic_block = ir_logic_block_create();  // NEW: Create logic block
    ctx.next_handler_id = 1;                    // NEW: Initialize handler counter
    ctx.compile_mode = IR_COMPILE_MODE_HYBRID;  // Use HYBRID mode for round-trip codegen
    ctx.source_structures = ir_source_structures_create();  // Create source structures
    ctx.static_block_counter = 0;               // Initialize static block counter
    ctx.current_static_block_id = NULL;         // Not in static block initially
    ctx.target_platform = KRY_TARGET_LUA;       // Default to Lua target
    ctx.source_file_path = NULL;                // No source file path available yet
    ctx.base_directory = base_directory ? strdup(base_directory) : NULL;  // Set from parameter for import resolution
    ctx.skip_import_expansion = skip_import_expansion;  // Control import expansion for multi-file KIR

    // Always create a fresh IR context for component ID generation
    // Save any existing context to avoid losing plugin state
    IRContext* old_context = g_ir_context;
    IRContext* ir_ctx = ir_create_context();
    ir_set_context(ir_ctx);
    bool created_new_context = true;  // Always true now since we always create fresh
    (void)old_context;  // May be used later if we need to preserve plugin state

    // Process top-level variable declarations (const/let/var)
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* var_node = ast->first_child;
        while (var_node) {
            if (var_node->type == KRY_NODE_VAR_DECL && var_node != root_node) {

                const char* var_type = var_node->var_type ? var_node->var_type : "const";
                const char* var_name = var_node->name;

                // Store the variable in ctx->params so for-loop expansion can access it
                if (ctx.param_count < MAX_PARAMS && var_node->value) {
                    ctx.params[ctx.param_count].name = (char*)var_name;
                    ctx.params[ctx.param_count].value = NULL;  // String representation
                    ctx.params[ctx.param_count].kry_value = var_node->value;  // Original KryValue
                    ctx.param_count++;
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
                        // Use number_value (not string_value) - it's stored as double
                        double num = var_node->value->number_value;
                        // Check if it's a float or int by examining fractional part
                        if (num != (int64_t)num) {
                            react_type = IR_REACTIVE_TYPE_FLOAT;
                            init_value.as_float = num;
                        } else {
                            react_type = IR_REACTIVE_TYPE_INT;
                            init_value.as_int = (int64_t)num;
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

                    }
                }
            }
            var_node = var_node->next_sibling;
        }
    }

    // Process top-level import statements
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* import_node = ast->first_child;
        while (import_node) {
            if (import_node->type == KRY_NODE_IMPORT) {

                const char* import_name = import_node->import_name ? import_node->import_name : import_node->name;
                const char* import_module = import_node->import_module;

                if (import_name && import_module) {
                    // Add import to source structures
                    if (ctx.source_structures) {
                        ir_source_structures_add_import(ctx.source_structures, import_name, import_module);
                    }

                    // Load the imported module - but skip expansion for multi-file KIR
                    // When skip_import_expansion is true, we only record the import reference
                    // and let the separate KIR file contain the actual component definition
                    if (!ctx.skip_import_expansion) {
                        load_imported_module(&ctx, import_name, import_module);
                    }
                }
            }
            import_node = import_node->next_sibling;
        }
    }

    // Process struct declarations
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* struct_node = ast->first_child;
        while (struct_node) {
            if (struct_node->type == KRY_NODE_STRUCT_DECL) {

                // Convert struct to IRStructType
                IRStructType* struct_type = kry_convert_struct_decl(&ctx, struct_node);
                if (struct_type && ctx.source_structures) {
                    // Expand struct_types array
                    if (ctx.source_structures->struct_type_count >=
                        ctx.source_structures->struct_type_capacity) {
                        ctx.source_structures->struct_type_capacity =
                            (ctx.source_structures->struct_type_capacity == 0) ? 16 :
                            ctx.source_structures->struct_type_capacity * 2;
                        ctx.source_structures->struct_types = (IRStructType**)realloc(
                            ctx.source_structures->struct_types,
                            sizeof(IRStructType*) * ctx.source_structures->struct_type_capacity
                        );
                    }

                    ctx.source_structures->struct_types[
                        ctx.source_structures->struct_type_count++
                    ] = struct_type;

                }
            }
            struct_node = struct_node->next_sibling;
        }
    }

    // Process module-level return statements (exports)
    // This handles module-only files like types.kry that only export structs/functions
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* export_node = ast->first_child;
        while (export_node) {
            if (export_node->type == KRY_NODE_MODULE_RETURN) {
                // Convert module return to exports
                convert_module_return(&ctx, export_node);
            }
            export_node = export_node->next_sibling;
        }
    }

    // Process top-level function declarations
    // These are functions defined at module scope (outside components)
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* func_node = ast->first_child;
        while (func_node) {
            if (func_node->type == KRY_NODE_FUNCTION_DECL) {
                // Convert function declaration to IRLogicFunction and add to logic block
                if (ctx.logic_block) {
                    // Use NULL scope for top-level functions (no component prefix)
                    IRLogicFunction* func = convert_function_decl(&ctx, func_node, NULL);
                    if (func) {
                        ir_logic_block_add_function(ctx.logic_block, func);
                    }
                }
            }
            func_node = func_node->next_sibling;
        }
    }

    // Register explicitly defined custom components (marked with 'component' keyword)

    // Only register components that are explicitly marked with 'component' keyword
    // This is the NEW way: use 'component MyComponent { ... }' to define custom components
    // The OLD way (automatically treating non-App components as custom) is removed
    if (root_node && root_node->is_component_definition) {

        // Convert the root component to use as a template
        IRComponent* template_comp = convert_node(&ctx, root_node);

        if (template_comp) {
            // Extract state variables from root component
            uint32_t state_var_count = 0;
            IRComponentStateVar* state_vars = extract_component_state_vars(
                root_node,
                &state_var_count
            );

            // Extract component props
            uint32_t prop_count = 0;
            IRComponentProp* props = extract_component_props(root_node, &prop_count);

            // Add to manifest as a custom component definition
            ir_reactive_manifest_add_component_def(
                ctx.manifest,
                root_node->name,           // Component name (export name)
                root_node->extends_parent, // Parent component for inheritance
                props, prop_count,         // Component props
                state_vars,
                state_var_count,
                template_comp
            );

            // Set the module_path field for the newly added component definition
            if (ctx.manifest->component_def_count > 0) {
                IRComponentDefinition* def = &ctx.manifest->component_defs[ctx.manifest->component_def_count - 1];
                if (def) {
                    // Use a simple path convention for now
                    char module_path[512];
                    snprintf(module_path, sizeof(module_path), "components/%s",
                            root_node->name);
                    def->module_path = strdup(module_path);
                    def->source_module = strdup(module_path);

                }
            }

        }
    } else if (root_node && root_node->name && strcmp(root_node->name, "App") != 0) {
    } else {
    }

    // Process top-level style blocks
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* style_node = ast->first_child;
        while (style_node) {
            if (style_node->type == KRY_NODE_STYLE_BLOCK && style_node->name) {

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
                    }

                    ir_style_properties_cleanup(&props);
                }
            }
            style_node = style_node->next_sibling;
        }
    }

    // Process top-level code blocks (@lua, @js)
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        KryNode* code_node = ast->first_child;
        while (code_node) {
            if (code_node->type == KRY_NODE_CODE_BLOCK && code_node->code_language && code_node->code_source) {

                // Generate a unique function name for this code block
                char func_name[256];
                static int top_level_code_block_counter = 0;
                snprintf(func_name, sizeof(func_name), "_top_level_code_block_%d", top_level_code_block_counter++);

                // Create logic function to hold the source code
                IRLogicFunction* func = ir_logic_function_create(func_name);
                if (func) {
                    // Direct @lua or @js block - just add the source
                    ir_logic_function_add_source(func, code_node->code_language, code_node->code_source);

                    // Add function to logic block
                    ir_logic_block_add_function(ctx.logic_block, func);
                }
            }
            code_node = code_node->next_sibling;
        }
    }

    // Track all component definitions in the manifest

    // If ast is a Root wrapper, scan its children; otherwise scan ast and siblings
    KryNode* def_node = NULL;
    if (ast->name && strcmp(ast->name, "Root") == 0 && ast->first_child) {
        def_node = ast->first_child;
    } else {
        def_node = ast;
    }

    uint32_t node_count = 0;
    while (def_node) {
        node_count++;

        if (def_node->is_component_definition && def_node->name) {
            // Convert the component definition template
            IRComponent* template_comp = convert_node(&ctx, def_node);

            if (template_comp) {
                // Extract state variables from component definition
                uint32_t state_var_count = 0;
                IRComponentStateVar* state_vars = extract_component_state_vars(
                    def_node,
                    &state_var_count
                );

                // Extract component props
                uint32_t prop_count = 0;
                IRComponentProp* props = extract_component_props(def_node, &prop_count);

                // Add to manifest (will be serialized as component_definitions in KIR)
                ir_reactive_manifest_add_component_def(
                    ctx.manifest,
                    def_node->name,
                    def_node->extends_parent,  // Parent component name for inheritance
                    props, prop_count,         // Component props
                    state_vars,
                    state_var_count,
                    template_comp
                );
            }
        }
        def_node = def_node->next_sibling;
    }

    // Convert AST to IR (may be NULL for module-only files like types.kry)
    IRComponent* root = root_node ? convert_node(&ctx, root_node) : NULL;

    // Free parser (includes AST)
    kry_parser_free(parser);

    // Module-only files are valid even without a UI root component
    // They contain struct types, exports, imports, variables, etc.
    bool has_content = root != NULL ||
                       (ctx.source_structures && (
                           ctx.source_structures->struct_type_count > 0 ||
                           ctx.source_structures->export_count > 0 ||
                           ctx.source_structures->import_count > 0 ||
                           ctx.source_structures->var_decl_count > 0));

    if (!has_content) {
        if (ctx.manifest) {
            ir_reactive_manifest_destroy(ctx.manifest);
        }
        if (ctx.logic_block) {
            ir_logic_block_free(ctx.logic_block);
        }
        if (ctx.source_structures) {
            ir_source_structures_destroy(ctx.source_structures);
        }
        if (ctx.base_directory) {
            free((char*)ctx.base_directory);
        }
        return NULL;
    }

    // Create source metadata (initialize ALL fields to avoid garbage pointers)
    IRSourceMetadata metadata = {0};  // Zero-initialize all fields
    metadata.source_language = "kry";
    metadata.compiler_version = "kryon-1.0.0";
    metadata.timestamp = NULL;
    metadata.source_file = NULL;

    // Serialize with complete KIR format (manifest + logic_block + metadata + source_structures)
    fprintf(stderr, "[DEBUG] Before serialization: manifest has %u variables\n", ctx.manifest->variable_count);
    for (uint32_t i = 0; i < ctx.manifest->variable_count; i++) {
        fprintf(stderr, "[DEBUG]   Var %u: id=%u, name=%s, initial_value=%s, scope=%s\n",
                i,
                ctx.manifest->variables[i].id,
                ctx.manifest->variables[i].name ? ctx.manifest->variables[i].name : "(null)",
                ctx.manifest->variables[i].initial_value_json ? ctx.manifest->variables[i].initial_value_json : "(null)",
                ctx.manifest->variables[i].scope ? ctx.manifest->variables[i].scope : "(null)");
    }
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
    if (ctx.base_directory) {
        free((char*)ctx.base_directory);
    }
    // Only destroy context if we created it (don't destroy context from plugin loading)
    if (created_new_context) {
        ir_destroy_context(ir_ctx);
        ir_set_context(NULL);  // Clear global pointer to avoid dangling reference
    }

    return json;
}

// Public API: Convert .kry source to KIR JSON without base directory
char* ir_kry_to_kir(const char* source, size_t length) {
    return ir_kry_to_kir_internal_ex(source, length, NULL, false);
}

// Public API: Convert .kry source to KIR JSON with base directory for import resolution
char* ir_kry_to_kir_with_base_dir(const char* source, size_t length, const char* base_directory) {
    return ir_kry_to_kir_internal_ex(source, length, base_directory, false);
}

// Public API: Convert .kry source to KIR JSON for single-module output (no import expansion)
// This is used for multi-file KIR codegen where each module gets its own KIR file
char* ir_kry_to_kir_single_module(const char* source, size_t length, const char* base_directory) {
    return ir_kry_to_kir_internal_ex(source, length, base_directory, true);
}

// ============================================================================
// Extended API with Full Error Collection (Phase 5)
// ============================================================================

char* kry_error_list_format(void* error_list) {
    if (!error_list) return NULL;
    
    KryErrorList* errors = (KryErrorList*)error_list;
    if (!errors->error_count && !errors->warning_count) {
        return NULL;
    }

    // Calculate required buffer size
    size_t size = 256;  // Start with reasonable size
    char* buffer = (char*)malloc(size);
    if (!buffer) return NULL;
    
    size_t pos = 0;
    KryError* err = errors->first;
    while (err) {
        const char* level_str = (err->level == KRY_ERROR_WARNING) ? "Warning" : "Error";
        int written = snprintf(buffer + pos, size - pos, "%s at line %u:%u: %s\n",
                              level_str, err->line, err->column, err->message);
        
        if (written >= (int)(size - pos)) {
            // Need more space
            size *= 2;
            char* new_buffer = (char*)realloc(buffer, size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            continue;  // Retry this error
        }
        
        pos += written;
        err = err->next;
    }

    return buffer;
}

void kry_error_list_free(void* error_list) {
    if (!error_list) return;
    
    KryErrorList* errors = (KryErrorList*)error_list;
    KryError* err = errors->first;
    while (err) {
        KryError* next = err->next;
        // Note: error->message and error->context are allocated from parser chunks
        // and will be freed when parser is freed, so we don't free them here
        free(err);
        err = next;
    }
    
    free(errors);
}

IRKryParseResultEx ir_kry_parse_ex2(const char* source, size_t length) {
    IRKryParseResultEx result = {NULL, NULL, NULL, NULL};
    
    if (!source) {
        return result;
    }

    // Create parser
    KryParser* parser = kry_parser_create(source, length);
    if (!parser) {
        return result;
    }

    // Parse AST (continues on errors, doesn't stop at first error)
    KryNode* ast = kry_parse(parser);

    // Convert to IR even if errors exist (for partial results)
    if (ast && !parser->errors.has_fatal) {
        // Find root node
        KryNode* root_node = ast;
        while (root_node && root_node->is_component_definition) {
            root_node = root_node->next_sibling;
        }

        if (root_node) {
            ConversionContext ctx = {0};
            ctx.ast_root = ast;
            ctx.parser = parser;  // For error reporting
            ctx.param_count = 0;
            ctx.manifest = ir_reactive_manifest_create();
            ctx.logic_block = ir_logic_block_create();
            ctx.next_handler_id = 1;
            ctx.target_platform = KRY_TARGET_LUA;

            // Convert AST to IR
            result.root = convert_node(&ctx, root_node);
            result.manifest = ctx.manifest;
            result.logic_block = ctx.logic_block;

            // Validate IR if conversion succeeded
            if (result.root) {
                kry_validate_ir(parser, result.root);
            }
        }
    }

    // Copy error list (allocated separately from parser)
    if (parser->errors.error_count > 0 || parser->errors.warning_count > 0) {
        result.errors = (KryErrorList*)malloc(sizeof(KryErrorList));
        if (result.errors) {
            *((KryErrorList*)result.errors) = parser->errors;
            
            // Detach from parser so errors aren't freed twice
            parser->errors.first = NULL;
            parser->errors.last = NULL;
        }
    }

    kry_parser_free(parser);
    return result;
}

