/**
 * C Code Generator - Expression and Statement Transpilation
 *
 * Functions to convert KIR expressions and statements to C code.
 */

#ifndef IR_C_EXPRESSION_H
#define IR_C_EXPRESSION_H

#include "../../third_party/cJSON/cJSON.h"
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for context type (defined in ir_c_internal.h)
struct CCodegenContext;

/**
 * Check if expression is a __range__ call and extract start/end
 *
 * @param expr The expression to check
 * @param out_start Output: start expression if range call
 * @param out_end Output: end expression if range call
 * @return true if it's a range call, false otherwise
 */
bool c_is_range_call(cJSON* expr, cJSON** out_start, cJSON** out_end);

/**
 * Convert KIR expression to C code string
 *
 * Handles all expression types:
 * - Literals (string, number, boolean, null)
 * - Variable references
 * - Binary operations (arithmetic, comparison, logical)
 * - Unary operations (not, neg)
 * - Member/index access
 * - Function/method calls
 * - Array literals
 * - Ternary conditionals
 *
 * @param expr The KIR expression JSON
 * @return malloc'd C code string, caller must free
 */
char* c_expr_to_c(cJSON* expr);

/**
 * Convert KIR statement to C code, write to output file
 *
 * Handles all statement types:
 * - Variable declarations (let/const/var)
 * - Assignments
 * - Return statements
 * - Expression statements
 * - If/else
 * - While loops
 * - For loops (classic and for-each)
 * - Break/continue
 * - Block statements
 *
 * @param output The output file to write to
 * @param stmt The KIR statement JSON
 * @param indent Indentation level (spaces)
 * @param output_path Path to output file (for error messages)
 * @return true on success, false on unsupported construct (hard error)
 */
bool c_stmt_to_c(FILE* output, cJSON* stmt, int indent, const char* output_path);

/**
 * Convert KIR statement to C code with context for variable tracking
 * @param ctx Codegen context (for local variable tracking, can be NULL)
 * @param output The output file to write to
 * @param stmt The KIR statement JSON
 * @param indent Indentation level (spaces)
 * @param output_path Path to output file (for error messages)
 * @return true on success, false on unsupported construct
 */
bool c_stmt_to_c_ctx(struct CCodegenContext* ctx, FILE* output, cJSON* stmt, int indent, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // IR_C_EXPRESSION_H
