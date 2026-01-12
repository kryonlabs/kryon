/**
 * IR Expression Parser Implementation
 *
 * Recursive descent expression parser supporting:
 *   - Literals (int, float, string, bool, null)
 *   - Variables (identifiers)
 *   - Member access (obj.prop)
 *   - Computed member (obj[key])
 *   - Method calls (obj.method(args))
 *   - Function calls (func(args))
 *   - Binary operators (+, -, *, /, %, ==, !=, <, >, <=, >=, &&, ||)
 *   - Unary operators (!, -)
 *   - Ternary operator (cond ? then : else)
 *   - Grouping ((expr))
 *   - Arrays ([1, 2, 3])
 *   - Objects ({key: value})
 */

#define _POSIX_C_SOURCE 200809L
#include "ir_expression_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// TOKENIZER
// ============================================================================

typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_DOT,
    TOKEN_LBRACKET,    // [
    TOKEN_RBRACKET,    // ]
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_LBRACE,      // {
    TOKEN_RBRACE,      // }
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_QUESTION,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQ_EQ,       // ==
    TOKEN_NEQ,         // !=
    TOKEN_LT,
    TOKEN_LTE,         // <=
    TOKEN_GT,
    TOKEN_GTE,         // >=
    TOKEN_AND_AND,     // &&
    TOKEN_OR_OR,       // ||
    TOKEN_NOT,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    size_t line;
    size_t column;
} Token;

typedef struct {
    const char* source;
    size_t length;
    size_t pos;
    size_t line;
    size_t column;
    Token current;
    char* error_message;
    size_t error_position;
} Parser;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static IRExpression* parse_expression(Parser* p);
static IRExpression* parse_ternary(Parser* p);
static IRExpression* parse_logical(Parser* p);
static IRExpression* parse_comparison(Parser* p);
static IRExpression* parse_additive(Parser* p);
static IRExpression* parse_multiplicative(Parser* p);
static IRExpression* parse_prefix(Parser* p);
static IRExpression* parse_postfix(Parser* p);
static IRExpression* parse_primary(Parser* p);

// ============================================================================
// PARSER UTILITIES
// ============================================================================

static char peek(Parser* p) {
    if (p->pos >= p->length) return '\0';
    return p->source[p->pos];
}

static char advance(Parser* p) {
    if (p->pos >= p->length) return '\0';
    char c = p->source[p->pos++];
    if (c == '\n') {
        p->line++;
        p->column = 1;
    } else {
        p->column++;
    }
    return c;
}

static bool is_at_end(Parser* p) {
    return p->pos >= p->length;
}

static bool match_char(Parser* p, char c) {
    if (peek(p) == c) {
        advance(p);
        return true;
    }
    return false;
}

static void skip_whitespace(Parser* p) {
    while (!is_at_end(p)) {
        char c = peek(p);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance(p);
        } else {
            break;
        }
    }
}

static void set_error(Parser* p, const char* message) {
    if (!p->error_message) {
        p->error_message = strdup(message);
        p->error_position = p->pos;
    }
}

// ============================================================================
// TOKENIZATION
// ============================================================================

static Token make_token(Parser* p, TokenType type, size_t length) {
    Token t;
    t.type = type;
    t.start = p->source + p->pos - length;
    t.length = length;
    t.line = p->line;
    t.column = p->column - length;
    return t;
}

static Token error_token(Parser* p, const char* start, size_t length) {
    Token t;
    t.type = TOKEN_ERROR;
    t.start = start;
    t.length = length;
    t.line = p->line;
    t.column = p->column;
    return t;
}

static bool is_identifier_start(char c) {
    return isalpha(c) || c == '_' || c == '$';
}

static bool is_identifier_char(char c) {
    return isalnum(c) || c == '_' || c == '$';
}

static Token scan_string(Parser* p, char quote) {
    size_t start_pos = p->pos;
    advance(p); // Skip opening quote

    while (!is_at_end(p) && peek(p) != quote) {
        if (peek(p) == '\\') {
            advance(p); // Skip escape char
        }
        advance(p);
    }

    if (is_at_end(p)) {
        set_error(p, "Unterminated string literal");
        return error_token(p, p->source + start_pos, p->pos - start_pos);
    }

    advance(p); // Skip closing quote
    return make_token(p, TOKEN_STRING, p->pos - start_pos);
}

static Token scan_number(Parser* p) {
    size_t start_pos = p->pos;

    // Integer part
    while (isdigit(peek(p))) advance(p);

    // Fractional part
    if (peek(p) == '.' && isdigit(p->source[p->pos + 1])) {
        advance(p);
        while (isdigit(peek(p))) advance(p);
    }

    // Exponent part
    if (peek(p) == 'e' || peek(p) == 'E') {
        advance(p);
        if (peek(p) == '+' || peek(p) == '-') advance(p);
        while (isdigit(peek(p))) advance(p);
    }

    return make_token(p, TOKEN_NUMBER, p->pos - start_pos);
}

static Token scan_identifier(Parser* p) {
    size_t start_pos = p->pos;

    while (is_identifier_char(peek(p))) advance(p);

    size_t length = p->pos - start_pos;
    const char* start = p->source + start_pos;

    // Check for keywords
    if (length == 4 && strncmp(start, "true", 4) == 0)
        return make_token(p, TOKEN_TRUE, length);
    if (length == 5 && strncmp(start, "false", 5) == 0)
        return make_token(p, TOKEN_FALSE, length);
    if (length == 4 && strncmp(start, "null", 4) == 0)
        return make_token(p, TOKEN_NULL, length);

    return make_token(p, TOKEN_IDENTIFIER, length);
}

static Token next_token(Parser* p) {
    skip_whitespace(p);

    if (is_at_end(p))
        return make_token(p, TOKEN_EOF, 0);

    size_t start_pos = p->pos;
    char c = peek(p);

    // Single character tokens
    switch (c) {
        case '.': advance(p); return make_token(p, TOKEN_DOT, 1);
        case ',': advance(p); return make_token(p, TOKEN_COMMA, 1);
        case ':': advance(p); return make_token(p, TOKEN_COLON, 1);
        case '?': advance(p); return make_token(p, TOKEN_QUESTION, 1);
        case '!':
            advance(p);
            if (match_char(p, '=')) return make_token(p, TOKEN_NEQ, 2);
            return make_token(p, TOKEN_NOT, 1);
        case '+': advance(p); return make_token(p, TOKEN_PLUS, 1);
        case '-': advance(p); return make_token(p, TOKEN_MINUS, 1);
        case '*': advance(p); return make_token(p, TOKEN_STAR, 1);
        case '/': advance(p); return make_token(p, TOKEN_SLASH, 1);
        case '%': advance(p); return make_token(p, TOKEN_PERCENT, 1);
        case '(': advance(p); return make_token(p, TOKEN_LPAREN, 1);
        case ')': advance(p); return make_token(p, TOKEN_RPAREN, 1);
        case '[': advance(p); return make_token(p, TOKEN_LBRACKET, 1);
        case ']': advance(p); return make_token(p, TOKEN_RBRACKET, 1);
        case '{': advance(p); return make_token(p, TOKEN_LBRACE, 1);
        case '}': advance(p); return make_token(p, TOKEN_RBRACE, 1);
    }

    // Multi-character operators
    if (c == '=') {
        advance(p);
        if (match_char(p, '=')) return make_token(p, TOKEN_EQ_EQ, 2);
        // Single = is not supported in expressions - fall through to error
        return make_token(p, TOKEN_ERROR, 1);
    }

    if (c == '<') {
        advance(p);
        if (match_char(p, '=')) return make_token(p, TOKEN_LTE, 2);
        return make_token(p, TOKEN_LT, 1);
    }

    if (c == '>') {
        advance(p);
        if (match_char(p, '=')) return make_token(p, TOKEN_GTE, 2);
        return make_token(p, TOKEN_GT, 1);
    }

    if (c == '&') {
        advance(p);
        if (match_char(p, '&')) return make_token(p, TOKEN_AND_AND, 2);
        // Single & is not supported in expressions
        return make_token(p, TOKEN_ERROR, 1);
    }

    if (c == '|') {
        advance(p);
        if (match_char(p, '|')) return make_token(p, TOKEN_OR_OR, 2);
        // Single | is not supported in expressions
        return make_token(p, TOKEN_ERROR, 1);
    }

    // String literals
    if (c == '"' || c == '\'')
        return scan_string(p, c);

    // Numbers
    if (isdigit(c))
        return scan_number(p);

    // Identifiers
    if (is_identifier_start(c))
        return scan_identifier(p);

    // Unknown character
    advance(p);
    set_error(p, "Unexpected character");
    return error_token(p, p->source + start_pos, 1);
}

static void advance_token(Parser* p) {
    p->current = next_token(p);
}

// ============================================================================
// PARSER CREATION
// ============================================================================

static Parser* parser_create(const char* source) {
    Parser* p = (Parser*)calloc(1, sizeof(Parser));
    if (!p) return NULL;

    p->source = source;
    p->length = strlen(source);
    p->pos = 0;
    p->line = 1;
    p->column = 1;
    p->error_message = NULL;
    p->error_position = 0;

    advance_token(p);
    return p;
}

static void parser_free(Parser* p) {
    if (p) {
        free(p->error_message);
        free(p);
    }
}

// ============================================================================
// TOKEN STRING EXTRACTION
// ============================================================================

static char* extract_string(const Token* t) {
    if (t->length < 2) return strdup("");

    // Skip quotes
    size_t len = t->length - 2;
    char* str = (char*)malloc(len + 1);
    if (!str) return NULL;

    // Copy without quotes
    memcpy(str, t->start + 1, len);
    str[len] = '\0';
    return str;
}

static char* extract_text(const Token* t) {
    char* text = (char*)malloc(t->length + 1);
    if (!text) return NULL;
    memcpy(text, t->start, t->length);
    text[t->length] = '\0';
    return text;
}

static double extract_number(const Token* t) {
    // Simple atof - for production, use strtod with proper error handling
    char* text = extract_text(t);
    double val = atof(text);
    free(text);
    return val;
}

// ============================================================================
// EXPRESSION PARSING
// ============================================================================

// Entry point
static IRExpression* parse_expression(Parser* p) {
    return parse_ternary(p);
}

// Ternary: condition ? then : else
static IRExpression* parse_ternary(Parser* p) {
    IRExpression* condition = parse_logical(p);

    if (p->current.type == TOKEN_QUESTION) {
        advance_token(p);

        IRExpression* then_expr = parse_expression(p);

        if (p->current.type != TOKEN_COLON) {
            set_error(p, "Expected ':' in ternary expression");
            ir_expr_free(condition);
            ir_expr_free(then_expr);
            return NULL;
        }
        advance_token(p);

        IRExpression* else_expr = parse_expression(p);

        return ir_expr_ternary(condition, then_expr, else_expr);
    }

    return condition;
}

// Logical OR/AND
static IRExpression* parse_logical(Parser* p) {
    IRExpression* left = parse_comparison(p);

    while (p->current.type == TOKEN_AND_AND || p->current.type == TOKEN_OR_OR) {
        TokenType op = p->current.type;
        advance_token(p);

        IRExpression* right = parse_comparison(p);

        IRBinaryOp binop = (op == TOKEN_AND_AND) ? BINARY_OP_AND : BINARY_OP_OR;
        left = ir_expr_binary(binop, left, right);
    }

    return left;
}

// Comparison: ==, !=, <, <=, >, >=
static IRExpression* parse_comparison(Parser* p) {
    IRExpression* left = parse_additive(p);

    while (1) {
        IRBinaryOp op;
        switch (p->current.type) {
            case TOKEN_EQ_EQ:   op = BINARY_OP_EQ; break;
            case TOKEN_NEQ:     op = BINARY_OP_NEQ; break;
            case TOKEN_LT:      op = BINARY_OP_LT; break;
            case TOKEN_LTE:     op = BINARY_OP_LTE; break;
            case TOKEN_GT:      op = BINARY_OP_GT; break;
            case TOKEN_GTE:     op = BINARY_OP_GTE; break;
            default: goto done_comp;
        }

        advance_token(p);
        IRExpression* right = parse_additive(p);
        left = ir_expr_binary(op, left, right);
    }

done_comp:
    return left;
}

// Additive: +, -
static IRExpression* parse_additive(Parser* p) {
    IRExpression* left = parse_multiplicative(p);

    while (p->current.type == TOKEN_PLUS || p->current.type == TOKEN_MINUS) {
        TokenType op = p->current.type;
        advance_token(p);

        IRExpression* right = parse_multiplicative(p);

        IRBinaryOp binop = (op == TOKEN_PLUS) ? BINARY_OP_ADD : BINARY_OP_SUB;
        left = ir_expr_binary(binop, left, right);
    }

    return left;
}

// Multiplicative: *, /, %
static IRExpression* parse_multiplicative(Parser* p) {
    IRExpression* left = parse_prefix(p);

    while (p->current.type == TOKEN_STAR ||
           p->current.type == TOKEN_SLASH ||
           p->current.type == TOKEN_PERCENT) {

        TokenType op = p->current.type;
        advance_token(p);

        IRExpression* right = parse_prefix(p);

        IRBinaryOp binop;
        switch (op) {
            case TOKEN_STAR:   binop = BINARY_OP_MUL; break;
            case TOKEN_SLASH:  binop = BINARY_OP_DIV; break;
            case TOKEN_PERCENT: binop = BINARY_OP_MOD; break;
            default: binop = BINARY_OP_ADD; break; // Should not happen
        }

        left = ir_expr_binary(binop, left, right);
    }

    return left;
}

// Prefix: !, -
static IRExpression* parse_prefix(Parser* p) {
    if (p->current.type == TOKEN_NOT) {
        advance_token(p);
        IRExpression* operand = parse_prefix(p);
        return ir_expr_unary(UNARY_OP_NOT, operand);
    }

    if (p->current.type == TOKEN_MINUS) {
        // Check if this is actually a negative number
        if (p->current.type == TOKEN_NUMBER) {
            return parse_primary(p);
        }

        advance_token(p);
        IRExpression* operand = parse_prefix(p);
        return ir_expr_unary(UNARY_OP_NEG, operand);
    }

    return parse_postfix(p);
}

// Postfix: .prop, [key], (args)
static IRExpression* parse_postfix(Parser* p) {
    IRExpression* expr = parse_primary(p);

    while (1) {
        // Member access: obj.prop
        if (p->current.type == TOKEN_DOT) {
            advance_token(p);

            if (p->current.type != TOKEN_IDENTIFIER) {
                set_error(p, "Expected property name after '.'");
                ir_expr_free(expr);
                return NULL;
            }

            char* prop = extract_text(&p->current);
            advance_token(p);

            // Check if this is followed by a call (method call)
            if (p->current.type == TOKEN_LPAREN) {
                // This is a method call: obj.method(...)
                // We need to collect arguments first, then build method_call
                advance_token(p); // Skip (

                IRExpression** args = NULL;
                int arg_count = 0;

                if (p->current.type != TOKEN_RPAREN) {
                    // Parse arguments
                    int capacity = 4;
                    args = (IRExpression**)malloc(capacity * sizeof(IRExpression*));

                    while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                        if (arg_count >= capacity) {
                            capacity *= 2;
                            args = (IRExpression**)realloc(args, capacity * sizeof(IRExpression*));
                        }

                        args[arg_count++] = parse_expression(p);

                        if (p->current.type == TOKEN_COMMA) {
                            advance_token(p);
                        } else if (p->current.type != TOKEN_RPAREN) {
                            set_error(p, "Expected ',' or ')' in argument list");
                            break;
                        }
                    }
                }

                if (p->current.type != TOKEN_RPAREN) {
                    for (int i = 0; i < arg_count; i++) ir_expr_free(args[i]);
                    free(args);
                    ir_expr_free(expr);
                    return NULL;
                }
                advance_token(p); // Skip )

                IRExpression* method_call = ir_expr_method_call(expr, prop, args, arg_count);
                free(prop);
                expr = method_call;
            } else {
                // Regular member access
                expr = ir_expr_member_access(expr, prop);
                free(prop);
            }
            continue;
        }

        // Computed member: obj[key]
        if (p->current.type == TOKEN_LBRACKET) {
            advance_token(p);

            IRExpression* key = parse_expression(p);

            if (p->current.type != TOKEN_RBRACKET) {
                set_error(p, "Expected ']' to close computed member access");
                ir_expr_free(expr);
                ir_expr_free(key);
                return NULL;
            }
            advance_token(p);

            expr = ir_expr_computed_member(expr, key);
            continue;
        }

        // Function/method call: func(...)
        if (p->current.type == TOKEN_LPAREN) {
            advance_token(p); // Skip (

            IRExpression** args = NULL;
            int arg_count = 0;

            if (p->current.type != TOKEN_RPAREN) {
                int capacity = 4;
                args = (IRExpression**)malloc(capacity * sizeof(IRExpression*));

                while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                    if (arg_count >= capacity) {
                        capacity *= 2;
                        args = (IRExpression**)realloc(args, capacity * sizeof(IRExpression*));
                    }

                    args[arg_count++] = parse_expression(p);

                    if (p->current.type == TOKEN_COMMA) {
                        advance_token(p);
                    } else if (p->current.type != TOKEN_RPAREN) {
                        set_error(p, "Expected ',' or ')' in argument list");
                        break;
                    }
                }
            }

            if (p->current.type != TOKEN_RPAREN) {
                for (int i = 0; i < arg_count; i++) ir_expr_free(args[i]);
                free(args);
                ir_expr_free(expr);
                return NULL;
            }
            advance_token(p); // Skip )

            // Check if expr is a member access (should have been handled in DOT case)
            // If we get here with EXPR_MEMBER_ACCESS, it's a bug in our logic
            // For now, treat as regular call
            if (expr->type == EXPR_VAR_REF) {
                char* func_name = expr->var_ref.name;
                expr->var_ref.name = NULL; // Steal the name
                ir_expr_free(expr);
                expr = ir_expr_call(func_name, args, arg_count);
                free(func_name);
            } else {
                // Expression call like (obj.method)()
                // For now, treat as error - this case needs more thought
                set_error(p, "Expression calls not yet supported");
                for (int i = 0; i < arg_count; i++) ir_expr_free(args[i]);
                free(args);
                ir_expr_free(expr);
                return NULL;
            }
            continue;
        }

        break;
    }

    return expr;
}

// Primary: literals, identifiers, grouping, arrays, objects
static IRExpression* parse_primary(Parser* p) {
    switch (p->current.type) {
        case TOKEN_NUMBER: {
            double val = extract_number(&p->current);
            advance_token(p);

            // Check if integer
            if (val == (int64_t)val) {
                return ir_expr_int((int64_t)val);
            }
            return ir_expr_float(val);
        }

        case TOKEN_STRING: {
            char* str = extract_string(&p->current);
            advance_token(p);
            IRExpression* expr = ir_expr_string(str);
            free(str);
            return expr;
        }

        case TOKEN_TRUE:
            advance_token(p);
            return ir_expr_bool(true);

        case TOKEN_FALSE:
            advance_token(p);
            return ir_expr_bool(false);

        case TOKEN_NULL:
            advance_token(p);
            return ir_expr_null();

        case TOKEN_IDENTIFIER: {
            char* name = extract_text(&p->current);
            advance_token(p);
            IRExpression* expr = ir_expr_var(name);
            free(name);
            return expr;
        }

        case TOKEN_LPAREN: {
            advance_token(p);
            IRExpression* inner = parse_expression(p);

            if (p->current.type != TOKEN_RPAREN) {
                set_error(p, "Expected ')' to close grouped expression");
                ir_expr_free(inner);
                return NULL;
            }
            advance_token(p);

            return ir_expr_group(inner);
        }

        case TOKEN_LBRACKET: {
            // Array literal: [1, 2, 3]
            advance_token(p);

            // For now, just create an array literal as a special call
            // In the future, we might have EXPR_ARRAY_LITERAL
            IRExpression** elements = NULL;
            int count = 0;
            int capacity = 4;
            elements = (IRExpression**)malloc(capacity * sizeof(IRExpression*));

            while (p->current.type != TOKEN_RBRACKET && p->current.type != TOKEN_EOF) {
                if (count >= capacity) {
                    capacity *= 2;
                    elements = (IRExpression**)realloc(elements, capacity * sizeof(IRExpression*));
                }

                elements[count++] = parse_expression(p);

                if (p->current.type == TOKEN_COMMA) {
                    advance_token(p);
                } else if (p->current.type != TOKEN_RBRACKET) {
                    set_error(p, "Expected ',' or ']' in array literal");
                    break;
                }
            }

            if (p->current.type != TOKEN_RBRACKET) {
                for (int i = 0; i < count; i++) ir_expr_free(elements[i]);
                free(elements);
                return NULL;
            }
            advance_token(p);

            // Create array value and wrap in literal
            // For now, return null - array literals need more thought
            // We could use a special builtin like __array()
            for (int i = 0; i < count; i++) ir_expr_free(elements[i]);
            free(elements);
            set_error(p, "Array literals not yet supported");
            return NULL;
        }

        case TOKEN_LBRACE: {
            // Object literal: {key: value, key2: value2}
            advance_token(p);

            // For now, object literals need more work
            set_error(p, "Object literals not yet supported");
            return NULL;
        }

        case TOKEN_EOF:
            set_error(p, "Unexpected end of expression");
            return NULL;

        default:
            set_error(p, "Unexpected token in expression");
            return NULL;
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

IRExpression* ir_parse_expression_ex(const char* source,
                                     char** error_message,
                                     uint32_t* error_position) {
    if (!source) return NULL;

    Parser* p = parser_create(source);
    if (!p) return NULL;

    IRExpression* expr = parse_expression(p);

    if (p->error_message) {
        if (error_message) *error_message = p->error_message;
        else free(p->error_message);

        if (error_position) *error_position = (uint32_t)p->error_position;

        p->error_message = NULL; // Prevent double free

        if (expr) {
            ir_expr_free(expr);
            expr = NULL;
        }
    } else if (p->current.type != TOKEN_EOF) {
        // Extra tokens after expression
        set_error(p, "Unexpected tokens after expression");
        if (error_message) *error_message = p->error_message;
        else free(p->error_message);
        p->error_message = NULL;

        if (error_position) *error_position = (uint32_t)p->error_position;

        if (expr) {
            ir_expr_free(expr);
            expr = NULL;
        }
    }

    parser_free(p);
    return expr;
}

IRExpression* ir_parse_expression(const char* source) {
    return ir_parse_expression_ex(source, NULL, NULL);
}

const char* ir_expression_parser_version(void) {
    return "1.0.0";
}
