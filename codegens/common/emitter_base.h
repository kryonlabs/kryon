/**
 * @file emitter_base.h
 * @brief Shared Emitter Utilities for All Codegens
 *
 * Provides a common base context and utility functions for code emitters.
 * Extracts reusable patterns from the C codegen's modular structure.
 *
 * Benefits:
 * - Consistent emitter API across all codegens
 * - Reduced code duplication
 * - Shared indentation and formatting logic
 * - Standardized error handling
 */

#ifndef EMITTER_BASE_H
#define EMITTER_BASE_H

#include "../codegen_options.h"
#include "../codegen_common.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Emitter Context
 * ============================================================================ */

/**
 * @brief Base context for all code emitters
 *
 * This structure provides common state and utilities for code generation.
 * Language-specific emitters can extend this with additional fields.
 */
typedef struct {
    StringBuilder* output;        /**< Output string builder */
    int indent_level;             /**< Current indentation level */
    int indent_size;              /**< Spaces per indent level */
    char* indent_string;          /**< Custom indent string (NULL for default) */
    CodegenOptionsBase* opts;     /**< Codegen options (may be NULL) */
    void* language_data;          /**< Language-specific context (opaque) */
    bool at_line_start;           /**< Track if we're at line start for auto-indent */
} CodegenEmitterContext;

/* ============================================================================
 * Context Lifecycle
 * ============================================================================ */

/**
 * @brief Initialize emitter context
 *
 * Creates a new emitter context with default settings.
 *
 * @param ctx Context structure to initialize
 * @param opts Codegen options (may be NULL for defaults)
 * @return true on success, false on failure
 */
bool emitter_init(CodegenEmitterContext* ctx, CodegenOptionsBase* opts);

/**
 * @brief Cleanup emitter context
 *
 * Releases resources associated with the emitter context.
 * Does NOT free the context structure itself.
 *
 * @param ctx Context structure to cleanup
 */
void emitter_cleanup(CodegenEmitterContext* ctx);

/* ============================================================================
 * Indentation Management
 * ============================================================================ */

/**
 * @brief Increase indentation level
 *
 * @param ctx Emitter context
 */
void emitter_indent(CodegenEmitterContext* ctx);

/**
 * @brief Decrease indentation level
 *
 * @param ctx Emitter context
 */
void emitter_dedent(CodegenEmitterContext* ctx);

/**
 * @brief Get current indentation string
 *
 * Returns the current indentation as a string (spaces or tabs).
 * The returned string is valid until the next emitter call.
 *
 * @param ctx Emitter context
 * @return Indentation string (static buffer, do not free)
 */
const char* emitter_get_indent(CodegenEmitterContext* ctx);

/* ============================================================================
 * Output Functions
 * ============================================================================ */

/**
 * @brief Write a line with automatic indentation
 *
 * Writes the text followed by a newline, with automatic indentation.
 * Sets at_line_start to true for the next call.
 *
 * @param ctx Emitter context
 * @param text Text to write (without newline)
 */
void emitter_write_line(CodegenEmitterContext* ctx, const char* text);

/**
 * @brief Write text without indentation or newline
 *
 * Writes raw text to the output. Does not add indentation or newline.
 * Useful for continuing a line from a previous write.
 *
 * @param ctx Emitter context
 * @param text Text to write
 */
void emitter_write(CodegenEmitterContext* ctx, const char* text);

/**
 * @brief Write formatted text with automatic indentation
 *
 * Like printf but with indentation support. Adds a newline at the end.
 *
 * @param ctx Emitter context
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void emitter_write_line_fmt(CodegenEmitterContext* ctx, const char* fmt, ...);

/**
 * @brief Write formatted text without indentation or newline
 *
 * Like printf but without indentation or newline.
 *
 * @param ctx Emitter context
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void emitter_write_fmt(CodegenEmitterContext* ctx, const char* fmt, ...);

/**
 * @brief Write a newline
 *
 * @param ctx Emitter context
 */
void emitter_newline(CodegenEmitterContext* ctx);

/**
 * @brief Write blank lines
 *
 * Writes the specified number of blank lines.
 *
 * @param ctx Emitter context
 * @param count Number of blank lines to write
 */
void emitter_blank_lines(CodegenEmitterContext* ctx, int count);

/* ============================================================================
 * Code Generation Helpers
 * ============================================================================ */

/**
 * @brief Write a comment line
 *
 * Writes a comment with the appropriate comment syntax for the language.
 * Default implementation uses C-style comments (//).
 * Override by setting language_data with custom comment handler.
 *
 * @param ctx Emitter context
 * @param text Comment text
 */
void emitter_write_comment(CodegenEmitterContext* ctx, const char* text);

/**
 * @brief Write a block comment
 *
 * Writes a multi-line comment with the appropriate syntax.
 * Default implementation uses C-style block comments.
 *
 * @param ctx Emitter context
 * @param text Comment text (may contain newlines)
 */
void emitter_write_block_comment(CodegenEmitterContext* ctx, const char* text);

/**
 * @brief Start a code block
 *
 * Writes an opening brace and increases indentation.
 * Default implementation uses C-style braces ({).
 *
 * @param ctx Emitter context
 */
void emitter_start_block(CodegenEmitterContext* ctx);

/**
 * @brief End a code block
 *
 * Decreases indentation and writes a closing brace.
 * Default implementation uses C-style braces (}).
 *
 * @param ctx Emitter context
 */
void emitter_end_block(CodegenEmitterContext* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get the output string
 *
 * Returns the generated code as a string. Caller must free.
 *
 * @param ctx Emitter context
 * @return Newly allocated string, or NULL on error
 */
char* emitter_get_output(CodegenEmitterContext* ctx);

/**
 * @brief Clear the output buffer
 *
 * Clears all generated output but keeps the context intact.
 *
 * @param ctx Emitter context
 */
void emitter_clear_output(CodegenEmitterContext* ctx);

/**
 * @brief Reset context to initial state
 *
 * Resets indentation and clears output, but preserves options.
 *
 * @param ctx Emitter context
 */
void emitter_reset(CodegenEmitterContext* ctx);

/* ============================================================================
 * Language-Specific Extensions
 * ============================================================================ */

/**
 * @brief Set language-specific data
 *
 * Associates language-specific context with the emitter.
 * This can be used to override default behavior (e.g., comment style).
 *
 * @param ctx Emitter context
 * @param data Language-specific data (opaque pointer)
 * @param cleanup Function to call when cleaning up (may be NULL)
 */
void emitter_set_language_data(CodegenEmitterContext* ctx, void* data, void (*cleanup)(void*));

/* ============================================================================
 * Comment Style Handlers
 * ============================================================================ */

/**
 * @brief Comment style enumeration
 */
typedef enum {
    EMITTER_COMMENT_C,           /**< C-style: // comment */
    EMITTER_COMMENT_C_BLOCK,     /**< C block style comment */
    EMITTER_COMMENT_CPP,         /**< C++: // comment */
    EMITTER_COMMENT_SHELL,       /**< Shell: # comment */
    EMITTER_COMMENT_HTML,        /**< HTML: <!-- comment --> */
    EMITTER_COMMENT_LUA,         /**< Lua: -- comment */
    EMITTER_COMMENT_LIMBO,       /**< Limbo: # comment */
    EMITTER_COMMENT_MARKDOWN     /**< Markdown: HTML-style */
} EmitterCommentStyle;

/**
 * @brief Set comment style for the emitter
 *
 * Changes the comment syntax used by emitter_write_comment().
 *
 * @param ctx Emitter context
 * @param style Comment style
 */
void emitter_set_comment_style(CodegenEmitterContext* ctx, EmitterCommentStyle style);

#ifdef __cplusplus
}
#endif

#endif // EMITTER_BASE_H
