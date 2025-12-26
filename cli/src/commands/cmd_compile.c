/**
 * Compile Command
 * Compiles source files to KIR (no code generation)
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Import compile_to_kir from cmd_build.c (we'll need to expose it)
// For now, duplicate the logic

static const char* detect_frontend(const char* source_file) {
    const char* ext = path_extension(source_file);

    if (strcmp(ext, ".tsx") == 0 || strcmp(ext, ".jsx") == 0) return "tsx";
    else if (strcmp(ext, ".md") == 0) return "markdown";
    else if (strcmp(ext, ".kry") == 0) return "kry";
    else if (strcmp(ext, ".nim") == 0) return "nim";
    else if (strcmp(ext, ".lua") == 0) return "lua";
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) return "c";
    else return NULL;
}

static int compile_to_kir(const char* source_file, const char* output_kir, const char* frontend) {
    printf("Compiling %s → %s (frontend: %s)\n", source_file, output_kir, frontend);

    if (strcmp(frontend, "markdown") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "kryon compile \"%s\" 2>&1 | grep -q 'KIR generated' && mv .kryon_cache/*.kir \"%s\" 2>/dev/null || kryon compile \"%s\" > /dev/null 2>&1",
                 source_file, output_kir, source_file);

        system(cmd);
        return file_exists(output_kir) ? 0 : 1;
    }
    else if (strcmp(frontend, "tsx") == 0 || strcmp(frontend, "jsx") == 0) {
        char cmd[2048];
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
    else {
        fprintf(stderr, "Error: Frontend '%s' not yet implemented in compile command\n", frontend);
        return 1;
    }
}

int cmd_compile(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Error: No source file specified\n");
        fprintf(stderr, "Usage: kryon compile <file>\n");
        return 1;
    }

    const char* source_file = argv[0];

    if (!file_exists(source_file)) {
        fprintf(stderr, "Error: Source file not found: %s\n", source_file);
        return 1;
    }

    // Detect frontend
    const char* frontend = detect_frontend(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        fprintf(stderr, "Supported extensions: .md, .tsx, .jsx, .kry, .nim, .lua, .c\n");
        return 1;
    }

    // Create cache directory
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Generate KIR filename
    const char* basename = strrchr(source_file, '/');
    basename = basename ? basename + 1 : source_file;

    char* name_copy = str_copy(basename);
    char* dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    char kir_file[1024];
    snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
    free(name_copy);

    // Compile to KIR
    int result = compile_to_kir(source_file, kir_file, frontend);

    if (result == 0) {
        printf("✓ KIR generated: %s\n", kir_file);
    } else {
        fprintf(stderr, "✗ Compilation failed\n");
    }

    return result;
}
