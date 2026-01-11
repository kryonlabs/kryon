/**
 * LuaJIT Bytecode Compiler
 * Compiles Lua source files to bytecode using LuaJIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

/**
 * Compile Lua source to bytecode using LuaJIT
 */
int luajit_compile(const char* input_lua, const char* output_bc) {
    if (!input_lua || !output_bc) {
        fprintf(stderr, "Error: NULL input or output path\n");
        return -1;
    }

    // Check if input exists
    struct stat st;
    if (stat(input_lua, &st) != 0) {
        fprintf(stderr, "Error: Input file not found: %s\n", input_lua);
        return -1;
    }

    // Build luajit command
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "luajit -b \"%s\" \"%s\"", input_lua, output_bc);

    printf("  Compiling %s to bytecode...\n", input_lua);
    int result = system(cmd);

    if (result != 0) {
        fprintf(stderr, "Error: LuaJIT compilation failed\n");
        return -1;
    }

    // Verify output was created
    if (stat(output_bc, &st) != 0) {
        fprintf(stderr, "Error: Bytecode file not created: %s\n", output_bc);
        return -1;
    }

    printf("  Created bytecode: %s (%zu bytes)\n", output_bc, (size_t)st.st_size);
    return 0;
}

/**
 * Compile multiple Lua files to bytecode
 */
int luajit_compile_multiple(const char** inputs, int count, const char* output_dir) {
    if (!inputs || count <= 0 || !output_dir) {
        return -1;
    }

    // Create output directory if needed
    struct stat st;
    if (stat(output_dir, &st) != 0) {
        char mkdir_cmd[PATH_MAX];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
        system(mkdir_cmd);
    }

    int errors = 0;
    for (int i = 0; i < count; i++) {
        const char* input = inputs[i];
        if (!input) continue;

        // Generate output filename
        const char* basename = strrchr(input, '/');
        basename = basename ? basename + 1 : input;

        char output_bc[PATH_MAX];
        snprintf(output_bc, sizeof(output_bc), "%s/%s.bc", output_dir, basename);

        if (luajit_compile(input, output_bc) != 0) {
            errors++;
        }
    }

    return errors;
}
