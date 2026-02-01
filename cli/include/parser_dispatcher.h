/**
 * @file parser_dispatcher.h
 * @brief Parser dispatcher for Kryon build system
 */

#ifndef KRYON_PARSER_DISPATCHER_H
#define KRYON_PARSER_DISPATCHER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Detect frontend type from file extension
 * @param source_file Path to source file
 * @return Frontend name (e.g., "kry", "tcl", "limbo") or NULL if unknown
 */
const char* parser_detect_frontend(const char* source_file);

/**
 * Check if a frontend format is supported
 * @param format Frontend name
 * @return true if supported
 */
bool parser_supports_format(const char* format);

/**
 * Compile source file to KIR
 * @param source_file Path to source file
 * @param output_kir Path where KIR should be written
 * @return 0 on success, non-zero on error
 */
int parser_compile_to_kir(const char* source_file, const char* output_kir);

#ifdef __cplusplus
}
#endif

#endif // KRYON_PARSER_DISPATCHER_H
