/**
 * @file ir_string_builder.h
 * @brief Dynamic string builder for efficient string concatenation
 *
 * Provides a growable buffer for building strings incrementally,
 * useful for code generation and JSON serialization.
 */

#ifndef IR_STRING_BUILDER_H
#define IR_STRING_BUILDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dynamic string buffer
 */
typedef struct IRStringBuilder {
    char* buffer;        /* Current position in buffer */
    size_t size;         /* Current size/position */
    size_t capacity;     /* Total capacity */
} IRStringBuilder;

/**
 * Create a new string builder
 * @param initial_capacity Starting capacity (0 for default)
 * @return New string builder or NULL on failure
 */
IRStringBuilder* ir_sb_create(size_t initial_capacity);

/**
 * Free a string builder
 * @param sb String builder to free (NULL is safe)
 */
void ir_sb_free(IRStringBuilder* sb);

/**
 * Append a null-terminated string
 * @param sb String builder
 * @param str String to append
 * @return true on success, false on failure
 */
bool ir_sb_append(IRStringBuilder* sb, const char* str);

/**
 * Append a string with known length
 * @param sb String builder
 * @param str String data
 * @param len Length of string
 * @return true on success, false on failure
 */
bool ir_sb_append_n(IRStringBuilder* sb, const char* str, size_t len);

/**
 * Append a formatted string
 * @param sb String builder
 * @param fmt Printf-style format string
 * @return true on success, false on failure
 */
bool ir_sb_appendf(IRStringBuilder* sb, const char* fmt, ...);

/**
 * Append a string followed by a newline
 * @param sb String builder
 * @param line String to append
 * @return true on success, false on failure
 */
bool ir_sb_append_line(IRStringBuilder* sb, const char* line);

/**
 * Append a formatted string followed by a newline
 * @param sb String builder
 * @param fmt Printf-style format string
 * @return true on success, false on failure
 */
bool ir_sb_append_linef(IRStringBuilder* sb, const char* fmt, ...);

/**
 * Add indentation (2 spaces per level)
 * @param sb String builder
 * @param level Number of indent levels
 * @return true on success, false on failure
 */
bool ir_sb_indent(IRStringBuilder* sb, int level);

/**
 * Build the final string
 * This transfers ownership of the buffer to the caller.
 * The string builder is reset and can be reused.
 * @param sb String builder
 * @return Allocated string (caller must free) or NULL
 */
char* ir_sb_build(IRStringBuilder* sb);

/**
 * Clear the string builder contents
 * @param sb String builder to clear
 */
void ir_sb_clear(IRStringBuilder* sb);

/**
 * Get current length of the string
 * @param sb String builder
 * @return Current string length
 */
size_t ir_sb_length(const IRStringBuilder* sb);

/**
 * Reserve capacity in the string builder
 * @param sb String builder
 * @param capacity Minimum capacity to reserve
 * @return true on success, false on failure
 */
bool ir_sb_reserve(IRStringBuilder* sb, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif // IR_STRING_BUILDER_H
