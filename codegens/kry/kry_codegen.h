#ifndef KRY_CODEGEN_H
#define KRY_CODEGEN_H

#include "../../ir/include/ir_core.h"
#include <stdbool.h>

// Kry codegen options
typedef struct KryCodegenOptions {
    bool format;                  // Run formatter if available
    bool preserve_comments;       // Preserve comments from original
} KryCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate .kry code from KIR JSON file
 *
 * Reads a .kir file and generates .kry code.
 * The generated code includes:
 * - Component tree using .kry syntax
 * - Event handlers from logic_block section
 * - Properties and styling
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .kry file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = kry_codegen_generate("app.kir", "app.kry");
 */
bool kry_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate .kry code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated .kry code, or NULL on error. Caller must free.
 */
char* kry_codegen_from_json(const char* kir_json);

/**
 * Generate .kry code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .kry file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool kry_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        KryCodegenOptions* options);

/**
 * Generate multi-file .kry code from main KIR file
 *
 * Reads main.kir file, generates .kry code for main and all imported modules.
 * Preserves directory structure similar to other multi-file codegens.
 *
 * @param kir_path Path to main .kir JSON file
 * @param output_dir Directory where generated .kry files should be written
 * @return bool true on success, false on error
 */
bool kry_codegen_generate_multi(const char* kir_path, const char* output_dir);

#ifdef __cplusplus
}
#endif

#endif // KRY_CODEGEN_H
