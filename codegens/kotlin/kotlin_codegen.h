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

/**
 * Generate multiple Kotlin files from multi-file KIR by reading linked KIR files
 *
 * This function reads the main.kir file, generates Kotlin code from its KIR representation,
 * then follows the imports array to find and process linked component KIR files.
 * Generates MainActivity.kt as entry and one .kt per component with package structure.
 *
 * @param kir_path Path to main.kir JSON file (usually .kryon_cache/main.kir)
 * @param output_dir Directory where generated Kotlin files should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = ir_generate_kotlin_code_multi(".kryon_cache/main.kir", "output/");
 */
bool ir_generate_kotlin_code_multi(const char* kir_path, const char* output_dir);

#endif // IR_KOTLIN_CODEGEN_H
