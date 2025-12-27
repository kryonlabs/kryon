#ifndef TSX_CODEGEN_H
#define TSX_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// TSX codegen options
typedef struct TsxCodegenOptions {
    bool format;                  // Format output
    bool use_prettier;            // Use Prettier formatter
    bool semicolons;              // Include semicolons
} TsxCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate TypeScript React (.tsx) code from KIR JSON file
 *
 * Reads a .kir file and generates TypeScript React code with:
 * - React component imports
 * - TypeScript interfaces for props
 * - Function components with type annotations
 * - useState hooks for reactive state
 * - Event handlers
 * - kryonApp configuration
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .tsx file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = tsx_codegen_generate("app.kir", "app.tsx");
 */
bool tsx_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate TSX code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated TSX code, or NULL on error. Caller must free.
 */
char* tsx_codegen_from_json(const char* kir_json);

/**
 * Generate TypeScript React code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .tsx file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool tsx_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        TsxCodegenOptions* options);

#ifdef __cplusplus
}
#endif

#endif // TSX_CODEGEN_H
