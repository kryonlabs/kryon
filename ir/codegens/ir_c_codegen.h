/**
 * C Code Generator - KIR → C Source Code
 *
 * Generates idiomatic C code with Kryon DSL from KIR JSON files
 * Supports full round-trip conversion: C → KIR → C
 */

#ifndef IR_C_CODEGEN_H
#define IR_C_CODEGEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate C source code from a KIR file
 *
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .c file
 * @return true on success, false on error
 */
bool ir_generate_c_code(const char* kir_path, const char* output_path);

/**
 * Generate C source code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @param output_path Path to output .c file
 * @return true on success, false on error
 */
bool ir_generate_c_code_from_string(const char* kir_json, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // IR_C_CODEGEN_H
