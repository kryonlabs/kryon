/**
 * Kryon .kry AST to IR Converter
 *
 * Converts the parsed .kry AST into IR component trees.
 */

#include "ir_kry_parser.h"
#include "ir_kry_ast.h"
#include "ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

static void apply_property(IRComponent* component, const char* name, KryValue* value) {
    if (!component || !name || !value) return;

    IRStyle* style = ir_get_style(component);
    if (!style) {
        style = ir_create_style();
        ir_set_style(component, style);
    }

    // Text content
    if (strcmp(name, "text") == 0 && value->type == KRY_VALUE_STRING) {
        ir_set_text_content(component, value->string_value);
        return;
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

static IRComponent* convert_node(KryNode* node) {
    if (!node) return NULL;

    if (node->type == KRY_NODE_COMPONENT) {
        // Create component
        IRComponentType type = get_component_type(node->name);
        IRComponent* component = ir_create_component(type);
        if (!component) return NULL;

        // Process children
        KryNode* child = node->first_child;
        while (child) {
            if (child->type == KRY_NODE_PROPERTY) {
                // Apply property
                apply_property(component, child->name, child->value);
            } else if (child->type == KRY_NODE_COMPONENT) {
                // Recursively convert child component
                IRComponent* child_component = convert_node(child);
                if (child_component) {
                    ir_add_child(component, child_component);
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

    // Convert AST to IR
    IRComponent* root = convert_node(ast);

    // Free parser (includes AST)
    kry_parser_free(parser);

    return root;
}
