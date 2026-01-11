/**
 * LuaJIT Build Module
 *
 * Functions for building Lua applications to standalone binaries
 */

#ifndef KRYON_LUAJIT_BUILD_H
#define KRYON_LUAJIT_BUILD_H

#include <stddef.h>

/**
 * Compile Lua source to bytecode using LuaJIT
 *
 * @param input_lua  Path to input Lua file
 * @param output_bc  Path for output bytecode file
 * @return 0 on success, -1 on error
 */
int luajit_compile(const char* input_lua, const char* output_bc);

/**
 * Compile multiple Lua files to bytecode
 *
 * @param inputs     Array of input file paths
 * @param count      Number of files
 * @param output_dir Directory for output bytecode files
 * @return Number of errors encountered
 */
int luajit_compile_multiple(const char** inputs, int count, const char* output_dir);

/**
 * Embed bytecode file as C array
 *
 * @param bytecode_file Path to bytecode file
 * @param output_c      Path for output C file
 * @param array_name    Name for the byte array
 * @return 0 on success, -1 on error
 */
int embed_bytecode(const char* bytecode_file, const char* output_c, const char* array_name);

/**
 * Embed multiple bytecode files into a single C file
 *
 * @param bytecode_files Array of bytecode file paths
 * @param count          Number of files
 * @param output_c       Path for output C file
 * @param project_name   Project name prefix for arrays
 * @return 0 on success, -1 on error
 */
int embed_multiple_bytecode(const char** bytecode_files, int count,
                             const char* output_c,
                             const char* project_name);

/**
 * Build Lua application to standalone binary
 *
 * @param project_name Name of the project (used for binary name)
 * @param entry_lua    Path to main Lua entry file
 * @param output_binary Path for output binary
 * @param runtime_dir  Path to Kryon runtime files (optional)
 * @param verbose      Enable verbose output
 * @return 0 on success, -1 on error
 */
int build_lua_binary(const char* project_name,
                     const char* entry_lua,
                     const char* output_binary,
                     const char* runtime_dir,
                     int verbose);

/**
 * Check if LuaJIT is available on the system
 *
 * @return 1 if available, 0 otherwise
 */
int check_luajit_available(void);

#endif /* KRYON_LUAJIT_BUILD_H */
