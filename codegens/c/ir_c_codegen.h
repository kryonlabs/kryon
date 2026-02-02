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

/**
 * Generate multiple C files from multi-file KIR by reading linked KIR files
 *
 * This function reads the main.kir file, generates C code from its KIR representation,
 * then follows the imports array to find and process linked component KIR files.
 * Generates main.c, .h/.c pairs per component with proper include guards.
 *
 * @param kir_path Path to main.kir JSON file (usually .kryon_cache/main.kir)
 * @param output_dir Directory where generated C files should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = ir_generate_c_code_multi(".kryon_cache/main.kir", "output/");
 */
bool ir_generate_c_code_multi(const char* kir_path, const char* output_dir);

/* ============================================================================
 * TKIR-based Codegen API (New Pipeline)
 * ============================================================================ */

/**
 * C codegen options
 */
typedef struct {
    bool include_comments;      /**< Add comments to generated code */
    bool generate_types;        /**< Generate type definitions */
    bool include_headers;       /**< Include standard headers */
} CCodegenOptions;

/**
 * Generate C source from TKIR JSON string
 * @param wir_json TKIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Generated C code string (caller must free), or NULL on error
 */
char* c_codegen_from_wir(const char* wir_json, CCodegenOptions* options);

/**
 * Generate C source from TKIR JSON file
 * @param wir_path Path to input .wir file
 * @param output_path Path to output .c file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool c_codegen_from_wir_file(const char* wir_path, const char* output_path,
                                CCodegenOptions* options);

/**
 * Generate C source from KIR via TKIR (recommended)
 * @param kir_json KIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Generated C code string (caller must free), or NULL on error
 */
char* ir_generate_c_code_from_json_via_wir(const char* kir_json, CCodegenOptions* options);

/**
 * Generate C source from KIR file via TKIR (recommended)
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .c file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool ir_generate_c_code_via_wir(const char* kir_path, const char* output_path,
                                   CCodegenOptions* options);

/**
 * Initialize C TKIR emitter
 * Registers the emitter with the TKIR emitter registry.
 */
void c_wir_emitter_init(void);

/**
 * Cleanup C TKIR emitter
 * Unregisters the emitter from the TKIR emitter registry.
 */
void c_wir_emitter_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // IR_C_CODEGEN_H
