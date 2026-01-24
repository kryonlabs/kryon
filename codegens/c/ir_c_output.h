/**
 * C Code Generator - Output Utilities
 *
 * Basic output functions for writing indented C code.
 */

#ifndef IR_C_OUTPUT_H
#define IR_C_OUTPUT_H

#include "ir_c_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write indentation at current level
 */
void c_write_indent(CCodegenContext* ctx);

/**
 * Write a line with indentation
 */
void c_writeln(CCodegenContext* ctx, const char* str);

/**
 * Write raw text without indentation
 */
void c_write_raw(CCodegenContext* ctx, const char* str);

#ifdef __cplusplus
}
#endif

#endif // IR_C_OUTPUT_H
