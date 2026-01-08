/**
 * Codegen Command
 * Generate source code from KIR files
 * Now supports config-driven workflow using kryon.toml
 */

#include "kryon_cli.h"
#include "build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codegens/kry/kry_codegen.h"
#include "../../codegens/nim/nim_codegen.h"
#include "../../codegens/lua/lua_codegen.h"
#include "../../codegens/tsx/tsx_codegen.h"
#include "../../codegens/c/ir_c_codegen.h"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Check if a string is a valid codegen target
 */
static bool is_valid_target(const char* target) {
    if (!target) return false;
    return strcmp(target, "kry") == 0 ||
           strcmp(target, "tsx") == 0 ||
           strcmp(target, "nim") == 0 ||
           strcmp(target, "lua") == 0 ||
           strcmp(target, "c") == 0;
}

/**
 * Print usage information
 */
static void print_codegen_usage(const char* error) {
    if (error) {
        fprintf(stderr, "Error: %s\n\n", error);
    }

    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Config-driven (recommended):\n");
    fprintf(stderr, "    kryon codegen <target> [--output=<dir>] [--input=<file>]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Direct KIR input:\n");
    fprintf(stderr, "    kryon codegen <target> <input.kir> <output>\n");
    fprintf(stderr, "    kryon codegen <input.kir> --lang=<target> --output=<output>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Targets:\n");
    fprintf(stderr, "  kry    - Generate .kry source code (round-trip)\n");
    fprintf(stderr, "  tsx    - Generate TypeScript React code\n");
    fprintf(stderr, "  nim    - Generate Nim source code\n");
    fprintf(stderr, "  lua    - Generate Lua source code (multi-file)\n");
    fprintf(stderr, "  c      - Generate C source code\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  kryon codegen kry\n");
    fprintf(stderr, "    Uses build.entry from kryon.toml, outputs to build/kry/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen nim --output=gen/\n");
    fprintf(stderr, "    Uses build.entry, outputs to gen/nim/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen lua --input=other.lua\n");
    fprintf(stderr, "    Uses other.lua as source, outputs to build/lua/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen kry app.kir output.kry\n");
    fprintf(stderr, "    Direct KIR to codegen (legacy)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "kryon.toml:\n");
    fprintf(stderr, "  [build]\n");
    fprintf(stderr, "    entry = \"main.lua\"\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  [codegen]\n");
    fprintf(stderr, "    output_dir = \"gen\"  # optional, defaults to build\n");
}

/* ============================================================================
 * Command Entry Point
 * ============================================================================ */

int cmd_codegen(int argc, char** argv) {
    const char* target = NULL;
    const char* input = NULL;
    const char* output = NULL;
    bool use_config = false;

    if (argc == 0) {
        print_codegen_usage("Missing target language");
        return 1;
    }

    // Check if first arg is a target (not a file)
    const char* first = argv[0];
    if (strstr(first, ".kir") != NULL || strstr(first, ".lua") != NULL ||
        strstr(first, ".kry") != NULL || strstr(first, ".tsx") != NULL ||
        strstr(first, ".nim") != NULL || strstr(first, ".c") != NULL ||
        strstr(first, ".h") != NULL) {
        // First arg is a file - old syntax: codegen <input.kir> --lang=<target>
        input = first;
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--lang=", 7) == 0) {
                target = argv[i] + 7;
            } else if (strncmp(argv[i], "--output=", 9) == 0) {
                output = argv[i] + 9;
            }
        }
        if (!target) {
            print_codegen_usage("--lang= required when using file input");
            return 1;
        }
    } else if (argc >= 3 && strstr(argv[1], ".kir") != NULL) {
        // Old positional syntax: codegen <target> <input.kir> <output>
        target = first;
        input = argv[1];
        output = argv[2];
    } else {
        // New config-driven syntax: codegen <target> [--output=<dir>] [--input=<file>]
        target = first;
        use_config = true;

        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--output=", 9) == 0) {
                output = argv[i] + 9;
            } else if (strncmp(argv[i], "--input=", 8) == 0) {
                input = argv[i] + 8;  // Override config entry
            }
        }
    }

    // Validate target
    if (!is_valid_target(target)) {
        fprintf(stderr, "Error: Invalid target '%s'\n\n", target ? target : "(null)");
        print_codegen_usage(NULL);
        return 1;
    }

    // Config-driven workflow
    if (use_config) {
        return codegen_pipeline(input, target, output, NULL);
    }

    // Direct KIR to codegen (legacy path for backward compatibility)
    if (!input) {
        print_codegen_usage("Input file required for direct codegen");
        return 1;
    }

    // Check if input file exists
    if (!file_exists(input)) {
        fprintf(stderr, "Error: Input file not found: %s\n", input);
        return 1;
    }

    // Compile to KIR first if needed
    char kir_file[1024];
    if (strstr(input, ".kir") == NULL) {
        // Source file - compile to KIR first
        const char* basename = strrchr(input, '/');
        basename = basename ? basename + 1 : input;
        char* name_copy = str_copy(basename);
        char* dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';

        snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
        free(name_copy);

        printf("Compiling %s → %s\n", input, kir_file);
        int result = compile_source_to_kir(input, kir_file);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to compile %s\n", input);
            return result;
        }
    } else {
        strncpy(kir_file, input, sizeof(kir_file) - 1);
        kir_file[sizeof(kir_file) - 1] = '\0';
    }

    // Generate code from KIR
    printf("Generating %s code: %s → %s\n", target, kir_file, output);
    int result = generate_from_kir(kir_file, target, output);

    if (result == 0) {
        printf("✓ Generated: %s\n", output);
    } else {
        fprintf(stderr, "✗ Code generation failed\n");
    }

    return result;
}
