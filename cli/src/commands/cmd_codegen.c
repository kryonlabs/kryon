/**
 * Codegen Command
 * Generate source code from KIR files
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codegens/nim/nim_codegen.h"
#include "../../codegens/lua/lua_codegen.h"

int cmd_codegen(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Error: Missing arguments\n\n");
        fprintf(stderr, "Usage: kryon codegen <language> <input.kir> <output>\n\n");
        fprintf(stderr, "Supported languages:\n");
        fprintf(stderr, "  nim    - Generate Nim source code\n");
        fprintf(stderr, "  lua    - Generate Lua source code\n");
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  kryon codegen nim app.kir app.nim\n");
        fprintf(stderr, "  kryon codegen lua app.kir app.lua\n");
        return 1;
    }

    const char* language = argv[0];
    const char* input_kir = argv[1];
    const char* output_file = argv[2];

    // Check if input file exists
    if (!file_exists(input_kir)) {
        fprintf(stderr, "Error: Input file not found: %s\n", input_kir);
        return 1;
    }

    printf("Generating %s code from %s → %s\n", language, input_kir, output_file);

    bool success = false;

    if (strcmp(language, "nim") == 0) {
        success = nim_codegen_generate(input_kir, output_file);
    } else if (strcmp(language, "lua") == 0) {
        success = lua_codegen_generate(input_kir, output_file);
    } else {
        fprintf(stderr, "Error: Unsupported language: %s\n\n", language);
        fprintf(stderr, "Supported languages:\n");
        fprintf(stderr, "  nim    - Generate Nim source code\n");
        fprintf(stderr, "  lua    - Generate Lua source code\n");
        return 1;
    }

    if (success) {
        printf("✓ Generated: %s\n", output_file);
        return 0;
    } else {
        fprintf(stderr, "✗ Code generation failed\n");
        return 1;
    }
}
