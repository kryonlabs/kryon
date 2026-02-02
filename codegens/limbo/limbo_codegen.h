#ifndef LIMBO_CODEGEN_H
#define LIMBO_CODEGEN_H

#include <stdbool.h>

/**
 * @brief Generate Limbo source (.b) from KIR file
 * @param kir_path Path to input .kir file (JSON)
 * @param output_path Path to output .b file (Limbo source)
 * @return true on success, false on error
 */
bool limbo_codegen_generate(const char* kir_path, const char* output_path);

/**
 * @brief Generate Limbo source from KIR JSON string
 * @param kir_json KIR JSON string
 * @return Allocated Limbo source string (caller must free), or NULL on error
 */
char* limbo_codegen_from_json(const char* kir_json);

/**
 * @brief Generate Limbo module files from KIR
 * @param kir_path Path to input .kir file
 * @param output_dir Directory for output .b files
 * @return true on success, false on error
 */
bool limbo_codegen_generate_multi(const char* kir_path, const char* output_dir);

/**
 * @brief Limbo codegen options
 */
typedef struct {
    bool include_comments;      // Add comments to generated code
    bool generate_types;        // Generate type definitions
    const char* module_name;    // Override module name (default: from filename)
    char** modules;             // Array of module names to include (e.g., "string", "sh")
    int modules_count;          // Number of modules
} LimboCodegenOptions;

/**
 * @brief Generate Limbo source with options
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .b file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_generate_with_options(const char* kir_path,
                                          const char* output_path,
                                          LimboCodegenOptions* options);

/* ============================================================================
 * TKIR-based Codegen API
 * ============================================================================ */

/**
 * @brief Generate Limbo source from TKIR JSON string
 * @param wir_json TKIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Allocated Limbo source string (caller must free), or NULL on error
 */
char* limbo_codegen_from_wir(const char* wir_json, LimboCodegenOptions* options);

/**
 * @brief Generate Limbo source from TKIR JSON file
 * @param wir_path Path to input .wir file
 * @param output_path Path to output .b file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_from_wir_file(const char* wir_path, const char* output_path,
                                   LimboCodegenOptions* options);

/**
 * @brief Generate Limbo source from KIR via TKIR (recommended)
 * @param kir_json KIR JSON string
 * @param options Codegen options (NULL for defaults)
 * @return Allocated Limbo source string (caller must free), or NULL on error
 */
char* limbo_codegen_from_json_via_wir(const char* kir_json, LimboCodegenOptions* options);

/**
 * @brief Generate Limbo source from KIR file via TKIR (recommended)
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .b file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool limbo_codegen_generate_via_wir(const char* kir_path, const char* output_path,
                                      LimboCodegenOptions* options);

/**
 * @brief Initialize Limbo TKIR emitter
 * Registers the emitter with the TKIR emitter registry.
 */
void limbo_wir_emitter_init(void);

/**
 * @brief Cleanup Limbo TKIR emitter
 * Unregisters the emitter from the TKIR emitter registry.
 */
void limbo_wir_emitter_cleanup(void);

#endif // LIMBO_CODEGEN_H
