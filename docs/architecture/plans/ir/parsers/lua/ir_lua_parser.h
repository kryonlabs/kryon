#ifndef IR_LUA_PARSER_H
#define IR_LUA_PARSER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse Lua DSL file to KIR JSON
 *
 * @param filepath Path to .lua file
 * @return KIR JSON string (caller must free), or NULL on error
 */
char* ir_lua_file_to_kir(const char* filepath);

/**
 * Parse Lua DSL source code to KIR JSON
 *
 * @param source Lua source code
 * @param length Length of source code
 * @param filename Optional filename for error messages
 * @return KIR JSON string (caller must free), or NULL on error
 */
char* ir_lua_source_to_kir(const char* source, size_t length, const char* filename);

#ifdef __cplusplus
}
#endif

#endif // IR_LUA_PARSER_H
