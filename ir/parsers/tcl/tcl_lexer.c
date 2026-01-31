/**
 * Tcl Lexer
 * Tokenizes Tcl/Tk source code
 */

#define _POSIX_C_SOURCE 200809L

#include "tcl_ast.h"
#include "../../codegens/codegen_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Forward declarations for StringBuilder */
extern StringBuilder* sb_create(size_t initial_capacity);
extern void sb_append(StringBuilder* sb, const char* str);
extern char* sb_get(StringBuilder* sb);
extern void sb_free(StringBuilder* sb);

/* Helper to convert StringBuilder to string */
static char* sbToString(StringBuilder* sb) {
    return sb_get(sb);
}

/* Helper to append a single character */
static void sb_append_char(StringBuilder* sb, char c) {
    char str[2] = {c, '\0'};
    sb_append(sb, str);
}

/* ============================================================================
 * Lexer State
 * ============================================================================ */

typedef struct {
    const char* source;
    size_t source_len;
    size_t position;
    int line;
    int column;
} TclLexer;

/* ============================================================================
 * Character Utilities
 * ============================================================================ */

static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static char peek_char(TclLexer* lexer) {
    if (lexer->position >= lexer->source_len) {
        return '\0';
    }
    return lexer->source[lexer->position];
}

static char get_char(TclLexer* lexer) {
    if (lexer->position >= lexer->source_len) {
        return '\0';
    }
    char c = lexer->source[lexer->position++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void skip_whitespace(TclLexer* lexer) {
    while (is_whitespace(peek_char(lexer))) {
        get_char(lexer);
    }
}

/* ============================================================================
 * Token Creation
 * ============================================================================ */

static TclToken* create_token(TclTokenType type, const char* value, int line, int column, int position) {
    TclToken* token = calloc(1, sizeof(TclToken));
    if (!token) return NULL;

    token->type = type;
    token->value = value ? strdup(value) : NULL;
    token->line = line;
    token->column = column;
    token->position = position;

    return token;
}

/* ============================================================================
 * Lexing Functions
 * ============================================================================ */

/**
 * Lex a quoted string: "..."
 * Tcl supports both double quotes and braces for strings
 */
static TclToken* lex_quoted_string(TclLexer* lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->position;

    char quote = get_char(lexer);  // Opening quote (" or {)
    char close_quote = (quote == '{') ? '}' : '"';
    bool is_brace = (quote == '{');

    StringBuilder* sb = sb_create(256);
    if (!sb) return NULL;

    bool escape_next = false;

    while (true) {
        char c = peek_char(lexer);

        if (c == '\0') {
            // Unterminated string
            sb_free(sb);
            return NULL;
        }

        if (is_brace) {
            // Braced strings: count nesting, handle escapes
            if (c == '}' && escape_next) {
                // Escaped closing brace
                sb_append_char(sb, c);
                get_char(lexer);
                escape_next = false;
            } else if (c == '}') {
                // Check brace depth
                get_char(lexer);  // consume }
                // Simple approach: first } ends the string
                // (Tcl allows nested braces, but for simplicity we don't count depth)
                break;
            } else if (c == '{') {
                sb_append_char(sb, c);
                get_char(lexer);
            } else if (c == '\\') {
                escape_next = true;
                get_char(lexer);
            } else {
                sb_append_char(sb, c);
                get_char(lexer);
            }
        } else {
            // Quoted strings: support escape sequences
            if (escape_next) {
                switch (c) {
                    case 'n': sb_append_char(sb, '\n'); break;
                    case 't': sb_append_char(sb, '\t'); break;
                    case 'r': sb_append_char(sb, '\r'); break;
                    case '\\': sb_append_char(sb, '\\'); break;
                    case '"': sb_append_char(sb, '"'); break;
                    case '$': sb_append_char(sb, '$'); break;
                    case '[': sb_append_char(sb, '['); break;
                    case ']': sb_append_char(sb, ']'); break;
                    default: sb_append_char(sb, c); break;
                }
                get_char(lexer);
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
                get_char(lexer);
            } else if (c == close_quote) {
                get_char(lexer);
                break;
            } else {
                sb_append_char(sb, c);
                get_char(lexer);
            }
        }
    }

    char* value = sbToString(sb);
    sb_free(sb);

    return create_token(TCL_TOKEN_STRING, value, start_line, start_column, start_pos);
}

/**
 * Lex a word (command or argument)
 * Words can contain: letters, digits, underscores, colons, dots, dashes
 */
static TclToken* lex_word(TclLexer* lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->position;

    StringBuilder* sb = sb_create(128);
    if (!sb) return NULL;

    // First character can be alpha, digit, or special Tcl chars (including # for colors like #ff0000)
    char c = peek_char(lexer);
    if (is_alpha(c) || is_digit(c) || c == '.' || c == ':' || c == '_' || c == '-' || c == '*' || c == '#') {
        sb_append_char(sb, c);
        get_char(lexer);
    } else {
        sb_free(sb);
        return NULL;
    }

    // Continue with alnum or special chars
    while (true) {
        c = peek_char(lexer);
        if (is_alnum(c) || c == '.' || c == ':' || c == '_' || c == '-' || c == '*' || c == '#') {
            sb_append_char(sb, c);
            get_char(lexer);
        } else {
            break;
        }
    }

    char* value = sbToString(sb);
    sb_free(sb);

    // Check if it's a number
    bool is_number = true;
    bool has_dot = false;
    for (size_t i = 0; value[i]; i++) {
        if (value[i] == '.') {
            if (has_dot) {
                is_number = false;
                break;
            }
            has_dot = true;
        } else if (!is_digit(value[i])) {
            is_number = false;
            break;
        }
    }

    if (is_number && value[0] != '\0') {
        free(value);
        // Recreate as number token
        return create_token(TCL_TOKEN_NUMBER, NULL, start_line, start_column, start_pos);
    }

    return create_token(TCL_TOKEN_WORD, value, start_line, start_column, start_pos);
}

/**
 * Lex a comment: # ...
 * In Tcl, # starts a comment only at the beginning of a command
 * (after whitespace or at start of line), not in the middle of arguments.
 */
static TclToken* lex_comment(TclLexer* lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->position;

    get_char(lexer);  // Skip #

    StringBuilder* sb = sb_create(256);
    if (!sb) return NULL;

    while (peek_char(lexer) != '\0' && peek_char(lexer) != '\n') {
        sb_append_char(sb, get_char(lexer));
    }

    char* value = sbToString(sb);
    sb_free(sb);

    return create_token(TCL_TOKEN_POUND, value, start_line, start_column, start_pos);
}

/**
 * Check if we're at the start of a command (where # would start a comment)
 * In Tcl, # starts a comment ONLY when it's at the very beginning of a line
 * (possibly with leading whitespace), NOT in the middle of command arguments.
 */
static bool at_command_start(TclLexer* lexer) {
    if (!lexer->source || lexer->position == 0) return true;

    // Look backwards from current position
    size_t pos = lexer->position;  // Position of the # character

    // Skip backwards whitespace/tabs ONLY
    while (pos > 0 && (lexer->source[pos - 1] == ' ' || lexer->source[pos - 1] == '\t')) {
        pos--;
    }

    // # is a comment ONLY if it's at start of file or right after a newline
    // (NOT after semicolons, NOT in the middle of arguments)
    if (pos == 0) return true;
    if (lexer->source[pos - 1] == '\n') return true;
    if (lexer->source[pos - 1] == '\r') return true;

    return false;
}

/**
 * Lex a variable substitution: $name or ${name}
 */
static TclToken* lex_variable(TclLexer* lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->position;

    get_char(lexer);  // Skip $

    StringBuilder* sb = sb_create(128);
    if (!sb) return NULL;

    if (peek_char(lexer) == '{') {
        // ${name} form
        get_char(lexer);  // Skip {
        while (peek_char(lexer) != '\0' && peek_char(lexer) != '}') {
            sb_append_char(sb, get_char(lexer));
        }
        if (peek_char(lexer) == '}') {
            get_char(lexer);  // Skip }
        }
    } else {
        // $name form
        while (is_alnum(peek_char(lexer)) || peek_char(lexer) == '_') {
            sb_append_char(sb, get_char(lexer));
        }
    }

    char* value = sbToString(sb);
    sb_free(sb);

    return create_token(TCL_TOKEN_DOLLAR, value, start_line, start_column, start_pos);
}

/**
 * Lex command substitution: [command]
 */
static TclToken* lex_bracket(TclLexer* lexer) {
    int start_line = lexer->line;
    int start_column = lexer->column;
    size_t start_pos = lexer->position;

    get_char(lexer);  // Skip [

    int depth = 1;
    StringBuilder* sb = sb_create(256);
    if (!sb) return NULL;

    while (peek_char(lexer) != '\0' && depth > 0) {
        char c = peek_char(lexer);
        if (c == '[') {
            depth++;
        } else if (c == ']') {
            depth--;
            if (depth == 0) {
                get_char(lexer);
                break;
            }
        }
        sb_append_char(sb, get_char(lexer));
    }

    char* value = sbToString(sb);
    sb_free(sb);

    return create_token(TCL_TOKEN_BRACKET, value, start_line, start_column, start_pos);
}

/* ============================================================================
 * Main Lexer
 * ============================================================================ */

/**
 * Tokenize Tcl source code
 */
TclTokenStream* tcl_lex(const char* source) {
    if (!source) return NULL;

    TclTokenStream* stream = calloc(1, sizeof(TclTokenStream));
    if (!stream) return NULL;

    stream->capacity = 1024;
    stream->tokens = calloc(stream->capacity, sizeof(TclToken));
    if (!stream->tokens) {
        free(stream);
        return NULL;
    }

    TclLexer lexer = {
        .source = source,
        .source_len = strlen(source),
        .position = 0,
        .line = 1,
        .column = 1
    };

    while (lexer.position < lexer.source_len) {
        skip_whitespace(&lexer);

        char c = peek_char(&lexer);
        if (c == '\0') break;

        TclToken* token = NULL;

        // Check for braced strings FIRST (before comments, because # inside braces is literal)
        if (c == '{') {
            token = lex_quoted_string(&lexer);
        } else if (c == '"') {
            token = lex_quoted_string(&lexer);
        // Only check for comments at command start (not inside strings)
        } else if (c == '#' && at_command_start(&lexer)) {
            token = lex_comment(&lexer);
        } else if (c == '$') {
            token = lex_variable(&lexer);
        } else if (c == '[') {
            token = lex_bracket(&lexer);
        } else if (c == '}') {
            token = create_token(TCL_TOKEN_RBRACE, "}", lexer.line, lexer.column, lexer.position);
            get_char(&lexer);
        } else if (c == ';') {
            token = create_token(TCL_TOKEN_SEMICOLON, ";", lexer.line, lexer.column, lexer.position);
            get_char(&lexer);
        } else if (c == '\\') {
            token = create_token(TCL_TOKEN_BACKSLASH, "\\", lexer.line, lexer.column, lexer.position);
            get_char(&lexer);
        } else {
            token = lex_word(&lexer);
        }

        if (token) {
            if (stream->token_count >= stream->capacity) {
                stream->capacity *= 2;
                stream->tokens = realloc(stream->tokens, stream->capacity * sizeof(TclToken));
                if (!stream->tokens) {
                    tcl_token_stream_free(stream);
                    return NULL;
                }
            }
            stream->tokens[stream->token_count++] = *token;
            free(token);  // Free the wrapper, not the token data
        }
    }

    // Add EOF token
    TclToken* eof = create_token(TCL_TOKEN_EOF, NULL, lexer.line, lexer.column, lexer.position);
    if (eof) {
        stream->tokens[stream->token_count++] = *eof;
        free(eof);
    }

    return stream;
}

/* ============================================================================
 * Token Stream Utilities
 * ============================================================================ */

void tcl_token_free(TclToken* token) {
    if (!token) return;
    if (token->value) {
        free(token->value);
    }
    free(token);
}

void tcl_token_stream_free(TclTokenStream* stream) {
    if (!stream) return;

    for (int i = 0; i < stream->token_count; i++) {
        if (stream->tokens[i].value) {
            free(stream->tokens[i].value);
        }
    }

    free(stream->tokens);
    free(stream);
}

TclToken* tcl_token_stream_peek(TclTokenStream* stream, int offset) {
    if (!stream || stream->current + offset >= stream->token_count) {
        return NULL;
    }
    return &stream->tokens[stream->current + offset];
}

TclToken* tcl_token_stream_consume(TclTokenStream* stream) {
    if (!stream || stream->current >= stream->token_count) {
        return NULL;
    }
    return &stream->tokens[stream->current++];
}
