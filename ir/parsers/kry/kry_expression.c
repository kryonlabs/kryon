/**
 * KRY Expression Transpiler Implementation
 *
 * Transpiles ES6-style expressions to both Lua and JavaScript.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_expression.h"
#include "kry_arrow_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

// ============================================================================
// Error Handling
// ============================================================================

static char* g_last_error = NULL;

static void set_error(const char* error) {
    if (g_last_error) {
        free(g_last_error);
    }
    g_last_error = error ? strdup(error) : NULL;
}

const char* kry_expr_get_error(void) {
    return g_last_error;
}

void kry_expr_clear_error(void) {
    if (g_last_error) {
        free(g_last_error);
        g_last_error = NULL;
    }
}

// ============================================================================
// Memory Management
// ============================================================================

static KryExprNode* create_node(KryExprType type) {
    KryExprNode* node = (KryExprNode*)calloc(1, sizeof(KryExprNode));
    if (node) {
        node->type = type;
    }
    return node;
}

void kry_expr_free(KryExprNode* node) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            if (node->literal.literal_type == KRY_LITERAL_STRING) {
                free(node->literal.string_val);
            }
            break;

        case KRY_EXPR_IDENTIFIER:
            free(node->identifier);
            break;

        case KRY_EXPR_BINARY_OP:
            kry_expr_free(node->binary_op.left);
            kry_expr_free(node->binary_op.right);
            break;

        case KRY_EXPR_UNARY_OP:
            kry_expr_free(node->unary_op.operand);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            kry_expr_free(node->property_access.object);
            free(node->property_access.property);
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            kry_expr_free(node->element_access.array);
            kry_expr_free(node->element_access.index);
            break;

        case KRY_EXPR_CALL:
            kry_expr_free(node->call.callee);
            for (size_t i = 0; i < node->call.arg_count; i++) {
                kry_expr_free(node->call.args[i]);
            }
            free(node->call.args);
            break;

        case KRY_EXPR_ARRAY:
            for (size_t i = 0; i < node->array.element_count; i++) {
                kry_expr_free(node->array.elements[i]);
            }
            free(node->array.elements);
            break;

        case KRY_EXPR_OBJECT:
            for (size_t i = 0; i < node->object.prop_count; i++) {
                free(node->object.keys[i]);
                kry_expr_free(node->object.values[i]);
            }
            free(node->object.keys);
            free(node->object.values);
            break;

        case KRY_EXPR_ARROW_FUNC:
            for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                free(node->arrow_func.params[i]);
            }
            free(node->arrow_func.params);
            kry_expr_free(node->arrow_func.body);
            break;

        case KRY_EXPR_MEMBER_EXPR:
            kry_expr_free(node->member_expr.object);
            free(node->member_expr.member);
            break;

        case KRY_EXPR_CONDITIONAL:
            kry_expr_free(node->conditional.condition);
            kry_expr_free(node->conditional.consequent);
            kry_expr_free(node->conditional.alternate);
            break;

        case KRY_EXPR_TEMPLATE:
            for (size_t i = 0; i < node->template_str.part_count; i++) {
                free(node->template_str.parts[i]);
            }
            free(node->template_str.parts);
            for (size_t i = 0; i < node->template_str.expr_count; i++) {
                kry_expr_free(node->template_str.expressions[i]);
            }
            free(node->template_str.expressions);
            break;
    }

    free(node);
}

// ============================================================================
// Simple Expression Parser (Recursive Descent)
// ============================================================================

typedef struct {
    const char* input;
    size_t pos;
    size_t length;
} ParserContext;

static void init_context(ParserContext* ctx, const char* input) {
    ctx->input = input;
    ctx->pos = 0;
    ctx->length = strlen(input);
}

static char peek(ParserContext* ctx) {
    if (ctx->pos >= ctx->length) return '\0';
    // Skip whitespace
    while (ctx->pos < ctx->length && isspace(ctx->input[ctx->pos])) {
        ctx->pos++;
    }
    if (ctx->pos >= ctx->length) return '\0';
    return ctx->input[ctx->pos];
}

static char consume(ParserContext* ctx) {
    if (ctx->pos >= ctx->length) return '\0';
    return ctx->input[ctx->pos++];
}

static bool match(ParserContext* ctx, char expected) {
    if (peek(ctx) == expected) {
        consume(ctx);
        return true;
    }
    return false;
}

static bool match_str(ParserContext* ctx, const char* str) {
    size_t len = strlen(str);
    if (ctx->pos + len <= ctx->length &&
        strncmp(ctx->input + ctx->pos, str, len) == 0) {
        ctx->pos += len;
        return true;
    }
    return false;
}

static bool is_identifier_char(char c) {
    return isalnum(c) || c == '_' || c == '$';
}

// Forward declarations
static KryExprNode* parse_expression(ParserContext* ctx);
static KryExprNode* parse_conditional(ParserContext* ctx);
static KryExprNode* parse_assignment(ParserContext* ctx);
static KryExprNode* parse_logical_or(ParserContext* ctx);
static KryExprNode* parse_logical_and(ParserContext* ctx);
static KryExprNode* parse_equality(ParserContext* ctx);
static KryExprNode* parse_comparison(ParserContext* ctx);
static KryExprNode* parse_additive(ParserContext* ctx);
static KryExprNode* parse_multiplicative(ParserContext* ctx);
static KryExprNode* parse_unary(ParserContext* ctx);
static KryExprNode* parse_member_expr(ParserContext* ctx);
static KryExprNode* parse_primary(ParserContext* ctx);
static KryExprNode* parse_arrow_func_params(ParserContext* ctx, KryExprNode* left);
static KryExprNode* parse_object_literal(ParserContext* ctx);
static KryExprNode* parse_array_literal(ParserContext* ctx);
static KryExprNode* parse_template_string(ParserContext* ctx);

// Parse starting point
KryExprNode* kry_expr_parse(const char* source, char** error_output) {
    if (!source || !*source) {
        if (error_output) {
            *error_output = strdup("Empty expression");
        }
        return NULL;
    }

    kry_expr_clear_error();
    ParserContext ctx;
    init_context(&ctx, source);

    KryExprNode* node = parse_expression(&ctx);
    if (!node) {
        const char* err = kry_expr_get_error();
        if (error_output && err) {
            *error_output = strdup(err);
        }
        return NULL;
    }

    return node;
}

static KryExprNode* parse_expression(ParserContext* ctx) {
    return parse_conditional(ctx);
}

static KryExprNode* parse_conditional(ParserContext* ctx) {
    KryExprNode* condition = parse_assignment(ctx);
    if (!condition) return NULL;

    // Check for ternary operator
    if (peek(ctx) == '?') {
        consume(ctx);  // Consume '?'

        KryExprNode* consequent = parse_expression(ctx);  // Full expression for then-branch
        if (!consequent) {
            kry_expr_free(condition);
            return NULL;
        }

        if (peek(ctx) != ':') {
            set_error("Expected ':' in ternary expression");
            kry_expr_free(condition);
            kry_expr_free(consequent);
            return NULL;
        }
        consume(ctx);  // Consume ':'

        KryExprNode* alternate = parse_conditional(ctx);  // Right-associative
        if (!alternate) {
            kry_expr_free(condition);
            kry_expr_free(consequent);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_CONDITIONAL);
        if (node) {
            node->conditional.condition = condition;
            node->conditional.consequent = consequent;
            node->conditional.alternate = alternate;
        }
        return node;
    }

    return condition;
}

static KryExprNode* parse_assignment(ParserContext* ctx) {
    KryExprNode* left = parse_logical_or(ctx);

    if (match(ctx, '=')) {
        KryExprNode* right = parse_assignment(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = KRY_BIN_OP_ASSIGN;
        node->binary_op.right = right;
        return node;
    }

    return left;
}

static KryExprNode* parse_logical_or(ParserContext* ctx) {
    KryExprNode* left = parse_logical_and(ctx);

    while (match_str(ctx, "||")) {
        KryExprNode* right = parse_logical_and(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = KRY_BIN_OP_OR;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_logical_and(ParserContext* ctx) {
    KryExprNode* left = parse_equality(ctx);

    while (match_str(ctx, "&&")) {
        KryExprNode* right = parse_equality(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = KRY_BIN_OP_AND;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_equality(ParserContext* ctx) {
    KryExprNode* left = parse_comparison(ctx);

    while (true) {
        KryBinaryOp op;
        if (match_str(ctx, "==")) {
            op = KRY_BIN_OP_EQ;
        } else if (match_str(ctx, "!=")) {
            op = KRY_BIN_OP_NEQ;
        } else {
            break;
        }

        KryExprNode* right = parse_comparison(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = op;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_comparison(ParserContext* ctx) {
    KryExprNode* left = parse_additive(ctx);

    while (true) {
        KryBinaryOp op;
        if (match_str(ctx, "<=")) {
            op = KRY_BIN_OP_LTE;
        } else if (match_str(ctx, ">=")) {
            op = KRY_BIN_OP_GTE;
        } else if (match(ctx, '<')) {
            op = KRY_BIN_OP_LT;
        } else if (match(ctx, '>')) {
            op = KRY_BIN_OP_GT;
        } else {
            break;
        }

        KryExprNode* right = parse_additive(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = op;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_additive(ParserContext* ctx) {
    KryExprNode* left = parse_multiplicative(ctx);

    while (true) {
        KryBinaryOp op;
        if (match(ctx, '+')) {
            op = KRY_BIN_OP_ADD;
        } else if (match(ctx, '-')) {
            op = KRY_BIN_OP_SUB;
        } else {
            break;
        }

        KryExprNode* right = parse_multiplicative(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = op;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_multiplicative(ParserContext* ctx) {
    KryExprNode* left = parse_unary(ctx);

    while (true) {
        KryBinaryOp op;
        if (match(ctx, '*')) {
            op = KRY_BIN_OP_MUL;
        } else if (match(ctx, '/')) {
            op = KRY_BIN_OP_DIV;
        } else if (match(ctx, '%')) {
            op = KRY_BIN_OP_MOD;
        } else {
            break;
        }

        KryExprNode* right = parse_unary(ctx);
        if (!right) {
            kry_expr_free(left);
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_BINARY_OP);
        node->binary_op.left = left;
        node->binary_op.op = op;
        node->binary_op.right = right;
        left = node;
    }

    return left;
}

static KryExprNode* parse_unary(ParserContext* ctx) {
    if (match(ctx, '!')) {
        KryExprNode* operand = parse_unary(ctx);
        if (!operand) return NULL;

        KryExprNode* node = create_node(KRY_EXPR_UNARY_OP);
        node->unary_op.op = KRY_UNARY_OP_NOT;
        node->unary_op.operand = operand;
        return node;
    }

    if (match(ctx, '-')) {
        KryExprNode* operand = parse_unary(ctx);
        if (!operand) return NULL;

        KryExprNode* node = create_node(KRY_EXPR_UNARY_OP);
        node->unary_op.op = KRY_UNARY_OP_NEGATE;
        node->unary_op.operand = operand;
        return node;
    }

    if (match_str(ctx, "typeof")) {
        KryExprNode* operand = parse_unary(ctx);
        if (!operand) return NULL;

        KryExprNode* node = create_node(KRY_EXPR_UNARY_OP);
        node->unary_op.op = KRY_UNARY_OP_TYPEOF;
        node->unary_op.operand = operand;
        return node;
    }

    return parse_member_expr(ctx);
}

static KryExprNode* parse_member_expr(ParserContext* ctx) {
    KryExprNode* left = parse_primary(ctx);

    while (left) {
        // Check for property access: obj.prop or obj["prop"]
        if (match(ctx, '.')) {
            // Parse property name
            size_t start = ctx->pos;
            while (is_identifier_char(peek(ctx))) {
                consume(ctx);
            }
            size_t len = ctx->pos - start;
            if (len == 0) {
                set_error("Expected property name after '.'");
                kry_expr_free(left);
                return NULL;
            }

            char* prop = strndup(ctx->input + start, len);

            // Check if this is followed by => (arrow function)
            size_t save_pos = ctx->pos;
            if (match_str(ctx, "=>")) {
                // This is an arrow function with destructuring or pattern
                ctx->pos = save_pos;
                free(prop);
                break;
            }

            KryExprNode* node = create_node(KRY_EXPR_PROPERTY_ACCESS);
            node->property_access.object = left;
            node->property_access.property = prop;
            node->property_access.is_computed = false;
            left = node;
        }
        // Check for array access: arr[index]
        else if (match(ctx, '[')) {
            KryExprNode* index = parse_expression(ctx);
            if (!index) {
                kry_expr_free(left);
                return NULL;
            }
            if (!match(ctx, ']')) {
                set_error("Expected ']' after array index");
                kry_expr_free(left);
                kry_expr_free(index);
                return NULL;
            }

            KryExprNode* node = create_node(KRY_EXPR_ELEMENT_ACCESS);
            node->element_access.array = left;
            node->element_access.index = index;
            left = node;
        }
        // Check for function call: func(args)
        else if (match(ctx, '(')) {
            // Parse arguments
            KryExprNode** args = NULL;
            size_t arg_count = 0;
            size_t capacity = 4;

            args = (KryExprNode**)malloc(sizeof(KryExprNode*) * capacity);

            while (!match(ctx, ')')) {
                if (arg_count >= capacity) {
                    capacity *= 2;
                    args = (KryExprNode**)realloc(args, sizeof(KryExprNode*) * capacity);
                }

                KryExprNode* arg = parse_expression(ctx);
                if (!arg) {
                    for (size_t i = 0; i < arg_count; i++) {
                        kry_expr_free(args[i]);
                    }
                    free(args);
                    kry_expr_free(left);
                    return NULL;
                }

                args[arg_count++] = arg;

                if (!match(ctx, ',')) {
                    if (peek(ctx) != ')') {
                        set_error("Expected ',' or ')' in argument list");
                        for (size_t i = 0; i < arg_count; i++) {
                            kry_expr_free(args[i]);
                        }
                        free(args);
                        kry_expr_free(left);
                        return NULL;
                    }
                }
            }

            KryExprNode* node = create_node(KRY_EXPR_CALL);
            node->call.callee = left;
            node->call.args = args;
            node->call.arg_count = arg_count;
            left = node;
        }
        // Check for arrow function: params => body
        else if (match_str(ctx, "=>")) {
            // left is the parameter (identifier or pattern)
            KryExprNode* node = parse_arrow_func_params(ctx, left);
            if (!node) {
                kry_expr_free(left);
                return NULL;
            }
            left = node;
        }
        else {
            break;
        }
    }

    return left;
}

static KryExprNode* parse_arrow_func_params(ParserContext* ctx, KryExprNode* left) {
    KryExprNode* node = create_node(KRY_EXPR_ARROW_FUNC);

    // Extract parameter name from identifier
    if (left->type == KRY_EXPR_IDENTIFIER) {
        node->arrow_func.params = (char**)malloc(sizeof(char*));
        node->arrow_func.params[0] = strdup(left->identifier);
        node->arrow_func.param_count = 1;
        kry_expr_free(left);
    } else {
        // For now, only support single identifier params
        set_error("Arrow function parameters must be identifiers");
        kry_expr_free(left);
        kry_expr_free(node);
        return NULL;
    }

    // Parse body (expression or block)
    char ch = peek(ctx);
    if (ch == '{') {
        // Block body: { statements }
        node->arrow_func.is_expression_body = false;
        consume(ctx);  // skip '{'
        size_t body_start = ctx->pos;
        int brace_depth = 1;
        while (brace_depth > 0 && ctx->pos < ctx->length) {
            char c = ctx->input[ctx->pos];
            if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
            if (brace_depth > 0) ctx->pos++;
        }
        size_t body_len = ctx->pos - body_start;
        char* body_text = strndup(ctx->input + body_start, body_len);
        // Create a node to hold block body text
        KryExprNode* body_node = create_node(KRY_EXPR_LITERAL);
        body_node->literal.literal_type = KRY_LITERAL_STRING;
        body_node->literal.string_val = body_text;
        node->arrow_func.body = body_node;
        if (ctx->pos < ctx->length) ctx->pos++;  // skip closing '}'
    } else {
        // Expression body: x * 2
        node->arrow_func.is_expression_body = true;
        node->arrow_func.body = parse_expression(ctx);
        if (!node->arrow_func.body) {
            for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                free(node->arrow_func.params[i]);
            }
            free(node->arrow_func.params);
            kry_expr_free(node);
            return NULL;
        }
    }

    return node;
}

static KryExprNode* parse_primary(ParserContext* ctx) {
    char ch = peek(ctx);

    // String literal
    if (ch == '"' || ch == '\'') {
        char quote = consume(ctx);
        size_t start = ctx->pos;

        while (peek(ctx) != quote && peek(ctx) != '\0') {
            if (peek(ctx) == '\\') {
                consume(ctx); // Skip escape char
            }
            consume(ctx);
        }

        size_t len = ctx->pos - start;
        if (!match(ctx, quote)) {
            set_error("Unterminated string literal");
            return NULL;
        }

        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_STRING;
        node->literal.string_val = strndup(ctx->input + start, len);
        return node;
    }

    // Template string (backtick strings with ${} interpolation)
    if (ch == '`') {
        return parse_template_string(ctx);
    }

    // Number literal
    if (isdigit(ch) || (ch == '.' && isdigit(ctx->input[ctx->pos + 1]))) {
        size_t start = ctx->pos;
        while (isdigit(peek(ctx))) {
            consume(ctx);
        }
        if (match(ctx, '.')) {
            while (isdigit(peek(ctx))) {
                consume(ctx);
            }
        }

        char* num_str = strndup(ctx->input + start, ctx->pos - start);
        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_NUMBER;
        node->literal.number_val = atof(num_str);
        free(num_str);
        return node;
    }

    // Array literal
    if (match(ctx, '[')) {
        return parse_array_literal(ctx);
    }

    // Object literal
    if (match(ctx, '{')) {
        return parse_object_literal(ctx);
    }

    // Parenthesized expression or arrow function with parenthesized params
    if (match(ctx, '(')) {
        // Save position to potentially backtrack
        size_t saved_pos = ctx->pos;

        // Check for parameterless arrow function: () => expr
        if (peek(ctx) == ')') {
            consume(ctx);  // consume ')'
            // Skip whitespace manually to check for =>
            while (ctx->pos < ctx->length && isspace(ctx->input[ctx->pos])) {
                ctx->pos++;
            }
            if (ctx->pos + 1 < ctx->length &&
                ctx->input[ctx->pos] == '=' && ctx->input[ctx->pos + 1] == '>') {
                // This is a parameterless arrow function
                ctx->pos += 2;  // consume '=>'
                KryExprNode* node = create_node(KRY_EXPR_ARROW_FUNC);
                node->arrow_func.params = NULL;
                node->arrow_func.param_count = 0;

                // Parse body (expression or block)
                char ch = peek(ctx);
                if (ch == '{') {
                    // Block body
                    node->arrow_func.is_expression_body = false;
                    consume(ctx);  // skip '{'
                    size_t body_start = ctx->pos;
                    int brace_depth = 1;
                    while (brace_depth > 0 && ctx->pos < ctx->length) {
                        char c = ctx->input[ctx->pos];
                        if (c == '{') brace_depth++;
                        else if (c == '}') brace_depth--;
                        if (brace_depth > 0) ctx->pos++;
                    }
                    size_t body_len = ctx->pos - body_start;
                    // Store raw body text for later processing
                    char* body_text = strndup(ctx->input + body_start, body_len);
                    // Create a special node to hold block body text
                    KryExprNode* body_node = create_node(KRY_EXPR_LITERAL);
                    body_node->literal.literal_type = KRY_LITERAL_STRING;
                    body_node->literal.string_val = body_text;
                    node->arrow_func.body = body_node;
                    if (ctx->pos < ctx->length) ctx->pos++;  // skip closing '}'
                } else {
                    // Expression body
                    node->arrow_func.is_expression_body = true;
                    node->arrow_func.body = parse_expression(ctx);
                    if (!node->arrow_func.body) {
                        kry_expr_free(node);
                        return NULL;
                    }
                }
                return node;
            }
            // Not an arrow function, restore position
            ctx->pos = saved_pos;
        }

        // Check for multi-parameter arrow function: (a, b, ...) => expr
        // Try to parse as parameter list
        char** params = NULL;
        size_t param_count = 0;
        size_t param_capacity = 4;
        params = (char**)malloc(sizeof(char*) * param_capacity);
        bool is_valid_params = true;

        while (is_valid_params && peek(ctx) != ')') {
            // Skip whitespace
            while (ctx->pos < ctx->length && isspace(ctx->input[ctx->pos])) {
                ctx->pos++;
            }

            // Parse identifier
            if (!isalpha(peek(ctx)) && peek(ctx) != '_' && peek(ctx) != '$') {
                is_valid_params = false;
                break;
            }

            size_t id_start = ctx->pos;
            while (ctx->pos < ctx->length && is_identifier_char(ctx->input[ctx->pos])) {
                ctx->pos++;
            }
            size_t id_len = ctx->pos - id_start;

            if (param_count >= param_capacity) {
                param_capacity *= 2;
                params = (char**)realloc(params, sizeof(char*) * param_capacity);
            }
            params[param_count++] = strndup(ctx->input + id_start, id_len);

            // Skip whitespace
            while (ctx->pos < ctx->length && isspace(ctx->input[ctx->pos])) {
                ctx->pos++;
            }

            // Check for comma or end
            if (peek(ctx) == ',') {
                consume(ctx);
            } else if (peek(ctx) != ')') {
                is_valid_params = false;
            }
        }

        if (is_valid_params && peek(ctx) == ')') {
            consume(ctx);  // consume ')'
            // Skip whitespace
            while (ctx->pos < ctx->length && isspace(ctx->input[ctx->pos])) {
                ctx->pos++;
            }
            // Check for =>
            if (ctx->pos + 1 < ctx->length &&
                ctx->input[ctx->pos] == '=' && ctx->input[ctx->pos + 1] == '>') {
                // This is a multi-parameter arrow function
                ctx->pos += 2;  // consume '=>'
                KryExprNode* node = create_node(KRY_EXPR_ARROW_FUNC);
                node->arrow_func.params = params;
                node->arrow_func.param_count = param_count;

                // Parse body (expression or block)
                char ch = peek(ctx);
                if (ch == '{') {
                    // Block body
                    node->arrow_func.is_expression_body = false;
                    consume(ctx);  // skip '{'
                    size_t body_start = ctx->pos;
                    int brace_depth = 1;
                    while (brace_depth > 0 && ctx->pos < ctx->length) {
                        char c = ctx->input[ctx->pos];
                        if (c == '{') brace_depth++;
                        else if (c == '}') brace_depth--;
                        if (brace_depth > 0) ctx->pos++;
                    }
                    size_t body_len = ctx->pos - body_start;
                    char* body_text = strndup(ctx->input + body_start, body_len);
                    KryExprNode* body_node = create_node(KRY_EXPR_LITERAL);
                    body_node->literal.literal_type = KRY_LITERAL_STRING;
                    body_node->literal.string_val = body_text;
                    node->arrow_func.body = body_node;
                    if (ctx->pos < ctx->length) ctx->pos++;  // skip closing '}'
                } else {
                    // Expression body
                    node->arrow_func.is_expression_body = true;
                    node->arrow_func.body = parse_expression(ctx);
                    if (!node->arrow_func.body) {
                        for (size_t i = 0; i < param_count; i++) free(params[i]);
                        free(params);
                        kry_expr_free(node);
                        return NULL;
                    }
                }
                return node;
            }
        }

        // Not an arrow function, free params and restore position
        for (size_t i = 0; i < param_count; i++) {
            free(params[i]);
        }
        free(params);
        ctx->pos = saved_pos;

        // Parse as normal parenthesized expression
        KryExprNode* expr = parse_expression(ctx);
        if (!expr) return NULL;
        if (!match(ctx, ')')) {
            set_error("Expected ')' after expression");
            kry_expr_free(expr);
            return NULL;
        }
        return expr;
    }

    // Keywords
    if (match_str(ctx, "true")) {
        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_BOOLEAN;
        node->literal.bool_val = true;
        return node;
    }

    if (match_str(ctx, "false")) {
        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_BOOLEAN;
        node->literal.bool_val = false;
        return node;
    }

    if (match_str(ctx, "null")) {
        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_NULL;
        return node;
    }

    if (match_str(ctx, "undefined")) {
        KryExprNode* node = create_node(KRY_EXPR_LITERAL);
        node->literal.literal_type = KRY_LITERAL_UNDEFINED;
        return node;
    }

    // Identifier
    if (isalpha(ch) || ch == '_' || ch == '$') {
        size_t start = ctx->pos;
        // Don't use peek() here as it skips whitespace and modifies position
        while (ctx->pos < ctx->length && is_identifier_char(ctx->input[ctx->pos])) {
            ctx->pos++;
        }
        size_t len = ctx->pos - start;

        KryExprNode* node = create_node(KRY_EXPR_IDENTIFIER);
        node->identifier = strndup(ctx->input + start, len);
        return node;
    }

    set_error("Unexpected character in expression");
    return NULL;
}

static KryExprNode* parse_array_literal(ParserContext* ctx) {
    KryExprNode** elements = NULL;
    size_t element_count = 0;
    size_t capacity = 4;

    elements = (KryExprNode**)malloc(sizeof(KryExprNode*) * capacity);

    while (!match(ctx, ']')) {
        if (element_count >= capacity) {
            capacity *= 2;
            elements = (KryExprNode**)realloc(elements, sizeof(KryExprNode*) * capacity);
        }

        KryExprNode* elem = parse_expression(ctx);
        if (!elem) {
            for (size_t i = 0; i < element_count; i++) {
                kry_expr_free(elements[i]);
            }
            free(elements);
            return NULL;
        }

        elements[element_count++] = elem;

        if (!match(ctx, ',')) {
            if (peek(ctx) != ']') {
                set_error("Expected ',' or ']' in array literal");
                for (size_t i = 0; i < element_count; i++) {
                    kry_expr_free(elements[i]);
                }
                free(elements);
                return NULL;
            }
        }
    }

    KryExprNode* node = create_node(KRY_EXPR_ARRAY);
    node->array.elements = elements;
    node->array.element_count = element_count;
    return node;
}

static KryExprNode* parse_object_literal(ParserContext* ctx) {
    char** keys = NULL;
    KryExprNode** values = NULL;
    size_t prop_count = 0;
    size_t capacity = 4;

    keys = (char**)malloc(sizeof(char*) * capacity);
    values = (KryExprNode**)malloc(sizeof(KryExprNode*) * capacity);

    while (!match(ctx, '}')) {
        if (prop_count >= capacity) {
            capacity *= 2;
            keys = (char**)realloc(keys, sizeof(char*) * capacity);
            values = (KryExprNode**)realloc(values, sizeof(KryExprNode*) * capacity);
        }

        // Parse key (identifier or string)
        char ch = peek(ctx);
        char* key = NULL;
        if (ch == '"' || ch == '\'') {
            // String key
            char quote = consume(ctx);
            size_t start = ctx->pos;
            while (peek(ctx) != quote && peek(ctx) != '\0') {
                if (peek(ctx) == '\\') consume(ctx);
                consume(ctx);
            }
            size_t len = ctx->pos - start;
            if (!match(ctx, quote)) {
                set_error("Unterminated string key");
                goto error;
            }
            key = strndup(ctx->input + start, len);
        } else {
            // Identifier key
            size_t start = ctx->pos;
            while (is_identifier_char(peek(ctx))) {
                consume(ctx);
            }
            size_t len = ctx->pos - start;
            if (len == 0) {
                set_error("Expected object key");
                goto error;
            }
            key = strndup(ctx->input + start, len);
        }

        if (!match(ctx, ':')) {
            set_error("Expected ':' after object key");
            free(key);
            goto error;
        }

        KryExprNode* value = parse_expression(ctx);
        if (!value) {
            free(key);
            goto error;
        }

        keys[prop_count] = key;
        values[prop_count] = value;
        prop_count++;

        if (!match(ctx, ',')) {
            if (peek(ctx) != '}') {
                set_error("Expected ',' or '}' in object literal");
                goto error;
            }
        }
    }

    KryExprNode* node = create_node(KRY_EXPR_OBJECT);
    node->object.keys = keys;
    node->object.values = values;
    node->object.prop_count = prop_count;
    return node;

error:
    for (size_t i = 0; i < prop_count; i++) {
        free(keys[i]);
        kry_expr_free(values[i]);
    }
    free(keys);
    free(values);
    return NULL;
}

/**
 * Parse template string (backtick strings with ${} interpolation)
 * Syntax: `literal ${expr} literal ${expr}`
 * Example: `Hello ${name}!` → parts=["Hello ", "!"], expressions=[name]
 */
static KryExprNode* parse_template_string(ParserContext* ctx) {
    if (!match(ctx, '`')) {
        set_error("Expected '`' at start of template string");
        return NULL;
    }

    KryExprNode* node = create_node(KRY_EXPR_TEMPLATE);
    if (!node) {
        return NULL;
    }

    // Initialize arrays for parts and expressions
    size_t part_capacity = 4;
    size_t expr_capacity = 4;
    char** parts = (char**)malloc(sizeof(char*) * part_capacity);
    KryExprNode** expressions = (KryExprNode**)malloc(sizeof(KryExprNode*) * expr_capacity);

    if (!parts || !expressions) {
        set_error("Out of memory");
        if (parts) free(parts);
        if (expressions) free(expressions);
        kry_expr_free(node);
        return NULL;
    }

    size_t part_count = 0;
    size_t expr_count = 0;

    size_t lit_start = ctx->pos;
    bool in_interp = false;

    while (ctx->pos < ctx->length) {
        if (!in_interp) {
            // Looking for ${ or closing `
            if (ctx->pos + 1 < ctx->length &&
                ctx->input[ctx->pos] == '$' &&
                ctx->input[ctx->pos + 1] == '{') {

                // Save literal part before ${
                size_t lit_len = ctx->pos - lit_start;
                char* lit = strndup(ctx->input + lit_start, lit_len);
                if (!lit) goto error;

                // Expand parts array if needed
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    char** new_parts = (char**)realloc(parts, sizeof(char*) * part_capacity);
                    if (!new_parts) {
                        free(lit);
                        goto error;
                    }
                    parts = new_parts;
                }
                parts[part_count++] = lit;

                // Skip ${
                ctx->pos += 2;
                in_interp = true;

                // Parse expression inside ${}
                KryExprNode* expr = parse_expression(ctx);
                if (!expr) {
                    set_error("Failed to parse template expression");
                    goto error;
                }

                // Expand expressions array if needed
                if (expr_count >= expr_capacity) {
                    expr_capacity *= 2;
                    KryExprNode** new_exprs = (KryExprNode**)realloc(expressions, sizeof(KryExprNode*) * expr_capacity);
                    if (!new_exprs) {
                        kry_expr_free(expr);
                        goto error;
                    }
                    expressions = new_exprs;
                }
                expressions[expr_count++] = expr;

                // Expect closing }
                if (!match(ctx, '}')) {
                    set_error("Expected '}' after template expression");
                    goto error;
                }

                // Reset interpolation mode and continue parsing literal
                in_interp = false;
                lit_start = ctx->pos;
            } else if (ctx->input[ctx->pos] == '`') {
                // End of template string
                consume(ctx);  // consume closing `

                // Save final literal part
                size_t lit_len = ctx->pos - 1 - lit_start;
                char* lit = strndup(ctx->input + lit_start, lit_len);
                if (!lit) goto error;

                // Expand parts array if needed
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    char** new_parts = (char**)realloc(parts, sizeof(char*) * part_capacity);
                    if (!new_parts) {
                        free(lit);
                        goto error;
                    }
                    parts = new_parts;
                }
                parts[part_count++] = lit;

                // Success - set node values
                node->template_str.parts = parts;
                node->template_str.part_count = part_count;
                node->template_str.expressions = expressions;
                node->template_str.expr_count = expr_count;

                return node;
            } else {
                // Regular character - consume it
                consume(ctx);
            }
        } else {
            // Should not happen - in_interp should be reset after }
            consume(ctx);
        }
    }

    // If we get here, the string was not terminated
    set_error("Unterminated template string");

error:
    // Cleanup on error
    for (size_t i = 0; i < part_count; i++) {
        free(parts[i]);
    }
    for (size_t i = 0; i < expr_count; i++) {
        kry_expr_free(expressions[i]);
    }
    free(parts);
    free(expressions);
    kry_expr_free(node);
    return NULL;
}

// ============================================================================
// Code Generation
// ============================================================================

typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} CodeBuffer;

static void buffer_init(CodeBuffer* buf) {
    buf->capacity = 256;
    buf->size = 0;
    buf->buffer = (char*)malloc(buf->capacity);
    buf->buffer[0] = '\0';
}

static void buffer_append(CodeBuffer* buf, const char* str) {
    size_t len = strlen(str);
    if (buf->size + len >= buf->capacity) {
        while (buf->size + len >= buf->capacity) {
            buf->capacity *= 2;
        }
        buf->buffer = (char*)realloc(buf->buffer, buf->capacity);
    }
    strcpy(buf->buffer + buf->size, str);
    buf->size += len;
}

static void buffer_append_char(CodeBuffer* buf, char c) {
    char str[2] = {c, '\0'};
    buffer_append(buf, str);
}

static char* buffer_extract(CodeBuffer* buf) {
    return buf->buffer;
}

static void generate_lua(KryExprNode* node, CodeBuffer* out);
static void generate_javascript(KryExprNode* node, CodeBuffer* out);
static void generate_hare(KryExprNode* node, CodeBuffer* out);

static void generate_lua_literal(KryExprNode* node, CodeBuffer* out) {
    switch (node->literal.literal_type) {
        case KRY_LITERAL_STRING:
            buffer_append_char(out, '"');
            buffer_append(out, node->literal.string_val);
            buffer_append_char(out, '"');
            break;
        case KRY_LITERAL_NUMBER: {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%g", node->literal.number_val);
            buffer_append(out, num_buf);
            break;
        }
        case KRY_LITERAL_BOOLEAN: {
            buffer_append(out, node->literal.bool_val ? "true" : "false");
            break;
        }
        case KRY_LITERAL_NULL:
        case KRY_LITERAL_UNDEFINED: {
            buffer_append(out, "nil");
            break;
        }
        default:
            break;
    }
}

static void generate_javascript_literal(KryExprNode* node, CodeBuffer* out) {
    switch (node->literal.literal_type) {
        case KRY_LITERAL_STRING:
            buffer_append_char(out, '"');
            buffer_append(out, node->literal.string_val);
            buffer_append_char(out, '"');
            break;
        case KRY_LITERAL_NUMBER: {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%g", node->literal.number_val);
            buffer_append(out, num_buf);
            break;
        }
        case KRY_LITERAL_BOOLEAN:
            buffer_append(out, node->literal.bool_val ? "true" : "false");
            break;
        case KRY_LITERAL_NULL:
            buffer_append(out, "null");
            break;
        case KRY_LITERAL_UNDEFINED:
            buffer_append(out, "undefined");
            break;
    }
}

static void generate_c_literal(KryExprNode* node, CodeBuffer* out) {
    switch (node->literal.literal_type) {
        case KRY_LITERAL_STRING: {
            buffer_append_char(out, '"');
            // Escape special characters for C strings
            const char* s = node->literal.string_val;
            while (*s) {
                switch (*s) {
                    case '"':  buffer_append(out, "\\\""); break;
                    case '\\': buffer_append(out, "\\\\"); break;
                    case '\n': buffer_append(out, "\\n"); break;
                    case '\t': buffer_append(out, "\\t"); break;
                    case '\r': buffer_append(out, "\\r"); break;
                    default:   buffer_append_char(out, *s); break;
                }
                s++;
            }
            buffer_append_char(out, '"');
            break;
        }
        case KRY_LITERAL_NUMBER: {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%g", node->literal.number_val);
            buffer_append(out, num_buf);
            break;
        }
        case KRY_LITERAL_BOOLEAN:
            buffer_append(out, node->literal.bool_val ? "true" : "false");
            break;
        case KRY_LITERAL_NULL:
        case KRY_LITERAL_UNDEFINED:
            buffer_append(out, "NULL");
            break;
    }
}

static void generate_hare_literal(KryExprNode* node, CodeBuffer* out) {
    switch (node->literal.literal_type) {
        case KRY_LITERAL_STRING: {
            buffer_append_char(out, '"');
            // Escape special characters for Hare strings
            const char* s = node->literal.string_val;
            while (*s) {
                switch (*s) {
                    case '"':  buffer_append(out, "\\\""); break;
                    case '\\': buffer_append(out, "\\\\"); break;
                    case '\n': buffer_append(out, "\\n"); break;
                    case '\t': buffer_append(out, "\\t"); break;
                    case '\r': buffer_append(out, "\\r"); break;
                    default:   buffer_append_char(out, *s); break;
                }
                s++;
            }
            buffer_append_char(out, '"');
            break;
        }
        case KRY_LITERAL_NUMBER: {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%g", node->literal.number_val);
            buffer_append(out, num_buf);
            break;
        }
        case KRY_LITERAL_BOOLEAN:
            buffer_append(out, node->literal.bool_val ? "true" : "false");
            break;
        case KRY_LITERAL_NULL:
        case KRY_LITERAL_UNDEFINED:
            buffer_append(out, "void");
            break;
    }
}

static const char* binary_op_to_lua(KryBinaryOp op) {
    switch (op) {
        case KRY_BIN_OP_ADD: return " + ";
        case KRY_BIN_OP_SUB: return " - ";
        case KRY_BIN_OP_MUL: return " * ";
        case KRY_BIN_OP_DIV: return " / ";
        case KRY_BIN_OP_MOD: return " % ";
        case KRY_BIN_OP_EQ: return " == ";
        case KRY_BIN_OP_NEQ: return " ~= ";
        case KRY_BIN_OP_LT: return " < ";
        case KRY_BIN_OP_GT: return " > ";
        case KRY_BIN_OP_LTE: return " <= ";
        case KRY_BIN_OP_GTE: return " >= ";
        case KRY_BIN_OP_AND: return " and ";
        case KRY_BIN_OP_OR: return " or ";
        case KRY_BIN_OP_ASSIGN: return " = ";
    }
    return "?";
}

static const char* binary_op_to_javascript(KryBinaryOp op) {
    switch (op) {
        case KRY_BIN_OP_ADD: return " + ";
        case KRY_BIN_OP_SUB: return " - ";
        case KRY_BIN_OP_MUL: return " * ";
        case KRY_BIN_OP_DIV: return " / ";
        case KRY_BIN_OP_MOD: return " % ";
        case KRY_BIN_OP_EQ: return " == ";
        case KRY_BIN_OP_NEQ: return " != ";
        case KRY_BIN_OP_LT: return " < ";
        case KRY_BIN_OP_GT: return " > ";
        case KRY_BIN_OP_LTE: return " <= ";
        case KRY_BIN_OP_GTE: return " >= ";
        case KRY_BIN_OP_AND: return " && ";
        case KRY_BIN_OP_OR: return " || ";
        case KRY_BIN_OP_ASSIGN: return " = ";
    }
    return "?";
}

static const char* binary_op_to_c(KryBinaryOp op) {
    switch (op) {
        case KRY_BIN_OP_ADD: return " + ";
        case KRY_BIN_OP_SUB: return " - ";
        case KRY_BIN_OP_MUL: return " * ";
        case KRY_BIN_OP_DIV: return " / ";
        case KRY_BIN_OP_MOD: return " % ";
        case KRY_BIN_OP_EQ: return " == ";
        case KRY_BIN_OP_NEQ: return " != ";   // C uses != (like JS, not Lua's ~=)
        case KRY_BIN_OP_LT: return " < ";
        case KRY_BIN_OP_GT: return " > ";
        case KRY_BIN_OP_LTE: return " <= ";
        case KRY_BIN_OP_GTE: return " >= ";
        case KRY_BIN_OP_AND: return " && ";   // C uses && (like JS, not Lua's "and")
        case KRY_BIN_OP_OR: return " || ";    // C uses || (like JS, not Lua's "or")
        case KRY_BIN_OP_ASSIGN: return " = ";
    }
    return "?";
}

static const char* binary_op_to_hare(KryBinaryOp op) {
    switch (op) {
        case KRY_BIN_OP_ADD: return " + ";
        case KRY_BIN_OP_SUB: return " - ";
        case KRY_BIN_OP_MUL: return " * ";
        case KRY_BIN_OP_DIV: return " / ";
        case KRY_BIN_OP_MOD: return " % ";
        case KRY_BIN_OP_EQ: return " == ";
        case KRY_BIN_OP_NEQ: return " != ";
        case KRY_BIN_OP_LT: return " < ";
        case KRY_BIN_OP_GT: return " > ";
        case KRY_BIN_OP_LTE: return " <= ";
        case KRY_BIN_OP_GTE: return " >= ";
        case KRY_BIN_OP_AND: return " && ";
        case KRY_BIN_OP_OR: return " || ";
        case KRY_BIN_OP_ASSIGN: return " = ";
    }
    return "?";
}

static void generate_lua(KryExprNode* node, CodeBuffer* out) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            generate_lua_literal(node, out);
            break;

        case KRY_EXPR_IDENTIFIER:
            buffer_append(out, node->identifier);
            break;

        case KRY_EXPR_BINARY_OP:
            buffer_append_char(out, '(');
            generate_lua(node->binary_op.left, out);
            buffer_append(out, binary_op_to_lua(node->binary_op.op));
            generate_lua(node->binary_op.right, out);
            buffer_append_char(out, ')');
            break;

        case KRY_EXPR_UNARY_OP:
            if (node->unary_op.op == KRY_UNARY_OP_NOT) {
                buffer_append(out, "not ");
            } else if (node->unary_op.op == KRY_UNARY_OP_NEGATE) {
                buffer_append_char(out, '-');
            }
            generate_lua(node->unary_op.operand, out);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            generate_lua(node->property_access.object, out);
            // In Lua, we use dot notation directly
            buffer_append_char(out, '.');
            buffer_append(out, node->property_access.property);
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            generate_lua(node->element_access.array, out);
            // Lua arrays are 1-indexed and use [] notation directly
            buffer_append_char(out, '[');
            // Convert 0-index to 1-index for Lua
            buffer_append(out, "(");
            generate_lua(node->element_access.index, out);
            buffer_append(out, " + 1)");
            buffer_append_char(out, ']');
            break;

        case KRY_EXPR_CALL: {
            generate_lua(node->call.callee, out);
            buffer_append_char(out, '(');
            for (size_t i = 0; i < node->call.arg_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_lua(node->call.args[i], out);
            }
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_ARRAY: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->array.element_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_lua(node->array.elements[i], out);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_OBJECT: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->object.prop_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->object.keys[i]);
                buffer_append(out, " = ");
                generate_lua(node->object.values[i], out);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_ARROW_FUNC: {
            // Lua function syntax
            buffer_append(out, "function(");
            for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->arrow_func.params[i]);
            }
            buffer_append(out, ") ");
            if (node->arrow_func.is_expression_body) {
                buffer_append(out, "return ");
                generate_lua(node->arrow_func.body, out);
                buffer_append(out, "; end");
            } else {
                // Block body - stored as literal string in body node
                if (node->arrow_func.body &&
                    node->arrow_func.body->type == KRY_EXPR_LITERAL &&
                    node->arrow_func.body->literal.literal_type == KRY_LITERAL_STRING) {
                    buffer_append(out, node->arrow_func.body->literal.string_val);
                }
                buffer_append(out, " end");
            }
            break;
        }

        case KRY_EXPR_CONDITIONAL: {
            // Lua doesn't have ternary - use: (condition and consequent) or alternate
            // Note: This has edge cases with falsy values, but works for most cases
            buffer_append(out, "((");
            generate_lua(node->conditional.condition, out);
            buffer_append(out, ") and (");
            generate_lua(node->conditional.consequent, out);
            buffer_append(out, ") or (");
            generate_lua(node->conditional.alternate, out);
            buffer_append(out, "))");
            break;
        }

        case KRY_EXPR_TEMPLATE: {
            // Lua doesn't have native template strings - use string concatenation
            // `Hello ${name}!` → "Hello " .. name .. "!"
            for (size_t i = 0; i < node->template_str.part_count; i++) {
                // Add literal part
                buffer_append_char(out, '"');
                buffer_append(out, node->template_str.parts[i]);
                buffer_append_char(out, '"');

                // Add concatenation operator if more parts coming
                if (i < node->template_str.expr_count) {
                    buffer_append(out, " .. ");
                }
            }

            // Add expressions with concatenation
            for (size_t i = 0; i < node->template_str.expr_count; i++) {
                generate_lua(node->template_str.expressions[i], out);

                // Add concatenation operator if not last expression
                if (i < node->template_str.expr_count - 1) {
                    buffer_append(out, " .. ");
                }
            }
            break;
        }

        default:
            buffer_append(out, "/* TODO */");
            break;
    }
}

static void generate_javascript(KryExprNode* node, CodeBuffer* out) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            generate_javascript_literal(node, out);
            break;

        case KRY_EXPR_IDENTIFIER:
            buffer_append(out, node->identifier);
            break;

        case KRY_EXPR_BINARY_OP:
            buffer_append_char(out, '(');
            generate_javascript(node->binary_op.left, out);
            buffer_append(out, binary_op_to_javascript(node->binary_op.op));
            generate_javascript(node->binary_op.right, out);
            buffer_append_char(out, ')');
            break;

        case KRY_EXPR_UNARY_OP:
            if (node->unary_op.op == KRY_UNARY_OP_NOT) {
                buffer_append_char(out, '!');
            } else if (node->unary_op.op == KRY_UNARY_OP_NEGATE) {
                buffer_append_char(out, '-');
            } else if (node->unary_op.op == KRY_UNARY_OP_TYPEOF) {
                buffer_append(out, "typeof ");
            }
            generate_javascript(node->unary_op.operand, out);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            generate_javascript(node->property_access.object, out);
            buffer_append_char(out, '.');
            buffer_append(out, node->property_access.property);
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            generate_javascript(node->element_access.array, out);
            buffer_append_char(out, '[');
            generate_javascript(node->element_access.index, out);
            buffer_append_char(out, ']');
            break;

        case KRY_EXPR_CALL: {
            generate_javascript(node->call.callee, out);
            buffer_append_char(out, '(');
            for (size_t i = 0; i < node->call.arg_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_javascript(node->call.args[i], out);
            }
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_ARRAY: {
            buffer_append_char(out, '[');
            for (size_t i = 0; i < node->array.element_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_javascript(node->array.elements[i], out);
            }
            buffer_append_char(out, ']');
            break;
        }

        case KRY_EXPR_OBJECT: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->object.prop_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->object.keys[i]);
                buffer_append(out, ": ");
                generate_javascript(node->object.values[i], out);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_ARROW_FUNC: {
            buffer_append_char(out, '(');
            for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->arrow_func.params[i]);
            }
            buffer_append_char(out, ')');
            buffer_append(out, " => ");
            if (node->arrow_func.is_expression_body) {
                generate_javascript(node->arrow_func.body, out);
            } else {
                // Block body - stored as literal string in body node
                buffer_append_char(out, '{');
                if (node->arrow_func.body &&
                    node->arrow_func.body->type == KRY_EXPR_LITERAL &&
                    node->arrow_func.body->literal.literal_type == KRY_LITERAL_STRING) {
                    buffer_append(out, node->arrow_func.body->literal.string_val);
                }
                buffer_append_char(out, '}');
            }
            break;
        }

        case KRY_EXPR_CONDITIONAL: {
            // JavaScript native ternary
            buffer_append_char(out, '(');
            generate_javascript(node->conditional.condition, out);
            buffer_append(out, " ? ");
            generate_javascript(node->conditional.consequent, out);
            buffer_append(out, " : ");
            generate_javascript(node->conditional.alternate, out);
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_TEMPLATE: {
            // JavaScript natively supports template strings - reconstruct original format
            // `Hello ${name}!` → `Hello ${name}!`
            buffer_append_char(out, '`');

            for (size_t i = 0; i < node->template_str.part_count; i++) {
                // Add literal part
                buffer_append(out, node->template_str.parts[i]);

                // Add interpolated expression
                if (i < node->template_str.expr_count) {
                    buffer_append_char(out, '$');
                    buffer_append_char(out, '{');
                    generate_javascript(node->template_str.expressions[i], out);
                    buffer_append_char(out, '}');
                }
            }

            buffer_append_char(out, '`');
            break;
        }

        default:
            buffer_append(out, "/* TODO */");
            break;
    }
}

static void generate_c_with_opts(KryExprNode* node, CodeBuffer* out, KryExprOptions* options);

static void generate_c(KryExprNode* node, CodeBuffer* out) {
    generate_c_with_opts(node, out, NULL);
}

static void generate_c_with_opts(KryExprNode* node, CodeBuffer* out, KryExprOptions* options) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            generate_c_literal(node, out);
            break;

        case KRY_EXPR_IDENTIFIER:
            buffer_append(out, node->identifier);
            break;

        case KRY_EXPR_BINARY_OP:
            buffer_append_char(out, '(');
            generate_c_with_opts(node->binary_op.left, out, options);
            buffer_append(out, binary_op_to_c(node->binary_op.op));
            generate_c_with_opts(node->binary_op.right, out, options);
            buffer_append_char(out, ')');
            break;

        case KRY_EXPR_UNARY_OP:
            if (node->unary_op.op == KRY_UNARY_OP_NOT) {
                buffer_append_char(out, '!');
            } else if (node->unary_op.op == KRY_UNARY_OP_NEGATE) {
                buffer_append_char(out, '-');
            } else if (node->unary_op.op == KRY_UNARY_OP_TYPEOF) {
                // C has no runtime typeof - emit warning comment
                buffer_append(out, "/* typeof unsupported */ 0");
                return;
            }
            generate_c_with_opts(node->unary_op.operand, out, options);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            generate_c_with_opts(node->property_access.object, out, options);
            buffer_append_char(out, '.');
            buffer_append(out, node->property_access.property);
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            generate_c_with_opts(node->element_access.array, out, options);
            buffer_append_char(out, '[');
            // C arrays are 0-indexed (like JS) - NO +1 adjustment like Lua!
            generate_c_with_opts(node->element_access.index, out, options);
            buffer_append_char(out, ']');
            break;

        case KRY_EXPR_CALL: {
            generate_c_with_opts(node->call.callee, out, options);
            buffer_append_char(out, '(');
            for (size_t i = 0; i < node->call.arg_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_c_with_opts(node->call.args[i], out, options);
            }
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_ARRAY: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->array.element_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_c_with_opts(node->array.elements[i], out, options);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_OBJECT: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->object.prop_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append_char(out, '.');
                buffer_append(out, node->object.keys[i]);
                buffer_append(out, " = ");
                generate_c_with_opts(node->object.values[i], out, options);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_ARROW_FUNC: {
            // Check if we have a registry for deferred generation
            if (options && options->arrow_registry) {
                // Analyze captures
                char** captures = NULL;
                size_t cap_count = 0;
                kry_arrow_get_captures(node, &captures, &cap_count);

                // Transpile body to C (recursively, without registry to avoid infinite loop)
                KryExprOptions body_opts = {KRY_TARGET_C, false, 0, NULL, NULL};
                char* body_code = kry_expr_transpile(node->arrow_func.body, &body_opts, NULL);

                // Register the arrow function
                uint32_t id = kry_arrow_register(
                    options->arrow_registry,
                    (const char**)node->arrow_func.params,
                    node->arrow_func.param_count,
                    body_code,
                    (const char**)captures,
                    cap_count,
                    options->context_hint,
                    node->arrow_func.is_expression_body
                );

                free(body_code);

                // Get the registered arrow function to retrieve its actual name
                KryArrowDef* def = kry_arrow_get(options->arrow_registry, id);
                const char* func_name = def ? def->name : "kry_arrow_unknown";

                // Emit function pointer reference
                char buf[128];
                if (cap_count == 0) {
                    // No captures - just function pointer
                    buffer_append(out, func_name);
                } else {
                    // Has captures - emit callback struct
                    snprintf(buf, sizeof(buf), "(KryCallback){.func = %s, .ctx = &_ctx_%u}", func_name, id);
                    buffer_append(out, buf);
                }

                // Free captures
                if (captures) {
                    for (size_t i = 0; i < cap_count; i++) {
                        free(captures[i]);
                    }
                    free(captures);
                }
            } else {
                // No registry - emit as comment placeholder (backward compatibility)
                buffer_append(out, "/* arrow function: (");
                for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                    if (i > 0) buffer_append(out, ", ");
                    buffer_append(out, node->arrow_func.params[i]);
                }
                buffer_append(out, ") => ... */ NULL");
            }
            break;
        }

        case KRY_EXPR_CONDITIONAL: {
            // C native ternary (same syntax as JavaScript)
            buffer_append_char(out, '(');
            generate_c_with_opts(node->conditional.condition, out, options);
            buffer_append(out, " ? ");
            generate_c_with_opts(node->conditional.consequent, out, options);
            buffer_append(out, " : ");
            generate_c_with_opts(node->conditional.alternate, out, options);
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_MEMBER_EXPR:
            generate_c_with_opts(node->member_expr.object, out, options);
            buffer_append_char(out, '.');
            buffer_append(out, node->member_expr.member);
            break;

        case KRY_EXPR_TEMPLATE: {
            // C doesn't have native template strings - use snprintf-style format
            // For now, use string concatenation
            // `Hello ${name}!` → "strcat("Hello ", name, "!")"

            buffer_append(out, "strcat(");

            // Add literal parts and expressions
            for (size_t i = 0; i < node->template_str.part_count; i++) {
                // Add literal part
                buffer_append_char(out, '"');
                // TODO: escape special characters for C strings
                buffer_append(out, node->template_str.parts[i]);
                buffer_append_char(out, '"');

                // Add comma if more parts coming
                if (i < node->template_str.expr_count) {
                    buffer_append(out, ", ");
                }
            }

            // Add expressions
            for (size_t i = 0; i < node->template_str.expr_count; i++) {
                generate_c_with_opts(node->template_str.expressions[i], out, options);

                // Add comma if not last expression
                if (i < node->template_str.expr_count - 1) {
                    buffer_append(out, ", ");
                }
            }

            buffer_append_char(out, ')');
            break;
        }

        default:
            buffer_append(out, "/* unsupported expression */");
            break;
    }
}

static void generate_hare(KryExprNode* node, CodeBuffer* out) {
    if (!node) return;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            generate_hare_literal(node, out);
            break;

        case KRY_EXPR_IDENTIFIER:
            buffer_append(out, node->identifier);
            break;

        case KRY_EXPR_BINARY_OP:
            buffer_append_char(out, '(');
            generate_hare(node->binary_op.left, out);
            buffer_append(out, binary_op_to_hare(node->binary_op.op));
            generate_hare(node->binary_op.right, out);
            buffer_append_char(out, ')');
            break;

        case KRY_EXPR_UNARY_OP:
            if (node->unary_op.op == KRY_UNARY_OP_NOT) {
                buffer_append_char(out, '!');
            } else if (node->unary_op.op == KRY_UNARY_OP_NEGATE) {
                buffer_append_char(out, '-');
            } else if (node->unary_op.op == KRY_UNARY_OP_TYPEOF) {
                buffer_append(out, "/* typeof unsupported */ void");
                return;
            }
            generate_hare(node->unary_op.operand, out);
            break;

        case KRY_EXPR_PROPERTY_ACCESS:
            generate_hare(node->property_access.object, out);
            buffer_append_char(out, '.');
            buffer_append(out, node->property_access.property);
            break;

        case KRY_EXPR_ELEMENT_ACCESS:
            generate_hare(node->element_access.array, out);
            buffer_append_char(out, '[');
            generate_hare(node->element_access.index, out);
            buffer_append_char(out, ']');
            break;

        case KRY_EXPR_CALL: {
            generate_hare(node->call.callee, out);
            buffer_append_char(out, '(');
            for (size_t i = 0; i < node->call.arg_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_hare(node->call.args[i], out);
            }
            buffer_append_char(out, ')');
            break;
        }

        case KRY_EXPR_ARRAY: {
            buffer_append_char(out, '[');
            for (size_t i = 0; i < node->array.element_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                generate_hare(node->array.elements[i], out);
            }
            buffer_append_char(out, ']');
            break;
        }

        case KRY_EXPR_OBJECT: {
            buffer_append_char(out, '{');
            for (size_t i = 0; i < node->object.prop_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->object.keys[i]);
                buffer_append(out, " = ");
                generate_hare(node->object.values[i], out);
            }
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_ARROW_FUNC: {
            // Hare fn syntax
            buffer_append(out, "fn(");
            for (size_t i = 0; i < node->arrow_func.param_count; i++) {
                if (i > 0) buffer_append(out, ", ");
                buffer_append(out, node->arrow_func.params[i]);
            }
            buffer_append(out, ") ");
            if (node->arrow_func.is_expression_body) {
                buffer_append(out, "= ");
                generate_hare(node->arrow_func.body, out);
            } else {
                // Block body
                buffer_append_char(out, '{');
                if (node->arrow_func.body &&
                    node->arrow_func.body->type == KRY_EXPR_LITERAL &&
                    node->arrow_func.body->literal.literal_type == KRY_LITERAL_STRING) {
                    buffer_append(out, node->arrow_func.body->literal.string_val);
                }
                buffer_append_char(out, '}');
            }
            break;
        }

        case KRY_EXPR_CONDITIONAL: {
            // Hare match expression for ternary (no native ternary)
            buffer_append(out, "match (");
            generate_hare(node->conditional.condition, out);
            buffer_append(out, ") {");
            buffer_append(out, "case true => ");
            generate_hare(node->conditional.consequent, out);
            buffer_append(out, ", ");
            buffer_append(out, "case => ");
            generate_hare(node->conditional.alternate, out);
            buffer_append_char(out, '}');
            break;
        }

        case KRY_EXPR_MEMBER_EXPR:
            generate_hare(node->member_expr.object, out);
            buffer_append_char(out, '.');
            buffer_append(out, node->member_expr.member);
            break;

        case KRY_EXPR_TEMPLATE: {
            // Transpile to Hare fmt::fprintf
            // `Hello ${name}!` → fmt::fprintf(stout, "Hello {}!", name)

            buffer_append(out, "fmt::fprintf(stout, \"");

            // Build format string with {} placeholders
            for (size_t i = 0; i < node->template_str.part_count; i++) {
                // Escape literal parts for Hare strings
                const char* lit = node->template_str.parts[i];
                for (size_t j = 0; lit[j]; j++) {
                    switch (lit[j]) {
                        case '"':  buffer_append(out, "\\\""); break;
                        case '\\': buffer_append(out, "\\\\"); break;
                        case '\n': buffer_append(out, "\\n"); break;
                        case '\t': buffer_append(out, "\\t"); break;
                        case '\r': buffer_append(out, "\\r"); break;
                        case '{':  buffer_append(out, "{{"); break;  // Escape {
                        case '}':  buffer_append(out, "}}"); break;  // Escape }
                        default: {
                            char buf[2] = {lit[j], '\0'};
                            buffer_append(out, buf);
                        }
                    }
                }

                // Add placeholder if not last part
                if (i < node->template_str.expr_count) {
                    buffer_append(out, "{}");
                }
            }

            buffer_append(out, "\"");

            // Add arguments
            for (size_t i = 0; i < node->template_str.expr_count; i++) {
                buffer_append(out, ", ");
                generate_hare(node->template_str.expressions[i], out);
            }

            buffer_append(out, ")");
            break;
        }

        default:
            buffer_append(out, "/* unsupported expression */");
            break;
    }
}

char* kry_expr_transpile(KryExprNode* node, KryExprOptions* options, size_t* output_length) {
    if (!node) {
        set_error("Null expression node");
        return NULL;
    }

    KryExprOptions default_opts = {KRY_TARGET_JAVASCRIPT, false, 0, NULL, NULL};
    if (!options) {
        options = &default_opts;
    }

    CodeBuffer buf;
    buffer_init(&buf);

    if (options->target == KRY_TARGET_LUA) {
        generate_lua(node, &buf);
    } else if (options->target == KRY_TARGET_C) {
        generate_c_with_opts(node, &buf, options);
    } else if (options->target == KRY_TARGET_HARE) {
        generate_hare(node, &buf);
    } else {
        generate_javascript(node, &buf);
    }

    if (output_length) {
        *output_length = buf.size;
    }

    return buffer_extract(&buf);
}

char* kry_expr_transpile_source(const char* source, KryExprTarget target, size_t* output_length) {
    char* error = NULL;
    KryExprNode* node = kry_expr_parse(source, &error);
    if (!node) {
        set_error(error);
        free(error);
        return NULL;
    }

    KryExprOptions options = {target, false, 0, NULL, NULL};
    char* result = kry_expr_transpile(node, &options, output_length);
    kry_expr_free(node);
    return result;
}
