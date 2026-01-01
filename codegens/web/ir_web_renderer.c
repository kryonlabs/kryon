#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <libgen.h>

#include "../../ir/ir_core.h"
#include "ir_web_renderer.h"
#include "html_generator.h"
#include "css_generator.h"
#include "wasm_bridge.h"

// Web IR Renderer Context
typedef struct WebIRRenderer {
    HTMLGenerator* html_generator;
    CSSGenerator* css_generator;
    WASMBridge* wasm_bridge;
    char output_directory[256];
    char* source_directory;  // Directory containing source HTML/assets
    bool generate_separate_files;
    bool include_javascript_runtime;
    bool include_wasm_modules;
} WebIRRenderer;

// JavaScript runtime template
static const char* javascript_runtime_template =
"// Kryon Web Runtime\n"
"// Auto-generated JavaScript for IR component interaction\n\n"
"let kryon_components = new Map();\n"
"let kryon_handlers = new Map();\n\n"
"function kryon_handle_click(component_id, handler_id) {\n"
"    console.log('Click on component', component_id, 'handler:', handler_id);\n"
"    // TODO: Call WASM handler when implemented\n"
"}\n\n"
"function kryon_handle_hover(component_id, handler_id) {\n"
"    console.log('Hover on component', component_id, 'handler:', handler_id);\n"
"    // TODO: Call WASM handler when implemented\n"
"}\n\n"
"function kryon_handle_leave(component_id) {\n"
"    console.log('Leave component', component_id);\n"
"}\n\n"
"function kryon_handle_focus(component_id) {\n"
"    console.log('Focus component', component_id);\n"
"}\n\n"
"function kryon_handle_blur(component_id) {\n"
"    console.log('Blur component', component_id);\n"
"}\n\n"
"// Initialize when DOM is ready\n"
"document.addEventListener('DOMContentLoaded', function() {\n"
"    console.log('Kryon Web Runtime initialized');\n"
"    \n"
"    // Find all Kryon components (buttons, inputs, etc. with IDs)\n"
"    const elements = document.querySelectorAll('[id]');\n"
"    elements.forEach(element => {\n"
"        // Extract numeric ID from btn-1, input-2, etc.\n"
"        const match = element.id.match(/(\\d+)$/);\n"
"        if (match) {\n"
"            const id = parseInt(match[1]);\n"
"            kryon_components.set(id, element);\n"
"        }\n"
"    });\n"
"    \n"
"    console.log('Found', kryon_components.size, 'Kryon components');\n"
"});\n\n";

WebIRRenderer* web_ir_renderer_create() {
    WebIRRenderer* renderer = malloc(sizeof(WebIRRenderer));
    if (!renderer) return NULL;

    renderer->html_generator = html_generator_create();
    renderer->css_generator = css_generator_create();
    renderer->wasm_bridge = wasm_bridge_create();

    if (!renderer->html_generator || !renderer->css_generator || !renderer->wasm_bridge) {
        if (renderer->html_generator) html_generator_destroy(renderer->html_generator);
        if (renderer->css_generator) css_generator_destroy(renderer->css_generator);
        if (renderer->wasm_bridge) wasm_bridge_destroy(renderer->wasm_bridge);
        free(renderer);
        return NULL;
    }

    strcpy(renderer->output_directory, ".");
    renderer->source_directory = NULL;
    renderer->generate_separate_files = true;
    renderer->include_javascript_runtime = true;
    renderer->include_wasm_modules = false;  // Disabled by default

    return renderer;
}

void web_ir_renderer_destroy(WebIRRenderer* renderer) {
    if (!renderer) return;

    if (renderer->html_generator) {
        html_generator_destroy(renderer->html_generator);
    }
    if (renderer->css_generator) {
        css_generator_destroy(renderer->css_generator);
    }
    if (renderer->wasm_bridge) {
        wasm_bridge_destroy(renderer->wasm_bridge);
    }
    if (renderer->source_directory) {
        free(renderer->source_directory);
    }
    free(renderer);
}

void web_ir_renderer_set_output_directory(WebIRRenderer* renderer, const char* directory) {
    if (!renderer || !directory) return;

    strncpy(renderer->output_directory, directory, sizeof(renderer->output_directory) - 1);
    renderer->output_directory[sizeof(renderer->output_directory) - 1] = '\0';

    if (renderer->wasm_bridge) {
        wasm_bridge_set_output_directory(renderer->wasm_bridge, directory);
    }
}

void web_ir_renderer_set_source_directory(WebIRRenderer* renderer, const char* directory) {
    if (!renderer) return;

    if (renderer->source_directory) {
        free(renderer->source_directory);
        renderer->source_directory = NULL;
    }

    if (directory) {
        renderer->source_directory = strdup(directory);
    }
}

void web_ir_renderer_set_generate_separate_files(WebIRRenderer* renderer, bool separate) {
    if (!renderer) return;
    renderer->generate_separate_files = separate;
}

void web_ir_renderer_set_include_javascript_runtime(WebIRRenderer* renderer, bool include) {
    if (!renderer) return;
    renderer->include_javascript_runtime = include;
}

void web_ir_renderer_set_include_wasm_modules(WebIRRenderer* renderer, bool include) {
    if (!renderer) return;
    renderer->include_wasm_modules = include;
}

void web_ir_renderer_set_inline_css(WebIRRenderer* renderer, bool inline_css) {
    if (!renderer || !renderer->html_generator) return;
    html_generator_set_inline_css(renderer->html_generator, inline_css);
}

static bool generate_javascript_runtime(WebIRRenderer* renderer) {
    if (!renderer || !renderer->include_javascript_runtime) return true;

    char js_filename[512];
    snprintf(js_filename, sizeof(js_filename), "%s/kryon.js", renderer->output_directory);

    FILE* js_file = fopen(js_filename, "w");
    if (!js_file) return false;

    bool success = (fprintf(js_file, "%s", javascript_runtime_template) >= 0);
    fclose(js_file);

    return success;
}

static bool collect_wasm_modules(WebIRRenderer* renderer, IRComponent* component) {
    if (!renderer || !renderer->wasm_bridge || !renderer->include_wasm_modules) {
        return true; // Skip WASM if disabled
    }

    // Extract logic modules from IR
    return wasm_bridge_extract_logic_from_ir(renderer->wasm_bridge, component);
}

// Collect image src paths from component tree
static void collect_assets(IRComponent* component, char** assets, int* count, int max) {
    if (!component) return;

    // Check for image component
    if (component->type == IR_COMPONENT_IMAGE && component->custom_data) {
        const char* src = (const char*)component->custom_data;

        // Skip external URLs (http://, https://, //, data:)
        if (strncmp(src, "http://", 7) == 0 ||
            strncmp(src, "https://", 8) == 0 ||
            strncmp(src, "//", 2) == 0 ||
            strncmp(src, "data:", 5) == 0) {
            // External URL - skip
        } else if (*count < max) {
            // Check if we already have this asset
            bool duplicate = false;
            for (int i = 0; i < *count; i++) {
                if (strcmp(assets[i], src) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                // Relative path - add to list
                assets[(*count)++] = strdup(src);
            }
        }
    }

    // Recurse into children
    for (int i = 0; i < component->child_count; i++) {
        collect_assets(component->children[i], assets, count, max);
    }
}

// Create parent directories recursively
static bool create_parent_dirs(const char* path) {
    char* path_copy = strdup(path);
    char* dir = dirname(path_copy);

    // Use mkdir -p equivalent
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir);
    int ret = system(cmd);

    free(path_copy);
    return ret == 0;
}

// Copy single asset preserving directory structure
static bool copy_asset(const char* source_dir, const char* output_dir, const char* asset_path) {
    char src_full[2048];
    char dst_full[2048];

    snprintf(src_full, sizeof(src_full), "%s/%s", source_dir, asset_path);
    snprintf(dst_full, sizeof(dst_full), "%s/%s", output_dir, asset_path);

    // Check if source exists
    struct stat st;
    if (stat(src_full, &st) != 0) {
        fprintf(stderr, "[ASSET] Source not found: %s\n", src_full);
        return false;
    }

    // Create parent directories in output
    create_parent_dirs(dst_full);

    // Copy file
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_full, dst_full);
    int ret = system(cmd);

    if (ret == 0) {
        printf("✅ Copied asset: %s\n", asset_path);
    }
    return ret == 0;
}

// Copy all collected assets from source to output directory
static void copy_collected_assets(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer->source_directory) return;

    char* assets[256];
    int asset_count = 0;

    // Collect all image references
    collect_assets(root, assets, &asset_count, 256);

    if (asset_count > 0) {
        printf("📦 Copying %d assets...\n", asset_count);
    }

    // Copy each asset
    for (int i = 0; i < asset_count; i++) {
        copy_asset(renderer->source_directory,
                  renderer->output_directory,
                  assets[i]);
        free(assets[i]);
    }
}

bool web_ir_renderer_render(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🌐 Rendering IR component tree to web format...\n");

    // Create output directory if it doesn't exist
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", renderer->output_directory);
    if (system(mkdir_cmd) != 0) {
        printf("Warning: Could not create output directory '%s'\n", renderer->output_directory);
    }

    // Generate HTML
    if (renderer->generate_separate_files) {
        char html_filename[512];
        snprintf(html_filename, sizeof(html_filename), "%s/index.html", renderer->output_directory);

        if (!html_generator_write_to_file(renderer->html_generator, root, html_filename)) {
            printf("❌ Failed to write HTML file\n");
            return false;
        }
        printf("✅ Generated HTML: %s\n", html_filename);
    }

    // Generate CSS (skip if embedded in HTML)
    if (renderer->generate_separate_files && !renderer->html_generator->options.embedded_css) {
        char css_filename[512];
        snprintf(css_filename, sizeof(css_filename), "%s/kryon.css", renderer->output_directory);

        if (!css_generator_write_to_file(renderer->css_generator, root, css_filename)) {
            printf("❌ Failed to write CSS file\n");
            return false;
        }
        printf("✅ Generated CSS: %s\n", css_filename);
    } else if (renderer->html_generator->options.embedded_css) {
        printf("✅ CSS embedded in HTML\n");
    }

    // Generate JavaScript runtime
    if (!generate_javascript_runtime(renderer)) {
        printf("❌ Failed to write JavaScript runtime\n");
        return false;
    }
    printf("✅ Generated JavaScript: %s/kryon.js\n", renderer->output_directory);

    // Collect and process WASM modules
    if (!collect_wasm_modules(renderer, root)) {
        printf("❌ Failed to collect WASM modules\n");
        return false;
    }
    printf("✅ Collected WASM modules\n");

    // Generate WASM files and JavaScript bindings
    if (renderer->include_wasm_modules && renderer->wasm_bridge) {
        if (!wasm_bridge_write_runtime_loader(renderer->wasm_bridge, renderer->output_directory)) {
            printf("❌ Failed to write WASM runtime loader\n");
            return false;
        }
        printf("✅ Generated WASM runtime loader\n");
    }

    // Copy referenced assets (images, etc.)
    copy_collected_assets(renderer, root);

    // Generate statistics
    const char* html_buffer = html_generator_get_buffer(renderer->html_generator);
    const char* css_buffer = css_generator_get_buffer(renderer->css_generator);

    if (html_buffer && css_buffer) {
        printf("📊 Generation Statistics:\n");
        printf("   HTML size: %zu bytes\n", html_generator_get_size(renderer->html_generator));
        printf("   CSS size: %zu bytes\n", css_generator_get_size(renderer->css_generator));
        printf("   Output directory: %s\n", renderer->output_directory);
    }

    printf("🎉 Web rendering complete!\n");
    return true;
}

bool web_ir_renderer_render_to_memory(WebIRRenderer* renderer, IRComponent* root,
                                       char** html_output, char** css_output, char** js_output) {
    if (!renderer || !root) return false;

    // Generate HTML to memory
    if (html_output) {
        const char* html = html_generator_generate(renderer->html_generator, root);
        *html_output = html ? strdup(html) : NULL;
    }

    // Generate CSS to memory
    if (css_output) {
        const char* css = css_generator_generate(renderer->css_generator, root);
        *css_output = css ? strdup(css) : NULL;
    }

    // Generate JavaScript runtime to memory
    if (js_output && renderer->include_javascript_runtime) {
        *js_output = strdup(javascript_runtime_template);
    }

    return true;
}

bool web_ir_renderer_validate_ir(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return false;

    printf("🔍 Validating IR component tree...\n");

    // Basic validation
    if (!ir_validate_component(root)) {
        printf("❌ IR validation failed\n");
        return false;
    }

    printf("✅ IR validation passed\n");
    return true;
}

void web_ir_renderer_print_tree_info(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("🌳 IR Component Tree Information:\n");
    ir_print_component_info(root, 0);
}

// Utility functions for debugging
void web_ir_renderer_print_stats(WebIRRenderer* renderer, IRComponent* root) {
    if (!renderer || !root) return;

    printf("📈 Web Renderer Statistics:\n");
    printf("   Output directory: %s\n", renderer->output_directory);
    printf("   Separate files: %s\n", renderer->generate_separate_files ? "Yes" : "No");
    printf("   JS Runtime: %s\n", renderer->include_javascript_runtime ? "Yes" : "No");
    printf("   HTML Generator: %s\n", renderer->html_generator ? "Created" : "Failed");
    printf("   CSS Generator: %s\n", renderer->css_generator ? "Created" : "Failed");
}

// Convenience function for quick rendering
bool web_render_ir_component(IRComponent* root, const char* output_dir) {
    return web_render_ir_component_with_options(root, output_dir, false);
}

// Convenience function with options
bool web_render_ir_component_with_options(IRComponent* root, const char* output_dir, bool embedded_css) {
    return web_render_ir_component_with_manifest(root, output_dir, embedded_css, NULL);
}

// Convenience function with manifest (for CSS variable support)
bool web_render_ir_component_with_manifest(IRComponent* root, const char* output_dir,
                                           bool embedded_css, IRReactiveManifest* manifest) {
    WebIRRenderer* renderer = web_ir_renderer_create();
    if (!renderer) return false;

    if (output_dir) {
        web_ir_renderer_set_output_directory(renderer, output_dir);
    }

    // Set embedded CSS option on the HTML generator
    if (embedded_css && renderer->html_generator) {
        renderer->html_generator->options.embedded_css = true;
    }

    // Set manifest on both generators for CSS variable output
    if (manifest) {
        if (renderer->css_generator) {
            css_generator_set_manifest(renderer->css_generator, manifest);
        }
        // Also set on HTML generator for embedded CSS mode
        if (renderer->html_generator) {
            html_generator_set_manifest(renderer->html_generator, manifest);
        }
    }

    bool success = web_ir_renderer_render(renderer, root);
    web_ir_renderer_destroy(renderer);

    return success;
}

// Set manifest on renderer
void web_ir_renderer_set_manifest(WebIRRenderer* renderer, IRReactiveManifest* manifest) {
    if (!renderer || !manifest || !renderer->css_generator) return;
    css_generator_set_manifest(renderer->css_generator, manifest);
}