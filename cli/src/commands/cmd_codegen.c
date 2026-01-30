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
           strcmp(target, "c") == 0 ||
           strcmp(target, "html") == 0 ||
           strcmp(target, "markdown") == 0 ||
           strcmp(target, "kir") == 0 ||
           strcmp(target, "limbo") == 0;
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
    fprintf(stderr, "  kry       - Generate .kry source code (round-trip)\n");
    fprintf(stderr, "  c         - Generate C source code\n");
    fprintf(stderr, "  html      - Generate HTML/CSS/JS for web\n");
    fprintf(stderr, "  markdown  - Generate Markdown documentation\n");
    fprintf(stderr, "  kir       - Generate KIR files (multi-file, preserves module structure)\n");
    fprintf(stderr, "  limbo     - Generate Limbo source code for Inferno/TaijiOS\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  kryon codegen kry\n");
    fprintf(stderr, "    Uses build.entry from kryon.toml, outputs to build/kry/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen kry --output=gen/\n");
    fprintf(stderr, "    Uses build.entry, outputs to gen/kry/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen kry --input=other.kry\n");
    fprintf(stderr, "    Uses other.kry as source, outputs to build/kry/\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  kryon codegen kry app.kir output.kry\n");
    fprintf(stderr, "    Direct KIR to codegen (legacy)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "kryon.toml:\n");
    fprintf(stderr, "  [build]\n");
    fprintf(stderr, "    entry = \"main.kry\"\n");
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
    if (strstr(first, ".kir") != NULL ||
        strstr(first, ".kry") != NULL ||
        strstr(first, ".c") != NULL || strstr(first, ".h") != NULL) {
        // First arg is a file - old syntax: codegen <input.kir> --lang=<target>
        input = first;
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--lang=", 7) == 0) {
                target = argv[i] + 7;
            } else if (strncmp(argv[i], "--output=", 9) == 0) {
                output = argv[i] + 9;
            } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                output = argv[++i];
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                fprintf(stderr, "Supported options: --lang=<target>, --output=<path>\n");
                return 1;
            }
        }
        if (!target) {
            print_codegen_usage("--lang= required when using file input");
            return 1;
        }
    } else if (argc >= 3 && (strstr(argv[1], ".kir") != NULL || strstr(argv[1], ".kry") != NULL)) {
        // Positional syntax: codegen <target> <input> <output>
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
            } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
                output = argv[++i];
            } else if (strncmp(argv[i], "--input=", 8) == 0) {
                input = argv[i] + 8;  // Override config entry
            } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
                input = argv[++i];
            } else if (argv[i][0] == '-') {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                fprintf(stderr, "Supported options: --output=<dir>, --input=<file>\n");
                return 1;
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

    // Special handling for kir target: use multi-file KIR generation for .kry source
    if (strcmp(target, "kir") == 0) {
        // Check if input is a .kry file
        const char* ext = strrchr(input, '.');
        if (ext && strcmp(ext, ".kry") == 0) {
            // Use codegen_pipeline for multi-file KIR generation
            return codegen_pipeline(input, target, output, NULL);
        }
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
