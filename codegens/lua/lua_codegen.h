#ifndef LUA_CODEGEN_H
#define LUA_CODEGEN_H

#include "../../ir/ir_core.h"
#include <stdbool.h>

// Lua codegen options
typedef struct LuaCodegenOptions {
    bool format;                  // Format output
    bool optimize;                // Generate optimized code
} LuaCodegenOptions;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate Lua DSL code from KIR JSON file
 *
 * Reads a .kir file and generates Lua code that uses the Kryon Lua DSL.
 * The generated code includes:
 * - Require statements (kryon.reactive, kryon.dsl)
 * - Reactive variable declarations using Reactive.state()
 * - Component tree using Lua table syntax
 * - Property assignments
 * - Children as array elements
 *
 * @param kir_path Path to .kir JSON file
 * @param output_path Path where generated .lua file should be written
 * @return bool true on success, false on error
 *
 * @example
 *   bool success = lua_codegen_generate("app.kir", "app.lua");
 */
bool lua_codegen_generate(const char* kir_path, const char* output_path);

/**
 * Generate Lua DSL code from KIR JSON string
 *
 * @param kir_json KIR JSON string
 * @return char* Generated Lua code, or NULL on error. Caller must free.
 */
char* lua_codegen_from_json(const char* kir_json);

/**
 * Generate Lua DSL code with codegen options
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
