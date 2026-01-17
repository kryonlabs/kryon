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

#include "../../ir/ir_core.h"
#include "../../ir/ir_serialization.h"
#include "../../ir/ir_capability.h"
#include "ir_web_renderer.h"
#include "html_generator.h"
#include "lua_bundler.h"

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
        // IMPORTANT: This must happen BEFORE destroying root since we need to
        // walk the KIR tree to extract component_id -> handler_index mappings
        if (g_ir_context && g_ir_context->source_metadata &&
            g_ir_context->source_metadata->source_language &&
            strcmp(g_ir_context->source_metadata->source_language, "lua") == 0) {

            printf("📦 Bundling Lua app for web...\n");

            LuaBundler* bundler = lua_bundler_create();
            if (bundler) {
                // Set project name for automatic handler namespace export
                if (project_name) {
                    lua_bundler_set_project_name(bundler, project_name);
                    printf("  Using namespace: __kryon_app__ (project: %s)\n", project_name);
                }

                // Setup bundler search paths for module resolution
                lua_bundler_add_search_path(bundler, source_dir);
                lua_bundler_use_web_modules(bundler, true);

                // Set kryon bindings path for kryon.* modules
                if (home) {
                    char bindings_path[PATH_MAX];
                    snprintf(bindings_path, sizeof(bindings_path), "%s/.local/share/kryon/bindings/lua", home);
                    lua_bundler_set_kryon_path(bundler, bindings_path);
                }

                // Get source file path from metadata for full bundling
                const char* source_file = g_ir_context->source_metadata->source_file;
                char* bundled_script = NULL;

                if (source_file && source_file[0] != '\0') {
                    // FULL BUNDLING MODE: Read main.lua and bundle all dependencies
                    // This includes all variables, functions, and modules that handlers need
                    printf("  Mode: Full bundling from source file\n");
                    printf("  Source: %s\n", source_file);
                    bundled_script = lua_bundler_generate_script(bundler, source_file, root);
                } else {
                    // FALLBACK: Generate from KIR only (handlers might have undefined references)
                    printf("  Mode: KIR-only (no source file path in metadata)\n");
                    printf("  Warning: Handlers may reference undefined variables/functions\n");
                    bundled_script = lua_bundler_generate_from_kir(bundler, root);
                }

                if (bundled_script) {
                    // Append to HTML file
                    char html_path[512];
                    snprintf(html_path, sizeof(html_path), "%s/index.html", output_dir);

                    // Read existing HTML
                    FILE* html_file = fopen(html_path, "r");
                    if (html_file) {
                        fseek(html_file, 0, SEEK_END);
                        long html_size = ftell(html_file);
                        fseek(html_file, 0, SEEK_SET);

                        char* html_content = malloc(html_size + 1);
                        if (html_content) {
                            size_t read = fread(html_content, 1, html_size, html_file);
                            html_content[read] = '\0';
                            fclose(html_file);

                            // Find </body> and insert script tag before it
                            char* body_end = strstr(html_content, "</body>");
                            if (body_end) {
                                // Strip <script type="application/lua"> wrapper for external file
                                const char* script_start = "<script type=\"application/lua\">\n";
                                const char* script_end = "\n</script>";
                                char* lua_code_start = bundled_script;
                                char* lua_code_only = NULL;

                                // Skip the opening script tag if present
                                if (strncmp(lua_code_start, script_start, strlen(script_start)) == 0) {
                                    lua_code_start += strlen(script_start);
                                }

                                // Find and remove the closing script tag
                                char* closing_tag = strstr(lua_code_start, script_end);
                                if (closing_tag) {
                                    size_t lua_len = closing_tag - lua_code_start;
                                    lua_code_only = malloc(lua_len + 1);
                                    if (lua_code_only) {
                                        memcpy(lua_code_only, lua_code_start, lua_len);
                                        lua_code_only[lua_len] = '\0';
                                    }
                                }

                                // Write Lua code to external file (without HTML script tags)
                                char lua_path[512];
                                snprintf(lua_path, sizeof(lua_path), "%s/app.lua", output_dir);
                                FILE* lua_file = fopen(lua_path, "w");
                                if (lua_file) {
                                    fputs(lua_code_only ? lua_code_only : lua_code_start, lua_file);
                                    fclose(lua_file);
                                    printf("✅ Wrote Lua code to app.lua\n");
                                }
                                if (lua_code_only) free(lua_code_only);

                                // Write new HTML with external script reference
                                html_file = fopen(html_path, "w");
                                if (html_file) {
                                    // Write everything before </body>
                                    fwrite(html_content, 1, body_end - html_content, html_file);
                                    // Write script tag to load external Lua file
                                    fprintf(html_file, "\n<script src=\"app.lua\" type=\"application/lua\"></script>\n");
                                    // Write </body></html>
                                    fprintf(html_file, "%s", body_end);
                                    fclose(html_file);
                                }
                            }
                            free(html_content);
                        } else {
                            fclose(html_file);
                        }
                    }
                    free(bundled_script);
                } else {
                    fprintf(stderr, "⚠️  Warning: Could not bundle Lua code\n");
                }

                lua_bundler_destroy(bundler);
            }
        }

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
