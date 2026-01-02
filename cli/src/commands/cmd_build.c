/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Codegen
 */

#include "kryon_cli.h"
#include "../template/docs_template.h"
#include "../utils/file_discovery.h"
#include "../../../ir/ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        // Run kryon compile for markdown
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "kryon compile \"%s\" > /dev/null 2>&1", source_file);
        system(cmd);

        // Calculate generated KIR filename (based on source basename)
        const char* slash = strrchr(source_file, '/');
        const char* basename = slash ? slash + 1 : source_file;
        char kir_name[512];
        strncpy(kir_name, basename, sizeof(kir_name) - 1);
        kir_name[sizeof(kir_name) - 1] = '\0';
        char* dot = strrchr(kir_name, '.');
        if (dot) *dot = '\0';

        char generated_kir[1024];
        snprintf(generated_kir, sizeof(generated_kir), ".kryon_cache/%s.kir", kir_name);

        // Move to desired output path
        if (file_exists(generated_kir)) {
            if (strcmp(generated_kir, output_kir) != 0) {
                char mv_cmd[2048];
                snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", generated_kir, output_kir);
                system(mv_cmd);
            }
            if (file_exists(output_kir)) {
                return 0;
            }
        }

        fprintf(stderr, "Error: Failed to compile markdown\n");
        return 1;
    }
    else if (strcmp(frontend, "tsx") == 0 || strcmp(frontend, "jsx") == 0) {
        // Use Bun to parse TSX/JSX
        char cmd[4096];
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
        char cmd[4096];
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
        char cmd[4096];
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

            char mv_cmd[4096];
            snprintf(mv_cmd, sizeof(mv_cmd), "mv \"%s\" \"%s\"", kir_file, output_kir);
            system(mv_cmd);
        }
        free(kir_file);

        return result;
    }
    else if (strcmp(frontend, "lua") == 0) {
        // Run Lua with Kryon bindings
        char cmd[4096];
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
    if (strcmp(target, "web") == 0) {
        // Generate HTML from KIR using web codegen (without starting dev server)
        char* cwd = dir_get_current();
        char abs_kir_path[2048];
        char abs_output_dir[2048];

        // Get absolute paths
        if (kir_file[0] == '/') {
            snprintf(abs_kir_path, sizeof(abs_kir_path), "%s", kir_file);
        } else {
            snprintf(abs_kir_path, sizeof(abs_kir_path), "%s/%s", cwd, kir_file);
        }

        if (output_dir[0] == '/') {
            snprintf(abs_output_dir, sizeof(abs_output_dir), "%s", output_dir);
        } else {
            snprintf(abs_output_dir, sizeof(abs_output_dir), "%s/%s", cwd, output_dir);
        }

        // Compile and run web codegen
        char codegen_cmd[8192];
        snprintf(codegen_cmd, sizeof(codegen_cmd),
                 "cd /mnt/storage/Projects/kryon/codegens/web && "
                 "gcc -std=c99 -O2 "
                 "-I../../ir -I../../ir/third_party/cJSON "
                 "kir_to_html.c ir_web_renderer.c html_generator.c css_generator.c wasm_bridge.c style_analyzer.c "
                 "../../ir/third_party/cJSON/cJSON.c "
                 "-L../../build -lkryon_ir -lm "
                 "-o /tmp/kryon_web_gen_%d 2>&1 && "
                 "LD_LIBRARY_PATH=/mnt/storage/Projects/kryon/build:$LD_LIBRARY_PATH "
                 "/tmp/kryon_web_gen_%d \"%s\" \"%s\" \"%s\" 2>&1",
                 getpid(), getpid(), cwd, abs_kir_path, abs_output_dir);

        free(cwd);

        int result = system(codegen_cmd);
        if (result != 0) {
            fprintf(stderr, "Error: HTML generation failed\n");
            return 1;
        }

        return 0;
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
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        fprintf(stderr, "Use a supported file extension (.tsx, .html, .md, .kry, etc.)\n");
        return 1;
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

/**
 * Build a discovered file with template application if needed
 */
static int build_discovered_file(DiscoveredFile* file, KryonConfig* config, const char* docs_template) {
    if (!file || !file->source_path) {
        fprintf(stderr, "Error: Invalid file entry\n");
        return 1;
    }

    printf("  %s → %s\n", file->source_path, file->output_path);

    // Detect frontend
    const char* frontend = detect_frontend(file->source_path);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", file->source_path);
        return 1;
    }

    // Create cache directory
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Generate KIR filename
    const char* slash = strrchr(file->source_path, '/');
    const char* basename = slash ? slash + 1 : file->source_path;
    char kir_name[512];
    strncpy(kir_name, basename, sizeof(kir_name) - 1);
    kir_name[sizeof(kir_name) - 1] = '\0';
    char* dot = strrchr(kir_name, '.');
    if (dot) *dot = '\0';

    char content_kir_file[1024];
    snprintf(content_kir_file, sizeof(content_kir_file), ".kryon_cache/%s_content.kir", kir_name);

    // Compile to KIR
    int result = compile_to_kir(file->source_path, content_kir_file, frontend);
    if (result != 0 || !file_exists(content_kir_file)) {
        fprintf(stderr, "Error: Compilation failed\n");
        return result != 0 ? result : 1;
    }

    // Extract directory from output path
    char output_dir[2048];
    strncpy(output_dir, file->output_path, sizeof(output_dir) - 1);
    output_dir[sizeof(output_dir) - 1] = '\0';
    char* last_slash = strrchr(output_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    }

    // Create output directory
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Apply docs template if this is a markdown file in docs/
    bool should_apply_template = false;
    if (docs_template && str_starts_with(file->source_path, "docs/") &&
        strcmp(frontend, "markdown") == 0 && file_exists(docs_template)) {
        should_apply_template = true;
    }

    if (should_apply_template) {
        DocsTemplateContext* template_ctx = docs_template_create(docs_template);
        if (template_ctx) {
            docs_template_scan_docs(template_ctx, "docs", "/docs");

            IRComponent* content_kir = ir_read_json_file(content_kir_file);
            if (content_kir) {
                IRComponent* combined_kir = docs_template_apply(template_ctx, content_kir, file->route);
                if (combined_kir) {
                    char combined_kir_file[1024];
                    snprintf(combined_kir_file, sizeof(combined_kir_file),
                             ".kryon_cache/%s_combined.kir", kir_name);

                    if (ir_write_json_file(combined_kir, NULL, combined_kir_file)) {
                        ir_destroy_component(content_kir);
                        ir_destroy_component(combined_kir);

                        result = invoke_codegen(combined_kir_file, config->build_target, output_dir);
                        docs_template_free(template_ctx);
                        return result;
                    }
                    ir_destroy_component(combined_kir);
                }
                ir_destroy_component(content_kir);
            }
            docs_template_free(template_ctx);
        }
    }

    // Use content KIR directly (no template or template failed)
    result = invoke_codegen(content_kir_file, config->build_target, output_dir);
    return result;
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

    // If specific file argument provided, build just that file
    if (argc > 0) {
        const char* source_file = argv[0];

        if (!file_exists(source_file)) {
            fprintf(stderr, "Error: Source file not found: %s\n", source_file);
            config_free(config);
            return 1;
        }

        result = build_single_file(source_file, config);
        config_free(config);
        return result;
    }

    // Auto-discover all buildable files in project
    DiscoveryResult* discovered = discover_project_files(".");

    if (!discovered || discovered->count == 0) {
        fprintf(stderr, "No buildable files found.\n");
        fprintf(stderr, "Create index.html or docs/*.md to get started.\n");
        if (discovered) free_discovery_result(discovered);
        config_free(config);
        return 1;
    }

    // Build all discovered files
    printf("Building %d file(s)...\n", discovered->count);
    for (uint32_t i = 0; i < discovered->count && result == 0; i++) {
        result = build_discovered_file(&discovered->files[i], config, discovered->docs_template);
    }

    if (result == 0) {
        printf("✓ Build complete\n");
    }

    free_discovery_result(discovered);
    config_free(config);
    return result;
}
