/**
 * @file krl_lexer.c
 * @brief KRL Lexer Implementation
 *
 * Tokenizes S-expression syntax.
 */

#include "krl_parser.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static bool is_at_end(KRLLexer *lexer) {
    return *lexer->current == '\0';
}

static char advance(KRLLexer *lexer) {
    lexer->current++;
    lexer->column++;
    return lexer->current[-1];
}

static char peek(KRLLexer *lexer) {
    return *lexer->current;
}

static char peek_next(KRLLexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(KRLLexer *lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    lexer->column++;
    return true;
}

static void skip_whitespace(KRLLexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                lexer->column = 1;
                advance(lexer);
                break;
            case ';':  // Comment to end of line
                while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                    advance(lexer);
                }
                break;
            default:
                return;
        }
    }
}

static KRLToken make_token(KRLLexer *lexer, KRLTokenType type) {
    KRLToken token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - (int)token.length;
    return token;
}

static KRLToken error_token(KRLLexer *lexer, const char *message) {
    KRLToken token;
    token.type = KRL_TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

// =============================================================================
// TOKEN SCANNING
// =============================================================================

static KRLToken string_token(KRLLexer *lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') {
            lexer->line++;
            lexer->column = 0;
        }
        if (peek(lexer) == '\\') {
            advance(lexer);  // Skip escape character
            if (!is_at_end(lexer)) advance(lexer);  // Skip escaped character
        } else {
            advance(lexer);
        }
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }

    // Closing quote
    advance(lexer);
    return make_token(lexer, KRL_TOKEN_STRING);
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_' || c == '-' || c == '?' || c == '!';
}

static bool is_symbol_char(char c) {
    return is_alpha(c) || is_digit(c) || c == '@' || c == '#' || c == '.' || c == '/';
}

static KRLToken number_token(KRLLexer *lexer) {
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }

    // Look for decimal point
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        advance(lexer);  // Consume '.'
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, KRL_TOKEN_FLOAT);
    }

    return make_token(lexer, KRL_TOKEN_INTEGER);
}

static KRLTokenType check_keyword(const char *start, size_t length,
                                   const char *rest, KRLTokenType type) {
    if (strlen(rest) == length && memcmp(start, rest, length) == 0) {
        return type;
    }
    return KRL_TOKEN_SYMBOL;
}

static KRLTokenType identify_symbol(const char *start, size_t length) {
    // Check for boolean keywords
    if (length == 4 && memcmp(start, "true", 4) == 0) {
        return KRL_TOKEN_TRUE;
    }
    if (length == 5 && memcmp(start, "false", 5) == 0) {
        return KRL_TOKEN_FALSE;
    }

    return KRL_TOKEN_SYMBOL;
}

static KRLToken symbol_token(KRLLexer *lexer) {
    while (is_symbol_char(peek(lexer))) {
        advance(lexer);
    }

    KRLTokenType type = identify_symbol(lexer->start, lexer->current - lexer->start);
    return make_token(lexer, type);
}

// =============================================================================
// PUBLIC API
// =============================================================================

void krl_lexer_init(KRLLexer *lexer, const char *source, const char *filename) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->filename = filename ? filename : "unknown.krl";
}

KRLToken krl_lexer_next_token(KRLLexer *lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) {
        return make_token(lexer, KRL_TOKEN_EOF);
    }

    char c = advance(lexer);

    // Numbers
    if (is_digit(c) || (c == '-' && is_digit(peek(lexer)))) {
        return number_token(lexer);
    }

    // Symbols and keywords
    if (is_alpha(c) || c == '@') {
        return symbol_token(lexer);
    }

    switch (c) {
        case '(': return make_token(lexer, KRL_TOKEN_LPAREN);
        case ')': return make_token(lexer, KRL_TOKEN_RPAREN);
        case '"': return string_token(lexer);
        case ':':
            // Keyword-style identifier
            while (is_symbol_char(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, KRL_TOKEN_KEYWORD);
        case '$':
            // Variable reference
            while (is_symbol_char(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, KRL_TOKEN_VARIABLE);
        case '#':
            // Could be color or symbol
            while (is_symbol_char(peek(lexer))) {
                advance(lexer);
            }
            return make_token(lexer, KRL_TOKEN_SYMBOL);
    }

    return error_token(lexer, "Unexpected character");
}
