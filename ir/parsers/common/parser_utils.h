/**
 * Kryon Parser - Common Utilities
 *
 * Shared utilities for all parsers to reduce code duplication.
 * Provides common error handling, memory management, and AST node patterns.
 */

#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Error Handling System
// ============================================================================

/**
 * Parser error levels
 */
typedef enum {
    PARSER_ERROR_INFO,
    PARSER_ERROR_WARNING,
    PARSER_ERROR_ERROR,
    PARSER_ERROR_FATAL
} ParserErrorLevel;

/**
 * Parser error codes (common across all parsers)
 */
typedef enum {
    PARSER_OK = 0,
    PARSER_ERROR_SYNTAX,
    PARSER_ERROR_LEXICAL,
    PARSER_ERROR_MEMORY,
    PARSER_ERROR_IO,
    PARSER_ERROR_UNKNOWN,
    PARSER_ERROR_BUFFER_OVERFLOW,
    PARSER_ERROR_UNTERMINATED_STRING
} ParserErrorCode;

/**
 * Parser error structure
 */
typedef struct {
    ParserErrorCode code;
    ParserErrorLevel level;
    char message[512];
    int line;
    int column;
    const char* source_file;
} ParserError;

/**
 * Set a parser error with formatted message.
 * This is a generic error setter - each parser should have its own
 * error context that wraps this.
 *
 * @param error Error structure to populate
 * @param code Error code
 * @param level Error level
 * @param line Line number (0 if unknown)
 * @param column Column number (0 if unknown)
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void parser_error_set(ParserError* error, ParserErrorCode code, ParserErrorLevel level,
                      int line, int column, const char* format, ...);

/**
 * Set a parser error with va_list.
 *
 * @param error Error structure to populate
 * @param code Error code
 * @param level Error level
 * @param line Line number
 * @param column Column number
 * @param format Printf-style format string
 * @param args Variable arguments list
 */
void parser_error_set_v(ParserError* error, ParserErrorCode code, ParserErrorLevel level,
                        int line, int column, const char* format, va_list args);

/**
 * Get error message string from error code.
 *
 * @param code Error code
 * @return Static string describing the error
 */
const char* parser_error_string(ParserErrorCode code);

/**
 * Format error for display.
 * Returns a pointer to a static buffer.
 *
 * @param error Error structure
 * @return Formatted error string
 */
const char* parser_error_format(const ParserError* error);

// ============================================================================
// Memory Management Utilities
// ============================================================================

/**
 * Safe strdup that handles NULL input.
 *
 * @param str String to duplicate (can be NULL)
 * @return Allocated copy or NULL
 */
char* parser_strdup(const char* str);

/**
 * Safe strndup that handles NULL input.
 *
 * @param str String to duplicate (can be NULL)
 * @param n Maximum number of characters to copy
 * @return Allocated copy or NULL
 */
char* parser_strndup(const char* str, size_t n);

/**
 * Allocate and zero memory.
 *
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* parser_calloc(size_t size);

/**
 * Reallocate memory with NULL safety.
 *
 * @param ptr Existing pointer (can be NULL)
 * @param size New size
 * @return Pointer to reallocated memory, or NULL on failure
 */
void* parser_realloc(void* ptr, size_t size);

// ============================================================================
// String Parsing Utilities
// ============================================================================

/**
 * Parse an identifier from source string.
 * Identifier: [a-zA-Z_][a-zA-Z0-9_-]*
 *
 * @param source Source string
 * @param pos Current position (updated on success)
 * @param max_len Maximum length of source
 * @return Newly allocated identifier string, or NULL on failure
 */
char* parser_parse_identifier(const char* source, size_t* pos, size_t max_len);

/**
 * Parse a quoted string literal.
 * Handles escape sequences like \n, \t, \", \\, etc.
 *
 * @param source Source string
 * @param pos Current position (updated on success)
 * @param max_len Maximum length of source
 * @param out_error Optional error output
 * @return Newly allocated string content (without quotes), or NULL on failure
 */
char* parser_parse_string(const char* source, size_t* pos, size_t max_len,
                          ParserError* out_error);

/**
 * Parse a numeric literal (integer or floating point).
 *
 * @param source Source string
 * @param pos Current position (updated on success)
 * @param max_len Maximum length of source
 * @return Parsed numeric value, or 0.0 on failure
 */
double parser_parse_number(const char* source, size_t* pos, size_t max_len);

/**
 * Skip whitespace in source string.
 *
 * @param source Source string
 * @param pos Current position (updated)
 * @param max_len Maximum length of source
 */
void parser_skip_whitespace(const char* source, size_t* pos, size_t max_len);

/**
 * Peek at character at current position.
 *
 * @param source Source string
 * @param pos Current position
 * @param max_len Maximum length of source
 * @return Character at position, or '\0' if out of bounds
 */
char parser_peek(const char* source, size_t pos, size_t max_len);

/**
 * Check if current character matches expected and advance.
 *
 * @param source Source string
 * @param pos Current position (updated if match)
 * @param max_len Maximum length of source
 * @param expected Expected character
 * @return true if matched and advanced, false otherwise
 */
bool parser_match(const char* source, size_t* pos, size_t max_len, char expected);

/**
 * Advance position by one.
 *
 * @param source Source string
 * @param pos Current position (updated)
 * @param max_len Maximum length of source
 * @return Character at position before advance
 */
char parser_advance(const char* source, size_t* pos, size_t max_len);

// ============================================================================
// AST Node Creation Helpers
// ============================================================================

/**
 * Generic AST node structure (base type for all parser nodes)
 */
typedef struct ParserASTNode {
    int node_type;
    int line;
    int column;
    struct ParserASTNode** children;
    size_t child_count;
    size_t child_capacity;
} ParserASTNode;

/**
 * Initialize a base AST node.
 *
 * @param node Node to initialize
 * @param node_type Type identifier for the node
 * @param line Line number
 * @param column Column number
 */
void parser_ast_node_init(ParserASTNode* node, int node_type, int line, int column);

/**
 * Add a child to an AST node.
 *
 * @param node Parent node
 * @param child Child node to add
 * @return true on success, false on allocation failure
 */
bool parser_ast_add_child(ParserASTNode* node, ParserASTNode* child);

/**
 * Free all children of an AST node.
 * Does not free the parent node itself.
 *
 * @param node Node whose children should be freed
 * @param child_free Function to free individual children (NULL for simple free)
 */
void parser_ast_free_children(ParserASTNode* node,
                              void (*child_free)(ParserASTNode* child));

// ============================================================================
// File Reading Utilities
// ============================================================================

/**
 * Read entire file into a null-terminated string.
 * Caller is responsible for freeing the returned buffer.
 *
 * @param filepath Path to file
 * @param out_size Optional output for file size
 * @param out_error Optional error output
 * @return File contents as string, or NULL on failure
 */
char* parser_read_file(const char* filepath, size_t* out_size, ParserError* out_error);

/**
 * Get file size.
 *
 * @param filepath Path to file
 * @return File size in bytes, or -1 on error
 */
long parser_get_file_size(const char* filepath);

// ============================================================================
// Buffer Management
// ============================================================================

/**
 * Dynamic string buffer for parsing
 */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} ParserBuffer;

/**
 * Initialize a parser buffer.
 *
 * @param buf Buffer to initialize
 * @param initial_capacity Starting capacity
 * @return true on success, false on allocation failure
 */
bool parser_buffer_init(ParserBuffer* buf, size_t initial_capacity);

/**
 * Append character to buffer.
 *
 * @param buf Buffer to append to
 * @param ch Character to append
 * @return true on success, false on allocation failure
 */
bool parser_buffer_append(ParserBuffer* buf, char ch);

/**
 * Append string to buffer.
 *
 * @param buf Buffer to append to
 * @param str String to append
 * @return true on success, false on allocation failure
 */
bool parser_buffer_append_str(ParserBuffer* buf, const char* str);

/**
 * Null-terminate and get buffer contents.
 * Returns the internal buffer pointer (don't free separately).
 *
 * @param buf Buffer
 * @return Null-terminated string
 */
char* parser_buffer_get(ParserBuffer* buf);

/**
 * Free buffer resources.
 *
 * @param buf Buffer to free
 */
void parser_buffer_free(ParserBuffer* buf);

/**
 * Reset buffer to empty (keeps allocated memory).
 *
 * @param buf Buffer to reset
 */
void parser_buffer_reset(ParserBuffer* buf);

#ifdef __cplusplus
}
#endif

#endif // PARSER_UTILS_H
