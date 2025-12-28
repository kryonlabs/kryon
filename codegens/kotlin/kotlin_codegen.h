#ifndef IR_KOTLIN_CODEGEN_H
#define IR_KOTLIN_CODEGEN_H

#include <stdbool.h>

/**
 * Generate Kotlin MainActivity code from KIR file
 *
 * @param kir_path Path to input KIR file
 * @param output_path Path to output Kotlin file
 * @return true on success, false on failure
 */
bool ir_generate_kotlin_code(const char* kir_path, const char* output_path);

/**
 * Generate Kotlin MainActivity code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @param output_path Path to output Kotlin file
 * @return true on success, false on failure
 */
bool ir_generate_kotlin_code_from_string(const char* kir_json, const char* output_path);

#endif // IR_KOTLIN_CODEGEN_H
