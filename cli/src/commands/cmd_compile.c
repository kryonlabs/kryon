/**
 * Compile Command
 * Compiles source files to KIR (no code generation)
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_core.h"
#include "parsers/kry/kry_parser.h"
#include "parsers/markdown/markdown_parser.h"
#include "parsers/html/html_parser.h"
#include "ir_serialization.h"
#include "parsers/c/c_parser.h"
#include "parsers/tsx/tsx_parser.h"
#include "parsers/lua/lua_parser.h"

// Import compile_to_kir from cmd_build.c (we'll need to expose it)
// For now, duplicate the logic

static const char* detect_frontend(const char* source_file) {
    const char* ext = path_extension(source_file);

    if (strcmp(ext, ".kry") == 0) return "kry";
    else if (strcmp(ext, ".kir") == 0) return "kir";
    else if (strcmp(ext, ".md") == 0) return "markdown";
    else if (strcmp(ext, ".html") == 0) return "html";
    else if (strcmp(ext, ".tsx") == 0 || strcmp(ext, ".jsx") == 0) return "tsx";
    else if (strcmp(ext, ".nim") == 0) return "nim";
    else if (strcmp(ext, ".lua") == 0) return "lua";
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) return "c";
    else return NULL;
}

static int compile_to_kir(const char* source_file, const char* output_kir, const char* frontend) {
    printf("Compiling %s → %s (frontend: %s)\n", source_file, output_kir, frontend);

    // HTML uses file-based parser to detect and load external CSS
    if (strcmp(frontend, "html") == 0) {
        char* json = ir_html_file_to_kir(source_file);
        if (!json) {
            fprintf(stderr, "Error: Failed to convert %s to KIR\n", source_file);
            return 1;
        }

        // Write to output file
        FILE* out = fopen(output_kir, "w");
        if (!out) {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
            free(json);
            return 1;
        }

        fprintf(out, "%s\n", json);
        fclose(out);
        free(json);
        return 0;
    }

    if (strcmp(frontend, "markdown") == 0 || strcmp(frontend, "kry") == 0) {
        // Read the source file
        FILE* f = fopen(source_file, "r");
        if (!f) {
            fprintf(stderr, "Error: Failed to open %s\n", source_file);
            return 1;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* source = (char*)malloc(size + 1);
        if (!source) {
            fprintf(stderr, "Error: Out of memory\n");
            fclose(f);
            return 1;
        }

        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);

        // Convert to KIR JSON based on frontend type
        char* json = NULL;
        if (strcmp(frontend, "kry") == 0) {
            // Use ir_kry_to_kir for complete manifest serialization
            json = ir_kry_to_kir(source, size);
        } else if (strcmp(frontend, "markdown") == 0) {
            json = ir_markdown_to_kir(source, size);
        }

        free(source);

        if (!json) {
            fprintf(stderr, "Error: Failed to convert %s to KIR\n", source_file);
            return 1;
        }

        // Write to output file
        FILE* out = fopen(output_kir, "w");
        if (!out) {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
            free(json);
            return 1;
        }

        fprintf(out, "%s\n", json);
        fclose(out);

        free(json);
        return 0;
    }
    else if (strcmp(frontend, "tsx") == 0 || strcmp(frontend, "jsx") == 0) {
        // Use C TSX parser (takes file path directly)
        char* json = ir_tsx_file_to_kir(source_file);

        if (!json) {
            fprintf(stderr, "Error: Failed to convert %s to KIR\n", source_file);
            return 1;
        }

        // Write to output file
        FILE* out = fopen(output_kir, "w");
        if (!out) {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
            free(json);
            return 1;
        }

        fprintf(out, "%s\n", json);
        fclose(out);

        free(json);
        return 0;
    }
    else if (strcmp(frontend, "c") == 0) {
        // Use C parser (compile-and-execute approach, takes file path directly)
        char* json = ir_c_file_to_kir(source_file);

        if (!json) {
            fprintf(stderr, "Error: Failed to convert %s to KIR\n", source_file);
            return 1;
        }

        // Write to output file
        FILE* out = fopen(output_kir, "w");
        if (!out) {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
            free(json);
            return 1;
        }

        fprintf(out, "%s\n", json);
        fclose(out);

        free(json);
        return 0;
    }
    else if (strcmp(frontend, "lua") == 0) {
        // Use Lua parser (execute-and-serialize approach, takes file path directly)
        char* json = ir_lua_file_to_kir(source_file);

        if (!json) {
            fprintf(stderr, "Error: Failed to convert %s to KIR\n", source_file);
            return 1;
        }

        // Strip any debug output before the JSON (find first '{')
        char* json_start = strchr(json, '{');
        if (!json_start) {
            fprintf(stderr, "Error: No valid JSON found in parser output\n");
            free(json);
            return 1;
        }

        // Write to output file
        FILE* out = fopen(output_kir, "w");
        if (!out) {
            fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
            free(json);
            return 1;
        }

        fprintf(out, "%s\n", json_start);
        fclose(out);

        free(json);
        return 0;
    }
    else if (strcmp(frontend, "nim") == 0) {
        fprintf(stderr, "Error: Nim is not an input format\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Nim is an OUTPUT format generated by the codegen:\n");
        fprintf(stderr, "  kryon codegen nim <input.kir> <output.nim>\n\n");
        fprintf(stderr, "Input formats (frontends):\n");
        fprintf(stderr, "  .kry        - Kryon DSL\n");
        fprintf(stderr, "  .md         - Markdown documents\n");
        fprintf(stderr, "  .html       - HTML documents\n");
        fprintf(stderr, "  .tsx, .jsx  - TypeScript/JavaScript with JSX\n");
        fprintf(stderr, "  .c          - C source with Kryon API\n");
        fprintf(stderr, "  .lua        - Lua with Kryon API\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Output formats (codegens):\n");
        fprintf(stderr, "  .nim        - Nim with Kryon DSL\n");
        fprintf(stderr, "  .lua        - Lua with Kryon API\n");
        fprintf(stderr, "  .c          - C source code\n");
        return 1;
    }
    else {
        fprintf(stderr, "Error: Frontend '%s' not yet implemented in compile command\n", frontend);
        return 1;
    }
}

int cmd_compile(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Error: No source file specified\n");
        fprintf(stderr, "Usage: kryon compile <file> [--output=<file>] [--preserve-static] [--no-cache]\n");
        return 1;
    }

    const char* source_file = argv[0];
    const char* output_file = NULL;
    bool use_cache = true;
    // bool preserve_static = false;  // TODO: implement preserve-static for codegen

    // Parse flags
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--output=", 9) == 0) {
            output_file = argv[i] + 9;
        } else if (strcmp(argv[i], "--no-cache") == 0) {
            use_cache = false;
        } else if (strcmp(argv[i], "--preserve-static") == 0) {
            // preserve_static = true;  // TODO: implement
        }
    }

    if (!file_exists(source_file)) {
        fprintf(stderr, "Error: Source file not found: %s\n", source_file);
        return 1;
    }

    // Detect frontend
    const char* frontend = detect_frontend(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        fprintf(stderr, "Supported extensions: .kry, .kir, .md, .html, .tsx, .jsx, .nim, .lua, .c\n");
        return 1;
    }

    // Determine output path
    char kir_file[1024];
    if (output_file) {
        // Use specified output path
        snprintf(kir_file, sizeof(kir_file), "%s", output_file);
    } else {
        // Create cache directory if using cache
        if (use_cache && !file_is_directory(".kryon_cache")) {
            dir_create(".kryon_cache");
        }

        // Generate KIR filename
        const char* basename = strrchr(source_file, '/');
        basename = basename ? basename + 1 : source_file;

        char* name_copy = str_copy(basename);
        char* dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';

        if (use_cache) {
            snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
        } else {
            snprintf(kir_file, sizeof(kir_file), "%s.kir", name_copy);
        }
        free(name_copy);
    }

    // Ensure output directory exists
    char* last_slash = strrchr(kir_file, '/');
    if (last_slash) {
        char* dir_path = str_copy(kir_file);
        dir_path[last_slash - kir_file] = '\0';
        if (!file_is_directory(dir_path)) {
            dir_create(dir_path);
        }
        free(dir_path);
    }

    // Load kryon.toml and set up plugin paths for Lua compilation
    if (strcmp(frontend, "lua") == 0) {
        KryonConfig* config = config_find_and_load();
        if (config && config->plugins_count > 0) {
            // Build plugin paths string (colon-separated absolute paths)
            char plugin_paths[4096] = "";
            char* cwd = dir_get_current();

            for (int i = 0; i < config->plugins_count; i++) {
                if (!config->plugins[i].enabled || !config->plugins[i].path) {
                    continue;
                }

                // Resolve plugin path to absolute
                char* abs_path;
                if (config->plugins[i].path[0] == '/') {
                    // Already absolute
                    abs_path = str_copy(config->plugins[i].path);
                } else {
                    // Relative to current directory
                    abs_path = path_join(cwd, config->plugins[i].path);
                }

                // Add to plugin_paths string
                if (plugin_paths[0] != '\0') {
                    strncat(plugin_paths, ":", sizeof(plugin_paths) - strlen(plugin_paths) - 1);
                }
                strncat(plugin_paths, abs_path, sizeof(plugin_paths) - strlen(plugin_paths) - 1);

                free(abs_path);
            }

            if (plugin_paths[0] != '\0') {
                setenv("KRYON_PLUGIN_PATHS", plugin_paths, 1);
            }

            free(cwd);
        }

        if (config) {
            config_free(config);
        }
    }

    // Compile to KIR
    int result = compile_to_kir(source_file, kir_file, frontend);

    if (result == 0) {
        printf("✓ KIR generated: %s\n", kir_file);
    } else {
        fprintf(stderr, "✗ Compilation failed\n");
    }

    return result;
}
