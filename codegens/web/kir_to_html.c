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
#include "../../ir/ir_plugin.h"
#include "ir_web_renderer.h"
#include "html_generator.h"
#include "lua_bundler.h"

// Global IR context (from ir_core.c)
extern IRContext* g_ir_context;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <source_dir> <input.kir> <output_dir> [--embedded-css] [--project-name <name>]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  source_dir         Directory containing source files (for asset copying)\n");
        fprintf(stderr, "  --embedded-css     Embed CSS in <style> tag instead of external file\n");
        fprintf(stderr, "  --project-name     Project name for handler namespace (e.g., 'habits')\n");
        return 1;
    }

    const char* source_dir = argv[1];
    const char* kir_file = argv[2];
    const char* output_dir = argv[3];
    bool embedded_css = false;
    const char* project_name = NULL;

    // Parse optional flags
    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "--embedded-css") == 0) {
            embedded_css = true;
        } else if (strcmp(argv[i], "--project-name") == 0 && i + 1 < argc) {
            project_name = argv[++i];
        }
    }

    printf("Converting KIR to HTML...\n");
    printf("  Source: %s\n", source_dir);
    printf("  Input: %s\n", kir_file);
    printf("  Output: %s/\n", output_dir);
    if (embedded_css) {
        printf("  Mode: Embedded CSS\n");
    }

    // Load plugins for web rendering (e.g., syntax highlighting)
    uint32_t plugin_count = 0;
    IRPluginDiscoveryInfo** plugins = ir_plugin_discover(NULL, &plugin_count);
    if (plugins && plugin_count > 0) {
        for (uint32_t i = 0; i < plugin_count; i++) {
            IRPluginDiscoveryInfo* info = plugins[i];
            if (ir_plugin_load_with_metadata(info->path, info->name, info)) {
                fprintf(stderr, "[kryon][plugin] Loaded plugin '%s' v%s\n",
                        info->name, info->version);
            }
        }
        ir_plugin_free_discovery(plugins, plugin_count);
    }

    // Load KIR file with manifest (for CSS variable support)
    IRReactiveManifest* manifest = NULL;
    IRComponent* root = ir_read_json_file_with_manifest(kir_file, &manifest);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

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
        // Check if this is a Lua app and bundle the Lua code
        // IMPORTANT: This must happen BEFORE destroying root since we need to
        // walk the KIR tree to extract component_id -> handler_index mappings
        if (g_ir_context && g_ir_context->source_metadata &&
            g_ir_context->source_metadata->source_language &&
            strcmp(g_ir_context->source_metadata->source_language, "lua") == 0) {

            printf("📦 Bundling Lua code for web...\n");

            LuaBundler* bundler = lua_bundler_create();
            if (bundler) {
                // Set up search paths using installed location
                char* home = getenv("HOME");
                char kryon_lua_path[PATH_MAX];
                if (home) {
                    snprintf(kryon_lua_path, sizeof(kryon_lua_path),
                             "%s/.local/share/kryon/bindings/lua", home);
                } else {
                    snprintf(kryon_lua_path, sizeof(kryon_lua_path), "/usr/local/share/kryon/bindings/lua");
                }

                lua_bundler_set_kryon_path(bundler, kryon_lua_path);
                lua_bundler_add_search_path(bundler, source_dir);
                // Note: kryon-storage plugins should be configured in kryon.toml
                // not hardcoded here
                lua_bundler_use_web_modules(bundler, true);

                // Set project name for automatic handler namespace export
                if (project_name) {
                    lua_bundler_set_project_name(bundler, project_name);
                    printf("  Using namespace: _G.%s\n", project_name);
                }

                // Bundle the main Lua file with handler map from KIR
                // The bundler extracts component_id -> handler_index mapping from root
                // Pass input kir_file as the source file reference
                char* bundled_script = lua_bundler_generate_script(bundler,
                    kir_file, root);

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

    return success ? 0 : 1;
}
