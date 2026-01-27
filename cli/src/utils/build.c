/**
 * Build Utilities
 *
 * Shared build/compile/run logic for kryon CLI commands.
 * Eliminates duplication between cmd_run, cmd_build, and cmd_compile.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "kryon_cli.h"
#include "build.h"

#include "../../../ir/include/ir_serialization.h"
#include "../../../ir/include/ir_builder.h"
#include "../../ir/parsers/kry/kry_parser.h"
#include "../template/docs_template.h"
#include "../../../codegens/kry/kry_codegen.h"
#include "../../../codegens/c/ir_c_codegen.h"
#include "../../../codegens/kotlin/kotlin_codegen.h"
#include "../../../codegens/markdown/markdown_codegen.h"

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

    if (strcmp(ext, ".kir") == 0) {
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
    else if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
        return "c";
    }
    else {
        return NULL;
    }
}

/* ============================================================================
 * KIR Compilation Helper Forward Declarations
 * ============================================================================ */

static char* read_file_contents(const char* path, size_t* out_size);
static char* module_to_file_path(const char* module_path, const char* base_dir);
static char* module_to_kir_path(const char* module_path);
static char** extract_imports_from_kir(const char* kir_json, int* out_count);

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

    // Markdown: use kryon compile (simpler than reimplementing)
    if (strcmp(frontend, "markdown") == 0) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "kryon compile \"%s\" > /dev/null 2>&1", source_file);
        int compile_result = system(cmd);
        if (compile_result != 0) {
            fprintf(stderr, "Warning: Markdown compilation may have issues (code %d)\n", compile_result);
        }

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
                int mv_result = system(mv_cmd);
                if (mv_result != 0) {
                    fprintf(stderr, "Warning: Failed to move file (code %d)\n", mv_result);
                }
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

    // .kry DSL: use native C parser
    if (strcmp(frontend, "kry") == 0) {
        // Read the source file
        char* source = NULL;
        size_t source_size = 0;
        FILE* f = fopen(source_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            source_size = ftell(f);
            fseek(f, 0, SEEK_SET);
            source = malloc(source_size + 1);
            if (source) {
                size_t bytes_read = fread(source, 1, source_size, f);
                if (bytes_read != source_size) {
                    fprintf(stderr, "Warning: Only read %zu of %zu bytes from %s\n",
                            bytes_read, source_size, source_file);
                    source_size = bytes_read;
                }
                source[source_size] = '\0';
            }
            fclose(f);
        }

        if (!source) {
            fprintf(stderr, "Error: Failed to read .kry file: %s\n", source_file);
            return 1;
        }

        // Extract base directory from source_file for import resolution
        char base_dir[2048] = ".";
        const char* last_slash = strrchr(source_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - source_file;
            if (dir_len < sizeof(base_dir)) {
                memcpy(base_dir, source_file, dir_len);
                base_dir[dir_len] = '\0';
            }
        }

        // Get module name (filename without extension)
        char module_name[256] = "main";
        const char* basename = last_slash ? last_slash + 1 : source_file;
        const char* dot = strrchr(basename, '.');
        if (dot) {
            size_t name_len = dot - basename;
            if (name_len < sizeof(module_name)) {
                memcpy(module_name, basename, name_len);
                module_name[name_len] = '\0';
            }
        }

        // Compile using native C parser - single module mode for multi-file KIR
        // This generates KIR for just this file without expanding imports inline
        char* main_kir = ir_kry_to_kir_single_module(source, source_size, base_dir);
        free(source);

        if (!main_kir) {
            fprintf(stderr, "Error: Failed to compile .kry file\n");
            return 1;
        }

        // Check if output_kir is a directory (ends with '/' or is an existing directory)
        size_t output_len = strlen(output_kir);
        bool is_multi_file = (output_len > 0 && output_kir[output_len - 1] == '/') ||
                             file_is_directory(output_kir);

        if (is_multi_file) {
            // Multi-file output: generate separate KIR files for main and all imports
            dir_create_recursive(output_kir);

            // Write main KIR
            char main_kir_path[2048];
            snprintf(main_kir_path, sizeof(main_kir_path), "%s/%s.kir",
                     output_kir[output_len - 1] == '/' ?
                     (char*)output_kir : output_kir, module_name);
            // Remove double slashes if any
            if (output_kir[output_len - 1] == '/') {
                snprintf(main_kir_path, sizeof(main_kir_path), "%s%s.kir", output_kir, module_name);
            }

            FILE* out = fopen(main_kir_path, "w");
            if (!out) {
                fprintf(stderr, "Error: Could not write %s\n", main_kir_path);
                free(main_kir);
                return 1;
            }
            fprintf(out, "%s\n", main_kir);
            fclose(out);
            printf("✓ Generated: %s.kir\n", module_name);
            int files_written = 1;

            // Extract initial imports and process recursively
            int initial_count = 0;
            char** initial_imports = extract_imports_from_kir(main_kir, &initial_count);
            free(main_kir);
            main_kir = NULL;

            // Use fixed-size queue for BFS processing of imports
            char* import_queue[256];
            int queue_count = 0;

            // Copy initial imports to queue
            if (initial_imports) {
                for (int i = 0; i < initial_count && queue_count < 256; i++) {
                    if (initial_imports[i]) {
                        import_queue[queue_count++] = initial_imports[i];
                    }
                }
                free(initial_imports);
            }

            // Track processed modules to avoid duplicates
            char* processed[256];
            int processed_count = 0;

            // Process imports queue (BFS)
            for (int i = 0; i < queue_count; i++) {
                char* mod = import_queue[i];
                if (!mod) continue;

                // Skip already processed
                bool already_done = false;
                for (int j = 0; j < processed_count; j++) {
                    if (strcmp(processed[j], mod) == 0) {
                        already_done = true;
                        break;
                    }
                }
                if (already_done) {
                    free(mod);
                    import_queue[i] = NULL;
                    continue;
                }

                // Skip internal/plugin modules
                if (strncmp(mod, "kryon/", 6) == 0 ||
                    strcmp(mod, "storage") == 0 ||
                    strcmp(mod, "datetime") == 0 ||
                    strcmp(mod, "math") == 0 ||
                    strcmp(mod, "uuid") == 0) {
                    free(mod);
                    import_queue[i] = NULL;
                    continue;
                }

                // Resolve module to file path
                char* mod_file = module_to_file_path(mod, base_dir);
                if (!mod_file) {
                    free(mod);
                    import_queue[i] = NULL;
                    continue;
                }

                // Read module source
                size_t mod_len = 0;
                char* mod_source = read_file_contents(mod_file, &mod_len);
                if (!mod_source) {
                    free(mod_file);
                    free(mod);
                    import_queue[i] = NULL;
                    continue;
                }

                // Get module's base directory for its imports
                char mod_base[2048];
                const char* mod_slash = strrchr(mod_file, '/');
                if (mod_slash) {
                    size_t len = mod_slash - mod_file;
                    memcpy(mod_base, mod_file, len);
                    mod_base[len] = '\0';
                } else {
                    strcpy(mod_base, ".");
                }

                // Generate KIR for module - single module mode (no import expansion)
                char* mod_kir = ir_kry_to_kir_single_module(mod_source, mod_len, mod_base);
                free(mod_source);
                free(mod_file);

                if (mod_kir) {
                    // Write module KIR
                    char* kir_path = module_to_kir_path(mod);
                    if (kir_path) {
                        char full_path[2048];
                        snprintf(full_path, sizeof(full_path), "%s/%s",
                                 output_kir[output_len - 1] == '/' ?
                                 (char*)output_kir : output_kir, kir_path);
                        // Handle trailing slash
                        if (output_kir[output_len - 1] == '/') {
                            snprintf(full_path, sizeof(full_path), "%s%s", output_kir, kir_path);
                        }

                        // Create parent directories
                        char* dir_copy = strdup(full_path);
                        if (dir_copy) {
                            char* dir_end = strrchr(dir_copy, '/');
                            if (dir_end) {
                                *dir_end = '\0';
                                dir_create_recursive(dir_copy);
                            }
                            free(dir_copy);
                        }

                        FILE* mod_out = fopen(full_path, "w");
                        if (mod_out) {
                            fprintf(mod_out, "%s\n", mod_kir);
                            fclose(mod_out);
                            printf("✓ Generated: %s\n", kir_path);
                            files_written++;
                        }
                        free(kir_path);
                    }

                    // Extract sub-imports and add to queue
                    int sub_count = 0;
                    char** sub_imports = extract_imports_from_kir(mod_kir, &sub_count);
                    if (sub_imports) {
                        for (int j = 0; j < sub_count && queue_count < 256; j++) {
                            if (sub_imports[j]) {
                                import_queue[queue_count++] = sub_imports[j];
                            }
                        }
                        free(sub_imports);
                    }
                    free(mod_kir);
                    mod_kir = NULL;
                }

                // Mark as processed
                if (processed_count < 256) {
                    processed[processed_count++] = strdup(mod);
                }
                free(mod);
                import_queue[i] = NULL;
            }

            // Cleanup processed list
            for (int i = 0; i < processed_count; i++) {
                if (processed[i]) {
                    free(processed[i]);
                }
            }

            printf("✓ Generated %d KIR files in %s\n", files_written, output_kir);
            return 0;
        } else {
            // Single-file output (for caching or explicit single file)
            FILE* out = fopen(output_kir, "w");
            if (!out) {
                fprintf(stderr, "Error: Failed to open output file: %s\n", output_kir);
                free(main_kir);
                return 1;
            }
            fprintf(out, "%s\n", main_kir);
            fclose(out);
            free(main_kir);
            return 0;
        }
    }

    fprintf(stderr, "Error: Unknown frontend type: %s\n", frontend);
    return 1;
}

/* ============================================================================
 * Web Codegen (HTML Generation)
 * ============================================================================ */

// External function from codegens/web/kir_to_html.c
extern int kir_to_html_main(const char* source_dir, const char* kir_file,
                             const char* output_dir, const char* project_name);

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

    free(cwd);

    // Call kir_to_html directly (linked into binary) instead of spawning subprocess
    return kir_to_html_main(source_dir ? source_dir : ".", abs_kir_path, abs_output_dir, project_name);
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

    // Only apply docs template to files within the docs folder
    // Check if route starts with "/docs" or source_path starts with "docs/"
    bool is_docs_file = false;
    if (route && strncmp(route, "/docs", 5) == 0) {
        is_docs_file = true;
    } else if (source_path && strncmp(source_path, "docs/", 5) == 0) {
        is_docs_file = true;
    }

    // Non-docs files: use content KIR directly without template
    if (!is_docs_file) {
        return generate_html_from_kir(content_kir_file, output_dir,
                                      config ? config->project_name : NULL,
                                      ".");
    }

    // Exclude docs/index.html from template (it's a landing page with its own layout)
    if (source_path && strcmp(source_path, "docs/index.html") == 0) {
        return generate_html_from_kir(content_kir_file, output_dir,
                                      config ? config->project_name : NULL,
                                      ".");
    }

    // No template available: use content KIR directly
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
    // Sanitize route for filename: /docs/getting-started -> docs_getting-started_combined.kir
    char combined_kir_file[1024];
    char safe_route[512];
    const char* src = route;
    char* dst = safe_route;
    // Skip leading slash
    if (*src == '/') src++;
    while (*src && dst < safe_route + sizeof(safe_route) - 1) {
        // Replace slashes with underscores
        *dst++ = (*src == '/') ? '_' : *src;
        src++;
    }
    *dst = '\0';
    snprintf(combined_kir_file, sizeof(combined_kir_file),
             ".kryon_cache/%s_combined.kir", safe_route);

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

int run_kir_on_desktop(const char* kir_file, const char* desktop_lib, const char* renderer_override) {
    (void)desktop_lib;
    (void)renderer_override;

    // 1. Generate C code from KIR to a temp directory
    char c_output_dir[1024];
    snprintf(c_output_dir, sizeof(c_output_dir), "/tmp/kryon_build_%d", getpid());

    printf("Generating C code from KIR...\n");
    if (!ir_generate_c_code_multi(kir_file, c_output_dir)) {
        fprintf(stderr, "Error: Failed to generate C code from KIR\n");
        return 1;
    }
    printf("✓ C code generated in %s\n", c_output_dir);

    // 2. Compile C to native binary
    char main_c[1024];
    snprintf(main_c, sizeof(main_c), "%s/main.c", c_output_dir);

    char exe_file[1024];
    snprintf(exe_file, sizeof(exe_file), "/tmp/kryon_app_%d", getpid());

    printf("Compiling to native executable...\n");

    // Find all .c files in the generated directory
    char additional_sources[4096] = "";
    {
        char find_cmd[2048];
        snprintf(find_cmd, sizeof(find_cmd),
                 "find \"%s\" -name '*.c' ! -name 'main.c' 2>/dev/null",
                 c_output_dir);

        FILE* fp = popen(find_cmd, "r");
        if (fp) {
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 0) {
                    strncat(additional_sources, "\"", sizeof(additional_sources) - strlen(additional_sources) - 1);
                    strncat(additional_sources, line, sizeof(additional_sources) - strlen(additional_sources) - 1);
                    strncat(additional_sources, "\" ", sizeof(additional_sources) - strlen(additional_sources) - 1);
                }
            }
            pclose(fp);
        }
    }

    // Get paths for compilation
    char* kryon_root = paths_get_kryon_root();
    char* bindings_path = kryon_root ? path_join(kryon_root, "bindings/c") : paths_get_bindings_path();
    char* ir_path = kryon_root ? path_join(kryon_root, "ir") : str_copy("ir");
    char* ir_include_path = kryon_root ? path_join(kryon_root, "ir/include") : str_copy("ir/include");
    char* desktop_path = kryon_root ? path_join(kryon_root, "runtime/desktop") : str_copy("runtime/desktop");
    char* cjson_path = kryon_root ? path_join(kryon_root, "ir/third_party/cJSON") : str_copy("ir/third_party/cJSON");
    char* tomlc99_path = kryon_root ? path_join(kryon_root, "third_party/tomlc99") : NULL;  // Only for dev builds
    char* ir_utils_path = kryon_root ? path_join(kryon_root, "ir/src/utils") : NULL;  // Only for dev builds
    char* sdk_include_path = kryon_root ? path_join(kryon_root, "include") : str_copy("include");
    char* build_path = kryon_root ? path_join(kryon_root, "build") : paths_get_build_path();

    char kryon_c[PATH_MAX];
    char kryon_dsl_c[PATH_MAX];
    char kryon_reactive_c[PATH_MAX];
    char kryon_reactive_ui_c[PATH_MAX];
    snprintf(kryon_c, sizeof(kryon_c), "%s/kryon.c", bindings_path);
    snprintf(kryon_dsl_c, sizeof(kryon_dsl_c), "%s/kryon_dsl.c", bindings_path);
    snprintf(kryon_reactive_c, sizeof(kryon_reactive_c), "%s/kryon_reactive.c", bindings_path);
    snprintf(kryon_reactive_ui_c, sizeof(kryon_reactive_ui_c), "%s/kryon_reactive_ui.c", bindings_path);

    // Build extra include paths for dev builds (tomlc99 and ir_utils contain headers needed by bindings)
    char extra_includes[512] = "";
    if (tomlc99_path) {
        snprintf(extra_includes + strlen(extra_includes), sizeof(extra_includes) - strlen(extra_includes),
                 "-I\"%s\" ", tomlc99_path);
    }
    if (ir_utils_path) {
        snprintf(extra_includes + strlen(extra_includes), sizeof(extra_includes) - strlen(extra_includes),
                 "-I\"%s\" ", ir_utils_path);
    }

    char compile_cmd[8192];
    int written = snprintf(compile_cmd, sizeof(compile_cmd),
             "gcc -std=gnu99 -O2 \"%s\" %s"
             "\"%s\" \"%s\" \"%s\" \"%s\" "
             "-o \"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "-I\"%s\" "
             "%s"
             "-L\"%s\" "
             "-lkryon_desktop -lkryon_sdl3 -lkryon_common -lkryon_ir -lm "
             "$(pkg-config --libs raylib 2>/dev/null || echo '-lraylib') "
             "$(pkg-config --libs sdl3 sdl3-ttf harfbuzz freetype2 fribidi 2>/dev/null || echo '')",
             main_c, additional_sources,
             kryon_c, kryon_dsl_c, kryon_reactive_c, kryon_reactive_ui_c, exe_file,
             c_output_dir, bindings_path, ir_path, ir_include_path, desktop_path, cjson_path, sdk_include_path,
             extra_includes, build_path);

    free(bindings_path);
    free(ir_path);
    free(ir_include_path);
    free(desktop_path);
    free(cjson_path);
    if (tomlc99_path) free(tomlc99_path);
    if (ir_utils_path) free(ir_utils_path);
    free(sdk_include_path);
    free(build_path);
    if (kryon_root) free(kryon_root);

    if (written < 0 || (size_t)written >= sizeof(compile_cmd)) {
        fprintf(stderr, "Error: Compile command too long\n");
        return 1;
    }

    int compile_result = system(compile_cmd);
    if (compile_result != 0) {
        fprintf(stderr, "Error: Failed to compile C code\n");
        // Cleanup temp directory
        char cleanup_cmd[1024];
        snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", c_output_dir);
        int cleanup_result = system(cleanup_cmd);
        (void)cleanup_result;
        return 1;
    }

    printf("✓ Compiled successfully\n");

    // 3. Run the executable
    printf("Running application...\n");
    char* lib_build_path = paths_get_build_path();
    char run_cmd[2048];
    snprintf(run_cmd, sizeof(run_cmd),
             "LD_LIBRARY_PATH=\"%s:$LD_LIBRARY_PATH\" \"%s\"",
             lib_build_path, exe_file);
    free(lib_build_path);

    int result = system(run_cmd);

    // 4. Cleanup
    remove(exe_file);
    {
        char cleanup_cmd[1024];
        snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf \"%s\"", c_output_dir);
        int cleanup_result = system(cleanup_cmd);
        (void)cleanup_result;
    }

    return result == 0 ? 0 : 1;
}

/* ============================================================================
 * Hot Reload Desktop Runner (REMOVED)
 * ============================================================================ */

/* run_kir_on_desktop_with_hot_reload() removed - we always compile through C codegen now */

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

    // Invoke codegen based on target (must be explicitly specified)
    const char* target = opts && opts->target ? opts->target : config->build_target;

    if (!target) {
        fprintf(stderr, "Error: No build target specified\n");
        fprintf(stderr, "Use --target=<name> or specify targets in kryon.toml\n");
        return 1;
    }

    const char* project_name = opts && opts->project_name ? opts->project_name :
                                (config ? config->project_name : NULL);

    // Use target handler registry for build dispatch
    result = target_handler_build(target, kir_file, output_dir, project_name, config);

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
    if (config && config->build_output_dir) {
        base_dir = str_copy(config->build_output_dir);
    } else {
        base_dir = str_copy("build");
    }

    // All codegens use <base>/<target>/ structure for organization
    char* result = path_join(base_dir, target);
    free(base_dir);
    return result;
}

/**
 * Helper: Read file contents
 */
static char* read_file_contents(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(content, 1, (size_t)size, f);
    content[bytes_read] = '\0';
    fclose(f);

    if (out_size) *out_size = bytes_read;
    return content;
}

/**
 * Helper: Convert module path (e.g., "components.habit_panel") to file path
 */
static char* module_to_file_path(const char* module_path, const char* base_dir) {
    if (!module_path) return NULL;

    // Convert dots to slashes
    char* path = strdup(module_path);
    if (!path) return NULL;

    for (char* p = path; *p; p++) {
        if (*p == '.') *p = '/';
    }

    // Build full path with .kry extension
    size_t base_len = base_dir ? strlen(base_dir) : 0;
    size_t path_len = strlen(path);
    char* full_path = (char*)malloc(base_len + path_len + 6); // +1 for /, +4 for .kry, +1 for null
    if (!full_path) {
        free(path);
        return NULL;
    }

    if (base_dir && base_len > 0) {
        sprintf(full_path, "%s/%s.kry", base_dir, path);
    } else {
        sprintf(full_path, "%s.kry", path);
    }

    free(path);
    return full_path;
}

/**
 * Helper: Convert module path to KIR output path
 */
static char* module_to_kir_path(const char* module_path) {
    if (!module_path) return NULL;

    char* path = strdup(module_path);
    if (!path) return NULL;

    for (char* p = path; *p; p++) {
        if (*p == '.') *p = '/';
    }

    char* kir_path = (char*)malloc(strlen(path) + 5); // +4 for .kir, +1 for null
    if (!kir_path) {
        free(path);
        return NULL;
    }
    sprintf(kir_path, "%s.kir", path);
    free(path);
    return kir_path;
}

/**
 * Helper: Extract imports from KIR JSON
 * Looks in source_structures.requires[].module
 */
static char** extract_imports_from_kir(const char* kir_json, int* out_count) {
    *out_count = 0;
    if (!kir_json) return NULL;

    // Find source_structures.requires array
    const char* requires_start = strstr(kir_json, "\"requires\"");
    if (!requires_start) return NULL;

    const char* arr_start = strchr(requires_start, '[');
    if (!arr_start) return NULL;

    // Find matching closing bracket (handle nested objects)
    int depth = 1;
    const char* arr_end = arr_start + 1;
    while (*arr_end && depth > 0) {
        if (*arr_end == '[' || *arr_end == '{') depth++;
        else if (*arr_end == ']' || *arr_end == '}') depth--;
        arr_end++;
    }
    if (depth != 0) return NULL;
    arr_end--; // Point to the ]

    // Count "module" entries
    int count = 0;
    const char* p = arr_start;
    while ((p = strstr(p + 1, "\"module\"")) && p < arr_end) {
        count++;
    }

    if (count == 0) return NULL;

    char** imports = (char**)malloc(sizeof(char*) * count);
    if (!imports) return NULL;

    // Extract module values
    int idx = 0;
    p = arr_start;
    while (idx < count && (p = strstr(p + 1, "\"module\"")) && p < arr_end) {
        // Find the colon after "module"
        const char* colon = strchr(p, ':');
        if (!colon || colon >= arr_end) break;

        // Find the opening quote of the value
        const char* val_start = strchr(colon, '"');
        if (!val_start || val_start >= arr_end) break;
        val_start++; // Skip the quote

        // Find the closing quote
        const char* val_end = strchr(val_start, '"');
        if (!val_end || val_end >= arr_end) break;

        size_t len = val_end - val_start;
        imports[idx] = (char*)malloc(len + 1);
        if (imports[idx]) {
            memcpy(imports[idx], val_start, len);
            imports[idx][len] = '\0';
            idx++;
        }
        p = val_end;
    }

    *out_count = idx;
    return imports;
}

int generate_from_kir(const char* kir_file, const char* target,
                      const char* output_path) {
    bool success = false;

    // All codegens use multi-file output by default
    if (strcmp(target, "kry") == 0) {
        success = kry_codegen_generate_multi(kir_file, output_path);
    } else if (strcmp(target, "c") == 0) {
        success = ir_generate_c_code_multi(kir_file, output_path);
    } else if (strcmp(target, "kotlin") == 0) {
        success = ir_generate_kotlin_code_multi(kir_file, output_path);
    } else if (strcmp(target, "markdown") == 0) {
        success = markdown_codegen_generate_multi(kir_file, output_path);
    } else if (strcmp(target, "kir") == 0) {
        // KIR target: just copy the KIR file to output (for single-file case)
        // Multi-file case is handled in codegen_pipeline
        char output_file[2048];
        snprintf(output_file, sizeof(output_file), "%s/main.kir", output_path);

        // Create output directory
        dir_create_recursive(output_path);

        // Read input KIR
        FILE* in = fopen(kir_file, "r");
        if (!in) {
            fprintf(stderr, "Error: Could not open KIR file: %s\n", kir_file);
            return 1;
        }

        FILE* out = fopen(output_file, "w");
        if (!out) {
            fclose(in);
            fprintf(stderr, "Error: Could not create output file: %s\n", output_file);
            return 1;
        }

        // Copy content
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
            fwrite(buffer, 1, n, out);
        }

        fclose(in);
        fclose(out);
        printf("✓ Generated: main.kir\n");
        success = true;
    } else {
        fprintf(stderr, "Error: Unsupported codegen target: %s\n", target);
        fprintf(stderr, "Supported targets: kry, c, kotlin, markdown, kir\n");
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

    // Special handling for kir target: use compile_source_to_kir with directory output
    // This generates multi-file KIR for .kry sources (main + imports)
    if (strcmp(target, "kir") == 0) {
        // Determine output directory
        const char* output = output_dir;
        char* output_to_free = NULL;
        if (!output) {
            output = output_to_free = get_codegen_output_dir(target, config);
        }

        // Build output path with trailing slash to trigger multi-file mode
        char output_with_slash[2048];
        size_t output_len = strlen(output);
        if (output_len > 0 && output[output_len - 1] != '/') {
            snprintf(output_with_slash, sizeof(output_with_slash), "%s/", output);
        } else {
            snprintf(output_with_slash, sizeof(output_with_slash), "%s", output);
        }

        printf("Generating KIR: %s → %s\n", source, output);

        // For .kry source, compile_source_to_kir handles multi-file output
        if (strcmp(frontend, "kry") == 0) {
            int result = compile_source_to_kir(source, output_with_slash);
            if (output_to_free) free(output_to_free);
            if (free_config) config_free(config);
            return result;
        } else if (strcmp(frontend, "kir") == 0) {
            // Source is already KIR - just copy it
            dir_create_recursive(output);
            char output_file[2048];
            const char* basename = strrchr(source, '/');
            basename = basename ? basename + 1 : source;
            snprintf(output_file, sizeof(output_file), "%s/%s", output, basename);

            FILE* in = fopen(source, "r");
            FILE* out_f = fopen(output_file, "w");
            if (in && out_f) {
                char buffer[4096];
                size_t n;
                while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                    fwrite(buffer, 1, n, out_f);
                }
                fclose(in);
                fclose(out_f);
                printf("✓ Copied: %s\n", basename);
            } else {
                if (in) fclose(in);
                if (out_f) fclose(out_f);
                fprintf(stderr, "Error: Could not copy KIR file\n");
                if (output_to_free) free(output_to_free);
                if (free_config) config_free(config);
                return 1;
            }

            if (output_to_free) free(output_to_free);
            if (free_config) config_free(config);
            return 0;
        } else {
            // For other sources, use compile_source_to_kir with directory
            // to generate multi-file output
            int result = compile_source_to_kir(source, output_with_slash);
            if (output_to_free) free(output_to_free);
            if (free_config) config_free(config);
            return result;
        }
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
    else if (strcmp(target, "c") == 0) ext = ".c";
    else {
        fprintf(stderr, "Error: Unknown target: %s\n", target);
        free(name_copy);
        if (output_to_free) free(output_to_free);
        if (free_config) config_free(config);
        return 1;
    }

    // Single-file output: build/<target>/main.ext
    snprintf(output_path, sizeof(output_path), "%s/%s%s", output, name_copy, ext);
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
