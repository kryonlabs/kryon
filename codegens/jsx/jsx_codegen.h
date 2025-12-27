#ifndef JSX_CODEGEN_H
#define JSX_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// JSX codegen options
typedef struct JsxCodegenOptions {
    bool format;                  // Format output
    bool use_prettier;            // Use Prettier formatter
    bool semicolons;              // Include semicolons
} JsxCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate JavaScript React (.jsx) code from KIR JSON file
 *
 * Reads a .kir file and generates JavaScript React code with:
 * - React component imports
 * - Function components (no TypeScript types)
 * - useState hooks for reactive state
 * - Event handlers
 * - kryonApp configuration
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .jsx file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = jsx_codegen_generate("app.kir", "app.jsx");
 */
bool jsx_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate JSX code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated JSX code, or NULL on error. Caller must free.
 */
char* jsx_codegen_from_json(const char* kir_json);

/**
 * Generate JavaScript React code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .jsx file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool jsx_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        JsxCodegenOptions* options);

#ifdef __cplusplus
}
#endif

#endif // JSX_CODEGEN_H
