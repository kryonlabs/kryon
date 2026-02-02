/**
 * @file tcl_codegen.h
 * @brief Tcl Language Code Generator
 *
 * Pure Tcl code generation without toolkit dependencies.
 * Handles Tcl syntax, variables, functions, and string escaping.
 */

#ifndef TCL_CODEGEN_H
#define TCL_CODEGEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Tcl Codegen Context
// ============================================================================

/**
 * Tcl code generation context
 * Maintains state during code generation
 */
typedef struct {
    FILE* output;           // Output file
    int indent_level;       // Current indentation level
    int indent_spaces;      // Spaces per indent (default: 4)
    bool use_tabs;          // Use tabs instead of spaces
    size_t line_length;     // Current line length (for formatting)
    int max_line_width;     // Maximum line width (0 = no limit)
    bool in_proc;           // Inside a proc definition
    bool needs_brace;       // Current expression needs braces
} TclCodegenContext;

/**
 * Initialize Tcl codegen context
 * @param ctx Context to initialize
 * @param output Output file
 * @return true on success, false on failure
 */
bool tcl_codegen_init(TclCodegenContext* ctx, FILE* output);

/**
 * Cleanup Tcl codegen context
 * @param ctx Context to cleanup
 */
void tcl_codegen_cleanup(TclCodegenContext* ctx);

// ============================================================================
// Tcl Syntax Generation
// ============================================================================

/**
 * Generate variable declaration
 * @param ctx Codegen context
 * @param name Variable name
 * @param value Initial value (or NULL for uninitialized)
 * @return true on success, false on failure
 */
bool tcl_gen_variable(TclCodegenContext* ctx, const char* name, const char* value);

/**
 * Generate variable set command
 * @param ctx Codegen context
 * @param name Variable name
 * @param value Value to set
 * @return true on success, false on failure
 */
bool tcl_gen_set(TclCodegenContext* ctx, const char* name, const char* value);

/**
 * Generate variable get (with $ substitution)
 * @param ctx Codegen context
 * @param name Variable name
 * @return true on success, false on failure
 */
bool tcl_gen_get(TclCodegenContext* ctx, const char* name);

/**
 * Generate procedure definition
 * @param ctx Codegen context
 * @param name Procedure name
 * @param args Arguments (comma-separated)
 * @param body Procedure body (multi-line)
 * @return true on success, false on failure
 */
bool tcl_gen_proc(TclCodegenContext* ctx, const char* name, const char* args, const char* body);

/**
 * Generate procedure call
 * @param ctx Codegen context
 * @param proc Procedure name
 * @param args Arguments (comma-separated)
 * @return true on success, false on failure
 */
bool tcl_gen_call(TclCodegenContext* ctx, const char* proc, const char* args);

/**
 * Generate if statement
 * @param ctx Codegen context
 * @param condition Condition expression
 * @param then_body Then branch (multi-line)
 * @param else_body Else branch (multi-line, or NULL)
 * @return true on success, false on failure
 */
bool tcl_gen_if(TclCodegenContext* ctx, const char* condition, const char* then_body, const char* else_body);

/**
 * Generate for loop
 * @param ctx Codegen context
 * @param var Loop variable
 * @param list List to iterate
 * @param body Loop body (multi-line)
 * @return true on success, false on failure
 */
bool tcl_gen_for(TclCodegenContext* ctx, const char* var, const char* list, const char* body);

/**
 * Generate while loop
 * @param ctx Codegen context
 * @param condition Loop condition
 * @param body Loop body (multi-line)
 * @return true on success, false on failure
 */
bool tcl_gen_while(TclCodegenContext* ctx, const char* condition, const char* body);

/**
 * Generate comment
 * @param ctx Codegen context
 * @param text Comment text (can be multi-line)
 * @return true on success, false on failure
 */
bool tcl_gen_comment(TclCodegenContext* ctx, const char* text);

// ============================================================================
// Tcl String Handling
// ============================================================================

/**
 * Escape a string for Tcl
 * Handles backslashes, quotes, brackets, braces, dollar signs
 * @param input Input string
 * @return Newly allocated escaped string (caller must free), or NULL on error
 */
char* tcl_escape_string(const char* input);

/**
 * Quote a string for Tcl
 * Adds braces if needed for safe substitution
 * @param input Input string
 * @return Newly allocated quoted string (caller must free), or NULL on error
 */
char* tcl_quote_string(const char* input);

/**
 * Generate a string literal
 * @param ctx Codegen context
 * @param str String to output
 * @return true on success, false on failure
 */
bool tcl_gen_string_literal(TclCodegenContext* ctx, const char* str);

// ============================================================================
// Tcl Expression Support
// ============================================================================

/**
 * Generate a list literal
 * @param ctx Codegen context
 * @param elements Array of element strings
 * @param count Number of elements
 * @return true on success, false on failure
 */
bool tcl_gen_list(TclCodegenContext* ctx, const char** elements, size_t count);

/**
 * Generate a dict literal
 * @param ctx Codegen context
 * @param keys Array of keys
 * @param values Array of values
 * @param count Number of key-value pairs
 * @return true on success, false on failure
 */
bool tcl_gen_dict(TclCodegenContext* ctx, const char** keys, const char** values, size_t count);

/**
 * Generate a command substitution
 * @param ctx Codegen context
 * @param command Command to substitute
 * @return true on success, false on failure
 */
bool tcl_gen_subst(TclCodegenContext* ctx, const char* command);

// ============================================================================
// Tcl Codegen Utilities
// ============================================================================

/**
 * Write indented line
 * @param ctx Codegen context
 * @param text Text to write (without newline)
 * @return true on success, false on failure
 */
bool tcl_write_line(TclCodegenContext* ctx, const char* text);

/**
 * Increase indentation
 * @param ctx Codegen context
 */
void tcl_indent(TclCodegenContext* ctx);

/**
 * Decrease indentation
 * @param ctx Codegen context
 */
void tcl_dedent(TclCodegenContext* ctx);

/**
 * Generate a newline
 * @param ctx Codegen context
 * @return true on success, false on failure
 */
bool tcl_gen_newline(TclCodegenContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // TCL_CODEGEN_H
