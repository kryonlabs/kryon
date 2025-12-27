/**
 * Nim Parser - Converts Nim Kryon DSL to KIR JSON
 */

#include "ir_nim_parser.h"
#include "nim_lexer.h"
#include "../../ir_builder.h"
#include "../../ir_serialization.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Parser state
typedef struct {
    NimLexer lexer;
    NimToken current;
    NimToken previous;
    bool had_error;
    bool panic_mode;
    char error_message[256];
} NimParser;

// Forward declarations
static IRComponent* parse_component(NimParser* parser);
static void parse_properties(NimParser* parser, IRComponent* component);
static void parse_children(NimParser* parser, IRComponent* component);

// Error handling
static void error_at(NimParser* parser, NimToken* token, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;

    snprintf(parser->error_message, sizeof(parser->error_message),
             "[line %d:%d] Error at '%.*s': %s",
             token->line, token->column,
             (int)token->length, token->start, message);

    fprintf(stderr, "%s\n", parser->error_message);
}

static void error(NimParser* parser, const char* message) {
    error_at(parser, &parser->previous, message);
}

static void error_at_current(NimParser* parser, const char* message) {
    error_at(parser, &parser->current, message);
}

// Token manipulation
static void advance(NimParser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = nim_lexer_next(&parser->lexer);

        // Skip comments and empty newlines at top level
        if (parser->current.type == NIM_TOK_COMMENT) {
            continue;
        }

        if (parser->current.type != NIM_TOK_ERROR) break;

        error_at_current(parser, parser->current.start);
    }
}

static bool check(NimParser* parser, NimTokenType type) {
    return parser->current.type == type;
}

static bool match(NimParser* parser, NimTokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(NimParser* parser, NimTokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    error_at_current(parser, message);
}

static void skip_newlines(NimParser* parser) {
    while (match(parser, NIM_TOK_NEWLINE)) {
        // Skip
    }
}

// String helpers
static char* token_to_string(NimToken* token) {
    char* str = malloc(token->length + 1);
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

static char* parse_string_literal(NimToken* token) {
    // Remove quotes from string
    const char* start = token->start + 1; // Skip opening quote
    size_t len = token->length - 2; // Remove both quotes

    // Handle triple-quoted strings
    if (token->length >= 6 &&
        token->start[0] == token->start[1] &&
        token->start[1] == token->start[2]) {
        start = token->start + 3;
        len = token->length - 6;
    }

    char* str = malloc(len + 1);
    memcpy(str, start, len);
    str[len] = '\0';
    return str;
}

// Component type mapping
static IRComponentType string_to_component_type(const char* name) {
    if (strcmp(name, "Container") == 0) return IR_COMPONENT_CONTAINER;
    if (strcmp(name, "Text") == 0) return IR_COMPONENT_TEXT;
    if (strcmp(name, "Button") == 0) return IR_COMPONENT_BUTTON;
    if (strcmp(name, "Column") == 0) return IR_COMPONENT_COLUMN;
    if (strcmp(name, "Row") == 0) return IR_COMPONENT_ROW;
    if (strcmp(name, "Image") == 0) return IR_COMPONENT_IMAGE;
    if (strcmp(name, "Input") == 0) return IR_COMPONENT_INPUT;
    if (strcmp(name, "Checkbox") == 0) return IR_COMPONENT_CHECKBOX;
    if (strcmp(name, "Dropdown") == 0) return IR_COMPONENT_DROPDOWN;
    if (strcmp(name, "Canvas") == 0) return IR_COMPONENT_CANVAS;
    if (strcmp(name, "Markdown") == 0) return IR_COMPONENT_MARKDOWN;
    if (strcmp(name, "Header") == 0 || strcmp(name, "Body") == 0) {
        return IR_COMPONENT_CONTAINER; // Special containers
    }
    return IR_COMPONENT_CONTAINER; // Default
}

// Color parsing helper
static IRColor parse_color(const char* color_str) {
    // Simple hex color parsing (#RRGGBB or #RGB)
    if (color_str[0] == '#') {
        unsigned int r, g, b;
        size_t len = strlen(color_str);
        if (len == 7) { // #RRGGBB
            sscanf(color_str + 1, "%02x%02x%02x", &r, &g, &b);
        } else if (len == 4) { // #RGB
            sscanf(color_str + 1, "%1x%1x%1x", &r, &g, &b);
            r = r * 17; g = g * 17; b = b * 17; // Expand
        } else {
            return ir_color_rgb(255, 255, 255); // Default white
        }
        return ir_color_rgb(r, g, b);
    }
    // Named colors
    return ir_color_named(color_str);
}

// Property parsing
static void set_property(IRComponent* component, const char* name, NimToken* value) {
    if (strcmp(name, "text") == 0 || strcmp(name, "content") == 0) {
        if (value->type == NIM_TOK_STRING) {
            char* str = parse_string_literal(value);
            ir_set_text_content(component, str);
            free(str);
        }
    } else if (strcmp(name, "width") == 0) {
        if (value->type == NIM_TOK_NUMBER) {
            char* str = token_to_string(value);
            ir_set_width(component, IR_DIMENSION_PX, atof(str));
            free(str);
        }
    } else if (strcmp(name, "height") == 0) {
        if (value->type == NIM_TOK_NUMBER) {
            char* str = token_to_string(value);
            ir_set_height(component, IR_DIMENSION_PX, atof(str));
            free(str);
        }
    } else if (strcmp(name, "background") == 0 || strcmp(name, "backgroundColor") == 0) {
        if (value->type == NIM_TOK_STRING) {
            char* str = parse_string_literal(value);
            IRColor color = parse_color(str);
            IRStyle* style = ir_get_style(component);
            if (!style) style = ir_create_style();
            ir_set_background_color(style, color.data.r, color.data.g, color.data.b, color.data.a);
            ir_set_style(component, style);
            free(str);
        }
    } else if (strcmp(name, "color") == 0) {
        if (value->type == NIM_TOK_STRING) {
            char* str = parse_string_literal(value);
            IRColor color = parse_color(str);
            IRStyle* style = ir_get_style(component);
            if (!style) {
                style = ir_create_style();
                ir_set_style(component, style);
            }
            ir_set_font(style, 0, NULL, color.data.r, color.data.g, color.data.b, color.data.a, false, false);
            free(str);
        }
    } else if (strcmp(name, "fontSize") == 0) {
        if (value->type == NIM_TOK_NUMBER) {
            char* str = token_to_string(value);
            IRStyle* style = ir_get_style(component);
            if (!style) {
                style = ir_create_style();
                ir_set_style(component, style);
            }
            ir_set_font(style, atof(str), NULL, 255, 255, 255, 255, false, false);
            free(str);
        }
    } else if (strcmp(name, "padding") == 0) {
        if (value->type == NIM_TOK_NUMBER) {
            char* str = token_to_string(value);
            float val = atof(str);
            ir_set_padding(component, val, val, val, val);
            free(str);
        }
    } else if (strcmp(name, "margin") == 0) {
        if (value->type == NIM_TOK_NUMBER) {
            char* str = token_to_string(value);
            float val = atof(str);
            ir_set_margin(component, val, val, val, val);
            free(str);
        }
    } else if (strcmp(name, "left") == 0 || strcmp(name, "top") == 0 ||
               strcmp(name, "right") == 0 || strcmp(name, "bottom") == 0 ||
               strcmp(name, "position") == 0 || strcmp(name, "justifyContent") == 0 ||
               strcmp(name, "alignItems") == 0) {
        // These properties require direct access which we'll skip for now
        // A full implementation would need layout API functions
    } else if (strcmp(name, "windowTitle") == 0 || strcmp(name, "windowWidth") == 0 ||
               strcmp(name, "windowHeight") == 0) {
        // Metadata - skip for now
    }
}

static void parse_properties(NimParser* parser, IRComponent* component) {
    skip_newlines(parser);

    while (!check(parser, NIM_TOK_EOF) && !check(parser, NIM_TOK_DEDENT)) {
        // Property assignment: name = value
        if (check(parser, NIM_TOK_IDENTIFIER)) {
            NimToken prop_name = parser->current;
            advance(parser);

            if (match(parser, NIM_TOK_EQUALS)) {
                NimToken value = parser->current;
                advance(parser);

                char* name_str = token_to_string(&prop_name);
                set_property(component, name_str, &value);
                free(name_str);

                skip_newlines(parser);
            } else if (check(parser, NIM_TOK_COLON)) {
                // This is a child component, not a property
                // Back up and let parse_children handle it
                parser->current = prop_name;
                break;
            }
        } else {
            break;
        }
    }
}

static void parse_children(NimParser* parser, IRComponent* parent) {
    skip_newlines(parser);

    if (match(parser, NIM_TOK_INDENT)) {
        while (!check(parser, NIM_TOK_DEDENT) && !check(parser, NIM_TOK_EOF)) {
            skip_newlines(parser);

            if (check(parser, NIM_TOK_DEDENT) || check(parser, NIM_TOK_EOF)) {
                break;
            }

            IRComponent* child = parse_component(parser);
            if (child) {
                ir_add_child(parent, child);
            }

            skip_newlines(parser);
        }

        if (!check(parser, NIM_TOK_EOF)) {
            consume(parser, NIM_TOK_DEDENT, "Expected dedent after block");
        }
    }
}

static IRComponent* parse_component(NimParser* parser) {
    if (!check(parser, NIM_TOK_IDENTIFIER)) {
        error_at_current(parser, "Expected component name");
        return NULL;
    }

    NimToken name_token = parser->current;
    advance(parser);

    char* name = token_to_string(&name_token);
    IRComponentType type = string_to_component_type(name);

    IRComponent* component = ir_create_component(type);
    free(name);

    if (!component) {
        error(parser, "Failed to create component");
        return NULL;
    }

    consume(parser, NIM_TOK_COLON, "Expected ':' after component name");
    skip_newlines(parser);

    // Parse properties and children
    if (match(parser, NIM_TOK_INDENT)) {
        skip_newlines(parser);

        // First, parse properties
        parse_properties(parser, component);

        // Then parse child components
        while (!check(parser, NIM_TOK_DEDENT) && !check(parser, NIM_TOK_EOF)) {
            skip_newlines(parser);

            if (check(parser, NIM_TOK_DEDENT) || check(parser, NIM_TOK_EOF)) {
                break;
            }

            if (check(parser, NIM_TOK_IDENTIFIER)) {
                IRComponent* child = parse_component(parser);
                if (child) {
                    ir_add_child(component, child);
                }
            } else {
                break;
            }

            skip_newlines(parser);
        }

        if (!check(parser, NIM_TOK_EOF)) {
            consume(parser, NIM_TOK_DEDENT, "Expected dedent after component block");
        }
    }

    return component;
}

static IRComponent* parse_body(NimParser* parser) {
    skip_newlines(parser);

    // Look for "let app = kryonApp:"
    bool found_app = false;
    while (!check(parser, NIM_TOK_EOF)) {
        if (check(parser, NIM_TOK_IDENTIFIER)) {
            NimToken tok = parser->current;
            char* str = token_to_string(&tok);

            if (strcmp(str, "kryonApp") == 0) {
                free(str);
                found_app = true;
                advance(parser);
                consume(parser, NIM_TOK_COLON, "Expected ':' after kryonApp");
                break;
            }

            free(str);
        }
        advance(parser);
    }

    if (!found_app) {
        error(parser, "Could not find kryonApp declaration");
        return NULL;
    }

    skip_newlines(parser);
    consume(parser, NIM_TOK_INDENT, "Expected indent after kryonApp:");

    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    skip_newlines(parser);

    // Parse Header and Body sections
    IRComponent* body_component = NULL;

    while (!check(parser, NIM_TOK_DEDENT) && !check(parser, NIM_TOK_EOF)) {
        skip_newlines(parser);

        if (check(parser, NIM_TOK_IDENTIFIER)) {
            NimToken section_name = parser->current;
            char* section_str = token_to_string(&section_name);

            if (strcmp(section_str, "Header") == 0) {
                free(section_str);
                advance(parser);
                consume(parser, NIM_TOK_COLON, "Expected ':' after Header");
                skip_newlines(parser);

                // Skip Header properties for now (windowTitle, etc.)
                if (match(parser, NIM_TOK_INDENT)) {
                    while (!check(parser, NIM_TOK_DEDENT) && !check(parser, NIM_TOK_EOF)) {
                        advance(parser);
                    }
                    if (!check(parser, NIM_TOK_EOF)) {
                        consume(parser, NIM_TOK_DEDENT, "Expected dedent");
                    }
                }
            } else if (strcmp(section_str, "Body") == 0) {
                free(section_str);
                advance(parser);
                consume(parser, NIM_TOK_COLON, "Expected ':' after Body");
                skip_newlines(parser);

                body_component = ir_create_component(IR_COMPONENT_CONTAINER);

                if (match(parser, NIM_TOK_INDENT)) {
                    skip_newlines(parser);

                    // Parse Body properties
                    parse_properties(parser, body_component);

                    // Parse Body children
                    while (!check(parser, NIM_TOK_DEDENT) && !check(parser, NIM_TOK_EOF)) {
                        skip_newlines(parser);

                        if (check(parser, NIM_TOK_IDENTIFIER)) {
                            IRComponent* child = parse_component(parser);
                            if (child) {
                                ir_add_child(body_component, child);
                            }
                        } else {
                            break;
                        }

                        skip_newlines(parser);
                    }

                    if (!check(parser, NIM_TOK_EOF)) {
                        consume(parser, NIM_TOK_DEDENT, "Expected dedent after Body");
                    }
                }
            } else {
                free(section_str);
                error_at_current(parser, "Expected Header or Body section");
                advance(parser);
            }
        } else {
            break;
        }

        skip_newlines(parser);
    }

    if (body_component) {
        // Use body as root
        ir_destroy_component(root);
        return body_component;
    }

    return root;
}

// Public API

char* ir_nim_to_kir(const char* source, size_t length) {
    if (!source) return NULL;

    NimParser parser;
    nim_lexer_init(&parser.lexer, source, length);
    parser.had_error = false;
    parser.panic_mode = false;
    parser.error_message[0] = '\0';

    advance(&parser); // Prime the parser

    IRComponent* root = parse_body(&parser);

    nim_lexer_free(&parser.lexer);

    if (parser.had_error || !root) {
        if (root) ir_destroy_component(root);
        return NULL;
    }

    // Serialize to KIR JSON
    char* kir_json = ir_serialize_json(root, NULL);

    ir_destroy_component(root);

    return kir_json;
}

char* ir_nim_file_to_kir(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filepath);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file
    char* source = malloc(size + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(source, 1, size, file);
    source[read] = '\0';
    fclose(file);

    char* kir = ir_nim_to_kir(source, read);
    free(source);

    return kir;
}
