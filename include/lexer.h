/**
 * @file lexer.h
 * @brief Kryon Lexical Analyzer (Lexer)
 * 
 * Complete tokenization system for KRY language syntax with Unicode support,
 * error recovery, and position tracking for debugging.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_LEXER_H
#define KRYON_INTERNAL_LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonLexer KryonLexer;
typedef struct KryonToken KryonToken;
typedef struct KryonSourceLocation KryonSourceLocation;

// =============================================================================
// TOKEN TYPES
// =============================================================================

/**
 * @brief Token types for KRY language
 */
typedef enum {
    // End of input
    KRYON_TOKEN_EOF = 0,
    
    // Literals
    KRYON_TOKEN_STRING,              // "hello world"
    KRYON_TOKEN_INTEGER,             // 42, -10
    KRYON_TOKEN_FLOAT,               // 3.14, -2.5
    KRYON_TOKEN_BOOLEAN_TRUE,        // true
    KRYON_TOKEN_BOOLEAN_FALSE,       // false
    KRYON_TOKEN_NULL,                // null
    
    // Identifiers and keywords
    KRYON_TOKEN_IDENTIFIER,          // element_name, property_name
    KRYON_TOKEN_VARIABLE,            // $variable_name
    KRYON_TOKEN_ELEMENT_TYPE,        // Button, Text, Container (capitalized)
    
    // Operators
    KRYON_TOKEN_COLON,               // :
    KRYON_TOKEN_SEMICOLON,           // ;
    KRYON_TOKEN_DOT,                 // .
    KRYON_TOKEN_RANGE,               // ..
    KRYON_TOKEN_COMMA,               // ,
    KRYON_TOKEN_EQUALS,              // =
    KRYON_TOKEN_NOT_EQUALS,          // !=
    KRYON_TOKEN_LESS_THAN,           // <
    KRYON_TOKEN_LESS_EQUAL,          // <=
    KRYON_TOKEN_GREATER_THAN,        // >
    KRYON_TOKEN_GREATER_EQUAL,       // >=
    KRYON_TOKEN_LOGICAL_AND,         // &&
    KRYON_TOKEN_LOGICAL_OR,          // ||
    KRYON_TOKEN_LOGICAL_NOT,         // !
    KRYON_TOKEN_PLUS,                // +
    KRYON_TOKEN_MINUS,               // -
    KRYON_TOKEN_MULTIPLY,            // *
    KRYON_TOKEN_DIVIDE,              // /
    KRYON_TOKEN_MODULO,              // %
    KRYON_TOKEN_QUESTION,            // ?
    KRYON_TOKEN_AMPERSAND,           // &
    
    // Brackets and braces
    KRYON_TOKEN_LEFT_BRACE,          // {
    KRYON_TOKEN_RIGHT_BRACE,         // }
    KRYON_TOKEN_LEFT_BRACKET,        // [
    KRYON_TOKEN_RIGHT_BRACKET,       // ]
    KRYON_TOKEN_LEFT_PAREN,          // (
    KRYON_TOKEN_RIGHT_PAREN,         // )
    
    // Special tokens
    KRYON_TOKEN_AT,                  // @
    KRYON_TOKEN_HASH,                // #
    KRYON_TOKEN_DOLLAR,              // $ (for variables)
    
    // Keywords
    KRYON_TOKEN_STYLE_KEYWORD,       // style
    KRYON_TOKEN_EXTENDS_KEYWORD,     // extends
    
    // Directives
    KRYON_TOKEN_STYLE_DIRECTIVE,     // @style
    KRYON_TOKEN_STYLES_DIRECTIVE,    // @styles
    KRYON_TOKEN_THEME_DIRECTIVE,     // @theme
    KRYON_TOKEN_VARIABLE_DIRECTIVE,  // @var
    KRYON_TOKEN_VARIABLES_DIRECTIVE, // @variables
    KRYON_TOKEN_FUNCTION_DIRECTIVE,  // @function
    KRYON_TOKEN_STORE_DIRECTIVE,     // @store
    KRYON_TOKEN_WATCH_DIRECTIVE,     // @watch
    KRYON_TOKEN_ON_MOUNT_DIRECTIVE,  // @on_mount
    KRYON_TOKEN_ON_UNMOUNT_DIRECTIVE,// @on_unmount
    KRYON_TOKEN_IMPORT_DIRECTIVE,    // @import
    KRYON_TOKEN_EXPORT_DIRECTIVE,    // @export
    KRYON_TOKEN_INCLUDE_DIRECTIVE,   // @include
    KRYON_TOKEN_METADATA_DIRECTIVE,  // @metadata
    KRYON_TOKEN_EVENT_DIRECTIVE,     // @event
    KRYON_TOKEN_COMPONENT_DIRECTIVE, // @component
    KRYON_TOKEN_PROPS_DIRECTIVE,     // @props
    KRYON_TOKEN_SLOTS_DIRECTIVE,     // @slots
    KRYON_TOKEN_LIFECYCLE_DIRECTIVE, // @lifecycle
    KRYON_TOKEN_STATE_DIRECTIVE,     // @state
    KRYON_TOKEN_CONST_DIRECTIVE,     // @const
    KRYON_TOKEN_ONLOAD_DIRECTIVE,    // @onload
    KRYON_TOKEN_FOR_DIRECTIVE,       // @for
    KRYON_TOKEN_CONST_FOR_DIRECTIVE, // @const_for
    
    // Keywords
    KRYON_TOKEN_IN_KEYWORD,          // in (for @const_for loops)
    
    // Template interpolation
    KRYON_TOKEN_TEMPLATE_START,      // ${
    KRYON_TOKEN_TEMPLATE_END,        // }
    
    // Units
    KRYON_TOKEN_UNIT_PX,             // px
    KRYON_TOKEN_UNIT_PERCENT,        // %
    KRYON_TOKEN_UNIT_EM,             // em
    KRYON_TOKEN_UNIT_REM,            // rem
    KRYON_TOKEN_UNIT_VW,             // vw
    KRYON_TOKEN_UNIT_VH,             // vh
    KRYON_TOKEN_UNIT_PT,             // pt
    
    // Script content
    KRYON_TOKEN_SCRIPT_CONTENT,      // Raw script content in function bodies
    
    // Comments
    KRYON_TOKEN_LINE_COMMENT,        // # comment
    KRYON_TOKEN_BLOCK_COMMENT,       // /* comment */
    
    // Whitespace and newlines
    KRYON_TOKEN_WHITESPACE,          // spaces, tabs
    KRYON_TOKEN_NEWLINE,             // \n, \r\n
    
    // Error token
    KRYON_TOKEN_ERROR,               // Invalid token
    
    // Total count
    KRYON_TOKEN_COUNT
} KryonTokenType;

// =============================================================================
// SOURCE LOCATION
// =============================================================================

/**
 * @brief Source code location for debugging
 */
struct KryonSourceLocation {
    const char *filename;            ///< Source filename
    uint32_t line;                   ///< Line number (1-based)
    uint32_t column;                 ///< Column number (1-based)
    uint32_t offset;                 ///< Byte offset from start
    uint32_t length;                 ///< Token length in bytes
};

// =============================================================================
// TOKEN STRUCTURE
// =============================================================================

/**
 * @brief Token with value and location information
 */
struct KryonToken {
    KryonTokenType type;             ///< Token type
    const char *lexeme;              ///< Original text (not null-terminated)
    size_t lexeme_length;            ///< Length of lexeme
    KryonSourceLocation location;    ///< Source location
    
    // Token value (union for different types)
    union {
        char *string_value;          ///< String literal value (allocated)
        int64_t int_value;           ///< Integer value
        double float_value;          ///< Float value
        bool bool_value;             ///< Boolean value
    } value;
    
    bool has_value;                  ///< Whether value union is valid
};

// =============================================================================
// LEXER CONFIGURATION
// =============================================================================

/**
 * @brief Lexer configuration options
 */
typedef struct {
    bool preserve_whitespace;        ///< Keep whitespace tokens
    bool preserve_comments;          ///< Keep comment tokens
    bool track_positions;            ///< Track detailed position info
    bool unicode_support;            ///< Enable Unicode processing
    bool strict_mode;                ///< Strict syntax checking
} KryonLexerConfig;

// =============================================================================
// LEXER STRUCTURE
// =============================================================================

/**
 * @brief Kryon lexer state
 */
struct KryonLexer {
    // Input
    const char *source;              ///< Source code text
    size_t source_length;            ///< Length of source
    const char *filename;            ///< Source filename (for errors)
    
    // Current position
    const char *current;             ///< Current character pointer
    const char *start;               ///< Start of current token
    uint32_t line;                   ///< Current line (1-based)
    uint32_t column;                 ///< Current column (1-based)
    uint32_t offset;                 ///< Current byte offset
    
    // Configuration
    KryonLexerConfig config;         ///< Lexer configuration
    
    // Token buffer
    KryonToken *tokens;              ///< Array of tokens
    size_t token_count;              ///< Number of tokens
    size_t token_capacity;           ///< Token array capacity
    size_t current_token;            ///< Current token index for parsing
    
    // State tracking for context-sensitive parsing
    bool expecting_script_body;      ///< Whether we're expecting a function body
    
    // Error tracking
    bool has_error;                  ///< Whether lexer encountered errors
    char error_message[256];         ///< Last error message
    KryonSourceLocation error_location; ///< Error location
    
    // Statistics
    uint32_t total_lines;            ///< Total lines processed
    uint32_t total_tokens;           ///< Total tokens generated
    double processing_time;          ///< Time spent lexing (seconds)
};

// =============================================================================
// LEXER API
// =============================================================================

/**
 * @brief Create a new lexer
 * @param source Source code text
 * @param source_length Length of source (0 to calculate)
 * @param filename Source filename (can be NULL)
 * @param config Lexer configuration (can be NULL for defaults)
 * @return Pointer to lexer, or NULL on failure
 */
KryonLexer *kryon_lexer_create(const char *source, size_t source_length,
                              const char *filename, const KryonLexerConfig *config);

/**
 * @brief Destroy lexer and free resources
 * @param lexer Lexer to destroy
 */
void kryon_lexer_destroy(KryonLexer *lexer);

/**
 * @brief Tokenize the entire source
 * @param lexer The lexer
 * @return true on success, false on error
 */
bool kryon_lexer_tokenize(KryonLexer *lexer);

/**
 * @brief Get next token (for incremental parsing)
 * @param lexer The lexer
 * @return Pointer to next token, or NULL at end
 */
const KryonToken *kryon_lexer_next_token(KryonLexer *lexer);

/**
 * @brief Peek at token without consuming it
 * @param lexer The lexer
 * @param offset Offset from current position (0 = current, 1 = next, etc.)
 * @return Pointer to token, or NULL if out of bounds
 */
const KryonToken *kryon_lexer_peek_token(const KryonLexer *lexer, size_t offset);

/**
 * @brief Get current token
 * @param lexer The lexer
 * @return Pointer to current token, or NULL
 */
const KryonToken *kryon_lexer_current_token(const KryonLexer *lexer);

/**
 * @brief Reset lexer to beginning
 * @param lexer The lexer
 */
void kryon_lexer_reset(KryonLexer *lexer);

/**
 * @brief Check if lexer has more tokens
 * @param lexer The lexer
 * @return true if more tokens available
 */
bool kryon_lexer_has_more_tokens(const KryonLexer *lexer);

/**
 * @brief Get all tokens as array
 * @param lexer The lexer
 * @param out_count Output for token count
 * @return Array of tokens
 */
const KryonToken *kryon_lexer_get_tokens(const KryonLexer *lexer, size_t *out_count);

/**
 * @brief Get lexer error information
 * @param lexer The lexer
 * @return Error message, or NULL if no error
 */
const char *kryon_lexer_get_error(const KryonLexer *lexer);

/**
 * @brief Check if a token type is a directive
 * @param type Token type to check
 * @return true if the token is a directive, false otherwise
 */
bool kryon_token_is_directive(KryonTokenType type);

/**
 * @brief Check if a token type is a unit
 * @param type Token type to check
 * @return true if the token is a unit, false otherwise
 */
bool kryon_token_is_unit(KryonTokenType type);

/**
 * @brief Get lexer statistics
 * @param lexer The lexer
 * @param out_lines Output for total lines
 * @param out_tokens Output for total tokens
 * @param out_time Output for processing time
 */
void kryon_lexer_get_stats(const KryonLexer *lexer, uint32_t *out_lines,
                          uint32_t *out_tokens, double *out_time);

// =============================================================================
// TOKEN UTILITIES
// =============================================================================

/**
 * @brief Get token type name
 * @param type Token type
 * @return Token type name string
 */
const char *kryon_token_type_name(KryonTokenType type);

/**
 * @brief Check if token type is a literal
 * @param type Token type
 * @return true if token is a literal value
 */
bool kryon_token_is_literal(KryonTokenType type);

/**
 * @brief Check if token type is an operator
 * @param type Token type
 * @return true if token is an operator
 */
bool kryon_token_is_operator(KryonTokenType type);

/**
 * @brief Check if token type is a directive
 * @param type Token type
 * @return true if token is a directive (@style, @store, etc.)
 */
bool kryon_token_is_directive(KryonTokenType type);

/**
 * @brief Check if token type is a bracket/brace
 * @param type Token type
 * @return true if token is a bracket or brace
 */
bool kryon_token_is_bracket(KryonTokenType type);

/**
 * @brief Format source location as string
 * @param location Source location
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 * @return Number of characters written
 */
size_t kryon_source_location_format(const KryonSourceLocation *location,
                                   char *buffer, size_t buffer_size);

/**
 * @brief Copy token lexeme to null-terminated string
 * @param token The token
 * @return Allocated string copy (caller must free)
 */
char *kryon_token_copy_lexeme(const KryonToken *token);

/**
 * @brief Compare token lexeme with string
 * @param token The token
 * @param str String to compare
 * @return true if lexeme matches string
 */
bool kryon_token_lexeme_equals(const KryonToken *token, const char *str);

/**
 * @brief Print token for debugging
 * @param token The token
 * @param file Output file (NULL for stdout)
 */
void kryon_token_print(const KryonToken *token, FILE *file);

// =============================================================================
// LEXER CONFIGURATION HELPERS
// =============================================================================

/**
 * @brief Get default lexer configuration
 * @return Default configuration
 */
KryonLexerConfig kryon_lexer_default_config(void);

/**
 * @brief Create configuration for minimal parsing
 * @return Configuration without whitespace/comments
 */
KryonLexerConfig kryon_lexer_minimal_config(void);

/**
 * @brief Create configuration for IDE/debugging
 * @return Configuration with all information preserved
 */
KryonLexerConfig kryon_lexer_ide_config(void);

// =============================================================================
// KEYWORD RECOGNITION
// =============================================================================

/**
 * @brief Check if string is a reserved keyword
 * @param str String to check
 * @param length Length of string
 * @return Token type if keyword, KRYON_TOKEN_IDENTIFIER otherwise
 */
KryonTokenType kryon_lexer_classify_keyword(const char *str, size_t length);

/**
 * @brief Check if string is a directive keyword
 * @param str String to check (without @)
 * @param length Length of string
 * @return Token type if directive, KRYON_TOKEN_IDENTIFIER otherwise
 */
KryonTokenType kryon_lexer_classify_directive(const char *str, size_t length);

// =============================================================================
// UNICODE SUPPORT
// =============================================================================

/**
 * @brief Decode UTF-8 character
 * @param source UTF-8 string
 * @param out_codepoint Output for Unicode codepoint
 * @return Number of bytes consumed, or 0 on error
 */
size_t kryon_lexer_decode_utf8(const char *source, uint32_t *out_codepoint);

/**
 * @brief Check if Unicode codepoint is valid for identifier
 * @param codepoint Unicode codepoint
 * @param is_start true if checking start of identifier
 * @return true if valid for identifier
 */
bool kryon_lexer_is_identifier_char(uint32_t codepoint, bool is_start);

/**
 * @brief Check if Unicode codepoint is whitespace
 * @param codepoint Unicode codepoint
 * @return true if whitespace
 */
bool kryon_lexer_is_whitespace(uint32_t codepoint);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_LEXER_H