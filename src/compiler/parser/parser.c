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
#include "lib9.h"


#include "parser.h"
#include "memory.h"
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
static bool parse_component_prop_declaration(KryonParser *parser, KryonASTNode *component);
static KryonASTNode *parse_state_definition(KryonParser *parser);
static KryonASTNode *parse_const_definition(KryonParser *parser);
static KryonASTNode *parse_for_directive(KryonParser *parser);
static KryonASTNode *parse_if_directive(KryonParser *parser);
static KryonASTNode *parse_event_directive(KryonParser *parser);
static KryonASTNode *parse_onload_directive(KryonParser *parser);
static KryonASTNode *parse_directive(KryonParser *parser);
static KryonASTNode *parse_property(KryonParser *parser);
static KryonASTNode *parse_expression(KryonParser *parser);
static KryonASTNode *parse_postfix(KryonParser *parser);
static KryonASTNode *parse_primary(KryonParser *parser);
static KryonASTNode *parse_literal(KryonParser *parser);
static KryonASTNode *parse_variable(KryonParser *parser);
static KryonASTNode *parse_template(KryonParser *parser);
static KryonASTNode *parse_template_string(KryonParser *parser, const char* template_str);
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
// LANGUAGE VALIDATION
// =============================================================================

/**
 * @brief Check if a language identifier is supported
 * @param lang Language identifier string
 * @return true if supported, false otherwise
 */
static bool is_supported_language(const char *lang) {
    if (lang == NULL) {
        return false;
    }

    // Supported languages
    const char *supported[] = {
        "",      // Native Kryon (default)
        "rc",    // rc shell (Plan 9/Inferno)
        "js",    // JavaScript (reserved)
        "lua",   // Lua (reserved)
        NULL
    };

    for (int i = 0; supported[i] != NULL; i++) {
        if (strcmp(lang, supported[i]) == 0) {
            return true;
        }
    }

    return false;
}

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
    fprintf(stderr, "[DEBUG] Lexer produced %zu tokens\n", token_count);
    if (!tokens || token_count == 0) {
        fprintf(stderr, "[DEBUG] No tokens produced by lexer\n");
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
                    case KRYON_AST_ONLOAD_DIRECTIVE:
                    case KRYON_AST_FOR_DIRECTIVE:
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
    fprintf(stderr, "[DEBUG] Starting parse_document\n");
    
    KryonASTNode *root = kryon_ast_create_node(parser, KRYON_AST_ROOT, 
                                              &parser->tokens[0].location);
    if (!root) {
        fprintf(stderr, "[DEBUG] Failed to create root node\n");
        return NULL;
    }
    fprintf(stderr, "[DEBUG] Created root node\n");
    
    // Parse top-level declarations  
    fprintf(stderr, "[DEBUG] Starting parsing loop, current_token=%zu, token_count=%zu\n", parser->current_token, parser->token_count);
    
    // Debug: Show first 10 tokens
    fprintf(stderr, "[DEBUG] First 10 tokens:\n");
    for (size_t i = 0; i < (parser->token_count < 10 ? parser->token_count : 10); i++) {
        const KryonToken* tok = &parser->tokens[i];
        fprintf(stderr, "  [%zu] type=%d, lexeme='%.*s'\n", i, tok->type, (int)tok->lexeme_length, tok->lexeme ? tok->lexeme : "NULL");
    }
    
    while (!at_end(parser)) {
        KryonASTNode *node = NULL;
        const KryonToken *current = peek(parser);
        fprintf(stderr, "[DEBUG] Processing token type %d at index %zu\n", current->type, parser->current_token);

        if (check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) ||
            check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing style block\n");
            node = parse_style_block(parser);
        } else if (check_token(parser, KRYON_TOKEN_THEME_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing theme definition\n");
            node = parse_theme_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_VARIABLE_DIRECTIVE) ||
                   check_token(parser, KRYON_TOKEN_VARIABLES_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing variable definition\n");
            node = parse_variable_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing function definition\n");
            node = parse_function_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_COMPONENT_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing component definition\n");
            node = parse_component_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_CONST_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing const definition\n");
            node = parse_const_definition(parser);
        } else if (check_token(parser, KRYON_TOKEN_ONLOAD_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing onload directive\n");
            node = parse_onload_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing for directive\n");
            node = parse_for_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
            fprintf(stderr, "[DEBUG] Parsing if directive\n");
            node = parse_if_directive(parser);
        } else if (kryon_token_is_directive(peek(parser)->type)) {
            fprintf(stderr, "[DEBUG] Parsing directive\n");
            // Handle specialized directive parsers
            if (check_token(parser, KRYON_TOKEN_EVENT_DIRECTIVE)) {
                node = parse_event_directive(parser);
            } else {
                node = parse_directive(parser);
            }
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            fprintf(stderr, "[DEBUG] Parsing element\n");
            node = parse_element(parser);
        } else {
            fprintf(stderr, "[DEBUG] Unknown token type %d, error\n", current->type);
            // Don't fail the entire parse for unknown tokens - just skip them
            // parser_error(parser, "Expected element, style definition, theme definition, or directive");
            advance(parser); // Skip invalid token
            continue;
        }
        
        if (node) {
            fprintf(stderr, "[DEBUG] Successfully parsed node of type %s\n", kryon_ast_node_type_name(node->type));
            if (!kryon_ast_add_child(root, node)) {
                fprintf(stderr, "[DEBUG] Failed to add node to AST\n");
                parser_error(parser, "Failed to add node to AST");
            } else {
                fprintf(stderr, "[DEBUG] Successfully added node to root (children: %zu)\n", root->data.element.child_count);
            }
        } else {
            fprintf(stderr, "[DEBUG] Failed to parse node\n");
        }
    }
    
    fprintf(stderr, "[DEBUG] Finished parsing document, root has %zu children\n", root->data.element.child_count);
    return root;
}

static KryonASTNode *parse_element(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_element: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
        fprintf(stderr, "[DEBUG] parse_element: Not an element type token\n");
        parser_error(parser, "Expected element type");
        return NULL;
    }
    
    // Check nesting depth
    if (parser->nesting_depth >= parser->config.max_nesting_depth) {
        fprintf(stderr, "[DEBUG] parse_element: Maximum nesting depth exceeded\n");
        parser_error(parser, "Maximum nesting depth exceeded");
        return NULL;
    }
    
    const KryonToken *type_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_element: Creating element of type %.*s\n", 
           (int)type_token->lexeme_length, type_token->lexeme);
    
    KryonASTNode *element = kryon_ast_create_node(parser, KRYON_AST_ELEMENT,
                                                 &type_token->location);
    if (!element) {
        fprintf(stderr, "[DEBUG] parse_element: Failed to create AST node\n");
        return NULL;
    }
    fprintf(stderr, "[DEBUG] parse_element: Created AST node\n");
    
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
        } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
            // @for directive - parse as specialized for directive and add as child
            KryonASTNode *for_directive = parse_for_directive(parser);
            if (for_directive && !kryon_ast_add_child(element, for_directive)) {
                parser_error(parser, "Failed to add for directive");
            }
        } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
            // @if directive - parse as conditional directive and add as child
            KryonASTNode *if_directive = parse_if_directive(parser);
            if (if_directive && !kryon_ast_add_child(element, if_directive)) {
                parser_error(parser, "Failed to add if directive");
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
                    fprintf(stderr, "[DEBUG] parse_element: Property parsing failed, skipping to recovery\n");
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
            fprintf(stderr, "[DEBUG] parse_element: Unexpected token type %d (lexeme: %.*s) at position %zu - skipping\n", 
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
    fprintf(stderr, "[DEBUG] parse_style_block: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) &&
        !check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
        fprintf(stderr, "[DEBUG] parse_style_block: Not a style directive\n");
        parser_error(parser, "Expected 'style' or 'styles' directive");
        return NULL;
    }
    
    const KryonToken *directive_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_style_block: Found style directive\n");
    
    // Expect style name
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        fprintf(stderr, "[DEBUG] parse_style_block: Expected string after style\n");
        parser_error(parser, "Expected style name string");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_style_block: Style name: %s\n", name_token->value.string_value);
    
    KryonASTNode *style = kryon_ast_create_node(parser, KRYON_AST_STYLE_BLOCK,
                                               &directive_token->location);
    if (!style) {
        fprintf(stderr, "[DEBUG] parse_style_block: Failed to create style node\n");
        return NULL;
    }
    fprintf(stderr, "[DEBUG] parse_style_block: Created style node\n");
    
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
    
    if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
        parser_error(parser, "Expected '=' after property name");
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
    return parse_ternary(parser);
}

// Ternary conditional: expr ? expr : expr
static KryonASTNode *parse_ternary(KryonParser *parser) {
    KryonASTNode *condition = parse_logical_or(parser);
    if (!condition) return NULL;
    
    if (match_token(parser, KRYON_TOKEN_QUESTION)) {
        KryonASTNode *true_expr = parse_expression(parser);
        if (!true_expr) {
            parser_error(parser, "Expected expression after '?'");
            return condition;
        }
        
        if (!match_token(parser, KRYON_TOKEN_COLON)) {
            parser_error(parser, "Expected ':' in ternary expression");
            return condition;
        }
        
        KryonASTNode *false_expr = parse_expression(parser);
        if (!false_expr) {
            parser_error(parser, "Expected expression after ':'");
            return condition;
        }
        
        KryonASTNode *ternary = kryon_ast_create_node(parser, KRYON_AST_TERNARY_OP, 
                                                     &previous(parser)->location);
        if (!ternary) return condition;
        
        ternary->data.ternary_op.condition = condition;
        ternary->data.ternary_op.true_expr = true_expr;
        ternary->data.ternary_op.false_expr = false_expr;
        
        condition->parent = ternary;
        true_expr->parent = ternary;
        false_expr->parent = ternary;
        
        return ternary;
    }
    
    return condition;
}

// Logical OR: expr || expr
static KryonASTNode *parse_logical_or(KryonParser *parser) {
    KryonASTNode *left = parse_logical_and(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_LOGICAL_OR)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_logical_and(parser);
        if (!right) {
            parser_error(parser, "Expected expression after '||'");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP, 
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Logical AND: expr && expr
static KryonASTNode *parse_logical_and(KryonParser *parser) {
    KryonASTNode *left = parse_equality(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_LOGICAL_AND)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_equality(parser);
        if (!right) {
            parser_error(parser, "Expected expression after '&&'");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP,
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Equality: expr == expr, expr != expr
static KryonASTNode *parse_equality(KryonParser *parser) {
    KryonASTNode *left = parse_comparison(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_EQUALS) || match_token(parser, KRYON_TOKEN_NOT_EQUALS)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_comparison(parser);
        if (!right) {
            parser_error(parser, "Expected expression after equality operator");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP,
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Comparison: expr < expr, expr <= expr, expr > expr, expr >= expr  
static KryonASTNode *parse_comparison(KryonParser *parser) {
    KryonASTNode *left = parse_additive(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_LESS_THAN) || 
           match_token(parser, KRYON_TOKEN_LESS_EQUAL) ||
           match_token(parser, KRYON_TOKEN_GREATER_THAN) ||
           match_token(parser, KRYON_TOKEN_GREATER_EQUAL)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_additive(parser);
        if (!right) {
            parser_error(parser, "Expected expression after comparison operator");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP,
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Addition and subtraction: expr + expr, expr - expr
static KryonASTNode *parse_additive(KryonParser *parser) {
    KryonASTNode *left = parse_multiplicative(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_PLUS) || match_token(parser, KRYON_TOKEN_MINUS)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_multiplicative(parser);
        if (!right) {
            parser_error(parser, "Expected expression after '+' or '-'");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP,
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Multiplication, division, and modulo: expr * expr, expr / expr, expr % expr
static KryonASTNode *parse_multiplicative(KryonParser *parser) {
    KryonASTNode *left = parse_unary(parser);
    if (!left) return NULL;
    
    while (match_token(parser, KRYON_TOKEN_MULTIPLY) || 
           match_token(parser, KRYON_TOKEN_DIVIDE) ||
           match_token(parser, KRYON_TOKEN_MODULO)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *right = parse_unary(parser);
        if (!right) {
            parser_error(parser, "Expected expression after '*', '/', or '%'");
            return left;
        }
        
        KryonASTNode *binary = kryon_ast_create_node(parser, KRYON_AST_BINARY_OP,
                                                    &operator->location);
        if (!binary) return left;
        
        binary->data.binary_op.left = left;
        binary->data.binary_op.right = right;
        binary->data.binary_op.operator = operator->type;
        
        left->parent = binary;
        right->parent = binary;
        left = binary;
    }
    
    return left;
}

// Unary expressions: -expr, +expr, !expr
static KryonASTNode *parse_unary(KryonParser *parser) {
    if (match_token(parser, KRYON_TOKEN_MINUS) || 
        match_token(parser, KRYON_TOKEN_PLUS) ||
        match_token(parser, KRYON_TOKEN_LOGICAL_NOT)) {
        const KryonToken *operator = previous(parser);
        KryonASTNode *operand = parse_unary(parser);
        if (!operand) {
            parser_error(parser, "Expected expression after unary operator");
            return NULL;
        }
        
        KryonASTNode *unary = kryon_ast_create_node(parser, KRYON_AST_UNARY_OP,
                                                   &operator->location);
        if (!unary) return operand;
        
        unary->data.unary_op.operand = operand;
        unary->data.unary_op.operator = operator->type;
        operand->parent = unary;
        
        return unary;
    }
    
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

    // Handle parenthesized expressions (expression)
    if (check_token(parser, KRYON_TOKEN_LEFT_PAREN)) {
        advance(parser); // consume '('
        KryonASTNode *expr = parse_expression(parser);
        if (!expr) {
            parser_error(parser, "Expected expression after '('");
            return NULL;
        }
        if (!match_token(parser, KRYON_TOKEN_RIGHT_PAREN)) {
            parser_error(parser, "Expected ')' after expression");
            return NULL;
        }
        return expr;
    }

    // Handle array literals [item1, item2, ...]
    if (check_token(parser, KRYON_TOKEN_LEFT_BRACKET)) {
        return parse_array_literal(parser);
    }

    // Handle object literals {key: value, ...}
    if (check_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        return parse_object_literal(parser);
    }
    
    // Handle component instances (element types as property values)
    if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
        // Check if this is a component instance (element type followed by braces)
        if (parser->current_token + 1 < parser->token_count && 
            parser->tokens[parser->current_token + 1].type == KRYON_TOKEN_LEFT_BRACE) {
            // This is a component instance, parse as an element
            return parse_element(parser);
        }
        // If it's just an element type without braces, treat as identifier
        const KryonToken *token = advance(parser);
        KryonASTNode *identifier = kryon_ast_create_node(parser, KRYON_AST_IDENTIFIER,
                                                        &token->location);
        if (!identifier) {
            return NULL;
        }
        identifier->data.identifier.name = kryon_token_copy_lexeme(token);
        return identifier;
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
    
    parser_error(parser, "Expected literal, variable, template expression, identifier, or component instance");
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
        case KRYON_TOKEN_STRING: {
            const char* str_value = token->value.string_value;
            
            // Check if string contains template variables like ${variable}
            if (str_value && strstr(str_value, "${") != NULL) {
                fprintf(stderr, "🔄 Detected template variables in string: '%s' - parsing into segments\n", str_value);
                fprintf(stderr, "🔍 DEBUG: About to call parse_template_string with parser=%p\n", (void*)parser);
                
                // Parse template string into segments and replace the literal node
                KryonASTNode *template_node = parse_template_string(parser, str_value);
                if (template_node) {
                    // Replace literal with template node
                    literal->type = KRYON_AST_TEMPLATE;
                    literal->data.template = template_node->data.template;
                    kryon_free(template_node); // Free the wrapper node, keep the data
                } else {
                    // Fallback to regular string if parsing fails
                    literal->data.literal.value = kryon_ast_value_string(str_value);
                }
            } else {
                // Regular string literal without template variables
                literal->data.literal.value = kryon_ast_value_string(str_value);
            }
            break;
        }
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


// THIS IS THE NEW, CORRECTED FUNCTION
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
    
    // Directly allocate a new string for the variable name, skipping the '$'.
    const char* start = token->lexeme;
    size_t length = token->lexeme_length;

    if (length > 0 && *start == '$') {
        start++;    // Move the start pointer past the '$'
        length--;   // Decrease the length by 1
    }

    // Allocate memory for the name + a null terminator.
    char* name_copy = malloc(length + 1);
    if (!name_copy) {
        // Handle allocation failure, cleanup node, etc.
        // For simplicity, we'll just set name to NULL.
        variable->data.variable.name = NULL; 
    } else {
        memcpy(name_copy, start, length);
        name_copy[length] = '\0';
        variable->data.variable.name = name_copy;
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
    
    // Initialize template segments for direct ${...} syntax
    template_node->data.template.segments = NULL;
    template_node->data.template.segment_count = 0;
    template_node->data.template.segment_capacity = 0;
    
    // Parse the expression inside ${...}
    KryonASTNode *expression = parse_expression(parser);
    if (expression) {
        // For direct ${variable} syntax, create a single variable segment
        if (expression->type == KRYON_AST_VARIABLE) {
            // Allocate segments array
            template_node->data.template.segments = kryon_alloc(sizeof(KryonASTNode*));
            template_node->data.template.segment_capacity = 1;
            template_node->data.template.segment_count = 1;
            template_node->data.template.segments[0] = expression;
        } else {
            // For complex expressions, we'd need more sophisticated handling
            // For now, treat as error
            parser_error(parser, "Only simple variable references supported in ${...} syntax");
        }
    }
    
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
        if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
            parser_error(parser, "Expected '=' after property name");
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
    const KryonToken *token = advance(parser); // consume event directive
    
    KryonASTNode *directive = kryon_ast_create_node(parser, KRYON_AST_EVENT_DIRECTIVE, &token->location);
    if (!directive) {
        parser_error(parser, "Failed to create event directive AST node");
        return NULL;
    }
    
    // Parse event type string (e.g., "keyboard", "mouse")
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        parser_error(parser, "Expected event type string after event");
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
            fprintf(stderr, "[DEBUG] Event binding parser: Expected STRING, got token type %d\n", peek(parser)->type);
            parser_error(parser, "Expected event pattern string");
            break;
        }
        
        const KryonToken *pattern_token = advance(parser);
        
        if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
            parser_error(parser, "Expected '=' after event pattern");
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

static KryonASTNode *parse_onload_directive(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_onload_directive: Starting\n");

    if (!check_token(parser, KRYON_TOKEN_ONLOAD_DIRECTIVE) &&
        !check_token(parser, KRYON_TOKEN_ON_MOUNT_DIRECTIVE) &&
        !check_token(parser, KRYON_TOKEN_ON_CREATE_DIRECTIVE)) {
        fprintf(stderr, "[DEBUG] parse_onload_directive: Not a lifecycle directive\n");
        parser_error(parser, "Expected 'onload', 'mount', or 'oncreate' directive");
        return NULL;
    }

    const KryonToken *onload_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_onload_directive: Found lifecycle directive\n");
    
    // REQUIRED language specification for lifecycle directives
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        parser_error(parser,
            "Language specification required for lifecycle directive. "
            "Use @on_mount \"rc\" { ... } for rc shell or "
            "@on_mount \"\" { ... } for native Kryon.");
        return NULL;
    }

    const KryonToken *language_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_onload_directive: Language: %s\n", language_token->value.string_value);

    // Validate language identifier
    if (!is_supported_language(language_token->value.string_value)) {
        char error_msg[256];
        snprint(error_msg, sizeof(error_msg),
                "Unsupported language: '%s'. Supported languages: '', 'rc', 'js', 'lua'",
                language_token->value.string_value);
        parser_error(parser, error_msg);
        return NULL;
    }

    // Expect opening brace for script content
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' to start onload body");
        return NULL;
    }
    
    // Parse script content until closing brace
    // We need to capture everything between braces as raw script text
    size_t start_pos = parser->current_token;
    int brace_count = 1;
    
    while (!at_end(parser) && brace_count > 0) {
        const KryonToken *current = advance(parser);
        if (current->type == KRYON_TOKEN_LEFT_BRACE) {
            brace_count++;
        } else if (current->type == KRYON_TOKEN_RIGHT_BRACE) {
            brace_count--;
        }
    }
    
    if (brace_count > 0) {
        parser_error(parser, "Expected '}' to close onload script");
        return NULL;
    }
    
    // Extract script content from tokens
    size_t end_pos = parser->current_token - 1; // Before the closing brace
    
    // Build script text from tokens, preserving original spacing
    // First, calculate total length needed
    size_t total_length = 0;
    for (size_t i = start_pos; i < end_pos; i++) {
        const KryonToken *tok = &parser->tokens[i];
        total_length += tok->lexeme_length;
        // Add space after most tokens except punctuation
        if (i < end_pos - 1 && tok->type != KRYON_TOKEN_LEFT_PAREN && 
            tok->type != KRYON_TOKEN_DOT && tok->type != KRYON_TOKEN_LEFT_BRACKET) {
            total_length++; // for space
        }
    }
    
    char *script_code = kryon_alloc(total_length + 1);
    if (!script_code) {
        parser_error(parser, "Failed to allocate memory for script code");
        return NULL;
    }
    
    script_code[0] = '\0';
    for (size_t i = start_pos; i < end_pos; i++) {
        const KryonToken *tok = &parser->tokens[i];
        if (tok->lexeme && tok->lexeme_length > 0) {
            strncat(script_code, tok->lexeme, tok->lexeme_length);
            
            // Add space after token based on context
            if (i < end_pos - 1) {
                const KryonToken *next_tok = &parser->tokens[i + 1];
                // Don't add space before/after punctuation that should be joined
                bool no_space_after = (tok->type == KRYON_TOKEN_DOT || 
                                      tok->type == KRYON_TOKEN_LEFT_PAREN ||
                                      tok->type == KRYON_TOKEN_LEFT_BRACKET ||
                                      tok->type == KRYON_TOKEN_COLON ||
                                      tok->type == KRYON_TOKEN_ASSIGN);
                
                bool no_space_before = (next_tok->type == KRYON_TOKEN_DOT ||
                                       next_tok->type == KRYON_TOKEN_LEFT_PAREN ||
                                       next_tok->type == KRYON_TOKEN_RIGHT_PAREN ||
                                       next_tok->type == KRYON_TOKEN_LEFT_BRACKET ||
                                       next_tok->type == KRYON_TOKEN_RIGHT_BRACKET ||
                                       next_tok->type == KRYON_TOKEN_COLON ||
                                       next_tok->type == KRYON_TOKEN_ASSIGN ||
                                       next_tok->type == KRYON_TOKEN_COMMA);
                
                if (!no_space_after && !no_space_before) {
                    strcat(script_code, " ");
                }
            }
        }
    }
    
    fprintf(stderr, "[DEBUG] parse_onload_directive: Extracted script code (%zu chars): %.100s...\n", 
           strlen(script_code), script_code);
    
    // Create onload directive node
    KryonASTNode *onload_node = kryon_ast_create_node(parser, KRYON_AST_ONLOAD_DIRECTIVE, 
                                                     &onload_token->location);
    if (!onload_node) {
        kryon_free(script_code);
        parser_error(parser, "Failed to create onload directive node");
        return NULL;
    }
    
    // Store language and code in script structure
    // Language is now mandatory, always present
    onload_node->data.script.language = kryon_strdup(language_token->value.string_value);
    onload_node->data.script.code = script_code;
    
    fprintf(stderr, "[DEBUG] parse_onload_directive: Created onload node with lang='%s', code_len=%zu\n",
           onload_node->data.script.language ? onload_node->data.script.language : "",
           strlen(onload_node->data.script.code));
    
    return onload_node;
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
        case KRYON_TOKEN_ONLOAD_DIRECTIVE:
            ast_type = KRYON_AST_ONLOAD_DIRECTIVE;
            break;
        case KRYON_TOKEN_FOR_DIRECTIVE:
            ast_type = KRYON_AST_FOR_DIRECTIVE;
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
        parser_error(parser, "Expected 'theme' directive");
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
    const KryonToken *directive_token = advance(parser); // consume var or variables directive

    // Check if this is variables (block syntax) or var (single variable)
    bool is_variables_block = (directive_token->type == KRYON_TOKEN_VARIABLES_DIRECTIVE);
    
    if (is_variables_block) {
        // Handle @variables { key: value, key2: value2 } syntax
        if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
            parser_error(parser, "Expected '{' after variables");
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
            
            if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
                parser_error(parser, "Expected '=' after variable name");
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
        if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
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
}

static KryonASTNode *parse_function_definition(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_function_definition: Starting\n");
    
    if (!check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE)) {
        fprintf(stderr, "[DEBUG] parse_function_definition: Not a function directive\n");
        parser_error(parser, "Expected 'function' directive");
        return NULL;
    }
    
    const KryonToken *function_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_function_definition: Found function directive\n");
    
    // REQUIRED language string
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        parser_error(parser,
            "Language specification required for function. "
            "Use function \"rc\" name() {} for rc shell, "
            "function \"\" name() {} for native Kryon, "
            "or function \"js\"/\"lua\" for other languages.");
        return NULL;
    }

    const KryonToken *language_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_function_definition: Language: %s\n", language_token->value.string_value);

    // Validate language identifier
    if (!is_supported_language(language_token->value.string_value)) {
        char error_msg[256];
        snprint(error_msg, sizeof(error_msg),
                "Unsupported language: '%s'. Supported languages: '', 'rc', 'js', 'lua'",
                language_token->value.string_value);
        parser_error(parser, error_msg);
        return NULL;  // Hard error, not warning
    }

    // Expect function name
    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        fprintf(stderr, "[DEBUG] parse_function_definition: Expected function name\n");
        parser_error(parser, "Expected function name");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    fprintf(stderr, "[DEBUG] parse_function_definition: Function name: %.*s\n", 
           (int)name_token->lexeme_length, name_token->lexeme);
    
    KryonASTNode *func_def = kryon_ast_create_node(parser, KRYON_AST_FUNCTION_DEFINITION,
                                                  &function_token->location);
    if (!func_def) {
        fprintf(stderr, "[DEBUG] parse_function_definition: Failed to create function node\n");
        return NULL;
    }
    fprintf(stderr, "[DEBUG] parse_function_definition: Created function node\n");
    
    // Language is now mandatory, always present
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
    fprintf(stderr, "[DEBUG] parse_component_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume component keyword
    
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
    component->data.component.parent_component = NULL;
    component->data.component.override_props = NULL;
    component->data.component.override_count = 0;
    component->data.component.state_vars = NULL;
    component->data.component.state_count = 0;
    component->data.component.functions = NULL;
    component->data.component.function_count = 0;
    component->data.component.on_create = NULL;
    component->data.component.on_mount = NULL;
    component->data.component.on_unmount = NULL;
    component->data.component.body_elements = NULL;
    component->data.component.body_count = 0;
    component->data.component.body_capacity = 0;
    
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
            if (match_token(parser, KRYON_TOKEN_ASSIGN)) {
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

    // Parse 'extend' keyword if present
    if (match_token(parser, KRYON_TOKEN_EXTENDS_KEYWORD)) {
        if (!check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            parser_error(parser, "Expected parent component name after extend");
        } else {
            const KryonToken *parent_token = advance(parser);
            component->data.component.parent_component = kryon_token_copy_lexeme(parent_token);
            fprintf(stderr, "[DEBUG] parse_component_definition: Component extends '%s'\n",
                   component->data.component.parent_component);
        }
    }

    // Parse component body (contains both parent overrides and component content)
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' to start component body");
        return component;
    }

    // If we have a parent, first parse parent property overrides
    if (component->data.component.parent_component) {
        // Parse override properties until we hit var/state/function or an element
        while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
            // Stop parsing overrides when we hit directives, props, or elements
            if (check_token(parser, KRYON_TOKEN_PROPS_DIRECTIVE) ||
                check_token(parser, KRYON_TOKEN_STATE_DIRECTIVE) ||
                check_token(parser, KRYON_TOKEN_VARIABLE_DIRECTIVE) ||
                check_token(parser, KRYON_TOKEN_FUNCTION_DIRECTIVE) ||
                check_token(parser, KRYON_TOKEN_ONLOAD_DIRECTIVE) ||
                check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
                break;
            }

            if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected property name or directive in component body");
                break;
            }

            const KryonToken *prop_name_token = advance(parser);

            if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
                parser_error(parser, "Expected '=' after property name");
                break;
            }

            // Parse property value
            KryonASTNode *prop_value = parse_expression(parser);
            if (!prop_value) {
                parser_error(parser, "Expected property value");
                break;
            }

            // Create a property node for this override
            KryonASTNode *prop_node = kryon_ast_create_node(parser, KRYON_AST_PROPERTY,
                                                            &prop_name_token->location);
            if (prop_node) {
                prop_node->data.property.name = kryon_token_copy_lexeme(prop_name_token);
                prop_node->data.property.value = prop_value;

                // Add to override_props array
                size_t new_count = component->data.component.override_count + 1;
                KryonASTNode **new_overrides = realloc(component->data.component.override_props,
                                                      new_count * sizeof(KryonASTNode*));
                if (new_overrides) {
                    component->data.component.override_props = new_overrides;
                    component->data.component.override_props[component->data.component.override_count] = prop_node;
                    component->data.component.override_count = new_count;
                }
            }

            // Optional comma or newline
            if (check_token(parser, KRYON_TOKEN_COMMA)) {
                advance(parser);
            }
        }
    }

    fprintf(stderr, "[DEBUG] parse_component_definition: Starting component body parsing at token %zu\n", parser->current_token);
    
    // Parse component contents (state variables, functions, and UI)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        fprintf(stderr, "[DEBUG] parse_component_definition: Processing token %zu (type %d) in component body\n",
               parser->current_token, peek(parser)->type);
        if (check_token(parser, KRYON_TOKEN_PROPS_DIRECTIVE)) {
            if (!parse_component_prop_declaration(parser, component)) {
                synchronize(parser);
            }
        } else if (check_token(parser, KRYON_TOKEN_STATE_DIRECTIVE) || check_token(parser, KRYON_TOKEN_VARIABLE_DIRECTIVE)) {
            // Parse state or var variable (they're aliases inside components)
            KryonASTNode *state_var = NULL;
            if (check_token(parser, KRYON_TOKEN_STATE_DIRECTIVE)) {
                state_var = parse_state_definition(parser);
            } else {
                // var inside component is treated as state
                state_var = parse_variable_definition(parser);
                // Convert the variable to a state variable
                if (state_var && state_var->type == KRYON_AST_VARIABLE_DEFINITION) {
                    state_var->type = KRYON_AST_STATE_DEFINITION;
                }
            }
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
        } else if (check_token(parser, KRYON_TOKEN_ON_CREATE_DIRECTIVE)) {
            // Parse oncreate lifecycle hook
            KryonASTNode *create_hook = parse_onload_directive(parser);
            if (create_hook) {
                if (component->data.component.on_create) {
                    parser_error(parser, "Component can only have one oncreate hook");
                } else {
                    component->data.component.on_create = create_hook;
                }
            }
        } else if (check_token(parser, KRYON_TOKEN_ON_MOUNT_DIRECTIVE) || check_token(parser, KRYON_TOKEN_ONLOAD_DIRECTIVE)) {
            // Parse mount (or onload) lifecycle hook
            KryonASTNode *mount_hook = parse_onload_directive(parser);
            if (mount_hook) {
                if (component->data.component.on_mount) {
                    parser_error(parser, "Component can only have one mount hook");
                } else {
                    component->data.component.on_mount = mount_hook;
                }
            }
        } else if (check_token(parser, KRYON_TOKEN_ON_UNMOUNT_DIRECTIVE)) {
            // Parse unmount lifecycle hook
            // Reuse the parse_onload_directive logic but for unmount
            const KryonToken *unmount_token = advance(parser);

            // Expect language string
            if (!check_token(parser, KRYON_TOKEN_STRING)) {
                parser_error(parser, "Expected language string after unmount");
            } else {
                const KryonToken *language_token = advance(parser);

                // Expect opening brace for script content
                if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
                    parser_error(parser, "Expected '{' after unmount language");
                } else {
                    // Extract script code (same logic as parse_onload_directive)
                    size_t brace_count = 1;
                    size_t start_index = parser->current_token;

                    while (brace_count > 0 && !at_end(parser)) {
                        if (check_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
                            brace_count++;
                        } else if (check_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
                            brace_count--;
                        }
                        if (brace_count > 0) {
                            advance(parser);
                        }
                    }

                    if (brace_count > 0) {
                        parser_error(parser, "Expected '}' to close unmount script");
                    } else {
                        size_t end_index = parser->current_token;

                        // Build script code from tokens
                        size_t total_length = 0;
                        for (size_t i = start_index; i < end_index; i++) {
                            total_length += strlen(parser->tokens[i].lexeme) + 1;
                        }

                        char *script_code = kryon_alloc(total_length + 1);
                        script_code[0] = '\0';

                        for (size_t i = start_index; i < end_index; i++) {
                            if (i > start_index) {
                                strcat(script_code, " ");
                            }
                            strcat(script_code, parser->tokens[i].lexeme);
                        }

                        // Create unmount node
                        KryonASTNode *unmount_node = kryon_ast_create_node(parser, KRYON_AST_ONLOAD_DIRECTIVE,
                                                                           &unmount_token->location);
                        if (unmount_node) {
                            unmount_node->data.script.language = kryon_strdup(language_token->value.string_value);
                            unmount_node->data.script.code = script_code;

                            if (component->data.component.on_unmount) {
                                parser_error(parser, "Component can only have one unmount hook");
                            } else {
                                component->data.component.on_unmount = unmount_node;
                            }
                        } else {
                            kryon_free(script_code);
                        }

                        // Consume closing brace
                        match_token(parser, KRYON_TOKEN_RIGHT_BRACE);
                    }
                }
            }
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            // Parse UI body element
            fprintf(stderr, "[DEBUG] parse_component_definition: Parsing component UI body element at token %zu\n", parser->current_token);
            KryonASTNode *element = parse_element(parser);
            if (element) {
                // Expand body_elements array if needed
                if (component->data.component.body_count >= component->data.component.body_capacity) {
                    size_t new_capacity = component->data.component.body_capacity == 0 ? 4 : component->data.component.body_capacity * 2;
                    KryonASTNode **new_elements = realloc(component->data.component.body_elements, new_capacity * sizeof(KryonASTNode*));
                    if (!new_elements) {
                        parser_error(parser, "Failed to allocate memory for component body elements");
                    } else {
                        component->data.component.body_elements = new_elements;
                        component->data.component.body_capacity = new_capacity;
                    }
                }

                // Add element to body_elements array
                if (component->data.component.body_count < component->data.component.body_capacity) {
                    component->data.component.body_elements[component->data.component.body_count++] = element;
                    fprintf(stderr, "[DEBUG] parse_component_definition: Added body element %zu\n", component->data.component.body_count);
                }
            }
            fprintf(stderr, "[DEBUG] parse_component_definition: Finished parsing UI body, now at token %zu\n", parser->current_token);
        } else {
            fprintf(stderr, "[DEBUG] parse_component_definition: Unexpected token type %d in component body\n", peek(parser)->type);
            parser_error(parser, "Unexpected token in component body");
            advance(parser);
        }
    }
    
    fprintf(stderr, "[DEBUG] parse_component_definition: Finished component body parsing at token %zu\n", parser->current_token);
    
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after component body");
    }
    
    fprintf(stderr, "[DEBUG] parse_component_definition: Component parsing complete at token %zu\n", parser->current_token);
    fprintf(stderr, "[DEBUG] parse_component_definition: Created component '%s' with %zu parameters, %zu state vars, %zu functions\n",
           component->data.component.name, 
           component->data.component.parameter_count,
           component->data.component.state_count,
           component->data.component.function_count);
    
    component->type = KRYON_AST_COMPONENT;

    return component;
}

static KryonASTNode *parse_state_definition(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_state_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume state directive
    
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
    if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
        parser_error(parser, "Expected '=' after state variable name");
        return state_def;
    }
    
    // Parse state variable value (could be literal or reference to parameter)
    state_def->data.variable_def.value = parse_expression(parser);
    if (!state_def->data.variable_def.value) {
        parser_error(parser, "Expected state variable value");
    }
    
    fprintf(stderr, "[DEBUG] parse_state_definition: Created state variable '%s'\n", 
           state_def->data.variable_def.name);
    
    return state_def;
}

static KryonASTNode *parse_const_definition(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_const_definition: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume const directive
    
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
    if (!match_token(parser, KRYON_TOKEN_ASSIGN)) {
        parser_error(parser, "Expected '=' after constant name");
        return const_def;
    }
    
    // Parse constant value (array literal, object literal, or simple literal)
    const_def->data.const_def.value = parse_expression(parser);
    if (!const_def->data.const_def.value) {
        parser_error(parser, "Expected constant value");
    }
    
    fprintf(stderr, "[DEBUG] parse_const_definition: Created constant '%s'\n",
           const_def->data.const_def.name);

    return const_def;
}

static KryonASTNode *parse_for_directive(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_for_directive: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume for directive

    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected loop variable name");
        return NULL;
    }

    const KryonToken *first_var_token = advance(parser);

    // Check for comma (index, value pattern)
    const KryonToken *index_token = NULL;
    const KryonToken *value_token = NULL;

    if (match_token(parser, KRYON_TOKEN_COMMA)) {
        // We have "i, habit" pattern
        index_token = first_var_token;

        if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected value variable name after comma");
            return NULL;
        }

        value_token = advance(parser);
    } else {
        // We have "habit" pattern (no index)
        value_token = first_var_token;
    }

    // Expect 'in' keyword
    if (!match_token(parser, KRYON_TOKEN_IN_KEYWORD)) {
        parser_error(parser, "Expected 'in' after loop variable");
        return NULL;
    }

    KryonASTNode *for_directive = kryon_ast_create_node(parser, KRYON_AST_FOR_DIRECTIVE,
                                                        &directive_token->location);
    if (!for_directive) {
        return NULL;
    }

    // Initialize for_loop structure
    for_directive->data.for_loop.index_var_name = index_token ? kryon_token_copy_lexeme(index_token) : NULL;
    for_directive->data.for_loop.var_name = kryon_token_copy_lexeme(value_token);
    for_directive->data.for_loop.body = NULL;
    for_directive->data.for_loop.body_count = 0;
    for_directive->data.for_loop.body_capacity = 0;

    // Parse array expression
    if (check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        const KryonToken *array_token = advance(parser);
        for_directive->data.for_loop.array_name = kryon_token_copy_lexeme(array_token);
    } else {
        parser_error(parser, "Expected array name after 'in'");
        return for_directive;
    }
    
    // Expect opening brace
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' before loop body");
        return for_directive;
    }
    
    // Parse loop body (elements and directives)
    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        KryonASTNode *body_element = NULL;

        if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            body_element = parse_element(parser);
        } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
            body_element = parse_for_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
            body_element = parse_if_directive(parser);
        } else {
            parser_error(parser, "Expected element or directive in for loop body");
            break;
        }

        if (body_element) {
            if (!kryon_ast_add_child(for_directive, body_element)) {
                parser_error(parser, "Failed to add element to for loop body");
                return for_directive;
            }
        }
    }
    
    // Expect closing brace
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after loop body");
        return for_directive;
    }
    
    if (for_directive->data.for_loop.index_var_name) {
        fprintf(stderr, "[DEBUG] parse_for_directive: Created for loop '%s, %s' in '%s' with %zu body elements\n",
               for_directive->data.for_loop.index_var_name,
               for_directive->data.for_loop.var_name,
               for_directive->data.for_loop.array_name,
               for_directive->data.for_loop.body_count);
    } else {
        fprintf(stderr, "[DEBUG] parse_for_directive: Created for loop '%s' in '%s' with %zu body elements\n",
               for_directive->data.for_loop.var_name,
               for_directive->data.for_loop.array_name,
               for_directive->data.for_loop.body_count);
    }
    
    return for_directive;
}

static bool parse_component_prop_declaration(KryonParser *parser, KryonASTNode *component) {
    const KryonToken *prop_token = advance(parser); // consume 'prop'
    (void)prop_token;

    if (!check_token(parser, KRYON_TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected property name after 'prop'");
        return false;
    }

    const KryonToken *name_token = advance(parser);
    char *param_name = kryon_token_copy_lexeme(name_token);
    if (!param_name) {
        parser_error(parser, "Failed to allocate component parameter");
        return false;
    }

    char *default_value = NULL;
    if (match_token(parser, KRYON_TOKEN_ASSIGN)) {
        if (check_token(parser, KRYON_TOKEN_STRING) ||
            check_token(parser, KRYON_TOKEN_INTEGER) ||
            check_token(parser, KRYON_TOKEN_FLOAT) ||
            check_token(parser, KRYON_TOKEN_BOOLEAN_TRUE) ||
            check_token(parser, KRYON_TOKEN_BOOLEAN_FALSE) ||
            check_token(parser, KRYON_TOKEN_IDENTIFIER) ||
            check_token(parser, KRYON_TOKEN_NULL)) {
            const KryonToken *default_token = advance(parser);
            default_value = kryon_token_copy_lexeme(default_token);
        } else {
            parser_error(parser, "Expected default value after '='");
        }
    }

    size_t new_count = component->data.component.parameter_count + 1;
    char **new_params = realloc(component->data.component.parameters, new_count * sizeof(char*));
    char **new_defaults = realloc(component->data.component.param_defaults, new_count * sizeof(char*));
    if (!new_params || !new_defaults) {
        parser_error(parser, "Failed to allocate component parameters");
        if (new_params) component->data.component.parameters = new_params;
        if (new_defaults) component->data.component.param_defaults = new_defaults;
        kryon_free(param_name);
        if (default_value) kryon_free(default_value);
        return false;
    }

    component->data.component.parameters = new_params;
    component->data.component.param_defaults = new_defaults;
    component->data.component.parameters[component->data.component.parameter_count] = param_name;
    component->data.component.param_defaults[component->data.component.parameter_count] = default_value;
    component->data.component.parameter_count = new_count;

    fprintf(stderr, "[DEBUG] parse_component_definition: Added prop '%s' with default '%s'\n",
           param_name, default_value ? default_value : "<none>");

    return true;
}

/**
 * @brief Parse conditional directive (if, elif, else, const_if)
 * @param parser The parser
 * @param is_const True if this is const_if (compile-time), false for if (runtime)
 * @return AST node for the conditional, or NULL on error
 */
static KryonASTNode *parse_if_directive(KryonParser *parser) {
    fprintf(stderr, "[DEBUG] parse_if_directive: Starting\n");
    const KryonToken *directive_token = advance(parser); // consume if directive

    // Create the conditional node
    KryonASTNode *conditional = kryon_ast_create_node(parser, KRYON_AST_IF_DIRECTIVE, &directive_token->location);
    if (!conditional) {
        return NULL;
    }

    // Initialize conditional structure
    conditional->data.conditional.is_const = false;  /* Initialize to runtime if by default */
    conditional->data.conditional.condition = NULL;
    conditional->data.conditional.then_body = NULL;
    conditional->data.conditional.then_count = 0;
    conditional->data.conditional.then_capacity = 0;
    conditional->data.conditional.elif_conditions = NULL;
    conditional->data.conditional.elif_bodies = NULL;
    conditional->data.conditional.elif_counts = NULL;
    conditional->data.conditional.elif_count = 0;
    conditional->data.conditional.elif_capacity = 0;
    conditional->data.conditional.else_body = NULL;
    conditional->data.conditional.else_count = 0;
    conditional->data.conditional.else_capacity = 0;

    // Parse condition expression
    conditional->data.conditional.condition = parse_expression(parser);
    if (!conditional->data.conditional.condition) {
        parser_error(parser, "Expected condition expression after if");
        return conditional;
    }

    // Expect opening brace
    if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
        parser_error(parser, "Expected '{' after condition");
        return conditional;
    }

    // Parse then body
    conditional->data.conditional.then_capacity = 4;
    conditional->data.conditional.then_body = kryon_calloc(conditional->data.conditional.then_capacity,
                                                           sizeof(KryonASTNode*));

    while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
        KryonASTNode *body_element = NULL;

        if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            body_element = parse_element(parser);
        } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
            body_element = parse_for_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
            body_element = parse_if_directive(parser);
        } else {
            parser_error(parser, "Expected element in conditional body");
            break;
        }

        if (body_element) {
            // Resize if needed
            if (conditional->data.conditional.then_count >= conditional->data.conditional.then_capacity) {
                conditional->data.conditional.then_capacity *= 2;
                conditional->data.conditional.then_body = kryon_realloc(conditional->data.conditional.then_body,
                    conditional->data.conditional.then_capacity * sizeof(KryonASTNode*));
            }
            conditional->data.conditional.then_body[conditional->data.conditional.then_count++] = body_element;
        }
    }

    // Expect closing brace
    if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
        parser_error(parser, "Expected '}' after conditional body");
        return conditional;
    }

    // Parse optional elif/else blocks
    conditional->data.conditional.elif_capacity = 2;
    conditional->data.conditional.elif_conditions = kryon_calloc(conditional->data.conditional.elif_capacity,
                                                                 sizeof(KryonASTNode*));
    conditional->data.conditional.elif_bodies = kryon_calloc(conditional->data.conditional.elif_capacity,
                                                             sizeof(KryonASTNode**));
    conditional->data.conditional.elif_counts = kryon_calloc(conditional->data.conditional.elif_capacity,
                                                             sizeof(size_t));

    while (check_token(parser, KRYON_TOKEN_ELIF_DIRECTIVE)) {
        advance(parser); // consume elif

        // Resize elif arrays if needed
        if (conditional->data.conditional.elif_count >= conditional->data.conditional.elif_capacity) {
            conditional->data.conditional.elif_capacity *= 2;
            conditional->data.conditional.elif_conditions = kryon_realloc(conditional->data.conditional.elif_conditions,
                conditional->data.conditional.elif_capacity * sizeof(KryonASTNode*));
            conditional->data.conditional.elif_bodies = kryon_realloc(conditional->data.conditional.elif_bodies,
                conditional->data.conditional.elif_capacity * sizeof(KryonASTNode**));
            conditional->data.conditional.elif_counts = kryon_realloc(conditional->data.conditional.elif_counts,
                conditional->data.conditional.elif_capacity * sizeof(size_t));
        }

        // Parse elif condition
        KryonASTNode *elif_condition = parse_expression(parser);
        if (!elif_condition) {
            parser_error(parser, "Expected condition expression after elif");
            break;
        }

        conditional->data.conditional.elif_conditions[conditional->data.conditional.elif_count] = elif_condition;

        // Expect opening brace
        if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
            parser_error(parser, "Expected '{' after elif condition");
            break;
        }

        // Parse elif body
        size_t elif_capacity = 4;
        KryonASTNode **elif_body = kryon_calloc(elif_capacity, sizeof(KryonASTNode*));
        size_t elif_count = 0;

        while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
            KryonASTNode *body_element = NULL;

            if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
                body_element = parse_element(parser);
            } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
                body_element = parse_for_directive(parser);
            } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
                body_element = parse_if_directive(parser);
            } else {
                parser_error(parser, "Expected element in elif body");
                break;
            }

            if (body_element) {
                if (elif_count >= elif_capacity) {
                    elif_capacity *= 2;
                    elif_body = kryon_realloc(elif_body, elif_capacity * sizeof(KryonASTNode*));
                }
                elif_body[elif_count++] = body_element;
            }
        }

        conditional->data.conditional.elif_bodies[conditional->data.conditional.elif_count] = elif_body;
        conditional->data.conditional.elif_counts[conditional->data.conditional.elif_count] = elif_count;
        conditional->data.conditional.elif_count++;

        // Expect closing brace
        if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
            parser_error(parser, "Expected '}' after elif body");
            break;
        }
    }

    // Parse optional else block
    if (check_token(parser, KRYON_TOKEN_ELSE_DIRECTIVE)) {
        advance(parser); // consume else

        // Expect opening brace
        if (!match_token(parser, KRYON_TOKEN_LEFT_BRACE)) {
            parser_error(parser, "Expected '{' after else");
            return conditional;
        }

        // Parse else body
        conditional->data.conditional.else_capacity = 4;
        conditional->data.conditional.else_body = kryon_calloc(conditional->data.conditional.else_capacity,
                                                               sizeof(KryonASTNode*));

        while (!check_token(parser, KRYON_TOKEN_RIGHT_BRACE) && !at_end(parser)) {
            KryonASTNode *body_element = NULL;

            if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
                body_element = parse_element(parser);
            } else if (check_token(parser, KRYON_TOKEN_FOR_DIRECTIVE)) {
                body_element = parse_for_directive(parser);
            } else if (check_token(parser, KRYON_TOKEN_IF_DIRECTIVE)) {
                body_element = parse_if_directive(parser);
            } else {
                parser_error(parser, "Expected element in else body");
                break;
            }

            if (body_element) {
                if (conditional->data.conditional.else_count >= conditional->data.conditional.else_capacity) {
                    conditional->data.conditional.else_capacity *= 2;
                    conditional->data.conditional.else_body = kryon_realloc(conditional->data.conditional.else_body,
                        conditional->data.conditional.else_capacity * sizeof(KryonASTNode*));
                }
                conditional->data.conditional.else_body[conditional->data.conditional.else_count++] = body_element;
            }
        }

        // Expect closing brace
        if (!match_token(parser, KRYON_TOKEN_RIGHT_BRACE)) {
            parser_error(parser, "Expected '}' after else body");
            return conditional;
        }
    }

    fprintf(stderr, "[DEBUG] parse_if_directive: Created conditional with %zu then, %zu elif, %zu else elements\n",
           conditional->data.conditional.then_count,
           conditional->data.conditional.elif_count,
           conditional->data.conditional.else_count);

    return conditional;
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
        snprint(error_msg, msg_len, "%s at %s", message, location_str);
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
    if (parent->type != KRYON_AST_ROOT &&
        parent->type != KRYON_AST_ELEMENT &&
        parent->type != KRYON_AST_FOR_DIRECTIVE &&
        parent->type != KRYON_AST_IF_DIRECTIVE) {
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
            case KRYON_AST_METADATA_DIRECTIVE:
            case KRYON_AST_EVENT_DIRECTIVE:
            case KRYON_AST_ONLOAD_DIRECTIVE:
            case KRYON_AST_FOR_DIRECTIVE:
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
    
    // Handle different parent types with different child array structures
    if (parent->type == KRYON_AST_FOR_DIRECTIVE) {
        // @for directive uses data.for_loop.body
        if (parent->data.for_loop.body_count >= parent->data.for_loop.body_capacity) {
            size_t new_capacity = parent->data.for_loop.body_capacity ?
                                 parent->data.for_loop.body_capacity * 2 : 4;
            KryonASTNode **new_body = realloc(parent->data.for_loop.body,
                                              new_capacity * sizeof(KryonASTNode*));
            if (!new_body) {
                return false;
            }
            parent->data.for_loop.body = new_body;
            parent->data.for_loop.body_capacity = new_capacity;
        }
        parent->data.for_loop.body[parent->data.for_loop.body_count++] = child;
        child->parent = parent;
        return true;
    }

    if (parent->type == KRYON_AST_IF_DIRECTIVE) {
        // @if directive uses data.conditional.then_body
        if (parent->data.conditional.then_count >= parent->data.conditional.then_capacity) {
            size_t new_capacity = parent->data.conditional.then_capacity ?
                                 parent->data.conditional.then_capacity * 2 : 4;
            KryonASTNode **new_then = realloc(parent->data.conditional.then_body,
                                              new_capacity * sizeof(KryonASTNode*));
            if (!new_then) {
                return false;
            }
            parent->data.conditional.then_body = new_then;
            parent->data.conditional.then_capacity = new_capacity;
        }
        parent->data.conditional.then_body[parent->data.conditional.then_count++] = child;
        child->parent = parent;
        return true;
    }

    // ROOT and ELEMENT use data.element.children
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
    
    // Only elements, styles, metadata, and directive types can have properties
    if (parent->type != KRYON_AST_ELEMENT && 
        parent->type != KRYON_AST_STYLE_BLOCK && 
        parent->type != KRYON_AST_METADATA_DIRECTIVE &&
        parent->type != KRYON_AST_EVENT_DIRECTIVE &&
        parent->type != KRYON_AST_ONLOAD_DIRECTIVE &&
        parent->type != KRYON_AST_FOR_DIRECTIVE) {
        return false;
    }
    
    // Get the appropriate property array
    KryonASTNode ***properties;
    size_t *property_count;
    size_t *property_capacity;
    
    if (parent->type == KRYON_AST_ELEMENT || parent->type == KRYON_AST_METADATA_DIRECTIVE || parent->type == KRYON_AST_EVENT_DIRECTIVE || parent->type == KRYON_AST_ONLOAD_DIRECTIVE || parent->type == KRYON_AST_FOR_DIRECTIVE) {
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


/**
 * @brief Creates a property node without a parser context.
 * @details This is a simplified version of node creation for programmatic use.
 *          It does not add the node to the parser's tracking list, so the
 *          caller is responsible for freeing the memory.
 */
 static KryonASTNode* create_standalone_ast_node(KryonASTNodeType type) {
    KryonASTNode* node = calloc(1, sizeof(KryonASTNode));
    if (!node) return NULL;
    node->type = type;
    return node;
}

KryonASTNode* create_string_property_node(const char* name, const char* value) {
    // 1. Create the top-level property node
    KryonASTNode* prop_node = create_standalone_ast_node(KRYON_AST_PROPERTY);
    if (!prop_node) return NULL;
    prop_node->data.property.name = strdup(name);

    // 2. Create the literal value node that holds the string
    KryonASTNode* literal_node = create_standalone_ast_node(KRYON_AST_LITERAL);
    if (!literal_node) {
        free(prop_node->data.property.name);
        free(prop_node);
        return NULL;
    }
    literal_node->data.literal.value.type = KRYON_VALUE_STRING;
    literal_node->data.literal.value.data.string_value = strdup(value);

    // 3. Link the literal value to the property
    prop_node->data.property.value = literal_node;
    literal_node->parent = prop_node;
    
    return prop_node;
}

void add_property_to_element_node(KryonASTNode* element_node, KryonASTNode* property_node) {
    if (!element_node || element_node->type != KRYON_AST_ELEMENT || !property_node) {
        return;
    }

    // Expand the properties array if needed
    if (element_node->data.element.property_count >= element_node->data.element.property_capacity) {
        size_t new_capacity = element_node->data.element.property_capacity == 0 ? 4 : element_node->data.element.property_capacity * 2;
        KryonASTNode** new_props = realloc(element_node->data.element.properties, new_capacity * sizeof(KryonASTNode*));
        if (!new_props) {
            // In a real application, you'd handle this error more gracefully
            fprint(2, "ERROR: Failed to reallocate property array in optimizer.\n");
            return;
        }
        element_node->data.element.properties = new_props;
        element_node->data.element.property_capacity = new_capacity;
    }

    // Add the new property
    element_node->data.element.properties[element_node->data.element.property_count] = property_node;
    element_node->data.element.property_count++;
    property_node->parent = element_node;
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
        case KRYON_AST_FOR_DIRECTIVE: return "ForDirective";
        case KRYON_AST_IF_DIRECTIVE: return "IfDirective";
        case KRYON_AST_ELIF_DIRECTIVE: return "ElifDirective";
        case KRYON_AST_ELSE_DIRECTIVE: return "ElseDirective";
        case KRYON_AST_LITERAL: return "Literal";
        case KRYON_AST_VARIABLE: return "Variable";
        case KRYON_AST_TEMPLATE: return "Template";
        case KRYON_AST_ARRAY_LITERAL: return "ArrayLiteral";
        case KRYON_AST_OBJECT_LITERAL: return "ObjectLiteral";
        case KRYON_AST_MEMBER_ACCESS: return "MemberAccess";
        case KRYON_AST_ARRAY_ACCESS: return "ArrayAccess";
        case KRYON_AST_ERROR: return "Error";
        case KRYON_AST_COMPONENT: return "Component";
        default: return "Unknown";
    }
}

// =============================================================================
// TEMPLATE STRING PARSING
// =============================================================================

/**
 * @brief Parse a template string like "- ${todo}" into segments
 * @param parser Parser context 
 * @param template_str Template string to parse
 * @return Template AST node with segments, or NULL on error
 */
static KryonASTNode *parse_template_string(KryonParser *parser, const char* template_str) {
    if (!parser || !template_str) {
        fprintf(stderr, "❌ parse_template_string: Invalid parameters (parser=%p, template_str=%p)\n", 
               (void*)parser, (void*)template_str);
        return NULL;
    }
    
    // Create template node with current token location
    const KryonToken *current_token = &parser->tokens[parser->current_token - 1];
    KryonASTNode *template_node = kryon_ast_create_node(parser, KRYON_AST_TEMPLATE, &current_token->location);
    if (!template_node) {
        fprintf(stderr, "❌ parse_template_string: Failed to create template node\n");
        return NULL;
    }
    
    // Initialize template segments
    template_node->data.template.segments = NULL;
    template_node->data.template.segment_count = 0;
    template_node->data.template.segment_capacity = 0;
    
    const char *current = template_str;
    const char *start = current;
    
    fprintf(stderr, "🔍 Parsing template string: '%s'\n", template_str);
    
    while (*current) {
        // Look for ${variable} pattern
        const char *var_start = strstr(current, "${");
        
        if (!var_start) {
            // No more variables, add remaining text as literal segment
            if (*current) {
                // Create literal segment for remaining text
                KryonASTNode *literal_segment = kryon_ast_create_node(parser, KRYON_AST_LITERAL, &current_token->location);
                if (literal_segment) {
                    literal_segment->data.literal.value = kryon_ast_value_string(current);
                    
                    // Add to template segments
                    if (template_node->data.template.segment_count >= template_node->data.template.segment_capacity) {
                        size_t new_capacity = template_node->data.template.segment_capacity == 0 ? 2 : template_node->data.template.segment_capacity * 2;
                        template_node->data.template.segments = kryon_realloc(template_node->data.template.segments, 
                                                                              new_capacity * sizeof(KryonASTNode*));
                        template_node->data.template.segment_capacity = new_capacity;
                    }
                    template_node->data.template.segments[template_node->data.template.segment_count++] = literal_segment;
                    fprintf(stderr, "  📝 Added literal segment: '%s'\n", current);
                }
            }
            break;
        }
        
        // Add literal part before ${variable} if any
        if (var_start > current) {
            // Create literal segment for text before variable
            size_t literal_len = var_start - current;
            char *literal_text = kryon_alloc(literal_len + 1);
            if (literal_text) {
                strncpy(literal_text, current, literal_len);
                literal_text[literal_len] = '\0';
                
                KryonASTNode *literal_segment = kryon_ast_create_node(parser, KRYON_AST_LITERAL, &current_token->location);
                if (literal_segment) {
                    literal_segment->data.literal.value = kryon_ast_value_string(literal_text);
                    
                    // Add to template segments
                    if (template_node->data.template.segment_count >= template_node->data.template.segment_capacity) {
                        size_t new_capacity = template_node->data.template.segment_capacity == 0 ? 2 : template_node->data.template.segment_capacity * 2;
                        template_node->data.template.segments = kryon_realloc(template_node->data.template.segments, 
                                                                              new_capacity * sizeof(KryonASTNode*));
                        template_node->data.template.segment_capacity = new_capacity;
                    }
                    template_node->data.template.segments[template_node->data.template.segment_count++] = literal_segment;
                    fprintf(stderr, "  📝 Added literal segment: '%s'\n", literal_text);
                }
                kryon_free(literal_text);
            }
        }
        
        // Parse ${variable} 
        const char *var_end = strstr(var_start, "}");
        if (!var_end) {
            fprintf(stderr, "❌ Error: Unclosed variable reference in template\n");
            break;
        }
        
        // Extract variable name (skip ${ and })
        const char *var_name_start = var_start + 2;
        size_t var_name_len = var_end - var_name_start;
        
        if (var_name_len > 0) {
            char *var_name = kryon_alloc(var_name_len + 1);
            if (var_name) {
                strncpy(var_name, var_name_start, var_name_len);
                var_name[var_name_len] = '\0';
                
                // Create variable or member access segment based on content
                KryonASTNode *var_segment = NULL;
                
                // Check if this is a member access (contains a dot)
                char *dot_pos = strchr(var_name, '.');
                if (dot_pos) {
                    // This is a member access like "alignment.name"
                    // Split into object and property parts
                    size_t object_name_len = dot_pos - var_name;
                    char *object_name = kryon_alloc(object_name_len + 1);
                    char *property_name = kryon_strdup(dot_pos + 1);
                    
                    if (object_name && property_name) {
                        strncpy(object_name, var_name, object_name_len);
                        object_name[object_name_len] = '\0';
                        
                        // Create member access node
                        var_segment = kryon_ast_create_node(parser, KRYON_AST_MEMBER_ACCESS, &current_token->location);
                        if (var_segment) {
                            // Create object identifier node
                            KryonASTNode *object_node = kryon_ast_create_node(parser, KRYON_AST_IDENTIFIER, &current_token->location);
                            if (object_node) {
                                object_node->data.identifier.name = kryon_strdup(object_name);
                                var_segment->data.member_access.object = object_node;
                                var_segment->data.member_access.member = kryon_strdup(property_name);
                                fprintf(stderr, "  🔗 Added member access segment: '%s.%s'\n", object_name, property_name);
                            } else {
                                kryon_free(var_segment);
                                var_segment = NULL;
                            }
                        }
                    }
                    
                    if (object_name) kryon_free(object_name);
                    if (property_name) kryon_free(property_name);
                } else {
                    // Simple variable like "someVar"
                    var_segment = kryon_ast_create_node(parser, KRYON_AST_VARIABLE, &current_token->location);
                    if (var_segment) {
                        var_segment->data.variable.name = kryon_strdup(var_name);
                        fprintf(stderr, "  🔗 Added variable segment: '%s'\n", var_name);
                    }
                }
                
                // Add segment to template
                if (var_segment) {
                    // Add to template segments
                    if (template_node->data.template.segment_count >= template_node->data.template.segment_capacity) {
                        size_t new_capacity = template_node->data.template.segment_capacity == 0 ? 2 : template_node->data.template.segment_capacity * 2;
                        template_node->data.template.segments = kryon_realloc(template_node->data.template.segments, 
                                                                              new_capacity * sizeof(KryonASTNode*));
                        template_node->data.template.segment_capacity = new_capacity;
                    }
                    template_node->data.template.segments[template_node->data.template.segment_count++] = var_segment;
                }
                kryon_free(var_name);
            }
        }
        
        // Move past the variable reference
        current = var_end + 1;
    }
    
    fprintf(stderr, "✅ Template parsed into %zu segments\n", template_node->data.template.segment_count);
    return template_node;
}
