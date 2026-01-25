/**
 * C Code Generator - Multi-Module Support
 *
 * Functions for generating multiple C files from KIR module imports.
 */

#ifndef IR_C_MODULES_H
#define IR_C_MODULES_H

#include "../codegen_common.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert module_id to include guard name
 *
 * Converts "foo/bar" to "FOO_BAR_H"
 *
 * @param module_id Module identifier (e.g., "components/button")
 * @return malloc'd guard name string, caller must free
 */
char* c_module_to_guard_name(const char* module_id);

/**
 * Generate a header file (.h) for a C component
 *
 * @param module_id Module identifier
 * @param output_dir Output directory path
 * @return true on success
 */
bool c_generate_header_file(const char* module_id, const char* output_dir);

/**
 * Recursively process a module and its transitive imports
 *
 * Generates C code for the module and all imported modules.
 *
 * @param module_id Module identifier
 * @param kir_dir Directory containing .kir files
 * @param output_dir Output directory for generated .c/.h files
 * @param processed Tracker for already-processed modules
 * @return Number of files written
 */
int c_process_module_recursive(const char* module_id, const char* kir_dir,
                               const char* output_dir, CodegenProcessedModules* processed);

#ifdef __cplusplus
}
#endif

#endif // IR_C_MODULES_H
