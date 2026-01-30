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

#endif // LIMBO_CODEGEN_H
