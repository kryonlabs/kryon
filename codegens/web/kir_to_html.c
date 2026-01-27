/**
 * KIR to HTML Converter
 * Simple CLI tool to convert KIR files to HTML
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// PATH_MAX might not be defined on some systems
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "ir_core.h"
#include "ir_serialization.h"
#include "ir_capability.h"
#include "ir_web_renderer.h"
#include "html_generator.h"

// Global IR context (from ir_core.c)
extern IRContext* g_ir_context;

/**
 * Callable function for embedding into kryon CLI
 * Converts KIR to HTML without spawning a subprocess
 *
 * @param source_dir Directory containing source files (for asset copying)
 * @param kir_file Path to the KIR file to convert
 * @param output_dir Directory where HTML will be written
 * @param project_name Optional project name for Lua namespace (can be NULL)
 * @return 0 on success, non-zero on failure
 */
int kir_to_html_main(const char* source_dir, const char* kir_file,
                     const char* output_dir, const char* project_name) {
    if (!source_dir || !kir_file || !output_dir) {
        fprintf(stderr, "Error: source_dir, kir_file, and output_dir are required\n");
        return 1;
    }

    printf("Converting KIR to HTML...\n");
    printf("  Source: %s\n", source_dir);
    printf("  Input: %s\n", kir_file);
    printf("  Output: %s/\n", output_dir);
    if (project_name) {
        printf("  Project: %s\n", project_name);
    }

    // Initialize capability system and load plugins for web rendering
    ir_capability_registry_init();

    // Get HOME once for use throughout this function
    const char* home = getenv("HOME");

    // Only load plugins if not already loaded by CLI
    // Check if syntax plugin is already registered (type 42 = CODE_BLOCK)
    if (!ir_capability_has_web_renderer(42)) {
        // Load plugins from standard plugin directories
        if (home) {
            // Try ~/.local/share/kryon/plugins first (where CLI installs them)
            char plugin_dir[PATH_MAX];
            snprintf(plugin_dir, sizeof(plugin_dir), "%s/.local/share/kryon/plugins", home);

            // Try to load common plugins
            const char* plugin_names[] = {"syntax", "flowchart", NULL};
            for (int i = 0; plugin_names[i]; i++) {
                char so_path[PATH_MAX];
                // Use the kryon_ prefix that matches the actual library names
                // Buffer is PATH_MAX (4096), which is sufficient for plugin paths
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(so_path, sizeof(so_path), "%s/%s/libkryon_%s.so", plugin_dir, plugin_names[i], plugin_names[i]);
#pragma GCC diagnostic pop

                if (ir_capability_load_plugin(so_path, plugin_names[i])) {
                    fprintf(stderr, "[kryon] Loaded plugin '%s'\n", plugin_names[i]);
                }
            }
        }
    }

    // Load KIR file with manifest (for CSS variable support)
    IRReactiveManifest* manifest = NULL;
    IRComponent* root = ir_read_json_file_with_manifest(kir_file, &manifest);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

    // Expand ForEach components before rendering
    // This is critical for web codegen - without expansion, ForEach renders as empty containers
    extern void ir_expand_foreach(IRComponent* root);
    ir_expand_foreach(root);

    if (manifest) {
        printf("  CSS Variables: %u found\n", manifest->variable_count);
    }

    // Create renderer manually to set source directory
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) {
        fprintf(stderr, "Error: Failed to create web renderer\n");
        ir_destroy_component(root);
        if (manifest) ir_reactive_manifest_destroy(manifest);
        return 1;
    }

    // Configure renderer
    web_ir_renderer_set_output_directory(renderer, output_dir);
    web_ir_renderer_set_source_directory(renderer, source_dir);
    if (manifest) {
        web_ir_renderer_set_manifest(renderer, manifest);
    }

    // Generate HTML
    bool success = web_ir_renderer_render(renderer, root);

    // Cleanup renderer (but keep root for Lua bundling)
    web_ir_renderer_destroy(renderer);

    if (success) {
        // Generate JavaScript reactive system from manifest (Phase 2: Self-Contained KIR)
        // This creates kryon-reactive.js with state proxy and DOM bindings
        if (manifest) {
            printf("📦 Generating JavaScript reactive system from manifest...\n");
            write_js_reactive_system(output_dir, manifest);
        } else {
            // Generate stub reactive system even without manifest
            printf("📦 Generating stub JavaScript reactive system (no manifest)...\n");
            write_js_reactive_system(output_dir, NULL);
        }
        // Check if this is a Lua app and bundle the Lua code
        printf("✓ HTML files generated successfully in %s/\n", output_dir);
    } else {
        fprintf(stderr, "✗ HTML generation failed\n");
    }

    // Cleanup - destroy root AFTER Lua bundling
    ir_destroy_component(root);
    if (manifest) {
        ir_reactive_manifest_destroy(manifest);
    }

    // Shutdown capability system and unload plugins
    ir_capability_registry_shutdown();

    return success ? 0 : 1;
}

// Only compile main() when building standalone (not when embedded in CLI)
#ifndef KRYON_EMBEDDED_CODEGEN
int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <source_dir> <input.kir> <output_dir> [--project-name <name>]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  source_dir         Directory containing source files (for asset copying)\n");
        fprintf(stderr, "  --project-name     Project name for handler namespace (e.g., 'habits')\n");
        return 1;
    }

    const char* source_dir = argv[1];
    const char* kir_file = argv[2];
    const char* output_dir = argv[3];
    const char* project_name = NULL;

    // Parse optional flags
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--project-name") == 0 && i + 1 < argc) {
            project_name = argv[++i];
        }
    }

    return kir_to_html_main(source_dir, kir_file, output_dir, project_name);
}
#endif  // KRYON_EMBEDDED_CODEGEN
