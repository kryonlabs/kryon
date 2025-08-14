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

#include "parser.h"
#include "memory.h"
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
static KryonASTNode *parse_component_definition(KryonParser *parser);
static KryonASTNode *parse_state_definition(KryonParser *parser);
static KryonASTNode *parse_const_definition(KryonParser *parser);
static KryonASTNode *parse_const_for_loop(KryonParser *parser);
static KryonASTNode *parse_event_directive(KryonParser *parser);
static KryonASTNode *parse_directive(KryonParser *parser);
static KryonASTNode *parse_property(KryonParser *parser);
static KryonASTNode *parse_expression(KryonParser *parser);
static KryonASTNode *parse_postfix(KryonParser *parser);
static KryonASTNode *parse_primary(KryonParser *parser);
static KryonASTNode *parse_literal(KryonParser *parser);
static KryonASTNode *parse_variable(KryonParser *parser);
static KryonASTNode *parse_template(KryonParser *parser);
static KryonASTNode *parse_array_literal(KryonParser *parser);
static KryonASTNode *parse_object_literal(KryonParser *parser);

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
        } else if (check_token(parser, KRYON_TOKEN_COMPONENT_DIRECTIVE)) {
            printf("[DEBUG] Parsing component definition\n");
            node = parse_component_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_CONST_DIRECTIVE)) {
            printf("[DEBUG] Parsing const definition\n");
            node = parse_const_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_CONST_FOR_DIRECTIVE)) {
            printf("[DEBUG] Parsing const_for loop\n");
            node = parse_const_for_loop(parser);
        } else if (kryon_token_is_directive(peek(parser)->type)) {
            printf("[DEBUG] Parsing directive\n");
            // Handle specialized directive parsers
            if (check_token(parser, KRYON_TOKEN_EVENT_DIRECTIVE)) {
                node = parse_event_directive(parser);
            } else {
                node = parse_directive(parser);
            }
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            printf("[DEBUG] Parsing element\n");
            node = parse_element(parser);
        } else {
            printf("[DEBUG] Unknown token type %d, error\n", current->type);
            // Don't fail the entire parse for unknown tokens - just skip them
            // parser_error(parser, "Expected element, style definition, theme definition, or directive");
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
        } else if (check_token(parser, KRYON_TOKEN_CONST_FOR_DIRECTIVE)) {
            // @const_for loop - expand inline
            KryonASTNode *const_for = parse_const_for_loop(parser);
            if (const_for && !kryon_ast_add_child(element, const_for)) {
                parser_error(parser, "Failed to add const_for loop");
            }
        } else if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            // Property (potentially comma-separated list)
            do {
                size_t before_property = parser->current_token;
                KryonASTNode *property = parse_property(parser);
                if (property) {
                    if (!kryon_ast_add_property(element, property)) {
                        parser_error(parser, "Failed to add property");
                    }
                } else {
                    // Property parsing failed, skip to next property or element
                    printf("[DEBUG] parse_element: Property parsing failed, skipping to recovery\n");
                    // Skip until we find a recognizable token
                    while (!at_end(parser) && 
                           !check_token(parser, KRYON_TOKEN_IDENTIFIER) &&
                           !check_token(parser, KRYON_TOKEN_ELEMENT_TYPE) &&
                           !check_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
                        advance(parser);
                    }
                    break; // Exit property parsing loop
                }
                
                // Check for comma to continue parsing more properties
                if (check_token(parser, KRYON_TOKEN_COMMA)) {
                    advance(parser); // consume comma
                    // Continue loop to parse next property
                } else {
                    break; // No comma, exit property parsing loop
                }
            } while (check_token(parser, KRYON_TOKEN_IDENTIFIER) && !at_end(parser));
        } else {
            printf("[DEBUG] parse_element: Unexpected token type %d (lexeme: %.*s) at position %zu - skipping\n", 
                   peek(parser)->type, (int)peek(parser)->lexeme_length, 
                   peek(parser)->lexeme, parser->current_token);
            // Skip unexpected tokens but still mark as error for critical failures
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
    return parse_postfix(parser);
}

static KryonASTNode *parse_postfix(KryonParser *parser) {
    KryonASTNode *node = parse_primary(parser);
    if (!node) {
        return NULL;
    }
    
    // Handle postfix operators: member access (.) and array access ([])
    while (true) {
        if (match_token(parser, KRYON_TOKEN_DOT)) {
            // Member access: object.member
            if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected property name after '.'");
                return node;
            }
            
            const KryonToken *member_token = advance(parser);
            KryonASTNode *member_access = kryon_ast_create_node(parser, KRYON_AST_MEMBER_ACCESS,
                                                               &member_token->location);
            if (!member_access) {
                return node;
            }
            
            member_access->data.member_access.object = node;
            member_access->data.member_access.member = kryon_token_copy_lexeme(member_token);
            node->parent = member_access;
            node = member_access;
            
        } else if (match_token(parser, KRYON_TOKEN_LEFT_BRACKET)) {
            // Array access: array[index]
            KryonASTNode *index = parse_expression(parser);
            if (!index) {
                parser_error(parser, "Expected array index expression");
                return node;
            }
            
            if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACKET)) {
                parser_error(parser, "Expected ']' after array index");
                return node;
            }
            
            KryonASTNode *array_access = kryon_ast_create_node(parser, KRYON_AST_ARRAY_ACCESS,
                                                              &previous(parser)->location);
            if (!array_access) {
                return node;
            }
            
            array_access->data.array_access.array = node;
            array_access->data.array_access.index = index;
            node->parent = array_access;
            index->parent = array_access;
            node = array_access;
            
        } else {
            // No more postfix operators
            break;
        }
    }
    
    return node;
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
    
    // Handle array literals [item1, item2, ...]
    if (check_token(parser, KRYON_TOKEN_LEFT_BRACKET)) {
        return parse_array_literal(parser);
    }
    
    // Handle object literals {key: value, ...}
    if (check_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        return parse_object_literal(parser);
    }
    
    // Handle identifiers (for parameter references like 'initialValue')
    if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        const KryonToken *token = advance(parser);
        KryonASTNode *identifier = kryon_ast_create_node(parser, KRYON_AST_IDENTIFIER,
                                                        &token->location);
        if (!identifier) {
            return NULL;
        }
        identifier->data.identifier.name = kryon_token_copy_lexeme(token);
        return identifier;
    }
    
    parser_error(parser, "Expected literal, variable, template expression, or identifier");
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

static KryonASTNode *parse_array_literal(KryonParser *parser) {
    const KryonToken *start_token = advance(parser); // consume '['
    
    KryonASTNode *array = kryon_ast_create_node(parser, KRYON_AST_ARRAY_LITERAL,
                                               &start_token->location);
    if (!array) {
        return NULL;
    }
    
    array->data.array_literal.elements = NULL;
    array->data.array_literal.element_count = 0;
    array->data.array_literal.element_capacity = 0;
    
    // Handle empty array []
    if (check_token(parser, KRYON_TOKEN_RIGHT_BRACKET)) {
        advance(parser); // consume ']'
        return array;
    }
    
    // Parse array elements
    do {
        KryonASTNode *element = parse_expression(parser);
        if (!element) {
            parser_error(parser, "Expected array element");
            break;
        }
        
        // Ensure we have space for the new element
        if (array->data.array_literal.element_count >= array->data.array_literal.element_capacity) {
            size_t new_capacity = array->data.array_literal.element_capacity == 0 ? 4 : 
                                  array->data.array_literal.element_capacity * 2;
            KryonASTNode **new_elements = kryon_realloc(array->data.array_literal.elements, 
                                                       new_capacity * sizeof(KryonASTNode *));
            if (!new_elements) {
                parser_error(parser, "Failed to allocate memory for array elements");
                return array;
            }
            array->data.array_literal.elements = new_elements;
            array->data.array_literal.element_capacity = new_capacity;
        }
        
        array->data.array_literal.elements[array->data.array_literal.element_count++] = element;
        element->parent = array;
        
    } while (match_token(parser, KRYON_TOKEN_COMMA));
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACKET)) {
        parser_error(parser, "Expected ']' after array elements");
    }
    
    return array;
}

static KryonASTNode *parse_object_literal(KryonParser *parser) {
    const KryonToken *start_token = advance(parser); // consume '{'
    
    KryonASTNode *object = kryon_ast_create_node(parser, KRYON_AST_OBJECT_LITERAL,
                                                &start_token->location);
    if (!object) {
        return NULL;
    }
    
    object->data.object_literal.properties = NULL;
    object->data.object_literal.property_count = 0;
    object->data.object_literal.property_capacity = 0;
    
    // Handle empty object {}
    if (check_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        advance(parser); // consume '}'
        return object;
    }
    
    // Parse object properties
    do {
        // Expect property name (identifier or string)
        if (!check_token(parser, KRYON_TOKEN_IDENTIFIER) && !check_token(parser, KRYON_TOKEN_STRING)) {
            parser_error(parser, "Expected property name");
            break;
        }
        
        const KryonToken *key_token = advance(parser);
        
        // Expect colon
        if (!match_token(parser, KRYON_TOKEN_COLON)) {
            parser_error(parser, "Expected ':' after property name");
            break;
        }
        
        // Parse property value
        KryonASTNode *value = parse_expression(parser);
        if (!value) {
            parser_error(parser, "Expected property value");
            break;
        }
        
        // Create property node
        KryonASTNode *property = kryon_ast_create_node(parser, KRYON_AST_PROPERTY,
                                                      &key_token->location);
        if (!property) {
            break;
        }
        
        property->data.property.name = kryon_token_copy_lexeme(key_token);
        property->data.property.value = value;
        value->parent = property;
        
        // Ensure we have space for the new property
        if (object->data.object_literal.property_count >= object->data.object_literal.property_capacity) {
            size_t new_capacity = object->data.object_literal.property_capacity == 0 ? 4 : 
                                  object->data.object_literal.property_capacity * 2;
            KryonASTNode **new_properties = kryon_realloc(object->data.object_literal.properties, 
                                                         new_capacity * sizeof(KryonASTNode *));
            if (!new_properties) {
                parser_error(parser, "Failed to allocate memory for object properties");
                return object;
            }
            object->data.object_literal.properties = new_properties;
            object->data.object_literal.property_capacity = new_capacity;
        }
        
        object->data.object_literal.properties[object->data.object_literal.property_count++] = property;
        property->parent = object;
        
    } while (match_token(parser, KRYON_TOKEN_COMMA));
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after object properties");
    }
    
    return object;
}

static KryonASTNode *parse_event_directive(KryonParser *parser) {
    const KryonToken *token = advance(parser); // consume @event
    
    KryonASTNode *directive = kryon_ast_create_node(parser, KRYON_AST_EVENT_DIRECTIVE, &token->location);
    if (!directive) {
        parser_error(parser, "Failed to create event directive AST node");
        return NULL;
    }
    
    // Parse event type string (e.g., "keyboard", "mouse")
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        parser_error(parser, "Expected event type string after @event");
        return directive;
    }
    
    const KryonToken *event_type_token = advance(parser);
    
    // Store the event type as the element_type field (reusing existing structure)
    directive->data.element.element_type = kryon_alloc(strlen(event_type_token->value.string_value) + 1);
    if (directive->data.element.element_type) {
        strcpy(directive->data.element.element_type, event_type_token->value.string_value);
    }
    
    // Parse event bindings in braces
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' after event type");
        return directive;
    }
    
    // Parse event bindings (key: value pairs where key is event pattern, value is function call)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        // Parse event binding: "pattern": functionCall()
        if (!check_token(parser, KRYON_TOKEN_STRING)) {
            printf("[DEBUG] Event binding parser: Expected STRING, got token type %d\n", peek(parser)->type);
            parser_error(parser, "Expected event pattern string");
            break;
        }
        
        const KryonToken *pattern_token = advance(parser);
        
        if (!match_token(parser, KRYON_TOKEN_COLON)) {
            parser_error(parser, "Expected ':' after event pattern");
            break;
        }
        
        // Parse function call: identifier()
        if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected function name after ':'");
            break;
        }
        
        const KryonToken *function_token = advance(parser);
        
        if (!match_token(parser, KRYON_TOKEN_LEFT_PAREN)) {
            parser_error(parser, "Expected '(' after function name");
            break;
        }
        
        if (!match_token(parser, KRYON_TOKEN_RIGHT_PAREN)) {
            parser_error(parser, "Expected ')' after function parameters");
            break;
        }
        
        // Create a function call expression node
        KryonASTNode *expression = kryon_ast_create_node(parser, KRYON_AST_FUNCTION_CALL, &function_token->location);
        if (!expression) {
            parser_error(parser, "Failed to create function call node");
            break;
        }
        
        // Store function name
        expression->data.function_call.function_name = kryon_token_copy_lexeme(function_token);
        expression->data.function_call.argument_count = 0;  // No parameters for now
        expression->data.function_call.arguments = NULL;
        
        // Create a property node for this event binding
        KryonASTNode *binding = kryon_ast_create_node(parser, KRYON_AST_PROPERTY, &pattern_token->location);
        if (binding) {
            // Use the string content as property name (without quotes)
            binding->data.property.name = kryon_alloc(strlen(pattern_token->value.string_value) + 1);
            if (binding->data.property.name) {
                strcpy(binding->data.property.name, pattern_token->value.string_value);
            }
            binding->data.property.value = expression;
            expression->parent = binding;
            
            kryon_ast_add_property(directive, binding);
            binding->parent = directive;
        }
    }
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' to close event directive");
    }
    
    return directive;
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
    
    // Check if this is @variables (block syntax) or @var (single variable)
    bool is_variables_block = (directive_token->type == KRYON_TOKEN_VARIABLES_DIRECTIVE);
    
    if (is_variables_block) {
        // Handle @variables { key: value, key2: value2 } syntax
        if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
            parser_error(parser, "Expected '{' after @variables");
            return NULL;
        }
        
        KryonASTNode *variables_block = kryon_ast_create_node(parser, KRYON_AST_VARIABLE_DEFINITION,
                                                             &directive_token->location);
        if (!variables_block) {
            return NULL;
        }
        
        // Initialize as container for multiple variables
        variables_block->data.variable_def.name = kryon_strdup("__variables_block__");
        variables_block->data.variable_def.type = NULL;
        variables_block->data.variable_def.value = NULL;
        variables_block->data.element.children = NULL;
        variables_block->data.element.child_count = 0;
        variables_block->data.element.child_capacity = 0;
        
        // Parse variable definitions inside the block
        while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !check_token(parser, KRYON_TOKEN_EOF)) {
            if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected variable name");
                break;
            }
            
            const KryonToken *name_token = advance(parser);
            
            if (!match_token(parser, KRYON_TOKEN_COLON)) {
                parser_error(parser, "Expected ':' after variable name");
                break;
            }
            
            // Create individual variable node
            KryonASTNode *var_node = kryon_ast_create_node(parser, KRYON_AST_VARIABLE_DEFINITION,
                                                          &name_token->location);
            if (var_node) {
                var_node->data.variable_def.name = kryon_token_copy_lexeme(name_token);
                var_node->data.variable_def.type = NULL;
                var_node->data.variable_def.value = parse_expression(parser);
                
                // Add to variables block
                if (!kryon_ast_add_child(variables_block, var_node)) {
                    kryon_free(var_node);
                }
            }
            
            // Optional comma
            if (check_token(parser, KRYON_TOKEN_COMMA)) {
                advance(parser);
            }
        }
        
        if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
            parser_error(parser, "Expected '}' after variables block");
        }
        
        return variables_block;
    } else {
        // Handle @var key: value syntax
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
        
        // Expect colon assignment
        if (!match_token(parser, KRYON_TOKEN_COLON)) {
            parser_error(parser, "Expected ':' after variable name");
            return var_def;
        }
        
        // Parse variable value
        var_def->data.variable_def.value = parse_expression(parser);
        if (!var_def->data.variable_def.value) {
            parser_error(parser, "Expected variable value");
        }
        
        return var_def;
    }
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
    
    // Parse function body - now expects SCRIPT_CONTENT token from lexer
    if (!check_token(parser, KRYON_TOKEN_SCRIPT_CONTENT)) {
        parser_error(parser, "Expected script content for function body");
        return func_def;
    }
    
    const KryonToken *script_token = advance(parser);
    const char *code = script_token->value.string_value;
    
    if (!code) {
        parser_error(parser, "Function body content is null");
        return func_def;
    }
    
    // Allocate and copy the script content
    char *code_buffer = kryon_alloc(strlen(code) + 1);
    if (!code_buffer) {
        parser_error(parser, "Failed to allocate memory for function code");
        return func_def;
    }
    
    strcpy(code_buffer, code);
    func_def->data.function_def.code = code_buffer;
    
    return func_def;
}

static KryonASTNode *parse_component_definition(KryonParser *parser) {
    printf("[DEBUG] parse_component_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume @component
    
    if (!check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
        parser_error(parser, "Expected component name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *component = kryon_ast_create_node(parser, KRYON_AST_COMPONENT,
                                                    &directive_token->location);
    if (!component) {
        return NULL;
    }
    
    component->data.component.name = kryon_token_copy_lexeme(name_token);
    component->data.component.parameters = NULL;
    component->data.component.param_defaults = NULL;
    component->data.component.parameter_count = 0;
    component->data.component.state_vars = NULL;
    component->data.component.state_count = 0;
    component->data.component.functions = NULL;
    component->data.component.function_count = 0;
    component->data.component.body = NULL;
    
    // Parse parameter list if present
    if (match_token(parser, KRYON_TOKEN_LEFT_PAREN)) {
        while (!check_token(parser, KRYON_TOKEN_RIGHT_PAREN) && !at_end(parser)) {
            if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected parameter name");
                break;
            }
            
            const KryonToken *param_token = advance(parser);
            
            // Expand parameters arrays
            size_t new_count = component->data.component.parameter_count + 1;
            char **new_params = realloc(component->data.component.parameters,
                                       new_count * sizeof(char*));
            char **new_defaults = realloc(component->data.component.param_defaults,
                                         new_count * sizeof(char*));
            if (!new_params || !new_defaults) {
                parser_error(parser, "Failed to allocate component parameters");
                break;
            }
            
            component->data.component.parameters = new_params;
            component->data.component.param_defaults = new_defaults;
            component->data.component.parameters[component->data.component.parameter_count] = 
                kryon_token_copy_lexeme(param_token);
            
            // Check for default value
            if (match_token(parser, KRYON_TOKEN_COLON)) {
                // Parse default value
                const KryonToken *default_token = peek(parser);
                if (default_token) {
                    advance(parser);
                    component->data.component.param_defaults[component->data.component.parameter_count] = 
                        kryon_token_copy_lexeme(default_token);
                } else {
                    component->data.component.param_defaults[component->data.component.parameter_count] = NULL;
                }
            } else {
                component->data.component.param_defaults[component->data.component.parameter_count] = NULL;
            }
            
            component->data.component.parameter_count = new_count;
            
            if (check_token(parser, KRYON_TOKEN_COMMA)) {
                advance(parser); // consume comma
            }
        }
        
        if (!match_token(parser, KRYON_TOKEN_RIGHT_PAREN)) {
            parser_error(parser, "Expected ')' after component parameters");
        }
    }
    
    // Parse component body
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' before component body");
        return component;
    }
    
    printf("[DEBUG] parse_component_definition: Starting component body parsing at token %zu\n", parser->current_token);
    
    // Parse component contents (state variables, functions, and UI)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        printf("[DEBUG] parse_component_definition: Processing token %zu (type %d) in component body\n", 
               parser->current_token, peek(parser)->type);
        if (check_token(parser, KRYON_TOKEN_STATE_DIRECTIVE)) {
            // Parse @state variable
            KryonASTNode *state_var = parse_state_definition(parser);
            if (state_var) {
                // Add to state vars array
                size_t new_count = component->data.component.state_count + 1;
                KryonASTNode **new_states = realloc(component->data.component.state_vars,
                                                   new_count * sizeof(KryonASTNode*));
                if (new_states) {
                    component->data.component.state_vars = new_states;
                    component->data.component.state_vars[component->data.component.state_count] = state_var;
                    component->data.component.state_count = new_count;
                }
            }
        } else if (check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE)) {
            // Parse component function
            KryonASTNode *func = parse_function_definition(parser);
            if (func) {
                // Add to functions array
                size_t new_count = component->data.component.function_count + 1;
                KryonASTNode **new_funcs = realloc(component->data.component.functions,
                                                  new_count * sizeof(KryonASTNode*));
                if (new_funcs) {
                    component->data.component.functions = new_funcs;
                    component->data.component.functions[component->data.component.function_count] = func;
                    component->data.component.function_count = new_count;
                }
            }
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            // Parse the UI body (single root element)
            if (!component->data.component.body) {
                printf("[DEBUG] parse_component_definition: Parsing component UI body element at token %zu\n", parser->current_token);
                component->data.component.body = parse_element(parser);
                printf("[DEBUG] parse_component_definition: Finished parsing UI body, now at token %zu\n", parser->current_token);
            } else {
                parser_error(parser, "Component can only have one root UI element");
                advance(parser);
            }
        } else {
            printf("[DEBUG] parse_component_definition: Unexpected token type %d in component body\n", peek(parser)->type);
            parser_error(parser, "Unexpected token in component body");
            advance(parser);
        }
    }
    
    printf("[DEBUG] parse_component_definition: Finished component body parsing at token %zu\n", parser->current_token);
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after component body");
    }
    
    printf("[DEBUG] parse_component_definition: Component parsing complete at token %zu\n", parser->current_token);
    printf("[DEBUG] parse_component_definition: Created component '%s' with %zu parameters, %zu state vars, %zu functions\n",
           component->data.component.name, 
           component->data.component.parameter_count,
           component->data.component.state_count,
           component->data.component.function_count);
    
    return component;
}

static KryonASTNode *parse_state_definition(KryonParser *parser) {
    printf("[DEBUG] parse_state_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume @state
    
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected state variable name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *state_def = kryon_ast_create_node(parser, KRYON_AST_STATE_DEFINITION,
                                                    &directive_token->location);
    if (!state_def) {
        return NULL;
    }
    
    state_def->data.variable_def.name = kryon_token_copy_lexeme(name_token);
    state_def->data.variable_def.type = NULL;
    state_def->data.variable_def.value = NULL;
    
    // Expect colon assignment
    if (!match_token(parser, KRYON_TOKEN_COLON)) {
        parser_error(parser, "Expected ':' after state variable name");
        return state_def;
    }
    
    // Parse state variable value (could be literal or reference to parameter)
    state_def->data.variable_def.value = parse_expression(parser);
    if (!state_def->data.variable_def.value) {
        parser_error(parser, "Expected state variable value");
    }
    
    printf("[DEBUG] parse_state_definition: Created state variable '%s'\n", 
           state_def->data.variable_def.name);
    
    return state_def;
}

static KryonASTNode *parse_const_definition(KryonParser *parser) {
    printf("[DEBUG] parse_const_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume @const
    
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected constant name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *const_def = kryon_ast_create_node(parser, KRYON_AST_CONST_DEFINITION,
                                                    &directive_token->location);
    if (!const_def) {
        return NULL;
    }
    
    const_def->data.const_def.name = kryon_token_copy_lexeme(name_token);
    const_def->data.const_def.value = NULL;
    
    // Expect colon assignment
    if (!match_token(parser, KRYON_TOKEN_COLON)) {
        parser_error(parser, "Expected ':' after constant name");
        return const_def;
    }
    
    // Parse constant value (array literal, object literal, or simple literal)
    const_def->data.const_def.value = parse_expression(parser);
    if (!const_def->data.const_def.value) {
        parser_error(parser, "Expected constant value");
    }
    
    printf("[DEBUG] parse_const_definition: Created constant '%s'\n", 
           const_def->data.const_def.name);
    
    return const_def;
}

static KryonASTNode *parse_const_for_loop(KryonParser *parser) {
    printf("[DEBUG] parse_const_for_loop: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume @const_for
    
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected loop variable name");
        return NULL;
    }
    
    const KryonToken *var_token = advance(parser);
    
    // Expect 'in' keyword
    if (!match_token(parser, KRYON_TOKEN_IN_KEYWORD)) {
        parser_error(parser, "Expected 'in' after loop variable");
        return NULL;
    }
    
    KryonASTNode *const_for = kryon_ast_create_node(parser, KRYON_AST_CONST_FOR_LOOP,
                                                    &directive_token->location);
    if (!const_for) {
        return NULL;
    }
    
    const_for->data.const_for_loop.var_name = kryon_token_copy_lexeme(var_token);
    const_for->data.const_for_loop.array_name = NULL;
    const_for->data.const_for_loop.is_range = false;
    const_for->data.const_for_loop.range_start = 0;
    const_for->data.const_for_loop.range_end = 0;
    const_for->data.const_for_loop.body = NULL;
    const_for->data.const_for_loop.body_count = 0;
    const_for->data.const_for_loop.body_capacity = 0;
    
    // Parse either array name or range expression
    if (check_token(parser, KRYON_TOKEN_INTEGER) || check_token(parser, KRYON_TOKEN_FLOAT)) {
        // Range expression: start..end
        const KryonToken *start_token = advance(parser);
        
        if (!match_token(parser, KRYON_TOKEN_RANGE)) {
            parser_error(parser, "Expected '..' after range start value");
            return const_for;
        }
        
        if (!(check_token(parser, KRYON_TOKEN_INTEGER) || check_token(parser, KRYON_TOKEN_FLOAT))) {
            parser_error(parser, "Expected end value after '..'");
            return const_for;
        }
        
        const KryonToken *end_token = advance(parser);
        
        // Parse the range values
        const_for->data.const_for_loop.is_range = true;
        const_for->data.const_for_loop.range_start = (int)strtol(start_token->lexeme, NULL, 10);
        const_for->data.const_for_loop.range_end = (int)strtol(end_token->lexeme, NULL, 10);
        
    } else if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        // Array name
        const KryonToken *array_token = advance(parser);
        const_for->data.const_for_loop.is_range = false;
        const_for->data.const_for_loop.array_name = kryon_token_copy_lexeme(array_token);
        
    } else {
        parser_error(parser, "Expected array name or range expression after 'in'");
        return const_for;
    }
    
    // Expect opening brace
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' before loop body");
        return const_for;
    }
    
    // Parse loop body (elements)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        KryonASTNode *body_element = NULL;
        
        if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            body_element = parse_element(parser);
        } else {
            parser_error(parser, "Expected element in loop body");
            synchronize(parser);
            continue;
        }
        
        if (body_element) {
            // Ensure we have space for the new element
            if (const_for->data.const_for_loop.body_count >= const_for->data.const_for_loop.body_capacity) {
                size_t new_capacity = const_for->data.const_for_loop.body_capacity == 0 ? 4 : 
                                      const_for->data.const_for_loop.body_capacity * 2;
                KryonASTNode **new_body = kryon_realloc(const_for->data.const_for_loop.body, 
                                                        new_capacity * sizeof(KryonASTNode *));
                if (!new_body) {
                    parser_error(parser, "Failed to allocate memory for loop body");
                    return const_for;
                }
                const_for->data.const_for_loop.body = new_body;
                const_for->data.const_for_loop.body_capacity = new_capacity;
            }
            
            const_for->data.const_for_loop.body[const_for->data.const_for_loop.body_count++] = body_element;
            body_element->parent = const_for;
        }
    }
    
    // Expect closing brace
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after loop body");
    }
    
    if (const_for->data.const_for_loop.is_range) {
        printf("[DEBUG] parse_const_for_loop: Created range loop '%s' in %d..%d with %zu body elements\n",
               const_for->data.const_for_loop.var_name,
               const_for->data.const_for_loop.range_start,
               const_for->data.const_for_loop.range_end,
               const_for->data.const_for_loop.body_count);
    } else {
        printf("[DEBUG] parse_const_for_loop: Created array loop '%s' in '%s' with %zu body elements\n",
               const_for->data.const_for_loop.var_name,
               const_for->data.const_for_loop.array_name,
               const_for->data.const_for_loop.body_count);
    }
    
    return const_for;
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
            case KRYON_AST_CONST_DEFINITION:
            case KRYON_AST_CONST_FOR_LOOP:
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
        case KRYON_AST_CONST_DEFINITION: return "ConstDefinition";
        case KRYON_AST_CONST_FOR_LOOP: return "ConstForLoop";
        case KRYON_AST_LITERAL: return "Literal";
        case KRYON_AST_VARIABLE: return "Variable";
        case KRYON_AST_TEMPLATE: return "Template";
        case KRYON_AST_ARRAY_LITERAL: return "ArrayLiteral";
        case KRYON_AST_OBJECT_LITERAL: return "ObjectLiteral";
        case KRYON_AST_MEMBER_ACCESS: return "MemberAccess";
        case KRYON_AST_ARRAY_ACCESS: return "ArrayAccess";
        case KRYON_AST_ERROR: return "Error";
        default: return "Unknown";
    }
}