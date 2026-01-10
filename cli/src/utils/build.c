/**
 * Build Utilities
 *
 * Shared build/compile/run logic for kryon CLI commands.
 * Eliminates duplication between cmd_run, cmd_build, and cmd_compile.
 */

#include "kryon_cli.h"
#include "build.h"

#include "../../../ir/ir_serialization.h"
#include "../../../ir/ir_builder.h"
#include "../../../ir/ir_executor.h"
#include "../../../ir/parsers/lua/lua_parser.h"
#include "../../../backends/desktop/ir_desktop_renderer.h"
#include "../template/docs_template.h"
#include "../../../codegens/kry/kry_codegen.h"
#include "../../../codegens/nim/nim_codegen.h"
#include "../../../codegens/lua/lua_codegen.h"
#include "../../../codegens/tsx/tsx_codegen.h"
#include "../../../codegens/c/ir_c_codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ============================================================================
 * Frontend Detection
 * ============================================================================ */

const char* detect_frontend_type(const char* source_file) {
    const char* ext = path_extension(source_file);

    if (strcmp(ext, ".tsx") == 0 || strcmp(ext, ".jsx") == 0) {
        return "tsx";
    }
    else if (strcmp(ext, ".kir") == 0) {
        return "kir";
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

/* ============================================================================
 * KIR Compilation
 * ============================================================================ */

int compile_source_to_kir(const char* source_file, const char* output_kir) {
    const char* frontend = detect_frontend_type(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
        return 1;
    }

    printf("Compiling %s → %s (frontend: %s)\n", source_file, output_kir, frontend);

    // Ensure cache directory exists
    if (!file_is_directory(".kryon_cache")) {
        dir_create(".kryon_cache");
    }

    // Lua: use direct parser
    if (strcmp(frontend, "lua") == 0) {
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

    // TSX: use bun parser
    if (strcmp(frontend, "tsx") == 0 || strcmp(frontend, "jsx") == 0) {
        char* tsx_parser = paths_get_tsx_parser_path();
        if (!tsx_parser || !file_exists(tsx_parser)) {
            fprintf(stderr, "Error: TSX parser not found\n");
            fprintf(stderr, "Please run 'make install' in the CLI directory\n");
            if (tsx_parser) free(tsx_parser);
            return 1;
        }

        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "bun run \"%s\" \"%s\" > \"%s\"",
                 tsx_parser, source_file, output_kir);
        free(tsx_parser);

        char* output = NULL;
        int result = process_run(cmd, &output);
        if (output && strlen(output) > 0) {
            fprintf(stderr, "%s", output);
            free(output);
        }
        return result;
    }

    // Markdown: use kryon compile (simpler than reimplementing)
    if (strcmp(frontend, "markdown") == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "kryon compile \"%s\" > /dev/null 2>&1", source_file);
        system(cmd);

        // Calculate generated KIR filename
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

    // HTML: use kryon compile
    if (strcmp(frontend, "html") == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "kryon compile \"%s\" --output=\"%s\" 2>&1",
                 source_file, output_kir);

        int result = system(cmd);
        if (file_exists(output_kir)) {
            return 0;
        }

        fprintf(stderr, "Error: Failed to compile HTML (exit code: %d)\n", result);
        return 1;
    }

    // .kry DSL: delegated to Nim CLI
    if (strcmp(frontend, "kry") == 0) {
        fprintf(stderr, "Error: .kry frontend requires Nim CLI integration\n");
        fprintf(stderr, "Use: kryon compile (Nim CLI) for .kry files\n");
        return 1;
    }

    // Nim: compile with KRYON_SERIALIZE_IR flag
    if (strcmp(frontend, "nim") == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "nim c -d:KRYON_SERIALIZE_IR -r \"%s\"",
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

    fprintf(stderr, "Error: Unknown frontend type: %s\n", frontend);
    return 1;
}

/* ============================================================================
 * Web Codegen (HTML Generation)
 * ============================================================================ */

int generate_html_from_kir(const char* kir_file, const char* output_dir,
                           const char* project_name, const char* source_dir) {
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

    // Run web codegen
    char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        free(cwd);
        return 1;
    }

    char kir_to_html_path[PATH_MAX];
    snprintf(kir_to_html_path, sizeof(kir_to_html_path),
             "%s/.local/share/kryon/bin/kir_to_html", home);

    if (!file_exists(kir_to_html_path)) {
        fprintf(stderr, "Error: kir_to_html not found at %s\n", kir_to_html_path);
        fprintf(stderr, "Please run 'make install' in the CLI directory\n");
        free(cwd);
        return 1;
    }

    char codegen_cmd[8192];
    if (project_name) {
        snprintf(codegen_cmd, sizeof(codegen_cmd),
                 "\"%s\" \"%s\" \"%s\" \"%s\" --project-name \"%s\" 2>&1",
                 kir_to_html_path, source_dir, abs_kir_path, abs_output_dir, project_name);
    } else {
        snprintf(codegen_cmd, sizeof(codegen_cmd),
                 "\"%s\" \"%s\" \"%s\" \"%s\" 2>&1",
                 kir_to_html_path, source_dir, abs_kir_path, abs_output_dir);
    }

    free(cwd);

    int result = system(codegen_cmd);
    if (result != 0) {
        fprintf(stderr, "Error: HTML generation failed\n");
        return 1;
    }

    return 0;
}

/* ============================================================================
 * Lua Runtime Detection
 * ============================================================================ */

bool kir_needs_lua_runtime(const char* kir_file) {
    fprintf(stderr, "[kir_needs_lua_runtime] About to load: %s\n", kir_file);
    // Load KIR to check metadata
    IRComponent* root = ir_read_json_file(kir_file);
    fprintf(stderr, "[kir_needs_lua_runtime] Loaded root: %p\n", (void*)root);
    if (!root) {
        return false;
    }

    bool needs_lua = false;
    fprintf(stderr, "[kir_needs_lua_runtime] Checking metadata, g_ir_context=%p\n", (void*)g_ir_context);
    if (g_ir_context && g_ir_context->source_metadata) {
        IRSourceMetadata* meta = g_ir_context->source_metadata;
        fprintf(stderr, "[kir_needs_lua_runtime] meta=%p, source_language=%p\n",
                (void*)meta, (void*)(meta->source_language));
        if (meta->source_language && strcmp(meta->source_language, "lua") == 0) {
            needs_lua = true;
        }
    }

    fprintf(stderr, "[kir_needs_lua_runtime] About to destroy root\n");
    ir_destroy_component(root);
    fprintf(stderr, "[kir_needs_lua_runtime] Destroyed root, returning needs_lua=%d\n", needs_lua);
    return needs_lua;
}

/* ============================================================================
 * Docs Template
 * ============================================================================ */

int build_with_docs_template(const char* content_kir_file,
                             const char* source_path,
                             const char* route,
                             const char* output_dir,
                             const char* docs_template_path,
                             KryonConfig* config) {
    // Extract route from source_path if not provided
    char route_buf[256];
    if (!route || strlen(route) == 0) {
        // Convert "docs/guide.md" -> "guide"
        const char* basename = strrchr(source_path, '/');
        basename = basename ? basename + 1 : source_path;
        strncpy(route_buf, basename, sizeof(route_buf) - 1);
        route_buf[sizeof(route_buf) - 1] = '\0';
        char* dot = strrchr(route_buf, '.');
        if (dot) *dot = '\0';
        route = route_buf;
    }

    // No template: use content KIR directly
    if (!docs_template_path || !file_exists(docs_template_path)) {
        return generate_html_from_kir(content_kir_file, output_dir,
                                      config ? config->project_name : NULL,
                                      ".");
    }

    // Create template context
    DocsTemplateContext* template_ctx = docs_template_create(docs_template_path);
    if (!template_ctx) {
        return generate_html_from_kir(content_kir_file, output_dir,
                                      config ? config->project_name : NULL,
                                      ".");
    }

    docs_template_scan_docs(template_ctx, "docs", "/docs");

    IRComponent* content_kir = ir_read_json_file(content_kir_file);
    if (!content_kir) {
        docs_template_free(template_ctx);
        return 1;
    }

    IRComponent* combined_kir = docs_template_apply(template_ctx, content_kir, route);
    if (!combined_kir) {
        ir_destroy_component(content_kir);
        docs_template_free(template_ctx);
        return 1;
    }

    // Write combined KIR to temp file
    char combined_kir_file[1024];
    snprintf(combined_kir_file, sizeof(combined_kir_file),
             ".kryon_cache/%s_combined.kir", route);

    if (!ir_write_json_file(combined_kir, NULL, combined_kir_file)) {
        ir_destroy_component(content_kir);
        ir_destroy_component(combined_kir);
        docs_template_free(template_ctx);
        return 1;
    }

    ir_destroy_component(content_kir);
    ir_destroy_component(combined_kir);
    docs_template_free(template_ctx);

    return generate_html_from_kir(combined_kir_file, output_dir,
                                  config ? config->project_name : NULL,
                                  ".");
}

/* ============================================================================
 * Desktop Execution
 * ============================================================================ */

int run_kir_with_lua_runtime(const char* kir_file) {
    char* kryon_root = paths_get_kryon_root();
    if (!kryon_root) {
        // Fallback to user install location
        char* home = paths_get_home_dir();
        if (home) {
            kryon_root = path_join(home, ".local/share/kryon");
            free(home);
        } else {
            kryon_root = str_copy("/usr/local/share/kryon");
        }
    }

    KryonConfig* kryon_config = config_find_and_load();

    // Build plugin paths from config and environment
    char plugin_paths_buffer[2048] = "";
    char* plugin_paths = getenv("KRYON_PLUGIN_PATHS");

    // If config has plugins, build paths
    if (kryon_config && kryon_config->plugins && kryon_config->plugins_count > 0) {
        char* cwd = dir_get_current();
        for (int i = 0; i < kryon_config->plugins_count; i++) {
            PluginDep* plugin = &kryon_config->plugins[i];
            if (!plugin->enabled || !plugin->path) continue;

            // Resolve relative paths
            char* resolved_path = NULL;
            if (plugin->path[0] == '/') {
                resolved_path = str_copy(plugin->path);
            } else {
                resolved_path = path_join(cwd, plugin->path);
            }

            // Add to plugin_paths_buffer
            if (strlen(plugin_paths_buffer) > 0) {
                strncat(plugin_paths_buffer, ":", sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
            }
            strncat(plugin_paths_buffer, resolved_path, sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
            free(resolved_path);
        }
        free(cwd);

        // If env var is also set, append it
        if (plugin_paths && strlen(plugin_paths) > 0) {
            if (strlen(plugin_paths_buffer) > 0) {
                strncat(plugin_paths_buffer, ":", sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
            }
            strncat(plugin_paths_buffer, plugin_paths, sizeof(plugin_paths_buffer) - strlen(plugin_paths_buffer) - 1);
        }

        // Use the combined paths
        if (strlen(plugin_paths_buffer) > 0) {
            plugin_paths = plugin_paths_buffer;
        }
    }

    char lua_cmd[8192];
    if (plugin_paths && strlen(plugin_paths) > 0) {
        // Build LUA_PATH, LUA_CPATH, and LD_LIBRARY_PATH from plugin paths
        char lua_path[4096] = "";
        char lua_cpath[2048] = "";
        char ld_library_path[2048] = "";

        // Start with kryon root paths
        snprintf(lua_path, sizeof(lua_path),
                 "%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua",
                 kryon_root, kryon_root);

        // Parse plugin_paths (colon-separated)
        char* paths_copy = strdup(plugin_paths);
        char* path_token = strtok(paths_copy, ":");
        while (path_token != NULL) {
            // Append to LUA_PATH
            size_t len = strlen(lua_path);
            snprintf(lua_path + len, sizeof(lua_path) - len,
                     ";%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua",
                     path_token, path_token);

            // Append to LUA_CPATH
            if (strlen(lua_cpath) > 0) {
                strncat(lua_cpath, ";", sizeof(lua_cpath) - strlen(lua_cpath) - 1);
            }
            len = strlen(lua_cpath);
            snprintf(lua_cpath + len, sizeof(lua_cpath) - len,
                     "%s/build/?.so", path_token);

            // Append to LD_LIBRARY_PATH
            if (strlen(ld_library_path) > 0) {
                strncat(ld_library_path, ":", sizeof(ld_library_path) - strlen(ld_library_path) - 1);
            }
            strncat(ld_library_path, path_token, sizeof(ld_library_path) - strlen(ld_library_path) - 1);
            strncat(ld_library_path, "/build", sizeof(ld_library_path) - strlen(ld_library_path) - 1);

            path_token = strtok(NULL, ":");
        }
        free(paths_copy);

        // Terminate LUA_PATH and LUA_CPATH
        strncat(lua_path, ";;", sizeof(lua_path) - strlen(lua_path) - 1);
        strncat(lua_cpath, ";;", sizeof(lua_cpath) - strlen(lua_cpath) - 1);

        snprintf(lua_cmd, sizeof(lua_cmd),
                 "KRYON_PLUGIN_PATHS=\"%s\" "
                 "LUA_PATH=\"%s\" "
                 "LUA_CPATH=\"%s\" "
                 "LD_LIBRARY_PATH=\"%s:%s/build:$LD_LIBRARY_PATH\" "
                 "luajit -e '"
                 "local Runtime = require(\"kryon.runtime\"); "
                 "local app = Runtime.loadKIR(\"%s\"); "
                 "Runtime.runDesktop(app)'",
                 plugin_paths, lua_path, lua_cpath, ld_library_path, kryon_root, kir_file);
    } else {
        snprintf(lua_cmd, sizeof(lua_cmd),
                 "LUA_PATH=\"%s/bindings/lua/?.lua;%s/bindings/lua/?/init.lua;;\" "
                 "LD_LIBRARY_PATH=\"%s/build:$LD_LIBRARY_PATH\" "
                 "luajit -e '"
                 "local Runtime = require(\"kryon.runtime\"); "
                 "local app = Runtime.loadKIR(\"%s\"); "
                 "Runtime.runDesktop(app)'",
                 kryon_root, kryon_root, kryon_root, kir_file);
    }

    if (kryon_config) config_free(kryon_config);
    free(kryon_root);

    int result = system(lua_cmd);
    return (result == 0) ? 0 : 1;
}

int run_kir_on_desktop(const char* kir_file, const char* desktop_lib) {
    // Get desktop lib path if not provided
    const char* lib = desktop_lib;
    if (!lib) {
        lib = getenv("KRYON_DESKTOP_LIB");
        if (!lib) {
            char* found_lib = paths_find_library("libkryon_desktop.so");
            if (!found_lib) {
                fprintf(stderr, "Error: Desktop renderer library not found\n");
                fprintf(stderr, "Please run: make install\n");
                return 1;
            }
            lib = found_lib;  // Note: leaked but used immediately
        }
    }

    // Load KIR file
    IRComponent* root = ir_read_json_file(kir_file);
    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file: %s\n", kir_file);
        return 1;
    }

    // Expand ForEach components
    extern void ir_expand_foreach(IRComponent* root);
    ir_expand_foreach(root);

    // Check for Lua runtime
    if (kir_needs_lua_runtime(kir_file)) {
        fprintf(stderr, "[run_kir_on_desktop] kir_needs_lua_runtime returned true, destroying root=%p\n", (void*)root);
        ir_destroy_component(root);
        fprintf(stderr, "[run_kir_on_desktop] Root destroyed, calling run_kir_with_lua_runtime\n");
        return run_kir_with_lua_runtime(kir_file);
    }

    // Setup executor
    IRExecutorContext* executor = ir_executor_create();
    if (executor) {
        printf("[executor] Loading KIR file for event handling\n");
        ir_executor_load_kir_file(executor, kir_file);
        ir_executor_set_root(executor, root);
        ir_executor_set_global(executor);
        printf("[executor] Global executor initialized\n");
    } else {
        fprintf(stderr, "[executor] WARNING: Failed to create executor!\n");
    }

    // Configure renderer from metadata
    int window_width = 800;
    int window_height = 600;
    const char* window_title = "Kryon App";

    if (g_ir_context && g_ir_context->metadata) {
        if (g_ir_context->metadata->window_width > 0) {
            window_width = g_ir_context->metadata->window_width;
        }
        if (g_ir_context->metadata->window_height > 0) {
            window_height = g_ir_context->metadata->window_height;
        }
        if (g_ir_context->metadata->window_title) {
            window_title = g_ir_context->metadata->window_title;
        }
    }

    DesktopRendererConfig config = desktop_renderer_config_sdl3(window_width, window_height, window_title);

    // Read kryon.toml to determine renderer
    KryonConfig* kryon_config = config_find_and_load();
    if (kryon_config && kryon_config->desktop_renderer) {
        if (strcmp(kryon_config->desktop_renderer, "raylib") == 0) {
            printf("Renderer: raylib (from kryon.toml)\n");
            config.backend_type = DESKTOP_BACKEND_RAYLIB;
        } else {
            printf("Renderer: sdl3 (from kryon.toml)\n");
            config.backend_type = DESKTOP_BACKEND_SDL3;
        }
        config_free(kryon_config);
    } else {
        printf("Renderer: sdl3 (default)\n");
        config.backend_type = DESKTOP_BACKEND_SDL3;
    }

    // Run the application
    printf("Running application...\n");
    bool success = desktop_render_ir_component(root, &config);

    // Cleanup
    if (executor) {
        ir_executor_destroy(executor);
    }
    ir_destroy_component(root);

    return success ? 0 : 1;
}

/* ============================================================================
 * Build Pipeline
 * ============================================================================ */

int build_source_file(const char* source_file, BuildOptions* opts, KryonConfig* config) {
    const char* frontend = detect_frontend_type(source_file);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source_file);
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

    // Determine output directory
    const char* output_dir = opts && opts->output_dir ? opts->output_dir :
                             (config && config->build_output_dir ? config->build_output_dir : "dist");

    // Create output directory
    if (!file_is_directory(output_dir)) {
        dir_create_recursive(output_dir);
    }

    // Invoke codegen based on target
    const char* target = opts && opts->target ? opts->target :
                          (config && config->build_target ? config->build_target : "web");

    const char* project_name = opts && opts->project_name ? opts->project_name :
                                (config ? config->project_name : NULL);

    if (strcmp(target, "web") == 0) {
        result = generate_html_from_kir(kir_file, output_dir, project_name, ".");
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

    if (result != 0) {
        fprintf(stderr, "Error: Code generation failed\n");
        return result;
    }

    printf("✓ Build complete: %s → %s\n", source_file, output_dir);
    return 0;
}

/* ============================================================================
 * Codegen Pipeline
 * ============================================================================ */

char* get_codegen_output_dir(const char* target, KryonConfig* config) {
    char* base_dir;

    // Determine base directory from config
    if (config && config->codegen_output_dir) {
        base_dir = str_copy(config->codegen_output_dir);
    } else if (config && config->build_output_dir) {
        base_dir = str_copy(config->build_output_dir);
    } else {
        base_dir = str_copy("build");
    }

    // All codegens use <base>/<target>/ structure for organization
    char* result = path_join(base_dir, target);
    free(base_dir);
    return result;
}

int generate_from_kir(const char* kir_file, const char* target,
                      const char* output_path) {
    bool success = false;

    if (strcmp(target, "kry") == 0) {
        success = kry_codegen_generate(kir_file, output_path);
    } else if (strcmp(target, "tsx") == 0) {
        success = tsx_codegen_generate(kir_file, output_path);
    } else if (strcmp(target, "nim") == 0) {
        success = nim_codegen_generate(kir_file, output_path);
    } else if (strcmp(target, "lua") == 0) {
        // Multi-file codegen is default for Lua
        success = lua_codegen_generate_multi(kir_file, output_path);
    } else if (strcmp(target, "c") == 0) {
        success = ir_generate_c_code(kir_file, output_path);
    } else {
        fprintf(stderr, "Error: Unsupported codegen target: %s\n", target);
        return 1;
    }

    return success ? 0 : 1;
}

int codegen_pipeline(const char* source_file, const char* target,
                     const char* output_dir, KryonConfig* config) {
    // Load config if not provided
    bool free_config = false;
    if (!config) {
        config = config_find_and_load();
        if (!config) {
            fprintf(stderr, "Error: Could not find kryon.toml\n");
            return 1;
        }
        free_config = true;
    }

    // Determine source file
    const char* source = source_file;
    if (!source) {
        if (!config->build_entry) {
            fprintf(stderr, "Error: No source file specified and no build.entry in kryon.toml\n");
            if (free_config) config_free(config);
            return 1;
        }
        source = config->build_entry;
    }

    // Detect source type
    const char* frontend = detect_frontend_type(source);
    if (!frontend) {
        fprintf(stderr, "Error: Could not detect frontend for %s\n", source);
        if (free_config) config_free(config);
        return 1;
    }

    // If source is already KIR, skip compilation
    bool needs_compile = (strcmp(frontend, "kir") != 0);

    // Generate KIR path
    char kir_file[1024];
    if (needs_compile) {
        // Create cache directory
        if (!file_is_directory(".kryon_cache")) {
            dir_create(".kryon_cache");
        }

        const char* basename = strrchr(source, '/');
        basename = basename ? basename + 1 : source;
        char* name_copy = str_copy(basename);
        char* dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';

        snprintf(kir_file, sizeof(kir_file), ".kryon_cache/%s.kir", name_copy);
        free(name_copy);

        // Compile source to KIR
        printf("Compiling %s → %s\n", source, kir_file);
        int result = compile_source_to_kir(source, kir_file);
        if (result != 0) {
            fprintf(stderr, "Error: Compilation failed\n");
            if (free_config) config_free(config);
            return result;
        }
    } else {
        // Source is KIR
        strncpy(kir_file, source, sizeof(kir_file) - 1);
        kir_file[sizeof(kir_file) - 1] = '\0';
    }

    // Determine output directory
    const char* output = output_dir;
    char* output_to_free = NULL;
    if (!output) {
        output = output_to_free = get_codegen_output_dir(target, config);
    }

    // Create output directory
    if (!file_is_directory(output)) {
        dir_create_recursive(output);
    }

    // Generate output filename
    // All targets use <base>/<target>/filename.ext structure
    char output_path[2048];

    // Get base filename from source (e.g., main.lua -> main)
    const char* basename = strrchr(source, '/');
    basename = basename ? basename + 1 : source;
    char* name_copy = str_copy(basename);
    char* dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    // Determine file extension for target
    const char* ext = NULL;
    if (strcmp(target, "kry") == 0) ext = ".kry";
    else if (strcmp(target, "nim") == 0) ext = ".nim";
    else if (strcmp(target, "c") == 0) ext = ".c";
    else if (strcmp(target, "tsx") == 0) ext = ".tsx";
    else if (strcmp(target, "lua") == 0) {
        // Lua multi-file: output is the directory itself
        snprintf(output_path, sizeof(output_path), "%s", output);
    } else {
        fprintf(stderr, "Error: Unknown target: %s\n", target);
        free(name_copy);
        if (output_to_free) free(output_to_free);
        if (free_config) config_free(config);
        return 1;
    }

    if (strcmp(target, "lua") != 0) {
        // Single-file output: build/<target>/main.ext
        snprintf(output_path, sizeof(output_path), "%s/%s%s", output, name_copy, ext);
    }
    free(name_copy);

    // Run codegen
    printf("Generating %s code: %s → %s\n", target, kir_file, output_path);
    int result = generate_from_kir(kir_file, target, output_path);

    if (result == 0) {
        printf("✓ Generated: %s\n", output_path);
    }

    if (output_to_free) free(output_to_free);
    if (free_config) config_free(config);
    return result;
}
