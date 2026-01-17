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

// Global IR context for source language detection
extern IRContext* g_ir_context;

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
"let kryon_modals = new Map();\n"
"let kryon_lua_ready = false;\n"
"let kryon_call_queue = [];\n"
"let kryon_current_element = null;\n\n"  // Track event target for Lua handlers
"// Lua Handler Bridge - calls embedded Lua handlers by component ID\n"
"// The Lua code (embedded in <script type=\"text/lua\">) registers kryonCallHandler\n"
"function kryonCallHandler(componentId, event) {\n"
"    // Capture the event target for Lua handlers to access\n"
"    if (event && event.target) {\n"
"        kryon_current_element = event.target;\n"
"    }\n"
"    console.log('[Kryon] Call handler for component:', componentId, 'ready:', kryon_lua_ready);\n"
"    if (kryon_lua_ready && window.kryonLuaCallHandler) {\n"
"        window.kryonLuaCallHandler(componentId);\n"
"    } else {\n"
"        // Queue calls until Lua is ready\n"
"        kryon_call_queue.push(componentId);\n"
"        console.log('[Kryon] Lua not ready, queued handler call:', componentId);\n"
"    }\n"
"}\n\n"
"// Called by Lua handlers to get the current event element\n"
"function __kryon_get_event_element() {\n"
"    return kryon_current_element || { data: {} };\n"
"}\n\n"
"// Called by Lua when initialization is complete\n"
"function kryon_lua_init_complete() {\n"
"    kryon_lua_ready = true;\n"
"    console.log('[Kryon] Lua runtime ready');\n"
"    // Process queued handler calls\n"
"    while (kryon_call_queue.length > 0) {\n"
"        const componentId = kryon_call_queue.shift();\n"
"        kryonCallHandler(componentId);\n"
"    }\n"
"}\n\n"
"// Reactive state change debounce timer\n"
"let kryon_state_change_timer = null;\n"
"let kryon_pending_changes = new Set();\n\n"
"// Called by Lua reactive system when state changes\n"
"// Debounces rapid changes and triggers re-render\n"
"function kryonStateChanged(key) {\n"
"    console.log('[Kryon] State changed:', key);\n"
"    kryon_pending_changes.add(key);\n"
"    \n"
"    // Debounce to batch rapid changes\n"
"    if (kryon_state_change_timer) {\n"
"        clearTimeout(kryon_state_change_timer);\n"
"    }\n"
"    \n"
"    kryon_state_change_timer = setTimeout(() => {\n"
"        console.log('[Kryon] Processing state changes:', Array.from(kryon_pending_changes));\n"
"        kryon_pending_changes.clear();\n"
"        kryon_state_change_timer = null;\n"
"        \n"
"        // For now, reload the page to reflect state changes\n"
"        // TODO: Implement smart partial updates using reactive bindings\n"
"        // window.location.reload();\n"
"    }, 50);\n"
"}\n\n"
"// Modal Management Functions\n"
"function kryon_open_modal(modalId) {\n"
"    const dialog = kryon_modals.get(modalId) || document.getElementById(modalId);\n"
"    if (dialog && !dialog.open) {\n"
"        dialog.showModal();\n"
"        console.log('[Kryon] Opened modal:', modalId);\n"
"    }\n"
"}\n\n"
"function kryon_close_modal(modalId) {\n"
"    const dialog = kryon_modals.get(modalId) || document.getElementById(modalId);\n"
"    if (dialog && dialog.open) {\n"
"        dialog.close();\n"
"        console.log('[Kryon] Closed modal:', modalId);\n"
"    }\n"
"}\n\n"
"function kryon_init_modals() {\n"
"    document.querySelectorAll('dialog.kryon-modal').forEach(dialog => {\n"
"        const id = dialog.id;\n"
"        kryon_modals.set(id, dialog);\n"
"        \n"
"        // Handle backdrop clicks (clicking outside modal content closes it)\n"
"        dialog.addEventListener('click', function(e) {\n"
"            if (e.target === dialog) {\n"
"                dialog.close();\n"
"                console.log('[Kryon] Modal closed via backdrop click:', id);\n"
"            }\n"
"        });\n"
"        \n"
"        // Auto-open modals that have data-modal-open='true'\n"
"        if (dialog.dataset.modalOpen === 'true') {\n"
"            dialog.showModal();\n"
"            console.log('[Kryon] Auto-opened modal:', id);\n"
"        }\n"
"    });\n"
"    \n"
"    // Auto-wire modal triggers (elements with data-opens-modal attribute)\n"
"    document.querySelectorAll('[data-opens-modal]').forEach(trigger => {\n"
"        const modalId = trigger.dataset.opensModal;\n"
"        trigger.addEventListener('click', function() {\n"
"            kryon_open_modal(modalId);\n"
"        });\n"
"    });\n"
"    \n"
"    console.log('[Kryon] Initialized', kryon_modals.size, 'modals');\n"
"}\n\n"
"// Tab Switching Functions\n"
"function kryonSelectTab(tabGroup, index) {\n"
"    if (!tabGroup) return;\n"
"    \n"
"    // Update selected index\n"
"    tabGroup.dataset.selected = index;\n"
"    \n"
"    // Update tab buttons\n"
"    const tabs = tabGroup.querySelectorAll('[role=\"tab\"]');\n"
"    tabs.forEach((tab, i) => {\n"
"        tab.setAttribute('aria-selected', i === index ? 'true' : 'false');\n"
"    });\n"
"    \n"
"    // Update panels\n"
"    const panels = tabGroup.querySelectorAll('[role=\"tabpanel\"]');\n"
"    panels.forEach((panel, i) => {\n"
"        if (i === index) {\n"
"            panel.removeAttribute('hidden');\n"
"        } else {\n"
"            panel.setAttribute('hidden', '');\n"
"        }\n"
"    });\n"
"    \n"
"    // Call Lua handler if tab has a component ID\n"
"    const selectedTab = tabs[index];\n"
"    if (selectedTab && selectedTab.dataset.componentId) {\n"
"        kryonCallHandler(parseInt(selectedTab.dataset.componentId));\n"
"    }\n"
"    \n"
"    console.log('[Kryon] Tab selected:', index);\n"
"}\n\n"
"// Keyboard navigation for tabs\n"
"document.addEventListener('keydown', function(e) {\n"
"    if (e.target.matches('[role=\"tab\"]')) {\n"
"        const tablist = e.target.closest('[role=\"tablist\"]');\n"
"        if (!tablist) return;\n"
"        const tabs = Array.from(tablist.querySelectorAll('[role=\"tab\"]'));\n"
"        const index = tabs.indexOf(e.target);\n"
"        let newIndex = index;\n"
"        \n"
"        if (e.key === 'ArrowRight') newIndex = (index + 1) % tabs.length;\n"
"        else if (e.key === 'ArrowLeft') newIndex = (index - 1 + tabs.length) % tabs.length;\n"
"        else if (e.key === 'Home') newIndex = 0;\n"
"        else if (e.key === 'End') newIndex = tabs.length - 1;\n"
"        else return;\n"
"        \n"
"        e.preventDefault();\n"
"        tabs[newIndex].click();\n"
"        tabs[newIndex].focus();\n"
"    }\n"
"});\n\n"
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
"    \n"
"    // Initialize modals\n"
"    kryon_init_modals();\n"
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
    for (int i = 0; i < (int)component->child_count; i++) {
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
    // Buffer is 8192 bytes: worst case is 3 + 2048 + 2 + 2048 + 1 = 4102, well within limit
    char cmd[8192];
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

    // Copy Fengari Lua VM only if the source language is Lua
    // This avoids including the 220KB Lua runtime for non-Lua projects
    bool needs_lua_runtime = false;
    if (g_ir_context && g_ir_context->source_metadata &&
        g_ir_context->source_metadata->source_language &&
        strcmp(g_ir_context->source_metadata->source_language, "lua") == 0) {
        needs_lua_runtime = true;
    }

    if (needs_lua_runtime) {
        // Try: KRYON_ROOT env, ~/.local/share/kryon, /usr/share/kryon, relative to cwd
        char fengari_src[1024] = {0};
        char fengari_dst[1024];
        const char* kryon_root = getenv("KRYON_ROOT");
        const char* home = getenv("HOME");

        // Try multiple locations
        const char* search_paths[] = {
            kryon_root ? kryon_root : "",
            home ? home : "",
            "/usr/share/kryon",
            "/usr/local/share/kryon",
            "."
        };
        const char* sub_paths[] = {
            "/codegens/web/vendor/fengari-web.min.js",
            "/.local/share/kryon/vendor/fengari-web.min.js",
            "/vendor/fengari-web.min.js",
            "/vendor/fengari-web.min.js",
            "/vendor/fengari-web.min.js"
        };

        for (int i = 0; i < 5; i++) {
            if (search_paths[i][0] == '\0') continue;
            snprintf(fengari_src, sizeof(fengari_src), "%s%s", search_paths[i], sub_paths[i]);
            FILE* test = fopen(fengari_src, "r");
            if (test) {
                fclose(test);
                break;
            }
            fengari_src[0] = '\0';
        }

        if (fengari_src[0] == '\0') {
            printf("⚠️  Warning: Fengari runtime not found. Set KRYON_ROOT or install to ~/.local/share/kryon\n");
        } else {
            snprintf(fengari_dst, sizeof(fengari_dst), "%s/fengari-web.min.js", renderer->output_directory);
            FILE* src_file = fopen(fengari_src, "rb");
            FILE* dst_file = fopen(fengari_dst, "wb");
            if (src_file && dst_file) {
                char buffer[8192];
                size_t bytes;
                while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                    fwrite(buffer, 1, bytes, dst_file);
                }
                fclose(src_file);
                fclose(dst_file);
                printf("✅ Copied Fengari Lua VM: %s\n", fengari_dst);
            } else {
                if (src_file) fclose(src_file);
                if (dst_file) fclose(dst_file);
                printf("⚠️  Warning: Could not copy Fengari runtime from %s\n", fengari_src);
            }
        }
    }

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

// ============================================================================
// JavaScript Reactive System Generation (Phase 2)
// ============================================================================

// Helper: escape string for JavaScript
static void js_escape_string(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;

    size_t di = 0;
    for (size_t si = 0; src[si] && di < dest_size - 1; si++) {
        char c = src[si];
        if (c == '\\' || c == '"' || c == '\'' || c == '\n' || c == '\r' || c == '\t') {
            if (di + 2 >= dest_size) break;
            dest[di++] = '\\';
            switch (c) {
                case '\n': dest[di++] = 'n'; break;
                case '\r': dest[di++] = 'r'; break;
                case '\t': dest[di++] = 't'; break;
                default: dest[di++] = c; break;
            }
        } else {
            dest[di++] = c;
        }
    }
    dest[di] = '\0';
}

// Helper: get JS type name for reactive var type
static const char* js_type_for_reactive_var(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "number";
        case IR_REACTIVE_TYPE_FLOAT: return "number";
        case IR_REACTIVE_TYPE_STRING: return "string";
        case IR_REACTIVE_TYPE_BOOL: return "boolean";
        case IR_REACTIVE_TYPE_CUSTOM: return "object";
        default: return "any";
    }
}

// Helper: get JS default value for reactive var type
static const char* js_default_for_reactive_var(IRReactiveVarType type) {
    switch (type) {
        case IR_REACTIVE_TYPE_INT: return "0";
        case IR_REACTIVE_TYPE_FLOAT: return "0.0";
        case IR_REACTIVE_TYPE_STRING: return "\"\"";
        case IR_REACTIVE_TYPE_BOOL: return "false";
        case IR_REACTIVE_TYPE_CUSTOM: return "{}";
        default: return "null";
    }
}

// Helper: get JS binding type string
static const char* js_binding_type_string(IRBindingType type) {
    switch (type) {
        case IR_BINDING_TEXT: return "textContent";
        case IR_BINDING_CONDITIONAL: return "visibility";
        case IR_BINDING_ATTRIBUTE: return "attribute";
        case IR_BINDING_FOR_LOOP: return "forEach";
        case IR_BINDING_CUSTOM: return "custom";
        default: return "unknown";
    }
}

// Generate JavaScript reactive system from manifest
char* generate_js_reactive_system(IRReactiveManifest* manifest) {
    // Calculate buffer size
    // Base template + variables + bindings + extra space
    size_t base_size = 4096;  // Base template size
    size_t var_size = manifest ? manifest->variable_count * 256 : 0;
    size_t binding_size = manifest ? manifest->binding_count * 256 : 0;
    size_t total_size = base_size + var_size + binding_size;

    char* output = malloc(total_size);
    if (!output) return NULL;

    output[0] = '\0';

    // Header
    strcat(output,
        "// Kryon Reactive System (Generated from IRReactiveManifest)\n"
        "// This file provides reactive state management for the web app\n"
        "// State changes automatically update the DOM via bindings\n\n"
    );

    // If no manifest or empty, generate stub
    if (!manifest || manifest->variable_count == 0) {
        strcat(output,
            "// No reactive variables found in manifest\n"
            "const kryonState = {};\n"
            "const kryonBindings = {};\n\n"
            "// Stub update function\n"
            "function kryonUpdateBindings(prop) {\n"
            "  console.log('[Kryon Reactive] No bindings configured for:', prop);\n"
            "}\n\n"
            "// Export for Lua bridge\n"
            "window.kryonState = kryonState;\n"
            "window.kryonUpdateBindings = kryonUpdateBindings;\n"
        );
        return output;
    }

    // Generate state object from manifest.variables
    strcat(output,
        "// Reactive state (from IRReactiveManifest.variables)\n"
        "const kryonStateData = {\n"
    );

    for (uint32_t i = 0; i < manifest->variable_count; i++) {
        IRReactiveVarDescriptor* var = &manifest->variables[i];
        if (!var->name) continue;

        char var_line[512];

        // Get initial value
        if (var->initial_value_json && strlen(var->initial_value_json) > 0) {
            // Use provided initial value JSON
            snprintf(var_line, sizeof(var_line), "  %s: %s,\n",
                     var->name, var->initial_value_json);
        } else {
            // Use default based on type
            switch (var->type) {
                case IR_REACTIVE_TYPE_INT:
                    snprintf(var_line, sizeof(var_line), "  %s: %ld,\n",
                             var->name, (long)var->value.as_int);
                    break;
                case IR_REACTIVE_TYPE_FLOAT:
                    snprintf(var_line, sizeof(var_line), "  %s: %f,\n",
                             var->name, var->value.as_float);
                    break;
                case IR_REACTIVE_TYPE_STRING:
                    if (var->value.as_string) {
                        char escaped[256];
                        js_escape_string(escaped, var->value.as_string, sizeof(escaped));
                        snprintf(var_line, sizeof(var_line), "  %s: \"%s\",\n",
                                 var->name, escaped);
                    } else {
                        snprintf(var_line, sizeof(var_line), "  %s: \"\",\n", var->name);
                    }
                    break;
                case IR_REACTIVE_TYPE_BOOL:
                    snprintf(var_line, sizeof(var_line), "  %s: %s,\n",
                             var->name, var->value.as_bool ? "true" : "false");
                    break;
                default:
                    snprintf(var_line, sizeof(var_line), "  %s: %s,\n",
                             var->name, js_default_for_reactive_var(var->type));
                    break;
            }
        }
        strcat(output, var_line);
    }

    strcat(output, "};\n\n");

    // Generate bindings map from manifest.bindings
    strcat(output,
        "// DOM bindings (from IRReactiveManifest.bindings)\n"
        "const kryonBindings = {\n"
    );

    // Group bindings by variable ID
    // First, create a map from var_id to var_name
    char var_names[64][64];  // Up to 64 variables
    for (uint32_t i = 0; i < manifest->variable_count && i < 64; i++) {
        if (manifest->variables[i].name) {
            strncpy(var_names[manifest->variables[i].id], manifest->variables[i].name, 63);
            var_names[manifest->variables[i].id][63] = '\0';
        }
    }

    // Track which variables have bindings
    bool has_bindings[64] = {false};
    for (uint32_t i = 0; i < manifest->binding_count; i++) {
        uint32_t var_id = manifest->bindings[i].reactive_var_id;
        if (var_id < 64) has_bindings[var_id] = true;
    }

    // Generate bindings grouped by variable
    for (uint32_t v = 0; v < manifest->variable_count && v < 64; v++) {
        uint32_t var_id = manifest->variables[v].id;
        if (!has_bindings[var_id]) continue;

        const char* var_name = manifest->variables[v].name;
        if (!var_name) continue;

        char binding_start[128];
        snprintf(binding_start, sizeof(binding_start), "  '%s': [\n", var_name);
        strcat(output, binding_start);

        for (uint32_t i = 0; i < manifest->binding_count; i++) {
            IRReactiveBinding* binding = &manifest->bindings[i];
            if (binding->reactive_var_id != var_id) continue;

            char binding_line[256];
            snprintf(binding_line, sizeof(binding_line),
                     "    { elementId: 'kryon-%u', type: '%s'%s },\n",
                     binding->component_id,
                     js_binding_type_string(binding->binding_type),
                     binding->expression ? ", expr: true" : "");
            strcat(output, binding_line);
        }

        strcat(output, "  ],\n");
    }

    strcat(output, "};\n\n");

    // Generate reactive proxy
    strcat(output,
        "// Reactive proxy for automatic DOM updates\n"
        "const kryonState = new Proxy(kryonStateData, {\n"
        "  set(target, prop, value) {\n"
        "    const oldValue = target[prop];\n"
        "    target[prop] = value;\n"
        "    if (oldValue !== value) {\n"
        "      kryonUpdateBindings(prop, value, oldValue);\n"
        "    }\n"
        "    return true;\n"
        "  },\n"
        "  get(target, prop) {\n"
        "    return target[prop];\n"
        "  }\n"
        "});\n\n"
    );

    // Generate update function
    strcat(output,
        "// Update DOM elements bound to a reactive property\n"
        "function kryonUpdateBindings(prop, newValue, oldValue) {\n"
        "  console.log('[Kryon Reactive] State changed:', prop, '=', newValue);\n"
        "  const bindings = kryonBindings[prop] || [];\n"
        "  bindings.forEach(binding => {\n"
        "    const el = document.getElementById(binding.elementId);\n"
        "    if (!el) {\n"
        "      console.warn('[Kryon Reactive] Element not found:', binding.elementId);\n"
        "      return;\n"
        "    }\n"
        "    switch (binding.type) {\n"
        "      case 'textContent':\n"
        "        el.textContent = String(newValue);\n"
        "        break;\n"
        "      case 'visibility':\n"
        "        el.style.display = newValue ? '' : 'none';\n"
        "        break;\n"
        "      case 'attribute':\n"
        "        // binding.attr specifies which attribute to update\n"
        "        if (binding.attr) el.setAttribute(binding.attr, newValue);\n"
        "        break;\n"
        "      case 'forEach':\n"
        "        // ForEach requires re-rendering the list\n"
        "        console.log('[Kryon Reactive] ForEach update for', prop);\n"
        "        break;\n"
        "    }\n"
        "  });\n"
        "}\n\n"
    );

    // Generate Lua bridge functions
    strcat(output,
        "// Lua bridge: Allow Lua handlers to access/modify state\n"
        "window.kryonState = kryonState;\n"
        "window.kryonUpdateBindings = kryonUpdateBindings;\n\n"
        "// Helper for Lua to get state\n"
        "window.kryonGetState = function(key) {\n"
        "  return kryonState[key];\n"
        "};\n\n"
        "// Helper for Lua to set state\n"
        "window.kryonSetState = function(key, value) {\n"
        "  kryonState[key] = value;\n"
        "};\n\n"
        "console.log('[Kryon Reactive] Reactive system initialized with',\n"
        "            Object.keys(kryonStateData).length, 'variables');\n"
    );

    return output;
}

// Write JavaScript reactive system to output directory
bool write_js_reactive_system(const char* output_dir, IRReactiveManifest* manifest) {
    if (!output_dir) return false;

    char* js_code = generate_js_reactive_system(manifest);
    if (!js_code) return false;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/kryon-reactive.js", output_dir);

    FILE* f = fopen(filepath, "w");
    if (!f) {
        free(js_code);
        return false;
    }

    fprintf(f, "%s", js_code);
    fclose(f);

    printf("✅ Generated JavaScript reactive system: %s\n", filepath);
    free(js_code);
    return true;
}