#include "nim_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// Helper functions
static bool is_at_end(NimLexer* lexer) {
    return lexer->current >= lexer->source + lexer->length;
}

static char peek(NimLexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return *lexer->current;
}

static char peek_next(NimLexer* lexer) {
    if (lexer->current + 1 >= lexer->source + lexer->length) return '\0';
    return *(lexer->current + 1);
}

static char advance(NimLexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    lexer->column++;
    return *lexer->current++;
}

static bool match(NimLexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    lexer->column++;
    return true;
}

static void skip_whitespace_inline(NimLexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        } else {
            break;
        }
    }
}

static void push_indent(NimLexer* lexer, int indent) {
    if (lexer->indent_stack_size >= lexer->indent_stack_capacity) {
        lexer->indent_stack_capacity *= 2;
        lexer->indent_stack = realloc(lexer->indent_stack,
                                      lexer->indent_stack_capacity * sizeof(int));
    }
    lexer->indent_stack[lexer->indent_stack_size++] = indent;
}

static int pop_indent(NimLexer* lexer) {
    if (lexer->indent_stack_size == 0) return 0;
    return lexer->indent_stack[--lexer->indent_stack_size];
}

static int current_indent_level(NimLexer* lexer) {
    if (lexer->indent_stack_size == 0) return 0;
    return lexer->indent_stack[lexer->indent_stack_size - 1];
}

// Public functions

void nim_lexer_init(NimLexer* lexer, const char* source, size_t length) {
    lexer->source = source;
    lexer->current = source;
    lexer->line_start = source;
    lexer->length = length > 0 ? length : strlen(source);
    lexer->line = 1;
    lexer->column = 1;
    lexer->current_indent = 0;
    lexer->at_line_start = true;
    lexer->pending_dedent_count = 0;

    lexer->indent_stack_capacity = 16;
    lexer->indent_stack = malloc(lexer->indent_stack_capacity * sizeof(int));
    lexer->indent_stack_size = 0;
    push_indent(lexer, 0); // Base indentation
}

void nim_lexer_free(NimLexer* lexer) {
    if (lexer->indent_stack) {
        free(lexer->indent_stack);
        lexer->indent_stack = NULL;
    }
}

bool nim_lexer_is_eof(NimLexer* lexer) {
    return is_at_end(lexer) && lexer->pending_dedent_count == 0;
}

NimToken nim_lexer_make_token(NimLexer* lexer, NimTokenType type) {
    NimToken token;
    token.type = type;
    token.start = lexer->current;
    token.length = 0;
    token.line = lexer->line;
    token.column = lexer->column;
    token.indent_level = lexer->current_indent;
    return token;
}

NimToken nim_lexer_error_token(NimLexer* lexer, const char* message) {
    NimToken token;
    token.type = NIM_TOK_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    token.indent_level = lexer->current_indent;
    return token;
}

const char* nim_token_type_str(NimTokenType type) {
    switch (type) {
        case NIM_TOK_IDENTIFIER: return "IDENTIFIER";
        case NIM_TOK_COLON: return "COLON";
        case NIM_TOK_EQUALS: return "EQUALS";
        case NIM_TOK_STRING: return "STRING";
        case NIM_TOK_NUMBER: return "NUMBER";
        case NIM_TOK_COMMA: return "COMMA";
        case NIM_TOK_DOT: return "DOT";
        case NIM_TOK_LPAREN: return "LPAREN";
        case NIM_TOK_RPAREN: return "RPAREN";
        case NIM_TOK_LBRACKET: return "LBRACKET";
        case NIM_TOK_RBRACKET: return "RBRACKET";
        case NIM_TOK_INDENT: return "INDENT";
        case NIM_TOK_DEDENT: return "DEDENT";
        case NIM_TOK_NEWLINE: return "NEWLINE";
        case NIM_TOK_COMMENT: return "COMMENT";
        case NIM_TOK_EOF: return "EOF";
        case NIM_TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static NimToken scan_string(NimLexer* lexer) {
    const char* start = lexer->current - 1; // Include opening quote
    char quote = *(lexer->current - 1);

    // Check for triple-quoted string
    bool triple = false;
    if (peek(lexer) == quote && peek_next(lexer) == quote) {
        triple = true;
        advance(lexer);
        advance(lexer);
    }

    while (!is_at_end(lexer)) {
        if (triple) {
            if (peek(lexer) == quote && peek_next(lexer) == quote &&
                lexer->current + 2 < lexer->source + lexer->length &&
                *(lexer->current + 2) == quote) {
                advance(lexer); // First quote
                advance(lexer); // Second quote
                advance(lexer); // Third quote
                break;
            }
            if (peek(lexer) == '\n') {
                lexer->line++;
                lexer->column = 0;
                lexer->line_start = lexer->current + 1;
            }
            advance(lexer);
        } else {
            if (peek(lexer) == quote) {
                advance(lexer);
                break;
            }
            if (peek(lexer) == '\n') {
                return nim_lexer_error_token(lexer, "Unterminated string");
            }
            if (peek(lexer) == '\\') {
                advance(lexer); // Skip backslash
                if (!is_at_end(lexer)) advance(lexer); // Skip escaped char
            } else {
                advance(lexer);
            }
        }
    }

    NimToken token = nim_lexer_make_token(lexer, NIM_TOK_STRING);
    token.start = start;
    token.length = lexer->current - start;
    return token;
}

static NimToken scan_number(NimLexer* lexer) {
    const char* start = lexer->current - 1;

    while (isdigit(peek(lexer))) {
        advance(lexer);
    }

    // Check for decimal
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer); // Consume '.'
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    // Check for exponent
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    NimToken token = nim_lexer_make_token(lexer, NIM_TOK_NUMBER);
    token.start = start;
    token.length = lexer->current - start;
    return token;
}

static NimToken scan_identifier(NimLexer* lexer) {
    const char* start = lexer->current - 1;

    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (isalnum(c) || c == '_') {
            advance(lexer);
        } else {
            break;
        }
    }

    NimToken token = nim_lexer_make_token(lexer, NIM_TOK_IDENTIFIER);
    token.start = start;
    token.length = lexer->current - start;
    return token;
}

static NimToken scan_comment(NimLexer* lexer) {
    const char* start = lexer->current - 1;

    while (!is_at_end(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }

    NimToken token = nim_lexer_make_token(lexer, NIM_TOK_COMMENT);
    token.start = start;
    token.length = lexer->current - start;
    return token;
}

static NimToken handle_indentation(NimLexer* lexer) {
    // Count spaces at start of line
    int spaces = 0;
    const char* start = lexer->current;

    while (peek(lexer) == ' ') {
        spaces++;
        advance(lexer);
    }

    // Skip lines that are empty or just comments
    if (peek(lexer) == '\n' || peek(lexer) == '#' || peek(lexer) == '\r') {
        return nim_lexer_make_token(lexer, NIM_TOK_NEWLINE); // Will be skipped
    }

    lexer->current_indent = spaces;
    int prev_indent = current_indent_level(lexer);

    if (spaces > prev_indent) {
        // Indent
        push_indent(lexer, spaces);
        NimToken token = nim_lexer_make_token(lexer, NIM_TOK_INDENT);
        token.indent_level = spaces;
        return token;
    } else if (spaces < prev_indent) {
        // Dedent - may need multiple DEDENTs
        lexer->pending_dedent_count = 0;

        while (lexer->indent_stack_size > 0 && current_indent_level(lexer) > spaces) {
            pop_indent(lexer);
            if (lexer->pending_dedent_count < 32) {
                lexer->pending_dedents[lexer->pending_dedent_count++] =
                    nim_lexer_make_token(lexer, NIM_TOK_DEDENT);
            }
        }

        if (current_indent_level(lexer) != spaces) {
            return nim_lexer_error_token(lexer, "Indentation error");
        }

        // Return first DEDENT
        if (lexer->pending_dedent_count > 0) {
            return lexer->pending_dedents[--lexer->pending_dedent_count];
        }
    }

    // Same indentation, continue scanning
    lexer->at_line_start = false;
    return nim_lexer_next(lexer);
}

NimToken nim_lexer_next(NimLexer* lexer) {
    // Return pending DEDENTs first
    if (lexer->pending_dedent_count > 0) {
        return lexer->pending_dedents[--lexer->pending_dedent_count];
    }

    // Handle indentation at line start
    if (lexer->at_line_start && !is_at_end(lexer)) {
        return handle_indentation(lexer);
    }

    skip_whitespace_inline(lexer);

    if (is_at_end(lexer)) {
        // Generate final DEDENTs for any remaining indentation
        if (lexer->indent_stack_size > 1) {
            pop_indent(lexer);
            return nim_lexer_make_token(lexer, NIM_TOK_DEDENT);
        }
        return nim_lexer_make_token(lexer, NIM_TOK_EOF);
    }

    char c = advance(lexer);

    // Single character tokens
    switch (c) {
        case ':': return nim_lexer_make_token(lexer, NIM_TOK_COLON);
        case '=': return nim_lexer_make_token(lexer, NIM_TOK_EQUALS);
        case ',': return nim_lexer_make_token(lexer, NIM_TOK_COMMA);
        case '.': return nim_lexer_make_token(lexer, NIM_TOK_DOT);
        case '(': return nim_lexer_make_token(lexer, NIM_TOK_LPAREN);
        case ')': return nim_lexer_make_token(lexer, NIM_TOK_RPAREN);
        case '[': return nim_lexer_make_token(lexer, NIM_TOK_LBRACKET);
        case ']': return nim_lexer_make_token(lexer, NIM_TOK_RBRACKET);

        case '\n':
            lexer->line++;
            lexer->column = 0;
            lexer->line_start = lexer->current;
            lexer->at_line_start = true;
            return nim_lexer_make_token(lexer, NIM_TOK_NEWLINE);

        case '#':
            return scan_comment(lexer);

        case '"':
        case '\'':
            return scan_string(lexer);
    }

    // Numbers
    if (isdigit(c)) {
        return scan_number(lexer);
    }

    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }

    return nim_lexer_error_token(lexer, "Unexpected character");
}

NimToken nim_lexer_peek(NimLexer* lexer) {
    // Save state
    const char* saved_current = lexer->current;
    int saved_line = lexer->line;
    int saved_column = lexer->column;
    int saved_indent = lexer->current_indent;
    bool saved_at_line_start = lexer->at_line_start;

    // Get next token
    NimToken token = nim_lexer_next(lexer);

    // Restore state
    lexer->current = saved_current;
    lexer->line = saved_line;
    lexer->column = saved_column;
    lexer->current_indent = saved_indent;
    lexer->at_line_start = saved_at_line_start;

    return token;
}
