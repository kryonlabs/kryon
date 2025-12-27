#ifndef IR_RUST_PARSER_H
#define IR_RUST_PARSER_H

#include "../../ir_core.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse Rust source using Kryon Rust API to KIR JSON
 *
 * Compiles and executes Rust code that uses kryon crate API to generate KIR.
 * The Rust code must call kryon_finalize() to produce output.
 *
 * @param source Rust source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @note Requires rustc or cargo
 *
 * @example
 *   const char* rust_src =
 *     "use kryon::*;\n"
 *     "fn main() {\n"
 *     "  let mut app = Kryon::new(\"App\", 800, 600);\n"
 *     "  let col = Column::new();\n"
 *     "  col.add_child(Text::new(\"Hello\"));\n"
 *     "  app.finalize(\"/tmp/out.kir\");\n"
 *     "}\n";
 *   char* kir_json = ir_rust_to_kir(rust_src, 0);
 *   if (kir_json) {
 *       // Use KIR JSON
 *       free(kir_json);
 *   }
 */
char* ir_rust_to_kir(const char* source, size_t length);

/**
 * Parse Rust source file to KIR JSON
 *
 * @param filepath Path to .rs file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_rust_file_to_kir(const char* filepath);

/**
 * Check if Rust compiler (rustc or cargo) is available
 *
 * @return true if Rust is found, false otherwise
 */
bool ir_rust_check_compiler_available(void);

#ifdef __cplusplus
}
#endif

#endif // IR_RUST_PARSER_H
