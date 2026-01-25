/**
 * Kryon .kry Property Handlers
 *
 * Property dispatch system for mapping .kry properties to IR components.
 * Extracted from kry_to_ir.c for modularity.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_to_ir_properties.h"
#include "kry_parser.h"
#include "../../include/ir_builder.h"
#include "../../include/ir_style.h"
#include "../../include/ir_logic.h"
#include "../parser_utils.h"     // For parser_parse_color_packed
#include "../../../include/kryon/capability.h"  // For kryon_property_parser_fn
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Internal Aliases for Context Functions
// ============================================================================

#define add_param(ctx, name, value) kry_ctx_add_param(ctx, name, value)
#define add_param_value(ctx, name, value) kry_ctx_add_param_value(ctx, name, value)
#define substitute_param(ctx, expr) kry_ctx_substitute_param(ctx, expr)
#define is_unresolved_expr(ctx, expr) kry_ctx_is_unresolved_expr(ctx, expr)

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

IRComponentType kry_get_component_type(const char* name) {
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
const char* kry_resolve_value_as_string(
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
bool kry_get_value_as_bool(KryValue* value, bool* out) {
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
    const char* text = kry_resolve_value_as_string(ctx, value, &is_unresolved);

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
    if (kry_get_value_as_bool(value, &checked)) {
        ir_set_checkbox_state(component, checked);
        return true;
    }
    return false;
}

// Handle value property (two-way binding for INPUT components)
static bool handle_value_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    // Only INPUT components support value binding
    if (component->type != IR_COMPONENT_INPUT) {
        return false;
    }

    bool is_unresolved;
    const char* resolved = kry_resolve_value_as_string(ctx, value, &is_unresolved);

    if (!resolved) return false;

    if (is_unresolved) {
        // Create property binding for runtime two-way binding
        const char* original = (value->type == KRY_VALUE_IDENTIFIER)
                              ? value->identifier : value->expression;
        ir_component_add_property_binding(component, name, original, resolved, "two_way");

        // Store expression for runtime tracking (reuse text_expression field)
        if (component->text_expression) free(component->text_expression);
        component->text_expression = strdup(original);
    }

    return true;
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
uint32_t kry_parse_color_value(ConversionContext* ctx, KryValue* value, const char* property_name, IRComponent* component) {
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

    uint32_t color = kry_parse_color_value(ctx, value, name, component);
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

    uint32_t color = kry_parse_color_value(ctx, value, name, component);
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

    uint32_t color = kry_parse_color_value(ctx, value, name, component);
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

// Handle title property (for Tab components)
static bool handle_tab_title_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    // Only handle Tab components
    if (component->type != IR_COMPONENT_TAB) {
        return false;
    }

    if (value->type == KRY_VALUE_STRING) {
        // Ensure tab_data exists
        if (!component->tab_data) {
            component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
        }
        if (component->tab_data) {
            if (component->tab_data->title) {
                free(component->tab_data->title);
            }
            component->tab_data->title = strdup(value->string_value);
        }
        return true;
    }
    return false;
}

// Handle reorderable property (for TabBar components)
static bool handle_tab_reorderable_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (component->type != IR_COMPONENT_TAB_BAR) {
        return false;
    }

    bool reorderable_val = false;
    if (!kry_get_value_as_bool(value, &reorderable_val)) {
        return false;
    }

    if (!component->tab_data) {
        component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
    }
    if (component->tab_data) {
        component->tab_data->reorderable = reorderable_val;
    }
    return true;
}

// Handle activeBackground property (for Tab components)
static bool handle_tab_active_background_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (component->type != IR_COMPONENT_TAB) {
        return false;
    }

    if (!component->tab_data) {
        component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
    }
    if (component->tab_data) {
        component->tab_data->active_background = kry_parse_color_value(ctx, value, name, component);
    }
    return true;
}

// Handle textColor property (for Tab components)
static bool handle_tab_text_color_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (component->type != IR_COMPONENT_TAB) {
        return false;
    }

    if (!component->tab_data) {
        component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
    }
    if (component->tab_data) {
        component->tab_data->text_color = kry_parse_color_value(ctx, value, name, component);
    }
    return true;
}

// Handle activeTextColor property (for Tab components)
static bool handle_tab_active_text_color_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    if (component->type != IR_COMPONENT_TAB) {
        return false;
    }

    if (!component->tab_data) {
        component->tab_data = (IRTabData*)calloc(1, sizeof(IRTabData));
    }
    if (component->tab_data) {
        component->tab_data->active_text_color = kry_parse_color_value(ctx, value, name, component);
    }
    return true;
}

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
            IRDropdownState* state = (IRDropdownState*)component->custom_data;
            if (state) {
                state->selected_index = -1;  // No selection by default
            }
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

// Handle placeholder property (for Dropdown components)
static bool handle_placeholder_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx;
    (void)name;

    if (component->type != IR_COMPONENT_DROPDOWN) {
        return false;
    }

    if (value->type != KRY_VALUE_STRING) {
        return false;
    }

    // Ensure dropdown state exists
    if (!component->custom_data) {
        component->custom_data = calloc(1, sizeof(IRDropdownState));
        IRDropdownState* state = (IRDropdownState*)component->custom_data;
        if (state) state->selected_index = -1;  // No selection by default
    }

    IRDropdownState* state = (IRDropdownState*)component->custom_data;
    if (!state) return false;

    // Free existing placeholder
    if (state->placeholder) {
        free(state->placeholder);
    }

    // Set new placeholder
    state->placeholder = value->string_value ? strdup(value->string_value) : NULL;

    return true;
}

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
        IRDropdownState* init_state = (IRDropdownState*)component->custom_data;
        if (init_state) {
            init_state->selected_index = -1;  // No selection by default
        }
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
IRAlignment kry_parse_alignment(const char* align_str) {
    if (strcmp(align_str, "center") == 0) return IR_ALIGNMENT_CENTER;
    else if (strcmp(align_str, "start") == 0) return IR_ALIGNMENT_START;
    else if (strcmp(align_str, "end") == 0) return IR_ALIGNMENT_END;
    else if (strcmp(align_str, "flex-start") == 0) return IR_ALIGNMENT_START;
    else if (strcmp(align_str, "flex-end") == 0) return IR_ALIGNMENT_END;
    // Kebab-case (CSS-style)
    else if (strcmp(align_str, "space-between") == 0) return IR_ALIGNMENT_SPACE_BETWEEN;
    else if (strcmp(align_str, "space-around") == 0) return IR_ALIGNMENT_SPACE_AROUND;
    else if (strcmp(align_str, "space-evenly") == 0) return IR_ALIGNMENT_SPACE_EVENLY;
    // CamelCase (KRY-style)
    else if (strcmp(align_str, "spaceBetween") == 0) return IR_ALIGNMENT_SPACE_BETWEEN;
    else if (strcmp(align_str, "spaceAround") == 0) return IR_ALIGNMENT_SPACE_AROUND;
    else if (strcmp(align_str, "spaceEvenly") == 0) return IR_ALIGNMENT_SPACE_EVENLY;
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

        IRAlignment alignment = kry_parse_alignment(align);
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            // Set cross axis (alignItems in CSS terms)
            // Note: Do NOT automatically set justify_content when alignItems is center,
            // as this would override any explicit justifyContent value
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

        IRAlignment alignment = kry_parse_alignment(justify);
        IRLayout* layout = ir_get_layout(component);
        if (layout) {
            layout->flex.justify_content = alignment;
        }
        return true;
    }

    return false;
}

// ============================================================================
// Table Component Property Handlers
// ============================================================================

// Helper to get or create table state
static IRTableState* get_or_create_table_state(IRComponent* component) {
    if (!component) return NULL;
    IRTableState* state = ir_get_table_state(component);
    if (!state) {
        state = ir_table_create_state();
        component->custom_data = (char*)state;
    }
    return state;
}

static bool handle_table_cellPadding_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    if (value->type == KRY_VALUE_NUMBER) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            ir_table_set_cell_padding(state, (float)value->number_value);
        }
        return true;
    }
    return false;
}

static bool handle_table_striped_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    bool val = false;
    if (value->type == KRY_VALUE_IDENTIFIER && strcmp(value->identifier, "true") == 0) {
        val = true;
    }
    if (val) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            ir_table_set_striped(state, true);
        }
    }
    return true;
}

static bool handle_table_showBorders_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    bool val = false;
    if (value->type == KRY_VALUE_IDENTIFIER && strcmp(value->identifier, "true") == 0) {
        val = true;
    }
    if (val) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            ir_table_set_show_borders(state, true);
        }
    }
    return true;
}

// Helper to parse hex color from string
static void parse_hex_color(const char* str, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!str) { *r = *g = *b = 0; return; }
    if (str[0] == '#') str++;
    unsigned int val = 0;
    sscanf(str, "%x", &val);
    *r = (val >> 16) & 0xFF;
    *g = (val >> 8) & 0xFF;
    *b = val & 0xFF;
}

static bool handle_table_headerBackground_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    if (value->type == KRY_VALUE_STRING) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            uint8_t r, g, b;
            parse_hex_color(value->string_value, &r, &g, &b);
            ir_table_set_header_background(state, r, g, b, 255);
        }
        return true;
    }
    return false;
}

static bool handle_table_evenRowBackground_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    if (value->type == KRY_VALUE_STRING) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            uint8_t r, g, b;
            parse_hex_color(value->string_value, &r, &g, &b);
            state->style.even_row_background = IR_COLOR_RGBA(r, g, b, 255);
        }
        return true;
    }
    return false;
}

static bool handle_table_oddRowBackground_property(
    ConversionContext* ctx,
    IRComponent* component,
    const char* name,
    KryValue* value
) {
    (void)ctx; (void)name;
    if (value->type == KRY_VALUE_STRING) {
        IRTableState* state = get_or_create_table_state(component);
        if (state) {
            uint8_t r, g, b;
            parse_hex_color(value->string_value, &r, &g, &b);
            state->style.odd_row_background = IR_COLOR_RGBA(r, g, b, 255);
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
    {"value",            handle_value_property,          false},

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
    {"title",            handle_tab_title_property,        false},
    {"selectedIndex",    handle_selectedIndex_property,    false},
    {"reorderable",      handle_tab_reorderable_property,  false},
    {"activeBackground", handle_tab_active_background_property, false},
    {"textColor",        handle_tab_text_color_property,   false},
    {"activeTextColor",  handle_tab_active_text_color_property, false},

    // Dropdown component properties
    {"placeholder",      handle_placeholder_property,      false},
    {"options",          handle_options_property,          false},

    // Table component properties
    {"cellPadding",      handle_table_cellPadding_property, false},
    {"striped",          handle_table_striped_property,     false},
    {"showBorders",      handle_table_showBorders_property, false},
    {"headerBackground", handle_table_headerBackground_property, false},
    {"evenRowBackground", handle_table_evenRowBackground_property, false},
    {"oddRowBackground", handle_table_oddRowBackground_property, false},

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
// Property Application (Main Entry Point)
// ============================================================================

bool kry_apply_property(ConversionContext* ctx, IRComponent* component, const char* name, KryValue* value) {
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
            kry_apply_property(ctx, component, "windowWidth", value);
        } else if (strcmp(name, "height") == 0 && value->type == KRY_VALUE_NUMBER) {
            // For App, height sets window height, but also fall through to set container height
            kry_apply_property(ctx, component, "windowHeight", value);
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
