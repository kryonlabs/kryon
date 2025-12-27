#ifndef NIM_LEXER_H
#define NIM_LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    NIM_TOK_IDENTIFIER,     // kryonApp, Container, Button, let, import, proc
    NIM_TOK_COLON,          // :
    NIM_TOK_EQUALS,         // =
    NIM_TOK_STRING,         // "text" or """multiline"""
    NIM_TOK_NUMBER,         // 100, 3.14
    NIM_TOK_COMMA,          // ,
    NIM_TOK_DOT,            // .
    NIM_TOK_LPAREN,         // (
    NIM_TOK_RPAREN,         // )
    NIM_TOK_LBRACKET,       // [
    NIM_TOK_RBRACKET,       // ]
    NIM_TOK_INDENT,         // Significant whitespace increase
    NIM_TOK_DEDENT,         // Significant whitespace decrease
    NIM_TOK_NEWLINE,        // End of line
    NIM_TOK_COMMENT,        // # comment
    NIM_TOK_EOF,            // End of file
    NIM_TOK_ERROR           // Lexer error
} NimTokenType;

typedef struct {
    NimTokenType type;
    const char* start;      // Pointer to start of token in source
    size_t length;          // Length of token
    int line;               // Line number (1-indexed)
    int column;             // Column number (1-indexed)
    int indent_level;       // Current indentation level (0, 2, 4, etc.)
} NimToken;

typedef struct {
    const char* source;     // Source code
    const char* current;    // Current position
    const char* line_start; // Start of current line
    size_t length;          // Total length
    int line;               // Current line (1-indexed)
    int column;             // Current column (1-indexed)
    int current_indent;     // Current indentation level
    int* indent_stack;      // Stack of indentation levels
    int indent_stack_size;  // Size of indent stack
    int indent_stack_capacity; // Capacity of indent stack
    bool at_line_start;     // True if at start of line
    NimToken pending_dedents[32]; // Queue for pending DEDENT tokens
    int pending_dedent_count;
} NimLexer;

/**
 * Initialize a lexer with source code
 */
void nim_lexer_init(NimLexer* lexer, const char* source, size_t length);

/**
 * Free lexer resources
 */
void nim_lexer_free(NimLexer* lexer);

/**
 * Get the next token
 */
NimToken nim_lexer_next(NimLexer* lexer);

/**
 * Peek at the next token without consuming it
 */
NimToken nim_lexer_peek(NimLexer* lexer);

/**
 * Check if lexer is at end of file
 */
bool nim_lexer_is_eof(NimLexer* lexer);

/**
 * Create a token
 */
NimToken nim_lexer_make_token(NimLexer* lexer, NimTokenType type);

/**
 * Create an error token
 */
NimToken nim_lexer_error_token(NimLexer* lexer, const char* message);

/**
 * Get string representation of token type
 */
const char* nim_token_type_str(NimTokenType type);

#endif // NIM_LEXER_H
