#ifndef IR_TSX_PARSER_H
#define IR_TSX_PARSER_H

#include "../include/ir_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse TSX/JSX source to KIR JSON
 *
 * Converts TypeScript/JavaScript React code into Kryon IR JSON format.
 * Currently uses Bun + TypeScript parser as backend.
 *
 * @param source TSX/JSX source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @note Requires Bun runtime to be installed: https://bun.sh
 *
 * @example
 *   const char* tsx = "export default function App() { return <div>Hello</div>; }";
 *   char* kir_json = ir_tsx_to_kir(tsx, 0);
 *   if (kir_json) {
 *       // Use KIR JSON
 *       free(kir_json);
 *   }
 */
char* ir_tsx_to_kir(const char* source, size_t length);

/**
 * Parse TSX/JSX file to KIR JSON
 *
 * @param filepath Path to .tsx or .jsx file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_tsx_file_to_kir(const char* filepath);

/**
 * Check if Bun runtime is available
 *
 * @return true if Bun is found, false otherwise
 */
bool ir_tsx_check_bun_available(void);

/**
 * Get path to TSX parser script
 *
 * Searches for tsx_to_kir.ts in known locations, writes embedded
 * script to cache if not found.
 *
 * @return const char* Path to parser script, or NULL if not found
 */
const char* ir_tsx_get_parser_script(void);

#ifdef __cplusplus
}
#endif

#endif // IR_TSX_PARSER_H
