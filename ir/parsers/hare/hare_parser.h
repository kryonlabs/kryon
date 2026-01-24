/**
 * Hare Parser - Converts Hare DSL source to KIR JSON
 *
 * This parser reads Hare source code that uses the Kryon Hare DSL
 * and converts it to KIR (Kryon Intermediate Representation) JSON format.
 *
 * The Hare DSL syntax:
 *   use kryon::*;
 *
 *   export fn main() void = {
 *       container { b => {
 *           b.width("100%");
 *           b.height("100%");
 *           b.child(row { r => {
 *               r.gap(10);
 *               r.child(text { t => {
 *                   t.text("Hello, Hare!");
 *                   t.font_size(32);
 *               }});
 *           }});
 *       }});
 *   };
 *
 * @note This is a simplified parser that handles the DSL subset used by Kryon.
 *       For full Hare language support, use the official Hare compiler.
 */

#ifndef IR_HARE_PARSER_H
#define IR_HARE_PARSER_H

#include "../../include/ir_core.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse Hare source using Kryon DSL to KIR JSON
 *
 * @param source Hare source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @example
 *   const char* hare_src =
 *     "use kryon::*;\n"
 *     "export fn main() void = {\n"
 *     "  container { b => b.width(\"100%\"); b.height(\"100%\"); };\n"
 *     "};\n";
 *   char* kir_json = ir_hare_to_kir(hare_src, 0);
 *   if (kir_json) {
 *     free(kir_json);
 *   }
 */
char* ir_hare_to_kir(const char* source, size_t length);

/**
 * Parse Hare source file to KIR JSON
 *
 * @param filepath Path to .ha file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_hare_file_to_kir(const char* filepath);

/**
 * Get the last error message from the Hare parser
 *
 * @return Error message, or NULL if no error
 */
const char* ir_hare_get_error(void);

/**
 * Clear the last error message
 */
void ir_hare_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // IR_HARE_PARSER_H
