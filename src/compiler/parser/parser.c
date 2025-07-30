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

// String duplication utility (since we don't have kryon_strdup)
static char *kryon_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = kryon_alloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonASTNode *parse_document(KryonParser *parser);
static KryonASTNode *parse_element(KryonParser *parser);
static KryonASTNode *parse_style_block(KryonParser *parser);
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
    if (!tokens || token_count == 0) {
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
                        kryon_free(node->data.style.properties);
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
    KryonASTNode *root = kryon_ast_create_node(parser, KRYON_AST_ROOT, 
                                              &parser->tokens[0].location);
    if (!root) {
        return NULL;
    }
    
    // Parse top-level declarations
    while (!at_end(parser)) {
        KryonASTNode *node = NULL;
        
        if (check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) ||
            check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
            node = parse_style_block(parser);
        } else if (kryon_token_is_directive(peek(parser)->type)) {
            node = parse_directive(parser);
        } else if (check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
            node = parse_element(parser);
        } else {
            parser_error(parser, "Expected element, style block, or directive");
            advance(parser); // Skip invalid token
            continue;
        }
        
        if (node && !kryon_ast_add_child(root, node)) {
            parser_error(parser, "Failed to add node to AST");
        }
    }
    
    return root;
}

static KryonASTNode *parse_element(KryonParser *parser) {
    if (!check_token(parser, KRYON_TOKEN_ELEMENT_TYPE)) {
        parser_error(parser, "Expected element type");
        return NULL;
    }
    
    // Check nesting depth
    if (parser->nesting_depth >= parser->config.max_nesting_depth) {
        parser_error(parser, "Maximum nesting depth exceeded");
        return NULL;
    }
    
    const KryonToken *type_token = advance(parser);
    
    KryonASTNode *element = kryon_ast_create_node(parser, KRYON_AST_ELEMENT,
                                                 &type_token->location);
    if (!element) {
        return NULL;
    }
    
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
    if (!check_token(parser, KRYON_TOKEN_STYLE_DIRECTIVE) &&
        !check_token(parser, KRYON_TOKEN_STYLES_DIRECTIVE)) {
        parser_error(parser, "Expected @style or @styles directive");
        return NULL;
    }
    
    const KryonToken *directive_token = advance(parser);
    
    // Expect style name
    if (!check_token(parser, KRYON_TOKEN_STRING)) {
        parser_error(parser, "Expected style name string");
        return NULL;
    }
    
    const KryonToken *name_token = advance(parser);
    
    KryonASTNode *style = kryon_ast_create_node(parser, KRYON_AST_STYLE_BLOCK,
                                               &directive_token->location);
    if (!style) {
        return NULL;
    }
    
    // Copy style name (remove quotes)
    style->data.style.name = kryon_strdup(name_token->value.string_value);
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
            literal->data.literal.value = kryon_ast_value_integer(token->value.int_value);
            break;
        case KRYON_TOKEN_FLOAT:
            literal->data.literal.value = kryon_ast_value_float(token->value.float_value);
            break;
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
    // Simplified directive parsing - just consume the directive for now
    const KryonToken *token = advance(parser);
    
    KryonASTNode *directive = kryon_ast_create_node(parser, KRYON_AST_STORE_DIRECTIVE,
                                                   &token->location);
    // TODO: Implement full directive parsing
    
    return directive;
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
    return parser->current_token >= parser->token_count ||
           peek(parser)->type == KRYON_TOKEN_EOF;
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
    
    // Expand children array if needed
    if (parent->data.element.child_count >= parent->data.element.child_capacity) {
        size_t new_capacity = parent->data.element.child_capacity ?
                             parent->data.element.child_capacity * 2 : 4;
        KryonASTNode **new_children = kryon_realloc(parent->data.element.children,
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
    
    // Only elements and styles can have properties
    if (parent->type != KRYON_AST_ELEMENT && parent->type != KRYON_AST_STYLE_BLOCK) {
        return false;
    }
    
    // Get the appropriate property array
    KryonASTNode ***properties;
    size_t *property_count;
    size_t *property_capacity;
    
    if (parent->type == KRYON_AST_ELEMENT) {
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
        KryonASTNode **new_properties = kryon_realloc(*properties,
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
        case KRYON_AST_STYLE_BLOCK: return "Style";
        case KRYON_AST_LITERAL: return "Literal";
        case KRYON_AST_VARIABLE: return "Variable";
        case KRYON_AST_TEMPLATE: return "Template";
        case KRYON_AST_ERROR: return "Error";
        default: return "Unknown";
    }
}