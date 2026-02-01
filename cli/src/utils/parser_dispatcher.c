/**
 * Parser Dispatcher
 *
 * Centralizes frontend detection and parser selection for Kryon.
 * Provides a clean API for compiling source files to KIR.
 *
 * Now uses the unified parser interface for all formats.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "parser_dispatcher.h"
#include "kryon_cli.h"
#include "cache.h"
#include "../../../ir/include/parser_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

/* ============================================================================
 * Frontend Detection
 * ============================================================================ */

/**
 * Detect frontend type from file extension
 */
const char* parser_detect_frontend(const char* source_file) {
    if (!source_file) {
        return NULL;
    }

    const char* ext = path_extension(source_file);

    if (strcmp(ext, ".kir") == 0) {
        return "kir";
    }
    else if (strcmp(ext, ".md") == 0 || strcmp(ext, ".markdown") == 0) {
        return "markdown";
    }
    else if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "html";
    }
    else if (strcmp(ext, ".kry") == 0) {
        return "kry";
    }
    else if (strcmp(ext, ".tcl") == 0) {
        return "tcl";
    }
    else if (strcmp(ext, ".b") == 0) {
        return "limbo";
    }
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
        return "c";
    }
    else {
        return NULL;
    }
}

/**
 * Check if a frontend type is supported
 */
bool parser_supports_format(const char* format) {
    if (!format) return false;

    return (strcmp(format, "kir") == 0 ||
            strcmp(format, "markdown") == 0 ||
            strcmp(format, "html") == 0 ||
            strcmp(format, "kry") == 0 ||
            strcmp(format, "tcl") == 0 ||
            strcmp(format, "limbo") == 0 ||
            strcmp(format, "c") == 0);
}

/* ============================================================================
 * Parser Compilation
 * ============================================================================ */

/**
 * Compile source file to KIR using kryon parse command
 * This is the primary entry point for source-to-KIR compilation.
 *
 * @param source_file Path to source file
 * @param output_kir Path where KIR should be written
 * @return 0 on success, non-zero on error
 */
int parser_compile_to_kir(const char* source_file, const char* output_kir) {
    if (!source_file || !output_kir) {
        fprintf(stderr, "Error: Invalid arguments to parser_compile_to_kir\n");
        return 1;
    }

    const char* frontend = parser_detect_frontend(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        return 1;
    }

    printf("Compiling %s → %s (frontend: %s)\n", source_file, output_kir, frontend);

    // Ensure cache directory exists
    if (!cache_ensure_dir()) {
        fprintf(stderr, "Error: Failed to create cache directory\n");
        return 1;
    }

    // Special case for KIR files (just copy)
    if (strcmp(frontend, "kir") == 0) {
        if (strcmp(source_file, output_kir) == 0) {
            return 0;  // Same file, nothing to do
        }

        char cp_cmd[2048];
        snprintf(cp_cmd, sizeof(cp_cmd), "cp \"%s\" \"%s\"", source_file, output_kir);
        int result = system(cp_cmd);

        if (file_exists(output_kir)) {
            return 0;
        }

        fprintf(stderr, "Error: Failed to copy KIR file (exit code: %d)\n", result);
        return 1;
    }

    // Use unified parser interface for all formats
    cJSON* kir = parser_parse_file_as(source_file, frontend);
    if (!kir) {
        fprintf(stderr, "Error: Failed to parse %s file\n", frontend);
        return 1;
    }

    // Write KIR to output file
    char* json_string = cJSON_Print(kir);
    cJSON_Delete(kir);

    if (!json_string) {
        fprintf(stderr, "Error: Failed to serialize KIR to JSON\n");
        return 1;
    }

    FILE* out = fopen(output_kir, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not write output file: %s\n", output_kir);
        free(json_string);
        return 1;
    }

    fprintf(out, "%s\n", json_string);
    fclose(out);
    free(json_string);

    return 0;
}
