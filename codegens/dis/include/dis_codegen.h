#ifndef DIS_CODEGEN_H
#define DIS_CODEGEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// DIS Codegen public API

/**
 * Generate DIS bytecode from KIR file
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .dis file
 * @return true on success, false on error
 */
bool dis_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate DIS bytecode from KIR JSON string
 * @param kir_json JSON string containing KIR data
 * @return Newly allocated C string containing DIS assembly, or NULL on error
 * @note Caller must free the returned string
 */
char* dis_codegen_from_json(const char* kir_json);

/**
 * Generate DIS bytecode for multi-file projects
 * @param kir_path Path to main .kir file
 * @param output_dir Directory to output .dis files
 * @return true on success, false on error
 */
bool dis_codegen_generate_multi(const char* kir_path, const char* output_dir);

/**
 * DIS Codegen options
 */
typedef struct {
    bool optimize;          // Enable optimizations
    bool debug_info;        // Include debug information
    uint32_t stack_extent;  // Stack growth increment (default: 4096)
    bool share_mp;          // Share MP between modules
    const char* module_name; // Module name (default: derived from input)
} DISCodegenOptions;

/**
 * Generate DIS bytecode with custom options
 * @param kir_path Path to input .kir file
 * @param output_path Path to output .dis file
 * @param options Codegen options (NULL for defaults)
 * @return true on success, false on error
 */
bool dis_codegen_generate_with_options(const char* kir_path,
                                       const char* output_path,
                                       DISCodegenOptions* options);

/**
 * Get last error message
 * @return Error message string, or NULL if no error
 */
const char* dis_codegen_get_error(void);

/**
 * Clear last error message
 */
void dis_codegen_clear_error(void);

/**
 * Set error message (internal use)
 * @param fmt Printf-style format string
 */
void dis_codegen_set_error(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // DIS_CODEGEN_H
