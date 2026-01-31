/**
 * Limbo AST Implementation
 *
 * Minimal AST node constructors for Limbo parser
 */

#include "limbo_ast.h"
#include <stdlib.h>
#include <string.h>

// Note: Most AST functions are implemented in limbo_parser.c
// This file contains additional utility functions

/**
 * Create a simple identifier expression
 */
LimboExpression* limbo_ast_identifier_create(const char* name, LimboToken token) {
    LimboExpression* expr = limbo_ast_expression_create(NODE_IDENTIFIER, token);
    if (!expr) return NULL;

    expr->as.identifier.name = name ? strdup(name) : NULL;
    return expr;
}

/**
 * Create a string literal expression
 */
LimboExpression* limbo_ast_string_literal_create(const char* value, LimboToken token) {
    LimboExpression* expr = limbo_ast_expression_create(NODE_LITERAL, token);
    if (!expr) return NULL;

    expr->as.string_literal.string_value = value ? strdup(value) : NULL;
    return expr;
}

/**
 * Create an integer literal expression
 */
LimboExpression* limbo_ast_integer_literal_create(long value, LimboToken token) {
    LimboExpression* expr = limbo_ast_expression_create(NODE_LITERAL, token);
    if (!expr) return NULL;

    expr->as.integer_literal.integer_value = value;
    return expr;
}

/**
 * Create a binary operation expression
 */
LimboExpression* limbo_ast_binary_op_create(LimboExpression* left, LimboTokenType op,
                                           LimboExpression* right, LimboToken token) {
    LimboExpression* expr = limbo_ast_expression_create(NODE_BINARY_OP, token);
    if (!expr) return NULL;

    expr->as.binary_op.left = left;
    expr->as.binary_op.op = op;
    expr->as.binary_op.right = right;
    return expr;
}

/**
 * Create a call expression
 */
LimboExpression* limbo_ast_call_create(LimboExpression* function, LimboExpression** args,
                                      size_t arg_count, LimboToken token) {
    LimboExpression* expr = limbo_ast_expression_create(NODE_CALL, token);
    if (!expr) return NULL;

    expr->as.call.function = function;
    expr->as.call.args = args;
    expr->as.call.arg_count = arg_count;
    return expr;
}
