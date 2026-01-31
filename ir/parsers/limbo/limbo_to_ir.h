/**
 * Limbo to KIR Converter
 *
 * Converts Limbo AST to Kryon IR (KIR JSON format)
 * Supports round-trip conversion for Limbo GUI applications
 */

#ifndef IR_LIMBO_TO_IR_H
#define IR_LIMBO_TO_IR_H

#include "limbo_ast.h"
#include "../../../third_party/cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert Limbo AST to KIR JSON
 *
 * @param module Limbo module AST
 * @return cJSON* KIR JSON object (caller must free), or NULL on error
 */
cJSON* limbo_ast_to_kir(LimboModule* module);

/**
 * Parse Limbo source directly to KIR JSON
 *
 * @param source Limbo source code
 * @param source_len Length of source (0 for null-terminated)
 * @return cJSON* KIR JSON object (caller must free), or NULL on error
 */
cJSON* limbo_parse_to_kir(const char* source, size_t source_len);

/**
 * Parse Limbo file to KIR JSON
 *
 * @param filepath Path to .b file
 * @return cJSON* KIR JSON object (caller must free), or NULL on error
 */
cJSON* limbo_file_to_kir(const char* filepath);

/**
 * Convert Limbo source to KIR JSON string
 *
 * @param source Limbo source code
 * @param source_len Length of source (0 for null-terminated)
 * @return char* KIR JSON string (caller must free), or NULL on error
 */
char* limbo_to_kir(const char* source, size_t source_len);

/**
 * Convert Limbo file to KIR JSON string
 *
 * @param filepath Path to .b file
 * @return char* KIR JSON string (caller must free), or NULL on error
 */
char* limbo_file_to_kir_string(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // IR_LIMBO_TO_IR_H
