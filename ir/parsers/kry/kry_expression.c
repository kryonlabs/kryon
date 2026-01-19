/**
 * KRY Expression Transpiler Implementation
 *
 * Transpiles ES6-style expressions to both Lua and JavaScript.
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_expression.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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
    return parse_assignment(ctx);
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
        // Block body: { return x; }
        node->arrow_func.is_expression_body = false;
        // TODO: Parse block statement
        set_error("Block arrow functions not yet implemented");
        kry_expr_free(node);
        return NULL;
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

    // Parenthesized expression
    if (match(ctx, '(')) {
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
                // Block body - not yet implemented
                buffer_append(out, "end");
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
                // Block body - not yet implemented
                buffer_append(out, "{ }");
            }
            break;
        }

        default:
            buffer_append(out, "/* TODO */");
            break;
    }
}

char* kry_expr_transpile(KryExprNode* node, KryExprOptions* options, size_t* output_length) {
    if (!node) {
        set_error("Null expression node");
        return NULL;
    }

    KryExprOptions default_opts = {KRY_TARGET_JAVASCRIPT, false, 0};
    if (!options) {
        options = &default_opts;
    }

    CodeBuffer buf;
    buffer_init(&buf);

    if (options->target == KRY_TARGET_LUA) {
        generate_lua(node, &buf);
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

    KryExprOptions options = {target, false, 0};
    char* result = kry_expr_transpile(node, &options, output_length);
    kry_expr_free(node);
    return result;
}
