/**
 * Codegen Command
 * Generate source code from KIR files
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codegens/kry/kry_codegen.h"
#include "../../codegens/nim/nim_codegen.h"
#include "../../codegens/lua/lua_codegen.h"
#include "../../codegens/tsx/tsx_codegen.h"
#include "../../codegens/c/ir_c_codegen.h"

int cmd_codegen(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Error: Missing arguments\n\n");
        fprintf(stderr, "Usage: kryon codegen <language> <input.kir> <output>\n");
        fprintf(stderr, "   or: kryon codegen <input.kir> --lang=<language> --output=<output>\n\n");
        fprintf(stderr, "Supported languages:\n");
        fprintf(stderr, "  kry    - Generate .kry source code (round-trip)\n");
        fprintf(stderr, "  tsx    - Generate TypeScript React code\n");
        fprintf(stderr, "  nim    - Generate Nim source code\n");
        fprintf(stderr, "  lua    - Generate Lua source code\n");
        fprintf(stderr, "  c      - Generate C source code\n");
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  kryon codegen kry app.kir app.kry\n");
        fprintf(stderr, "  kryon codegen tsx app.kir app.tsx\n");
        fprintf(stderr, "  kryon codegen nim app.kir app.nim\n");
        fprintf(stderr, "  kryon codegen c app.kir app.c\n");
        fprintf(stderr, "  kryon codegen app.kir --lang=kry --output=app.kry\n");
        return 1;
    }

    const char* language = NULL;
    const char* input_kir = NULL;
    const char* output_file = NULL;

    // Check if using flag-based syntax (first arg has .kir extension)
    if (argc >= 1 && strstr(argv[0], ".kir") != NULL) {
        // Flag-based: kryon codegen <input.kir> --lang=<language> --output=<output>
        input_kir = argv[0];

        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--lang=", 7) == 0) {
                language = argv[i] + 7;
            } else if (strncmp(argv[i], "--output=", 9) == 0) {
                output_file = argv[i] + 9;
            }
        }

        if (!language || !output_file) {
            fprintf(stderr, "Error: Missing required flags\n");
            fprintf(stderr, "Usage: kryon codegen <input.kir> --lang=<language> --output=<output>\n");
            return 1;
        }
    } else if (argc >= 3) {
        // Positional: kryon codegen <language> <input.kir> <output>
        language = argv[0];
        input_kir = argv[1];
        output_file = argv[2];
    } else {
        fprintf(stderr, "Error: Missing arguments\n\n");
        fprintf(stderr, "Usage: kryon codegen <language> <input.kir> <output>\n");
        fprintf(stderr, "   or: kryon codegen <input.kir> --lang=<language> --output=<output>\n");
        return 1;
    }

    // Check if input file exists
    if (!file_exists(input_kir)) {
        fprintf(stderr, "Error: Input file not found: %s\n", input_kir);
        return 1;
    }

    printf("Generating %s code from %s → %s\n", language, input_kir, output_file);

    bool success = false;

    if (strcmp(language, "kry") == 0) {
        success = kry_codegen_generate(input_kir, output_file);
    } else if (strcmp(language, "tsx") == 0) {
        success = tsx_codegen_generate(input_kir, output_file);
    } else if (strcmp(language, "nim") == 0) {
        success = nim_codegen_generate(input_kir, output_file);
    } else if (strcmp(language, "lua") == 0) {
        success = lua_codegen_generate(input_kir, output_file);
    } else if (strcmp(language, "c") == 0) {
        success = ir_generate_c_code(input_kir, output_file);
    } else {
        fprintf(stderr, "Error: Unsupported language: %s\n\n", language);
        fprintf(stderr, "Supported languages:\n");
        fprintf(stderr, "  kry    - Generate .kry source code (round-trip)\n");
        fprintf(stderr, "  tsx    - Generate TypeScript React code\n");
        fprintf(stderr, "  nim    - Generate Nim source code\n");
        fprintf(stderr, "  lua    - Generate Lua source code\n");
        fprintf(stderr, "  c      - Generate C source code\n");
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
