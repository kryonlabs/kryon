/**
 * @file parser.c
 * @brief Kryon Parser Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Recursive descent parser for KRY language that creates AST nodes from
 * tokenized input with comprehensive error recovery and validation.
 */

#include "internal/parser.h"
#include "internal/memory.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonASTNode *parse_document(KryonParser *parser);
static KryonASTNode *parse_element(KryonParser *parser);
static KryonASTNode *parse_style_block(KryonParser *parser);
static KryonASTNode *parse_theme_definition(KryonParser *parser);
static KryonASTNode *parse_variable_definition(KryonParser *parser);
static KryonASTNode *parse_function_definition(KryonParser *parser);
static KryonASTNode *parse_directive(KryonParser *parser);
static KryonASTNode *parse_property(KryonParser *parser);
static KryonASTNode *parse_expression(KryonParser *parser);
static KryonASTNode *parse_primary(KryonParser *parser);
static KryonASTNode *parse_literal(KryonParser *parser);
static KryonASTNode *parse_variable(KryonParser *parser);
static KryonASTNode *parse_template(KryonParser *parser);

static bool match_token(KryonParser *parser, KryonTokenType type);
static bool check_token(KryonParser *parser, KryonTokenType type);
static const KryonToken *advance(KryonParser *parser);
static const KryonToken *peek(KryonParser *parser);
static const KryonToken *previous(KryonParser *parser);
static bool at_end(KryonParser *parser);

static void parser_error(KryonParser *parser, const char *message);
static void synchronize(KryonParser *parser);
static void enter_panic_mode(KryonParser *parser);
static void exit_panic_mode(KryonParser *parser);

// =============================================================================
// PARSER CONFIGURATION
// =============================================================================

KryonParserConfig kryon_parser_default_config(void) {
    KryonParserConfig config = {
        .strict_mode = false,
        .error_recovery = true,
        .preserve_comments = false,
        .validate_properties = true,
        .max_nesting_depth = 64,
        .max_errors = 100
    };
    return config;
}

KryonParserConfig kryon_parser_strict_config(void) {
    KryonParserConfig config = {
        .strict_mode = true,
        .error_recovery = false,
        .preserve_comments = true,
        .validate_properties = true,
        .max_nesting_depth = 32,
        .max_errors = 10
    };
    return config;
}

KryonParserConfig kryon_parser_permissive_config(void) {
    KryonParserConfig config = {
        .strict_mode = false,
        .error_recovery = true,
        .preserve_comments = false,
        .validate_properties = false,
        .max_nesting_depth = 128,
        .max_errors = 1000
    };
    return config;
}

// =============================================================================
// PARSER LIFECYCLE
// =============================================================================

KryonParser *kryon_parser_create(KryonLexer *lexer, const KryonParserConfig *config) {
    if (!lexer) {
        return NULL;
    }
    
    // Get tokens from lexer
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    printf("[DEBUG] Lexer produced %zu tokens\n", token_count);
    if (!tokens || token_count == 0) {
        printf("[DEBUG] No tokens produced by lexer\n");
        return NULL;
    }
    
    
    KryonParser *parser = kryon_alloc(sizeof(KryonParser));
    if (!parser) {
        return NULL;
    }
    
    // Initialize parser state
    parser->lexer = lexer;
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current_token = 0;
    parser->nesting_depth = 0;
    
    // Set configuration
    if (config) {
        parser->config = *config;
    } else {
        parser->config = kryon_parser_default_config();
    }
    
    // Initialize AST
    parser->root = NULL;
    parser->next_node_id = 1;
    
    // Initialize error tracking
    parser->has_errors = false;
    parser->error_messages = NULL;
    parser->error_count = 0;
    parser->error_capacity = 0;
    parser->panic_mode = false;
    
    // Initialize memory management
    parser->all_nodes = NULL;
    parser->node_count = 0;
    parser->node_capacity = 0;
    
    // Initialize statistics
    parser->parsing_time = 0.0;
    parser->nodes_created = 0;
    parser->max_depth_reached = 0;
    
    return parser;
}

void kryon_parser_destroy(KryonParser *parser) {
    if (!parser) {
        return;
    }
    
    // Free all AST nodes
    if (parser->all_nodes) {
        for (size_t i = 0; i < parser->node_count; i++) {
            if (parser->all_nodes[i]) {
                // Free node-specific data
                KryonASTNode *node = parser->all_nodes[i];
                switch (node->type) {
                    case KRYON_AST_ELEMENT:
                        kryon_free(node->data.element.element_type);
                        kryon_free(node->data.element.children);
                        kryon_free(node->data.element.properties);
                        break;
                    case KRYON_AST_PROPERTY:
                        kryon_free(node->data.property.name);
                        break;
                    case KRYON_AST_STYLE_BLOCK:
                        kryon_free(node->data.style.name);
                        kryon_free(node->data.style.parent_name);
                        kryon_free(node->data.style.properties);
                        break;
                    case KRYON_AST_THEME_DEFINITION:
                        kryon_free(node->data.theme.group_name);
                        kryon_free(node->data.theme.variables);
                        break;
                    case KRYON_AST_VARIABLE_DEFINITION:
                        kryon_free(node->data.variable_def.name);
                        kryon_free(node->data.variable_def.type);
                        break;
                    case KRYON_AST_FUNCTION_DEFINITION:
                        kryon_free(node->data.function_def.language);
                        kryon_free(node->data.function_def.name);
                        kryon_free(node->data.function_def.code);
                        if (node->data.function_def.parameters) {
                            for (size_t j = 0; j < node->data.function_def.parameter_count; j++) {
                                kryon_free(node->data.function_def.parameters[j]);
                            }
                            kryon_free(node->data.function_def.parameters);
                        }
                        break;
                    case KRYON_AST_LITERAL:
                        kryon_ast_value_free(&node->data.literal.value);
                        break;
                    case KRYON_AST_VARIABLE:
                        kryon_free(node->data.variable.name);
                        break;
                    case KRYON_AST_FUNCTION_CALL:
                        kryon_free(node->data.function_call.function_name);
                        kryon_free(node->data.function_call.arguments);
                        break;
                    case KRYON_AST_MEMBER_ACCESS:
                        kryon_free(node->data.member_access.member);
                        break;
                    case KRYON_AST_ERROR:
                        kryon_free(node->data.error.message);
                        break;
                    case KRYON_AST_METADATA_DIRECTIVE:
                    case KRYON_AST_EVENT_DIRECTIVE:
                        // These use element structure for properties
                        kryon_free(node->data.element.element_type);
                        kryon_free(node->data.element.children);
                        kryon_free(node->data.element.properties);
                        break;
                    default:
                        break;
                }
                kryon_free(node);
            }
        }
        kryon_free(parser->all_nodes);
    }
    
    // Free error messages
    if (parser->error_messages) {
        for (size_t i = 0; i < parser->error_count; i++) {
            kryon_free(parser->error_messages[i]);
        }
        kryon_free(parser->error_messages);
    }
    
    kryon_free(parser);
}

// =============================================================================
// PARSING IMPLEMENTATION
// =============================================================================

bool kryon_parser_parse(KryonParser *parser) {
    if (!parser || !parser->tokens) {
        return false;
    }
    
    clock_t start_time = clock();
    
    // Reset parser state
    parser->current_token = 0;
    parser->nesting_depth = 0;
    parser->has_errors = false;
    parser->panic_mode = false;
    
    // Parse the document
    parser->root = parse_document(parser);
    
    // Calculate parsing time
    clock_t end_time = clock();
    parser->parsing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    return parser->root != NULL && !parser->has_errors;
}

static KryonASTNode *parse_document(KryonParser *parser) {
    printf("[DEBUG] Starting parse_document\n");
    
    KryonASTNode *root = kryon_ast_create_node(parser, KRYON_AST_ROOT, 
                                              &parser->tokens[0].location);
    if (!root) {
        printf("[DEBUG] Failed to create root node\n");
        return NULL;
    }
    printf("[DEBUG] Created root node\n");
    
    // Parse top-level declarations  
    printf("[DEBUG] Starting parsing loop, current_token=%zu, token_count=%zu\n", parser->current_token, parser->token_count);
    
    // Debug: Show first 10 tokens
    printf("[DEBUG] First 10 tokens:\n");
    for (size_t i = 0; i < (parser->token_count < 10 ? parser->token_count : 10); i++) {
        const KryonToken* tok = &parser->tokens[i];
        printf("  [%zu] type=%d, lexeme='%.*s'\n", i, tok->type, (int)tok->lexeme_length, tok->lexeme ? tok->lexeme : "NULL");
    }
    
    while (!at_end(parser)) {
        KryonASTNode *node = NULL;
        const KryonToken *current = peek(parser);
        printf("[DEBUG] Processing token type %d at index %zu\n", current->type, parser->current_token);
        
        if (check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) ||
            check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
            printf("[DEBUG] Parsing style block\n");
            node = parse_style_block(parser);
        } else if (check_token(parser, KRYON_TOKEN_THEME_DIRECTIVE)) {
            printf("[DEBUG] Parsing theme definition\n");
            node = parse_theme_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_VARIABLE_DIRECTIVE) ||
                   check_token(parser, KRYON_TOKEN_VARIABLES_DIRECTIVE)) {
            printf("[DEBUG] Parsing variable definition\n");
            node = parse_variable_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE)) {
            printf("[DEBUG] Parsing function definition\n");
            node = parse_function_definition(parser);
        } else if (kryon_token_is_directive(peek(parser)->type)) {
            printf("[DEBUG] Parsing directive\n");
            node = parse_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            printf("[DEBUG] Parsing element\n");
            node = parse_element(parser);
        } else {
            printf("[DEBUG] Unknown token type %d, error\n", current->type);
            parser_error(parser, "Expected element, style definition, theme definition, or directive");
            advance(parser); // Skip invalid token
            continue;
        }
        
        if (node) {
            printf("[DEBUG] Successfully parsed node of type %s\n", kryon_ast_node_type_name(node->type));
            if (!kryon_ast_add_child(root, node)) {
                printf("[DEBUG] Failed to add node to AST\n");
                parser_error(parser, "Failed to add node to AST");
            } else {
                printf("[DEBUG] Successfully added node to root (children: %zu)\n", root->data.element.child_count);
            }
        } else {
            printf("[DEBUG] Failed to parse node\n");
        }
    }
    
    printf("[DEBUG] Finished parsing document, root has %zu children\n", root->data.element.child_count);
    return root;
}

static KryonASTNode *parse_element(KryonParser *parser) {
    printf("[DEBUG] parse_element: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
        printf("[DEBUG] parse_element: Not an element type token\n");
        parser_error(parser, "Expected element type");
        return NULL;
    }
    
    // Check nesting depth
    if (parser->nesting_depth >= parser->config.max_nesting_depth) {
        printf("[DEBUG] parse_element: Maximum nesting depth exceeded\n");
        parser_error(parser, "Maximum nesting depth exceeded");
        return NULL;
    }
    
    const KryonToken *type_token = advance(parser);
    printf("[DEBUG] parse_element: Creating element of type %.*s\n", 
           (int)type_token->lexeme_length, type_token->lexeme);
    
    KryonASTNode *element = kryon_ast_create_node(parser, KRYON_AST_ELEMENT,
                                                 &type_token->location);
    if (!element) {
        printf("[DEBUG] parse_element: Failed to create AST node\n");
        return NULL;
    }
    printf("[DEBUG] parse_element: Created AST node\n");
    
    // Copy element type name
    element->data.element.element_type = kryon_token_copy_lexeme(type_token);
    element->data.element.children = NULL;
    element->data.element.child_count = 0;
    element->data.element.child_capacity = 0;
    element->data.element.properties = NULL;
    element->data.element.property_count = 0;
    element->data.element.property_capacity = 0;
    
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' after element type");
        return element;
    }
    
    parser->nesting_depth++;
    if (parser->nesting_depth > parser->max_depth_reached) {
        parser->max_depth_reached = parser->nesting_depth;
    }
    
    // Parse element body
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            // Child element
            KryonASTNode *child = parse_element(parser);
            if (child && !kryon_ast_add_child(element, child)) {
                parser_error(parser, "Failed to add child element");
            }
        } else if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            // Property
            KryonASTNode *property = parse_property(parser);
            if (property && !kryon_ast_add_property(element, property)) {
                parser_error(parser, "Failed to add property");
            }
        } else {
            parser_error(parser, "Expected property or child element");
            advance(parser); // Skip invalid token
        }
    }
    
    parser->nesting_depth--;
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after element body");
    }
    
    return element;
}

static KryonASTNode *parse_style_block(KryonParser *parser) {
    printf("[DEBUG] parse_style_block: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) &&
        !check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
        printf("[DEBUG] parse_style_block: Not a style directive\n");
        parser_error(parser, "Expected @style or @styles directive");
        return NULL;
    }
    
    const KryonToken *directive_token = advance(parser);
    printf("[DEBUG] parse_style_block: Found style directive\n");
    
    // Expect style name
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        printf("[DEBUG] parse_style_block: Expected string after @style\n");
        parser_error(parser, "Expected style name string");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    printf("[DEBUG] parse_style_block: Style name: %s\n", name_token->value.string_value);
    
    KryonASTNode *style = kryon_ast_create_node(parser, KRYON_AST_STYLE_BLOCK,
                                               &directive_token->location);
    if (!style) {
        printf("[DEBUG] parse_style_block: Failed to create style node\n");
        return NULL;
    }
    printf("[DEBUG] parse_style_block: Created style node\n");
    
    // Copy style name (remove quotes)
    style->data.style.name = kryon_strdup(name_token->value.string_value);
    style->data.style.parent_name = NULL;
    style->data.style.properties = NULL;
    style->data.style.property_count = 0;
    style->data.style.property_capacity = 0;
    
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' after style name");
        return style;
    }
    
    // Parse style properties
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            KryonASTNode *property = parse_property(parser);
            if (property && !kryon_ast_add_property(style, property)) {
                parser_error(parser, "Failed to add style property");
            }
        } else {
            parser_error(parser, "Expected style property");
            advance(parser);
        }
    }
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after style body");
    }
    
    return style;
}

static KryonASTNode *parse_property(KryonParser *parser) {
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected property name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *property = kryon_ast_create_node(parser, KRYON_AST_PROPERTY,
                                                  &name_token->location);
    if (!property) {
        return NULL;
    }
    
    property->data.property.name = kryon_token_copy_lexeme(name_token);
    
    if (!match_token(parser, KRYON_TOKEN_COLON)) {
        parser_error(parser, "Expected ':' after property name");
        return property;
    }
    
    // Parse property value
    property->data.property.value = parse_expression(parser);
    if (!property->data.property.value) {
        parser_error(parser, "Expected property value");
    }
    
    return property;
}

static KryonASTNode *parse_expression(KryonParser *parser) {
    return parse_primary(parser);
}

static KryonASTNode *parse_primary(KryonParser *parser) {
    if (check_token(parser, KRYON_TOKEN_STRING) ||
        check_token(parser, KRYON_TOKEN_INTEGER) ||
        check_token(parser, KRYON_TOKEN_FLOAT) ||
        check_token(parser, KRYON_TOKEN_BOOLEAN_TRUE) ||
        check_token(parser, KRYON_TOKEN_BOOLEAN_FALSE) ||
        check_token(parser, KRYON_TOKEN_NULL)) {
        return parse_literal(parser);
    }
    
    if (check_token(parser, KRYON_TOKEN_VARIABLE)) {
        return parse_variable(parser);
    }
    
    if (check_token(parser, KRYON_TOKEN_TEMPLATE_START)) {
        return parse_template(parser);
    }
    
    parser_error(parser, "Expected literal, variable, or template expression");
    return NULL;
}

static KryonASTNode *parse_literal(KryonParser *parser) {
    const KryonToken *token = advance(parser);
    
    KryonASTNode *literal = kryon_ast_create_node(parser, KRYON_AST_LITERAL,
                                                 &token->location);
    if (!literal) {
        return NULL;
    }
    
    switch (token->type) {
        case KRYON_TOKEN_STRING:
            literal->data.literal.value = kryon_ast_value_string(token->value.string_value);
            break;
        case KRYON_TOKEN_INTEGER:
        case KRYON_TOKEN_FLOAT: {
            // Check if followed by a unit
            if (peek(parser) && kryon_token_is_unit(peek(parser)->type)) {
                const KryonToken *unit_token = advance(parser);
                
                // Create a string value combining number and unit
                char value_with_unit[64];
                char unit_str[16];
                
                // Copy unit lexeme (it's not null-terminated)
                size_t unit_len = unit_token->lexeme_length < sizeof(unit_str) - 1 ? 
                                 unit_token->lexeme_length : sizeof(unit_str) - 1;
                memcpy(unit_str, unit_token->lexeme, unit_len);
                unit_str[unit_len] = '\0';
                
                // Create proper unit value instead of string
                double numeric_value;
                if (token->type == KRYON_TOKEN_INTEGER) {
                    numeric_value = (double)token->value.int_value;
                } else {
                    numeric_value = token->value.float_value;
                }
                
                literal->data.literal.value = kryon_ast_value_unit(numeric_value, unit_str);
            } else {
                // No unit, just the number
                if (token->type == KRYON_TOKEN_INTEGER) {
                    literal->data.literal.value = kryon_ast_value_integer(token->value.int_value);
                } else {
                    literal->data.literal.value = kryon_ast_value_float(token->value.float_value);
                }
            }
            break;
        }
        case KRYON_TOKEN_BOOLEAN_TRUE:
            literal->data.literal.value = kryon_ast_value_boolean(true);
            break;
        case KRYON_TOKEN_BOOLEAN_FALSE:
            literal->data.literal.value = kryon_ast_value_boolean(false);
            break;
        case KRYON_TOKEN_NULL:
            literal->data.literal.value = kryon_ast_value_null();
            break;
        default:
            parser_error(parser, "Invalid literal token");
            return NULL;
    }
    
    return literal;
}

static KryonASTNode *parse_variable(KryonParser *parser) {
    if (!check_token(parser, KRYON_TOKEN_VARIABLE)) {
        parser_error(parser, "Expected variable");
        return NULL;
    }
    
    const KryonToken *token = advance(parser);
    
    KryonASTNode *variable = kryon_ast_create_node(parser, KRYON_AST_VARIABLE,
                                                  &token->location);
    if (!variable) {
        return NULL;
    }
    
    // Copy variable name (without $)
    char *lexeme = kryon_token_copy_lexeme(token);
    if (lexeme && lexeme[0] == '$') {
        variable->data.variable.name = kryon_strdup(lexeme + 1);
        kryon_free(lexeme);
    } else {
        variable->data.variable.name = lexeme;
    }
    
    return variable;
}

static KryonASTNode *parse_template(KryonParser *parser) {
    if (!match_token(parser, KRYON_TOKEN_TEMPLATE_START)) {
        parser_error(parser, "Expected template start '${}'");
        return NULL;
    }
    
    KryonASTNode *template_node = kryon_ast_create_node(parser, KRYON_AST_TEMPLATE,
                                                       &previous(parser)->location);
    if (!template_node) {
        return NULL;
    }
    
    // Parse template expression
    template_node->data.template.expression = parse_expression(parser);
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after template expression");
    }
    
    return template_node;
}

static KryonASTNode *parse_directive(KryonParser *parser) {
    const KryonToken *token = advance(parser);
    
    // Map token type to AST node type
    KryonASTNodeType ast_type;
    switch (token->type) {
        case KRYON_TOKEN_STORE_DIRECTIVE:
            ast_type = KRYON_AST_STORE_DIRECTIVE;
            break;
        case KRYON_TOKEN_WATCH_DIRECTIVE:
            ast_type = KRYON_AST_WATCH_DIRECTIVE;
            break;
        case KRYON_TOKEN_ON_MOUNT_DIRECTIVE:
            ast_type = KRYON_AST_ON_MOUNT_DIRECTIVE;
            break;
        case KRYON_TOKEN_ON_UNMOUNT_DIRECTIVE:
            ast_type = KRYON_AST_ON_UNMOUNT_DIRECTIVE;
            break;
        case KRYON_TOKEN_IMPORT_DIRECTIVE:
            ast_type = KRYON_AST_IMPORT_DIRECTIVE;
            break;
        case KRYON_TOKEN_EXPORT_DIRECTIVE:
            ast_type = KRYON_AST_EXPORT_DIRECTIVE;
            break;
        case KRYON_TOKEN_INCLUDE_DIRECTIVE:
            ast_type = KRYON_AST_INCLUDE_DIRECTIVE;
            break;
        case KRYON_TOKEN_METADATA_DIRECTIVE:
            ast_type = KRYON_AST_METADATA_DIRECTIVE;
            break;
        case KRYON_TOKEN_EVENT_DIRECTIVE:
            ast_type = KRYON_AST_EVENT_DIRECTIVE;
            break;
        case KRYON_TOKEN_COMPONENT_DIRECTIVE:
            ast_type = KRYON_AST_COMPONENT;
            break;
        case KRYON_TOKEN_PROPS_DIRECTIVE:
            ast_type = KRYON_AST_PROPS;
            break;
        case KRYON_TOKEN_SLOTS_DIRECTIVE:
            ast_type = KRYON_AST_SLOTS;
            break;
        case KRYON_TOKEN_LIFECYCLE_DIRECTIVE:
            ast_type = KRYON_AST_LIFECYCLE;
            break;
        default:
            parser_error(parser, "Unknown directive type");
            return NULL;
    }
    
    KryonASTNode *directive = kryon_ast_create_node(parser, ast_type, &token->location);
    if (!directive) {
        parser_error(parser, "Failed to create directive AST node");
        return NULL;
    }
    
    // Parse directive body (properties in braces)
    if (check_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        advance(parser); // consume '{'
        
        while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
            KryonASTNode *property = parse_property(parser);
            if (property) {
                kryon_ast_add_property(directive, property);
            } else {
                // Skip to next property or end of block
                while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) &&
                       !check_token(parser, KRYON_TOKEN_IDENTIFIER) &&
                       !at_end(parser)) {
                    advance(parser);
                }
            }
        }
        
        if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
            parser_error(parser, "Expected '}' after directive body");
        }
    }
    
    return directive;
}

static KryonASTNode *parse_theme_definition(KryonParser *parser) {
    if (!check_token(parser, KRYON_TOKEN_THEME_DIRECTIVE)) {
        parser_error(parser, "Expected '@theme' directive");
        return NULL;
    }
    
    const KryonToken *theme_token = advance(parser);
    
    // Expect theme group name
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected theme group name");
        return NULL;
    }
    
    const KryonToken *group_token = advance(parser);
    
    KryonASTNode *theme = kryon_ast_create_node(parser, KRYON_AST_THEME_DEFINITION,
                                               &theme_token->location);
    if (!theme) {
        return NULL;
    }
    
    // Copy theme group name
    theme->data.theme.group_name = kryon_token_copy_lexeme(group_token);
    theme->data.theme.variables = NULL;
    theme->data.theme.variable_count = 0;
    theme->data.theme.variable_capacity = 0;
    
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' after theme group name");
        return theme;
    }
    
    // Parse theme variables (they look like properties)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            KryonASTNode *variable = parse_property(parser);
            if (variable) {
                // Expand variables array if needed
                if (theme->data.theme.variable_count >= theme->data.theme.variable_capacity) {
                    size_t new_capacity = theme->data.theme.variable_capacity ? 
                                         theme->data.theme.variable_capacity * 2 : 4;
                    KryonASTNode **new_variables = realloc(theme->data.theme.variables,
                                                          new_capacity * sizeof(KryonASTNode*));
                    if (!new_variables) {
                        parser_error(parser, "Failed to allocate theme variables");
                        break;
                    }
                    theme->data.theme.variables = new_variables;
                    theme->data.theme.variable_capacity = new_capacity;
                }
                
                theme->data.theme.variables[theme->data.theme.variable_count++] = variable;
                variable->parent = theme;
            }
        } else {
            parser_error(parser, "Expected theme variable");
            advance(parser);
        }
    }
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after theme body");
    }
    
    return theme;
}

static KryonASTNode *parse_variable_definition(KryonParser *parser) {
    const KryonToken *directive_token = advance(parser); // consume @var or @variables
    
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected variable name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *var_def = kryon_ast_create_node(parser, KRYON_AST_VARIABLE_DEFINITION,
                                                 &directive_token->location);
    if (!var_def) {
        return NULL;
    }
    
    var_def->data.variable_def.name = kryon_token_copy_lexeme(name_token);
    var_def->data.variable_def.type = NULL;
    var_def->data.variable_def.value = NULL;
    
    // Optional type annotation
    if (check_token(parser, KRYON_TOKEN_COLON)) {
        advance(parser); // consume ':'
        
        if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected variable type");
            return var_def;
        }
        
        const KryonToken *type_token = advance(parser);
        var_def->data.variable_def.type = kryon_token_copy_lexeme(type_token);
    }
    
    // Expect assignment
    if (!match_token(parser, KRYON_TOKEN_EQUALS)) {
        parser_error(parser, "Expected '=' after variable name");
        return var_def;
    }
    
    // Parse variable value
    var_def->data.variable_def.value = parse_expression(parser);
    if (!var_def->data.variable_def.value) {
        parser_error(parser, "Expected variable value");
    }
    
    return var_def;
}

static KryonASTNode *parse_function_definition(KryonParser *parser) {
    printf("[DEBUG] parse_function_definition: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE)) {
        printf("[DEBUG] parse_function_definition: Not a function directive\n");
        parser_error(parser, "Expected '@function' directive");
        return NULL;
    }
    
    const KryonToken *function_token = advance(parser);
    printf("[DEBUG] parse_function_definition: Found function directive\n");
    
    // Expect language string
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        printf("[DEBUG] parse_function_definition: Expected language string\n");
        parser_error(parser, "Expected function language string");
        return NULL;
    }
    
    const KryonToken *language_token = advance(parser);
    printf("[DEBUG] parse_function_definition: Language: %s\n", language_token->value.string_value);
    
    // Expect function name
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        printf("[DEBUG] parse_function_definition: Expected function name\n");
        parser_error(parser, "Expected function name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    printf("[DEBUG] parse_function_definition: Function name: %.*s\n", 
           (int)name_token->lexeme_length, name_token->lexeme);
    
    KryonASTNode *func_def = kryon_ast_create_node(parser, KRYON_AST_FUNCTION_DEFINITION,
                                                  &function_token->location);
    if (!func_def) {
        printf("[DEBUG] parse_function_definition: Failed to create function node\n");
        return NULL;
    }
    printf("[DEBUG] parse_function_definition: Created function node\n");
    
    func_def->data.function_def.language = kryon_strdup(language_token->value.string_value);
    func_def->data.function_def.name = kryon_token_copy_lexeme(name_token);
    func_def->data.function_def.parameters = NULL;
    func_def->data.function_def.parameter_count = 0;
    func_def->data.function_def.code = NULL;
    
    // Parse parameters
    if (!match_token(parser, KRYON_TOKEN_LEFT_PAREN)) {
        parser_error(parser, "Expected '(' after function name");
        return func_def;
    }
    
    // Parse parameter list
    while (!check_token(parser, KRYON_TOKEN_RIGHT_PAREN) && !at_end(parser)) {
        if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected parameter name");
            break;
        }
        
        const KryonToken *param_token = advance(parser);
        
        // Expand parameters array
        size_t new_count = func_def->data.function_def.parameter_count + 1;
        char **new_params = realloc(func_def->data.function_def.parameters,
                                   new_count * sizeof(char*));
        if (!new_params) {
            parser_error(parser, "Failed to allocate function parameters");
            break;
        }
        
        func_def->data.function_def.parameters = new_params;
        func_def->data.function_def.parameters[func_def->data.function_def.parameter_count] = 
            kryon_token_copy_lexeme(param_token);
        func_def->data.function_def.parameter_count = new_count;
        
        if (check_token(parser, KRYON_TOKEN_COMMA)) {
            advance(parser); // consume comma
        } else if (!check_token(parser, KRYON_TOKEN_RIGHT_PAREN)) {
            parser_error(parser, "Expected ',' or ')' in parameter list");
            break;
        }
    }
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_PAREN)) {
        parser_error(parser, "Expected ')' after parameter list");
        return func_def;
    }
    
    // Parse function body (simple string for now)
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' before function body");
        return func_def;
    }
    
    // Capture the function body by building the code string from tokens
    int brace_count = 1;
    const char *code_start = peek(parser)->lexeme;
    size_t total_length = 0;
    
    // First pass: calculate total length needed
    const KryonToken *current_token = peek(parser);
    int temp_brace_count = 1;
    size_t temp_index = parser->current_token;
    
    while (temp_brace_count > 0 && temp_index < parser->token_count) {
        if (parser->tokens[temp_index].type == KRYON_TOKEN_LEFT_BRACE) {
            temp_brace_count++;
        } else if (parser->tokens[temp_index].type == KRYON_TOKEN_RIGHT_BRACE) {
            temp_brace_count--;
        }
        
        if (temp_brace_count > 0) {
            total_length += parser->tokens[temp_index].lexeme_length;
            if (temp_index + 1 < parser->token_count) {
                // Add space between tokens
                total_length += 1;
            }
        }
        temp_index++;
    }
    
    // Allocate buffer for function code
    char *code_buffer = kryon_alloc(total_length + 1);
    if (!code_buffer) {
        parser_error(parser, "Failed to allocate memory for function code");
        return func_def;
    }
    
    // Second pass: build the code string
    size_t code_pos = 0;
    while (brace_count > 0 && !at_end(parser)) {
        const KryonToken *token = peek(parser);
        
        if (check_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
            brace_count++;
        } else if (check_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
            brace_count--;
        }
        
        if (brace_count > 0) {
            // Copy token lexeme to code buffer
            memcpy(code_buffer + code_pos, token->lexeme, token->lexeme_length);
            code_pos += token->lexeme_length;
            
            // Add space between tokens (except before closing punctuation)
            if (!at_end(parser) && parser->current_token + 1 < parser->token_count) {
                const KryonToken *next_token = &parser->tokens[parser->current_token + 1];
                if (next_token->type != KRYON_TOKEN_RIGHT_PAREN && 
                    next_token->type != KRYON_TOKEN_RIGHT_BRACE &&
                    next_token->type != KRYON_TOKEN_SEMICOLON) {
                    code_buffer[code_pos++] = ' ';
                }
            }
            
            advance(parser);
        }
    }
    
    code_buffer[code_pos] = '\0';
    
    if (brace_count == 0) {
        func_def->data.function_def.code = code_buffer;
        advance(parser); // consume final '}'
    } else {
        parser_error(parser, "Unterminated function body");
        kryon_free(code_buffer);
    }
    
    return func_def;
}

// =============================================================================
// TOKEN UTILITIES
// =============================================================================

static bool match_token(KryonParser *parser, KryonTokenType type) {
    if (check_token(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

static bool check_token(KryonParser *parser, KryonTokenType type) {
    if (at_end(parser)) {
        return false;
    }
    return peek(parser)->type == type;
}

static const KryonToken *advance(KryonParser *parser) {
    if (!at_end(parser)) {
        parser->current_token++;
    }
    return previous(parser);
}

static const KryonToken *peek(KryonParser *parser) {
    if (parser->current_token >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1]; // EOF token
    }
    return &parser->tokens[parser->current_token];
}

static const KryonToken *previous(KryonParser *parser) {
    if (parser->current_token == 0) {
        return &parser->tokens[0];
    }
    return &parser->tokens[parser->current_token - 1];
}

static bool at_end(KryonParser *parser) {
    bool result = parser->current_token >= parser->token_count ||
                  peek(parser)->type == KRYON_TOKEN_EOF;
    // Debug removed
    return result;
}

// =============================================================================
// ERROR HANDLING
// =============================================================================

static void parser_error(KryonParser *parser, const char *message) {
    if (parser->panic_mode) {
        return; // Suppress errors in panic mode
    }
    
    parser->has_errors = true;
    
    // Check error limit
    if (parser->error_count >= parser->config.max_errors) {
        return;
    }
    
    // Expand error array if needed
    if (parser->error_count >= parser->error_capacity) {
        size_t new_capacity = parser->error_capacity ? parser->error_capacity * 2 : 8;
        char **new_messages = kryon_realloc(parser->error_messages,
                                           new_capacity * sizeof(char*));
        if (!new_messages) {
            return;
        }
        parser->error_messages = new_messages;
        parser->error_capacity = new_capacity;
    }
    
    // Format error message with location
    const KryonToken *token = peek(parser);
    char location_str[128];
    kryon_source_location_format(&token->location, location_str, sizeof(location_str));
    
    size_t msg_len = strlen(message) + strlen(location_str) + 16;
    char *error_msg = kryon_alloc(msg_len);
    if (error_msg) {
        snprintf(error_msg, msg_len, "%s at %s", message, location_str);
        parser->error_messages[parser->error_count++] = error_msg;
    }
    
    if (parser->config.error_recovery) {
        enter_panic_mode(parser);
        synchronize(parser);
    }
}

static void synchronize(KryonParser *parser) {
    advance(parser);
    
    while (!at_end(parser)) {
        if (previous(parser)->type == KRYON_TOKEN_RIGHT_BRACE) {
            return;
        }
        
        switch (peek(parser)->type) {
            case KRYON_TOKEN_STYLE_DIRECTIVE:
            case KRYON_TOKEN_STYLES_DIRECTIVE:
            case KRYON_TOKEN_ELEMENT_TYPE:
                return;
            default:
                break;
        }
        
        advance(parser);
    }
    
    exit_panic_mode(parser);
}

static void enter_panic_mode(KryonParser *parser) {
    parser->panic_mode = true;
}

static void exit_panic_mode(KryonParser *parser) {
    parser->panic_mode = false;
}

// =============================================================================
// AST NODE MANAGEMENT
// =============================================================================

KryonASTNode *kryon_ast_create_node(KryonParser *parser, KryonASTNodeType type,
                                   const KryonSourceLocation *location) {
    KryonASTNode *node = kryon_alloc(sizeof(KryonASTNode));
    if (!node) {
        return NULL;
    }
    
    // Initialize node
    memset(node, 0, sizeof(KryonASTNode));
    node->type = type;
    node->location = *location;
    node->parent = NULL;
    node->node_id = parser->next_node_id++;
    
    // Add to parser's node tracking
    if (parser->node_count >= parser->node_capacity) {
        size_t new_capacity = parser->node_capacity ? parser->node_capacity * 2 : 32;
        KryonASTNode **new_nodes = kryon_realloc(parser->all_nodes,
                                                new_capacity * sizeof(KryonASTNode*));
        if (!new_nodes) {
            kryon_free(node);
            return NULL;
        }
        parser->all_nodes = new_nodes;
        parser->node_capacity = new_capacity;
    }
    
    parser->all_nodes[parser->node_count++] = node;
    parser->nodes_created++;
    
    return node;
}

bool kryon_ast_add_child(KryonASTNode *parent, KryonASTNode *child) {
    if (!parent || !child) {
        return false;
    }
    
    // Only certain node types can have children
    if (parent->type != KRYON_AST_ROOT && parent->type != KRYON_AST_ELEMENT) {
        return false;
    }
    
    // For ROOT nodes, validate that child types are allowed at root level
    if (parent->type == KRYON_AST_ROOT) {
        switch (child->type) {
            case KRYON_AST_ELEMENT:
            case KRYON_AST_STYLE_BLOCK:
            case KRYON_AST_THEME_DEFINITION:
            case KRYON_AST_VARIABLE_DEFINITION:
            case KRYON_AST_FUNCTION_DEFINITION:
            case KRYON_AST_METADATA_DIRECTIVE:
            case KRYON_AST_EVENT_DIRECTIVE:
            case KRYON_AST_STORE_DIRECTIVE:
            case KRYON_AST_WATCH_DIRECTIVE:
            case KRYON_AST_ON_MOUNT_DIRECTIVE:
            case KRYON_AST_ON_UNMOUNT_DIRECTIVE:
            case KRYON_AST_IMPORT_DIRECTIVE:
            case KRYON_AST_EXPORT_DIRECTIVE:
            case KRYON_AST_INCLUDE_DIRECTIVE:
            case KRYON_AST_COMPONENT:
            case KRYON_AST_PROPS:
            case KRYON_AST_SLOTS:
            case KRYON_AST_LIFECYCLE:
                // These are allowed at root level
                break;
            default:
                // Invalid child type for root
                return false;
        }
    }
    
    // Expand children array if needed
    if (parent->data.element.child_count >= parent->data.element.child_capacity) {
        size_t new_capacity = parent->data.element.child_capacity ?
                             parent->data.element.child_capacity * 2 : 4;
        KryonASTNode **new_children = realloc(parent->data.element.children,
                                                   new_capacity * sizeof(KryonASTNode*));
        if (!new_children) {
            return false;
        }
        parent->data.element.children = new_children;
        parent->data.element.child_capacity = new_capacity;
    }
    
    parent->data.element.children[parent->data.element.child_count++] = child;
    child->parent = parent;
    
    return true;
}

bool kryon_ast_add_property(KryonASTNode *parent, KryonASTNode *property) {
    if (!parent || !property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Only elements, styles, metadata, and event directives can have properties
    if (parent->type != KRYON_AST_ELEMENT && 
        parent->type != KRYON_AST_STYLE_BLOCK && 
        parent->type != KRYON_AST_METADATA_DIRECTIVE &&
        parent->type != KRYON_AST_EVENT_DIRECTIVE) {
        return false;
    }
    
    // Get the appropriate property array
    KryonASTNode ***properties;
    size_t *property_count;
    size_t *property_capacity;
    
    if (parent->type == KRYON_AST_ELEMENT || parent->type == KRYON_AST_METADATA_DIRECTIVE || parent->type == KRYON_AST_EVENT_DIRECTIVE) {
        properties = &parent->data.element.properties;
        property_count = &parent->data.element.property_count;
        property_capacity = &parent->data.element.property_capacity;
    } else {
        properties = &parent->data.style.properties;
        property_count = &parent->data.style.property_count;
        property_capacity = &parent->data.style.property_capacity;
    }
    
    // Expand property array if needed
    if (*property_count >= *property_capacity) {
        size_t new_capacity = *property_capacity ? *property_capacity * 2 : 4;
        KryonASTNode **new_properties = realloc(*properties,
                                                     new_capacity * sizeof(KryonASTNode*));
        if (!new_properties) {
            return false;
        }
        *properties = new_properties;
        *property_capacity = new_capacity;
    }
    
    (*properties)[(*property_count)++] = property;
    property->parent = parent;
    
    return true;
}

// =============================================================================
// PARSER API IMPLEMENTATION
// =============================================================================

const KryonASTNode *kryon_parser_get_root(const KryonParser *parser) {
    return parser ? parser->root : NULL;
}

const char **kryon_parser_get_errors(const KryonParser *parser, size_t *out_count) {
    if (!parser || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    *out_count = parser->error_count;
    return (const char**)parser->error_messages;
}

void kryon_parser_get_stats(const KryonParser *parser, double *out_time,
                           size_t *out_nodes, size_t *out_depth) {
    if (!parser) {
        if (out_time) *out_time = 0.0;
        if (out_nodes) *out_nodes = 0;
        if (out_depth) *out_depth = 0;
        return;
    }
    
    if (out_time) *out_time = parser->parsing_time;
    if (out_nodes) *out_nodes = parser->nodes_created;
    if (out_depth) *out_depth = parser->max_depth_reached;
}

// =============================================================================
// AST VALUE IMPLEMENTATION
// =============================================================================

KryonASTValue kryon_ast_value_string(const char *str) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_STRING;
    value.data.string_value = str ? kryon_strdup(str) : NULL;
    return value;
}

KryonASTValue kryon_ast_value_integer(int64_t val) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_INTEGER;
    value.data.int_value = val;
    return value;
}

KryonASTValue kryon_ast_value_float(double val) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_FLOAT;
    value.data.float_value = val;
    return value;
}

KryonASTValue kryon_ast_value_boolean(bool val) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_BOOLEAN;
    value.data.bool_value = val;
    return value;
}

KryonASTValue kryon_ast_value_null(void) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_NULL;
    return value;
}

KryonASTValue kryon_ast_value_color(uint32_t rgba) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_COLOR;
    value.data.color_value = rgba;
    return value;
}

KryonASTValue kryon_ast_value_unit(double val, const char *unit) {
    KryonASTValue value = {0};
    value.type = KRYON_VALUE_UNIT;
    value.data.unit_value.value = val;
    if (unit) {
        strncpy(value.data.unit_value.unit, unit, sizeof(value.data.unit_value.unit) - 1);
        value.data.unit_value.unit[sizeof(value.data.unit_value.unit) - 1] = '\0';
    }
    return value;
}

void kryon_ast_value_free(KryonASTValue *value) {
    if (!value) {
        return;
    }
    
    if (value->type == KRYON_VALUE_STRING && value->data.string_value) {
        kryon_free(value->data.string_value);
        value->data.string_value = NULL;
    }
}

const char *kryon_ast_node_type_name(KryonASTNodeType type) {
    switch (type) {
        case KRYON_AST_ROOT: return "Root";
        case KRYON_AST_ELEMENT: return "Element";
        case KRYON_AST_PROPERTY: return "Property";
        case KRYON_AST_STYLE_BLOCK: return "StyleBlock";
        case KRYON_AST_THEME_DEFINITION: return "ThemeDefinition";
        case KRYON_AST_VARIABLE_DEFINITION: return "VariableDefinition";
        case KRYON_AST_FUNCTION_DEFINITION: return "FunctionDefinition";
        case KRYON_AST_LITERAL: return "Literal";
        case KRYON_AST_VARIABLE: return "Variable";
        case KRYON_AST_TEMPLATE: return "Template";
        case KRYON_AST_ERROR: return "Error";
        default: return "Unknown";
    }
}