/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Codegen
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Detect frontend type by file extension
 */
static const char* detect_frontend(const char* source_file) {
    const char* ext = path_extension(source_file);

    if (strcmp(ext, ".tsx") == 0 || strcmp(ext, ".jsx") == 0) {
        return "tsx";
    }
    else if (strcmp(ext, ".md") == 0) {
        return "markdown";
    }
    else if (strcmp(ext, ".html") == 0) {
        return "html";
    }
    else if (strcmp(ext, ".kry") == 0) {
        return "kry";
    }
    else if (strcmp(ext, ".nim") == 0) {
        return "nim";
    }
    else if (strcmp(ext, ".lua") == 0) {
        return "lua";
    }
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
        return "c";
    }
    else {
        return NULL;
    }
}

/**
 * Compile frontend source to KIR
 * Returns 0 on success, non-zero on failure
 */
static int compile_to_kir(const char* source_file, const char* output_kir, const char* frontend) {
    printf("Compiling %s → %s (frontend: %s)\n", source_file, output_kir, frontend);

    if (strcmp(frontend, "markdown") == 0) {
        // Use Nim CLI for markdown compilation
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "kryon compile \"%s\" 2>&1 | grep -q 'KIR generated' && mv .kryon_cache/*.kir \"%s\" 2>/dev/null || kryon compile \"%s\" > /dev/null 2>&1",
                 source_file, output_kir, source_file);

        int result = system(cmd);

        // Check if KIR was generated
        if (file_exists(output_kir)) {
            return 0;
        }

        fprintf(stderr, "Error: Failed to compile markdown\n");
        return 1;
    }
    else if (strcmp(frontend, "tsx") == 0 || strcmp(frontend, "jsx") == 0) {
        // Use Bun to parse TSX/JSX
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "bun run /mnt/storage/Projects/kryon/cli_nim_backup/tsx_to_kir.ts \"%s\" > \"%s\"",
                 source_file, output_kir);

        char* output = NULL;
        int result = process_run(cmd, &output);
        if (output && strlen(output) > 0) {
            fprintf(stderr, "%s", output);
            free(output);
        }
        return result;
    }
    else if (strcmp(frontend, "html") == 0) {
        // Use kryon compile for HTML
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "kryon compile \"%s\" --output=\"%s\" 2>&1",
                 source_file, output_kir);

        int result = system(cmd);

        // Check if KIR was generated
        if (file_exists(output_kir)) {
            return 0;
        }

        fprintf(stderr, "Error: Failed to compile HTML\n");
        return 1;
    }
    else if (strcmp(frontend, "kry") == 0) {
        // .kry DSL frontend (delegated to Nim CLI)
        fprintf(stderr, "Error: .kry frontend requires Nim CLI integration\n");
        fprintf(stderr, "Use: kryon compile (Nim CLI) for .kry files\n");
        return 1;
    }
    else if (strcmp(frontend, "nim") == 0) {
        // Compile Nim with KRYON_SERIALIZE_IR flag
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "nim c -d:KRYON_SERIALIZE_IR -r \"%s\"",
                 source_file);

        char* output = NULL;
        int result = process_run(cmd, &output);
        if (output) {
            printf("%s", output);
            free(output);
        }

        // Move generated KIR to output path
        char* kir_file = str_copy(source_file);
        char* dot = strrchr(kir_file, '.');
        if (dot) {
            strcpy(dot, ".kir");

            char mv_cmd[1024];
            snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", kir_file, output_kir);
            system(mv_cmd);
        }
        free(kir_file);

        return result;
    }
    else if (strcmp(frontend, "lua") == 0) {
        // Run Lua with Kryon bindings
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "luajit -e 'KRYON_SERIALIZE_IR=true' \"%s\"",
                 source_file);

        char* output = NULL;
        int result = process_run(cmd, &output);
        if (output) {
            printf("%s", output);
            free(output);
        }
        return result;
    }
    else {
        fprintf(stderr, "Error: Unknown frontend type: %s\n", frontend);
        return 1;
    }
}

/**
 * Invoke codegen to generate output from KIR
 */
static int invoke_codegen(const char* kir_file, const char* target, const char* output_dir) {
    printf("Generating %s output from %s → %s\n", target, kir_file, output_dir);

    if (strcmp(target, "web") == 0) {
        // Use web codegen library
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "KRYON_LIB_PATH=/home/wao/.local/lib/libkryon_web.so kryon run \"%s\"",
                 kir_file);

        char* output = NULL;
        int result = process_run(cmd, &output);
        if (output) {
            printf("%s", output);
            free(output);
        }
        return result;
    }
    else if (strcmp(target, "desktop") == 0) {
        fprintf(stderr, "Error: Desktop target not yet implemented\n");
        return 1;
    }
    else if (strcmp(target, "terminal") == 0) {
        fprintf(stderr, "Error: Terminal target not yet implemented\n");
        return 1;
    }
    else {
        fprintf(stderr, "Error: Unknown build target: %s\n", target);
        return 1;
    }
}

/**
 * Build a single source file
 */
static int build_single_file(const char* source_file, KryonConfig* config) {
    // Always detect frontend from file extension
    const char* frontend = detect_frontend(source_file);
    if (!frontend) {
        // Fall back to config if detection fails
        frontend = config->build_frontend;
        if (!frontend) {
            fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
            fprintf(stderr, "Specify frontend in kryon.toml or use a supported extension\n");
            return 1;
        }
    }

    // Create cache directory
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Generate KIR filename
    char kir_file[1024];
    const char* basename = strrchr(source_file, '/');
    basename = basename ? basename + 1 : source_file;

    char* name_copy = str_copy(basename);
    char* dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
    free(name_copy);

    // Compile to KIR
    int result = compile_to_kir(source_file, kir_file, frontend);
    if (result != 0) {
        fprintf(stderr, "Error: Compilation to KIR failed\n");
        return result;
    }

    // Verify KIR was generated
    if (!file_exists(kir_file)) {
        fprintf(stderr, "Error: KIR file was not generated: %s\n", kir_file);
        return 1;
    }

    // Create output directory
    const char* output_dir = config->build_output_dir ? config->build_output_dir : "dist";
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Invoke codegen
    result = invoke_codegen(kir_file, config->build_target, output_dir);
    if (result != 0) {
        fprintf(stderr, "Error: Code generation failed\n");
        return result;
    }

    printf("✓ Build complete: %s → %s\n", source_file, output_dir);
    return 0;
}

/**
 * Build command entry point
 */
int cmd_build(int argc, char** argv) {
    // Load config
    KryonConfig* config = config_find_and_load();
    if (!config) {
        fprintf(stderr, "Error: Could not find or load kryon.toml\n");
        fprintf(stderr, "Run 'kryon new <name>' to create a new project\n");
        return 1;
    }

    // Validate config
    if (!config_validate(config)) {
        config_free(config);
        return 1;
    }

    int result = 0;

    // Determine what to build
    if (argc > 0) {
        // Build specific file
        const char* source_file = argv[0];

        if (!file_exists(source_file)) {
            fprintf(stderr, "Error: Source file not found: %s\n", source_file);
            config_free(config);
            return 1;
        }

        result = build_single_file(source_file, config);
    }
    else if (config->build_entry) {
        // Build entry file from config
        result = build_single_file(config->build_entry, config);
    }
    else if (config->pages_count > 0) {
        // Build multi-page project
        printf("Building multi-page project (%d pages)...\n", config->pages_count);

        for (int i = 0; i < config->pages_count; i++) {
            const char* source = config->build_pages[i].source;
            if (source) {
                printf("\nBuilding page %d/%d: %s\n", i + 1, config->pages_count, source);
                result = build_single_file(source, config);
                if (result != 0) {
                    break;
                }
            }
        }
    }
    else {
        fprintf(stderr, "Error: No source file specified\n");
        fprintf(stderr, "Usage: kryon build <file>  OR  specify build.entry or build.pages in kryon.toml\n");
        result = 1;
    }

    config_free(config);
    return result;
}
