/**
 * @file krl_parser.c
 * @brief KRL S-expression Parser Implementation
 *
 * Parses S-expressions from tokenized input.
 */

#include "krl_parser.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// S-EXPRESSION CONSTRUCTION
// =============================================================================

KRLSExp *krl_sexp_create_symbol(const char *symbol, int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_SYMBOL;
    sexp->line = line;
    sexp->column = column;
    sexp->data.symbol = strdup(symbol);
    return sexp;
}

KRLSExp *krl_sexp_create_string(const char *string, int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_STRING;
    sexp->line = line;
    sexp->column = column;
    sexp->data.string = strdup(string);
    return sexp;
}

KRLSExp *krl_sexp_create_integer(int64_t value, int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_INTEGER;
    sexp->line = line;
    sexp->column = column;
    sexp->data.integer = value;
    return sexp;
}

KRLSExp *krl_sexp_create_float(double value, int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_FLOAT;
    sexp->line = line;
    sexp->column = column;
    sexp->data.float_val = value;
    return sexp;
}

KRLSExp *krl_sexp_create_boolean(bool value, int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_BOOLEAN;
    sexp->line = line;
    sexp->column = column;
    sexp->data.boolean = value;
    return sexp;
}

KRLSExp *krl_sexp_create_list(int line, int column) {
    KRLSExp *sexp = kryon_alloc(sizeof(KRLSExp));
    sexp->type = KRL_SEXP_LIST;
    sexp->line = line;
    sexp->column = column;
    sexp->data.list.items = NULL;
    sexp->data.list.count = 0;
    sexp->data.list.capacity = 0;
    return sexp;
}

void krl_sexp_list_add(KRLSExp *list, KRLSExp *item) {
    if (list->type != KRL_SEXP_LIST) return;

    if (list->data.list.count >= list->data.list.capacity) {
        size_t new_capacity = list->data.list.capacity == 0 ? 8 : list->data.list.capacity * 2;
        KRLSExp **new_items = realloc(list->data.list.items, new_capacity * sizeof(KRLSExp*));
        if (new_items) {
            list->data.list.items = new_items;
            list->data.list.capacity = new_capacity;
        }
    }

    list->data.list.items[list->data.list.count++] = item;
}

void krl_sexp_free(KRLSExp *sexp) {
    if (!sexp) return;

    switch (sexp->type) {
        case KRL_SEXP_SYMBOL:
            if (sexp->data.symbol) free(sexp->data.symbol);
            break;
        case KRL_SEXP_STRING:
            if (sexp->data.string) free(sexp->data.string);
            break;
        case KRL_SEXP_LIST:
            for (size_t i = 0; i < sexp->data.list.count; i++) {
                krl_sexp_free(sexp->data.list.items[i]);
            }
            if (sexp->data.list.items) free(sexp->data.list.items);
            break;
        default:
            break;
    }

    kryon_free(sexp);
}

// =============================================================================
// PARSER FUNCTIONS
// =============================================================================

static void parser_error_at(KRLParser *parser, KRLToken *token, const char *message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "[%s:%d:%d] Error", parser->lexer.filename, token->line, token->column);

    if (token->type == KRL_TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == KRL_TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
}

static void parser_error(KRLParser *parser, const char *message) {
    parser_error_at(parser, &parser->previous, message);
}

static void advance(KRLParser *parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = krl_lexer_next_token(&parser->lexer);
        if (parser->current.type != KRL_TOKEN_ERROR) break;

        parser_error_at(parser, &parser->current, parser->current.start);
    }
}

static bool check(KRLParser *parser, KRLTokenType type) {
    return parser->current.type == type;
}

static bool match(KRLParser *parser, KRLTokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(KRLParser *parser, KRLTokenType type, const char *message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    parser_error_at(parser, &parser->current, message);
}

static char *token_to_string(KRLToken *token) {
    char *str = malloc(token->length + 1);
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

static KRLSExp *parse_sexp(KRLParser *parser);

static KRLSExp *parse_list(KRLParser *parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;

    KRLSExp *list = krl_sexp_create_list(line, column);

    while (!check(parser, KRL_TOKEN_RPAREN) && !check(parser, KRL_TOKEN_EOF)) {
        KRLSExp *item = parse_sexp(parser);
        if (item) {
            krl_sexp_list_add(list, item);
        }
    }

    consume(parser, KRL_TOKEN_RPAREN, "Expected ')' after list");
    return list;
}

static KRLSExp *parse_array(KRLParser *parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;

    KRLSExp *list = krl_sexp_create_list(line, column);

    // Add a special marker symbol to indicate this is an array
    KRLSExp *marker = krl_sexp_create_symbol("@array", line, column);
    krl_sexp_list_add(list, marker);

    while (!check(parser, KRL_TOKEN_RBRACKET) && !check(parser, KRL_TOKEN_EOF)) {
        KRLSExp *item = parse_sexp(parser);
        if (item) {
            krl_sexp_list_add(list, item);
        }
    }

    consume(parser, KRL_TOKEN_RBRACKET, "Expected ']' after array");
    return list;
}

static KRLSExp *parse_sexp(KRLParser *parser) {
    if (match(parser, KRL_TOKEN_LPAREN)) {
        return parse_list(parser);
    }

    if (match(parser, KRL_TOKEN_LBRACKET)) {
        return parse_array(parser);
    }

    if (match(parser, KRL_TOKEN_STRING)) {
        // Remove quotes from string
        char *str = token_to_string(&parser->previous);
        size_t len = strlen(str);
        if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
            // Process escape sequences
            char *unescaped = malloc(len);
            size_t j = 0;
            for (size_t i = 1; i < len - 1; i++) {
                if (str[i] == '\\' && i + 1 < len - 1) {
                    i++;
                    switch (str[i]) {
                        case 'n': unescaped[j++] = '\n'; break;
                        case 't': unescaped[j++] = '\t'; break;
                        case 'r': unescaped[j++] = '\r'; break;
                        case '\\': unescaped[j++] = '\\'; break;
                        case '"': unescaped[j++] = '"'; break;
                        default: unescaped[j++] = str[i]; break;
                    }
                } else {
                    unescaped[j++] = str[i];
                }
            }
            unescaped[j] = '\0';
            free(str);
            KRLSExp *sexp = krl_sexp_create_string(unescaped, parser->previous.line, parser->previous.column);
            free(unescaped);
            return sexp;
        }
        free(str);
    }

    if (match(parser, KRL_TOKEN_INTEGER)) {
        char *str = token_to_string(&parser->previous);
        int64_t value = strtoll(str, NULL, 10);
        free(str);
        return krl_sexp_create_integer(value, parser->previous.line, parser->previous.column);
    }

    if (match(parser, KRL_TOKEN_FLOAT)) {
        char *str = token_to_string(&parser->previous);
        double value = strtod(str, NULL);
        free(str);
        return krl_sexp_create_float(value, parser->previous.line, parser->previous.column);
    }

    if (match(parser, KRL_TOKEN_TRUE)) {
        return krl_sexp_create_boolean(true, parser->previous.line, parser->previous.column);
    }

    if (match(parser, KRL_TOKEN_FALSE)) {
        return krl_sexp_create_boolean(false, parser->previous.line, parser->previous.column);
    }

    if (match(parser, KRL_TOKEN_SYMBOL) || match(parser, KRL_TOKEN_KEYWORD) || match(parser, KRL_TOKEN_VARIABLE)) {
        char *str = token_to_string(&parser->previous);
        KRLSExp *sexp = krl_sexp_create_symbol(str, parser->previous.line, parser->previous.column);
        free(str);
        return sexp;
    }

    parser_error(parser, "Expected S-expression");
    return NULL;
}

// =============================================================================
// PUBLIC API
// =============================================================================

void krl_parser_init(KRLParser *parser, const char *source, const char *filename) {
    krl_lexer_init(&parser->lexer, source, filename);
    parser->had_error = false;
    parser->panic_mode = false;
    advance(parser);  // Prime the parser
}

KRLSExp **krl_parse(KRLParser *parser, size_t *out_count) {
    KRLSExp **sexps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    while (!match(parser, KRL_TOKEN_EOF)) {
        KRLSExp *sexp = parse_sexp(parser);
        if (sexp) {
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                KRLSExp **new_sexps = realloc(sexps, new_capacity * sizeof(KRLSExp*));
                if (new_sexps) {
                    sexps = new_sexps;
                    capacity = new_capacity;
                }
            }
            sexps[count++] = sexp;
        }

        if (parser->had_error) {
            // Clean up on error
            for (size_t i = 0; i < count; i++) {
                krl_sexp_free(sexps[i]);
            }
            if (sexps) free(sexps);
            *out_count = 0;
            return NULL;
        }
    }

    *out_count = count;
    return sexps;
}
