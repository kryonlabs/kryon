#ifndef LUA_CODEGEN_H
#define LUA_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Lua codegen options
typedef struct LuaCodegenOptions {
    bool format;                  // Run formatter if available
    bool preserve_comments;       // Preserve comments from original
    bool use_reactive;            // Include reactive state system
} LuaCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Lua DSL code from KIR JSON file
 *
 * Reads a .kir file and generates .lua code.
 * The generated code includes:
 * - Component tree using Lua DSL syntax  
 * - Event handlers from logic_block section
 * - Reactive state if present
 * - Properties and styling
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .lua file should be written
 * @return bool true on success, false on error
 */
bool lua_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate Lua DSL code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated .lua code, or NULL on error. Caller must free.
 */
char* lua_codegen_from_json(const char* kir_json);

/**
 * Generate Lua code with codegen options
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .lua file should be written
 * @param options Codegen options (NULL for defaults)
 * @return bool true on success, false on error
 */
bool lua_codegen_generate_with_options(const char* kir_path,
                                        const char* output_path,
                                        LuaCodegenOptions* options);

#ifdef __cplusplus
}
#endif

#endif // LUA_CODEGEN_H
