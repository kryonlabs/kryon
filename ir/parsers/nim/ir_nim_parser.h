#ifndef IR_NIM_PARSER_H
#define IR_NIM_PARSER_H

#include "../../ir_core.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse Nim source using Kryon DSL to KIR JSON
 *
 * Parses Nim code that uses the kryon_dsl module and converts
 * it to KIR (Kryon Intermediate Representation) JSON format.
 *
 * @param source Nim source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @example
 *   const char* nim_src =
 *     "import kryon_dsl\n"
 *     "let app = kryonApp:\n"
 *     "  Header:\n"
 *     "    windowTitle = \"Hello\"\n"
 *     "  Body:\n"
 *     "    Container:\n"
 *     "      Text:\n"
 *     "        text = \"Hello World\"\n";
 *   char* kir_json = ir_nim_to_kir(nim_src, 0);
 *   if (kir_json) {
 *       // Use KIR JSON
 *       free(kir_json);
 *   }
 */
char* ir_nim_to_kir(const char* source, size_t length);

/**
 * Parse Nim source file to KIR JSON
 *
 * @param filepath Path to .nim file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_nim_file_to_kir(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // IR_NIM_PARSER_H
