/**
 * Hare Code Generator
 * Generates Hare source code from KIR JSON
 */

#ifndef HARE_CODEGEN_H
#define HARE_CODEGEN_H

#include "../../ir/include/ir_core.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Hare source code from KIR JSON file
 *
 * Reads a .kir file and generates Hare code that uses the Kryon Hare DSL.
 * Supports multi-file output for projects with multiple modules/components.
 *
 * The generated code includes:
 * - Use statements for imports
 * - Component definitions using Hare's tagged unions
 * - Property assignments
 * - Children as array elements
 *
 * @param kir_path Path to main.kir JSON file (usually .kryon_cache/main.kir)
 * @param output_dir Directory where generated Hare files should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = hare_codegen_generate(".kryon_cache/main.kir", "output/");
 */
bool hare_codegen_generate(const char* kir_path, const char* output_dir);

/**
 * Generate Hare source code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated Hare code, or NULL on error. Caller must free.
 */
char* hare_codegen_from_json(const char* kir_json);

/**
 * Generate Hare code from KIR JSON file (legacy alias)
 *
 * This is an alias for hare_codegen_generate() for compatibility.
 * All Hare codegen uses multi-file output.
 *
 * @param kir_path Path to main.kir JSON file
 * @param output_dir Directory where generated Hare files should be written
 * @return bool true on success, false on error
 */
bool hare_codegen_generate_multi(const char* kir_path, const char* output_dir);

#ifdef __cplusplus
}
#endif

#endif // HARE_CODEGEN_H
