#ifndef IR_C_PARSER_H
#define IR_C_PARSER_H

#include "../include/ir_core.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse C source using Kryon C API to KIR JSON
 *
 * Compiles and executes C code that uses kryon.h API to generate KIR.
 * The C code must call kryon_finalize() to produce output.
 *
 * @param source C source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @note Requires gcc or clang compiler
 *
 * @example
 *   const char* c_src =
 *     "#include \"kryon.h\"\n"
 *     "int main() {\n"
 *     "  kryon_init(\"App\", 800, 600);\n"
 *     "  IRComponent* col = kryon_column();\n"
 *     "  kryon_add_child(col, kryon_text(\"Hello\"));\n"
 *     "  kryon_finalize(\"/tmp/out.kir\");\n"
 *     "}\n";
 *   char* kir_json = ir_c_to_kir(c_src, 0);
 *   if (kir_json) {
 *       // Use KIR JSON
 *       free(kir_json);
 *   }
 */
char* ir_c_to_kir(const char* source, size_t length);

/**
 * Parse C source file to KIR JSON
 *
 * @param filepath Path to .c file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_c_file_to_kir(const char* filepath);

/**
 * Check if a C compiler (gcc or clang) is available
 *
 * @return true if a compiler is found, false otherwise
 */
bool ir_c_check_compiler_available(void);

#ifdef __cplusplus
}
#endif

#endif // IR_C_PARSER_H
