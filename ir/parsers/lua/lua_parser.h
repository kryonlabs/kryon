#ifndef IR_LUA_PARSER_H
#define IR_LUA_PARSER_H

#include "../include/ir_core.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse Lua source using Kryon DSL to KIR JSON
 *
 * Executes Lua code that uses the kryon.dsl module and converts
 * it to KIR (Kryon Intermediate Representation) JSON format by
 * running the code with LuaJIT and serializing the IR tree.
 *
 * @param source Lua source text (UTF-8 encoded)
 * @param length Length of source in bytes (0 for null-terminated string)
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 *
 * @example
 *   const char* lua_src =
 *     "local UI = require('kryon.dsl')\n"
 *     "return UI.Column {\n"
 *     "  UI.Text { text = 'Hello World' }\n"
 *     "}\n";
 *   char* kir_json = ir_lua_to_kir(lua_src, 0);
 *   if (kir_json) {
 *       // Use KIR JSON
 *       free(kir_json);
 *   }
 */
char* ir_lua_to_kir(const char* source, size_t length);

/**
 * Parse Lua source file to KIR JSON
 *
 * @param filepath Path to .lua file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_lua_file_to_kir(const char* filepath);

/**
 * Check if LuaJIT is available on the system
 *
 * @return bool True if luajit executable is found
 */
bool ir_lua_check_luajit_available(void);

/**
 * Generate KIR for a plugin from its Lua binding file
 * This creates a proper KIR file describing the plugin's exports without executing it
 *
 * @param plugin_name Name of the plugin (e.g., "datetime")
 * @param binding_path Path to the plugin's Lua binding file
 * @return char* KIR JSON string, or NULL on error. Caller must free.
 */
char* ir_lua_generate_plugin_kir(const char* plugin_name, const char* binding_path);

/**
 * Generate plugin KIR files from kryon.toml configuration
 * Reads the [plugins] section and generates KIR for each plugin
 *
 * @param toml_path Path to kryon.toml file
 * @param output_dir Directory to write generated KIR files
 * @return bool True if successful
 */
bool ir_lua_generate_plugin_kirs_from_toml(const char* toml_path, const char* output_dir);

#ifdef __cplusplus
}
#endif

#endif // IR_LUA_PARSER_H
