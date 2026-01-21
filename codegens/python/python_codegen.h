#ifndef PYTHON_CODEGEN_H
#define PYTHON_CODEGEN_H

#include "../../ir/include/ir_core.h"
#include <stdbool.h>

// Python codegen options
typedef struct PythonCodegenOptions {
    bool format;                  // Format output
    bool optimize;                // Generate optimized code
} PythonCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Python DSL code from KIR JSON file
 *
 * Reads a .kir file and generates Python code that uses the Kryon Python DSL.
 * The generated code includes:
 * - Import statements (kryon.dsl)
 * - Component tree using Python dataclass syntax
 * - Property assignments
 * - Children as arguments
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .py file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = python_codegen_generate("app.kir", "app.py");
 */
bool python_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate Python DSL code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated Python code, or NULL on error. Caller must free.
 */
char* python_codegen_from_json(const char* kir_json);

/**
 * Generate Python DSL code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .py file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool python_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          PythonCodegenOptions* options);

/**
 * Generate multiple Python files from multi-file KIR by reading linked KIR files
 *
 * This function reads the main.kir file, generates Python code from its KIR representation,
 * then follows the imports array to find and process linked component KIR files.
 * Each KIR file is transformed to Python using python_codegen_from_json().
 *
 * @param kir_path Path to main.kir JSON file (usually .kryon_cache/main.kir)
 * @param output_dir Directory where generated Python files should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = python_codegen_generate_multi(".kryon_cache/main.kir", "output/");
 */
bool python_codegen_generate_multi(const char* kir_path, const char* output_dir);

#ifdef __cplusplus
}
#endif

#endif // PYTHON_CODEGEN_H
