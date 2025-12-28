/**
 * Kryon Lua Parser - C Wrapper
 * 
 * Calls the Lua-based parser (lua_to_kir.lua) to convert Lua DSL to KIR
 */

#include "ir_lua_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// Path to the Lua parser script (relative to build)
#define LUA_PARSER_SCRIPT "ir/parsers/lua/lua_to_kir.lua"

/**
 * Initialize Lua state and load parser script
 */
static lua_State* init_lua_parser(void) {
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Error: Failed to create Lua state\n");
        return NULL;
    }

    // Load standard libraries
    luaL_openlibs(L);

    // Load the parser script
    if (luaL_dofile(L, LUA_PARSER_SCRIPT) != LUA_OK) {
        fprintf(stderr, "Error loading Lua parser: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }

    // Parser module should be on stack now
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "Error: Lua parser did not return a table\n");
        lua_close(L);
        return NULL;
    }

    return L;
}

/**
 * Parse Lua source code to KIR JSON
 */
char* ir_lua_source_to_kir(const char* source, size_t length, const char* filename) {
    if (!source) return NULL;

    lua_State* L = init_lua_parser();
    if (!L) return NULL;

    // Stack: [parser_module]
    
    // Get the parse function: parser_module.parse
    lua_getfield(L, -1, "parse");
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "Error: Parser.parse is not a function\n");
        lua_close(L);
        return NULL;
    }

    // Stack: [parser_module, parse_function]

    // Push arguments: source code and filename
    lua_pushlstring(L, source, length);
    lua_pushstring(L, filename ? filename : "stdin");

    // Stack: [parser_module, parse_function, source, filename]

    // Call Parser.parse(source, filename)
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        fprintf(stderr, "Error parsing Lua: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }

    // Stack: [parser_module, result]

    // Get result (KIR JSON string)
    if (!lua_isstring(L, -1)) {
        fprintf(stderr, "Error: Parser did not return a string\n");
        lua_close(L);
        return NULL;
    }

    size_t result_len;
    const char* result = lua_tolstring(L, -1, &result_len);
    
    // Copy result before closing Lua state
    char* kir_json = malloc(result_len + 1);
    if (!kir_json) {
        lua_close(L);
        return NULL;
    }

    memcpy(kir_json, result, result_len);
    kir_json[result_len] = '\0';

    lua_close(L);
    return kir_json;
}

/**
 * Parse Lua file to KIR JSON
 */
char* ir_lua_file_to_kir(const char* filepath) {
    if (!filepath) return NULL;

    // Read file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(source, 1, size, f);
    source[read_size] = '\0';
    fclose(f);

    // Parse
    char* kir_json = ir_lua_source_to_kir(source, read_size, filepath);
    free(source);

    return kir_json;
}
