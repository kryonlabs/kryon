/**
 * Limbo Lexer
 *
 * Lexical analyzer for Limbo source code (.b files)
 * Tokenizes Limbo source for parsing
 */

#ifndef IR_LIMBO_LEXER_H
#define IR_LIMBO_LEXER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Limbo token types
 */
typedef enum {
    // End of file
    TOKEN_EOF,

    // Identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_INTEGER,
    TOKEN_REAL,
    TOKEN_CHAR,

    // Keywords
    TOKEN_MODULE,
    TOKEN_IMPLEMENT,
    TOKEN_IMPORT,
    TOKEN_CON,
    TOKEN_FN,
    TOKEN_TYPE,
    TOKEN_ADT,
    TOKEN_PICK,
    TOKEN_ADT_TAG,
    TOKEN_WITHIN,

    // Control flow
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_DO,
    TOKEN_FOR,
    TOKEN_CASE,
    TOKEN_OF,
    TOKEN_ALT,
    TOKEN_TAG,

    // Declarations
    TOKEN_CONST,
    TOKEN_VAR,

    // Types
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING_TYPE,
    TOKEN_BYTE,
    TOKEN_LIST,
    TOKEN_ARRAY,
    TOKEN_CHAN,
    TOKEN_REF,
    TOKEN_MODULE_TYPE,

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MOD,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,

    // Comparison
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    // Assignment
    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_MUL_ASSIGN,
    TOKEN_DIV_ASSIGN,
    TOKEN_AND_ASSIGN,
    TOKEN_OR_ASSIGN,
    TOKEN_LSHIFT_ASSIGN,
    TOKEN_RSHIFT_ASSIGN,

    // Increment/Decrement
    TOKEN_INCREMENT,
    TOKEN_DECREMENT,

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_DOT_DOT,
    TOKEN_PIPE,

    // Special
    TOKEN_ARROW,
    TOKEN_DOUBLE_COLON,

    // Invalid
    TOKEN_INVALID
} LimboTokenType;

/**
 * Limbo token
 */
typedef struct {
    LimboTokenType type;
    const char* text;       // Token text (points into source)
    size_t length;          // Length of token text
    int line;               // Line number (1-based)
    int column;             // Column number (1-based)
} LimboToken;

/**
 * Limbo lexer
 */
typedef struct {
    const char* source;     // Source code
    size_t source_len;      // Source length
    size_t position;        // Current position
    int line;               // Current line
    int column;             // Current column
    LimboToken peek_token;  // Peeked token
    bool has_peek;          // Whether peek_token is valid
} LimboLexer;

/**
 * Create lexer for Limbo source
 *
 * @param source Limbo source code
 * @param source_len Length of source (0 for null-terminated)
 * @return LimboLexer* Lexer (caller must free with limbo_lexer_free)
 */
LimboLexer* limbo_lex_create(const char* source, size_t source_len);

/**
 * Free lexer
 *
 * @param lexer Lexer to free
 */
void limbo_lex_free(LimboLexer* lexer);

/**
 * Get next token from lexer
 *
 * @param lexer Lexer
 * @return LimboToken Next token
 */
LimboToken limbo_next_token(LimboLexer* lexer);

/**
 * Peek at next token without consuming it
 *
 * @param lexer Lexer
 * @return LimboToken Next token (without consuming)
 */
LimboToken limbo_peek_token(LimboLexer* lexer);

/**
 * Get token type name as string
 *
 * @param type Token type
 * @return const char* Token name
 */
const char* limbo_token_name(LimboTokenType type);

#ifdef __cplusplus
}
#endif

#endif // IR_LIMBO_LEXER_H
