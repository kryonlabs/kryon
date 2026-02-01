/**
 * Codegen Pipeline
 *
 * Handles KIR-to-code generation for all supported targets.
 * Uses the unified codegen interface registry for all codegens.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "codegen_pipeline.h"
#include "kryon_cli.h"
#include "../../../codegens/codegen_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External function to register all codegens
extern int codegen_registry_register_all(void);

/* ============================================================================
 * Codegen Target Registry
 * ============================================================================ */

/**
 * Check if a codegen target exists
 * Now uses the unified codegen interface
 */
bool codegen_target_exists(const char* target) {
    if (!target) return false;

    // Special case for "kir" and "web"/"html" which are handled differently
    if (strcmp(target, "kir") == 0 ||
        strcmp(target, "web") == 0 ||
        strcmp(target, "html") == 0) {
        return true;
    }

    return codegen_get_interface(target) != NULL;
}

/* ============================================================================
 * Codegen Generation
 * ============================================================================ */

/**
 * Generate code from KIR file
 * Now uses the unified codegen interface registry
 *
 * @param kir_file Path to input KIR file
 * @param target Target codegen name
 * @param output_path Output directory or file path
 * @return 0 on success, non-zero on error
 */
int codegen_generate(const char* kir_file, const char* target, const char* output_path) {
    if (!kir_file || !target || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to codegen_generate\n");
        return 1;
    }

    // Special case for KIR target (just copy file)
    if (strcmp(target, "kir") == 0) {
        char output_file[2048];
        snprintf(output_file, sizeof(output_file), "%s/main.kir", output_path);

        // Create output directory
        dir_create_recursive(output_path);

        // Read input KIR
        FILE* in = fopen(kir_file, "r");
        if (!in) {
            fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_file);
            return 1;
        }

        FILE* out = fopen(output_file, "w");
        if (!out) {
            fclose(in);
            fprintf(stderr, "Error: Could not create output file: %s\n", output_file);
            return 1;
        }

        // Copy content
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, n, out);
        }

        fclose(in);
        fclose(out);
        printf("✓ Generated: main.kir\n");
        return 0;
    }

    // Special case for web/html (handled separately)
    if (strcmp(target, "web") == 0 || strcmp(target, "html") == 0) {
        fprintf(stderr, "Error: Web codegen should use generate_html_from_kir()\n");
        return 1;
    }

    // Use unified codegen interface
    bool success = codegen_generate_from_registry(kir_file, target, output_path);
    return success ? 0 : 1;
}

/**
 * Get output directory for a codegen target
 * Returns a newly allocated string that must be freed
 */
char* codegen_get_output_dir(const char* target) {
    if (!target) {
        return NULL;
    }

    // Default: build/<target>/
    char* output_dir = NULL;
    asprintf(&output_dir, "build/%s", target);
    return output_dir;
}

/**
 * List all supported codegen targets
 * Returns NULL-terminated array of strings (static, do not free)
 */
const char** codegen_list_targets(void) {
    static const char* targets[] = {
        "kry",      // KRY DSL (round-trip format)
        "c",        // C code
        "markdown", // Markdown documentation
        "kir",      // KIR (KIR intermediate format)
        "limbo",    // Limbo (Inferno/TaijiOS)
        "tcltk",    // Tcl/Tk
        "web",      // Web/HTML
        NULL
    };

    return targets;
}
