/**
 * Build Command
 * Orchestrates the build pipeline: Frontend → KIR → Codegen
 */

#include "kryon_cli.h"
#include "build.h"
#include "../template/docs_template.h"
#include "../utils/file_discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Build a single source file
 */
static int build_single_file(const char* source_file, KryonConfig* config) {
    const char* frontend = detect_frontend_type(source_file);
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

    // Compile to KIR using shared function
    int result = compile_source_to_kir(source_file, kir_file);
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

    // Generate HTML using shared function
    result = generate_html_from_kir(kir_file, output_dir, config->project_name, ".");
    if (result != 0) {
        fprintf(stderr, "Error: Code generation failed\n");
        return result;
    }

    printf("✓ Build complete: %s → %s\n", source_file, output_dir);
    return 0;
}

/**
 * Build a discovered file with template application if needed
 */
static int build_discovered_file(DiscoveredFile* file, KryonConfig* config, const char* docs_template) {
    if (!file || !file->source_path) {
        fprintf(stderr, "Error: Invalid file entry\n");
        return 1;
    }

    printf("  %s → %s\n", file->source_path, file->output_path);

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
    int result = compile_source_to_kir(file->source_path, content_kir_file);
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

    // Use shared docs template function
    result = build_with_docs_template(
        content_kir_file,
        file->source_path,
        file->route,
        output_dir,
        docs_template,
        config
    );

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
