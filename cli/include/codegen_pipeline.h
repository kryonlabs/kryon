/**
 * @file codegen_pipeline.h
 * @brief Codegen pipeline for Kryon build system
 */

#ifndef KRYON_CODEGEN_PIPELINE_H
#define KRYON_CODEGEN_PIPELINE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if a codegen target exists
 * @param target Target name
 * @return true if target is supported
 */
bool codegen_target_exists(const char* target);

/**
 * Generate code from KIR file
 * @param kir_file Path to input KIR file
 * @param target Target codegen name
 * @param output_path Output directory or file path
 * @return 0 on success, non-zero on error
 */
int codegen_generate(const char* kir_file, const char* target, const char* output_path);

/**
 * Get output directory for a codegen target
 * @param target Target name
 * @return Newly allocated string that must be freed, or NULL on error
 */
char* codegen_get_output_dir(const char* target);

/**
 * List all supported codegen targets
 * @return NULL-terminated array of strings (static, do not free)
 */
const char** codegen_list_targets(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_CODEGEN_PIPELINE_H
