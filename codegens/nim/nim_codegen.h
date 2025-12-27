#ifndef NIM_CODEGEN_H
#define NIM_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Nim codegen options
typedef struct NimCodegenOptions {
    bool format;                  // Run nimpretty formatter
    bool optimize;                // Generate optimized code
} NimCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Nim DSL code from KIR JSON file
 *
 * Reads a .kir file and generates Nim code that uses the kryon_dsl module.
 * The generated code includes:
 * - Import statements (kryon_dsl, reactive_system if needed)
 * - Reactive variable declarations
 * - Component tree using Nim DSL syntax
 * - Event handlers from sources section
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .nim file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = nim_codegen_generate("app.kir", "app.nim");
 */
bool nim_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate Nim DSL code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated Nim code, or NULL on error. Caller must free.
 */
char* nim_codegen_from_json(const char* kir_json);

/**
 * Generate Nim DSL code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .nim file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool nim_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        NimCodegenOptions* options);

#ifdef __cplusplus
}
#endif

#endif // NIM_CODEGEN_H
