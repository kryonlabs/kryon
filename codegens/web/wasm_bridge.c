#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../../ir/ir_core.h"
#include "wasm_bridge.h"

// WASM Bridge Context
struct WASMBridge {
    WASMModule* modules;
    char output_directory[256];
    char nim_compiler_path[512];
    int optimization_level;
    size_t module_count;
};


WASMBridge* wasm_bridge_create() {
    WASMBridge* bridge = malloc(sizeof(WASMBridge));
    if (!bridge) return NULL;

    bridge->modules = NULL;
    strcpy(bridge->output_directory, ".");

    // Try to find Nim compiler
    const char* nim_paths[] = {"nim", "/usr/bin/nim", "/usr/local/bin/nim", "~/.nimble/bin/nim"};
    bool found_nim = false;

    for (size_t i = 0; i < sizeof(nim_paths) / sizeof(nim_paths[0]); i++) {
        if (access(nim_paths[i], X_OK) == 0) {
            strcpy(bridge->nim_compiler_path, nim_paths[i]);
            found_nim = true;
            break;
        }
    }

    if (!found_nim) {
        strcpy(bridge->nim_compiler_path, "nim"); // Default fallback
    }

    bridge->optimization_level = 2; // Default optimization
    bridge->module_count = 0;

    return bridge;
}

void wasm_bridge_destroy(WASMBridge* bridge) {
    if (!bridge) return;

    // Free all modules
    WASMModule* module = bridge->modules;
    while (module) {
        WASMModule* next = module->next;
        if (module->module_id) free(module->module_id);
        if (module->source_file) free(module->source_file);
        if (module->wasm_output) free(module->wasm_output);
        if (module->js_glue_code) free(module->js_glue_code);
        free(module);
        module = next;
    }

    free(bridge);
}

void wasm_bridge_set_output_directory(WASMBridge* bridge, const char* directory) {
    if (!bridge || !directory) return;
    strncpy(bridge->output_directory, directory, sizeof(bridge->output_directory) - 1);
    bridge->output_directory[sizeof(bridge->output_directory) - 1] = '\0';
}

void wasm_bridge_set_nim_compiler(WASMBridge* bridge, const char* nim_compiler_path) {
    if (!bridge || !nim_compiler_path) return;
    strncpy(bridge->nim_compiler_path, nim_compiler_path, sizeof(bridge->nim_compiler_path) - 1);
    bridge->nim_compiler_path[sizeof(bridge->nim_compiler_path) - 1] = '\0';
}

void wasm_bridge_set_optimization_level(WASMBridge* bridge, int level) {
    if (!bridge) return;
    bridge->optimization_level = (level < 0) ? 0 : (level > 3) ? 3 : level;
}

WASMModule* wasm_bridge_create_module(WASMBridge* bridge, const char* module_id, IRComponent* component) {
    if (!bridge || !module_id || !component) return NULL;

    WASMModule* module = malloc(sizeof(WASMModule));
    if (!module) return NULL;

    module->module_id = strdup(module_id);
    module->source_file = NULL;
    module->wasm_output = NULL;
    module->js_glue_code = NULL;
    module->next = NULL;

    // Generate file paths
    char nim_filename[512];
    char wasm_filename[512];
    char js_filename[512];

    snprintf(nim_filename, sizeof(nim_filename), "%s/%s.nim", bridge->output_directory, module_id);
    snprintf(wasm_filename, sizeof(wasm_filename), "%s/%s.wasm", bridge->output_directory, module_id);
    snprintf(js_filename, sizeof(js_filename), "%s/%s.js", bridge->output_directory, module_id);

    module->source_file = strdup(nim_filename);
    module->wasm_output = strdup(wasm_filename);
    module->js_glue_code = strdup(js_filename);

    // Add to linked list
    if (!bridge->modules) {
        bridge->modules = module;
    } else {
        WASMModule* current = bridge->modules;
        while (current->next) {
            current = current->next;
        }
        current->next = module;
    }

    bridge->module_count++;
    return module;
}

bool wasm_bridge_write_nim_source(WASMBridge* bridge, WASMModule* module) {
    if (!bridge || !module || !module->source_file) return false;

    FILE* file = fopen(module->source_file, "w");
    if (!file) return false;

    fprintf(file, "# Auto-generated Nim WASM module for Kryon\n");
    fprintf(file, "# Module ID: %s\n\n", module->module_id);

    fprintf(file, "import jsffi\n");
    fprintf(file, "import dom\n\n");

    fprintf(file, "# Export functions for JavaScript interaction\n");

    // Generate placeholder functions based on module ID
    // In a real implementation, this would extract logic from IR
    fprintf(file, "proc handle_click(component_id: int) {.exportc.} =\n");
    fprintf(file, "  console.log(\"Click handler called for component: \" & $component_id)\n\n");

    fprintf(file, "proc handle_hover(component_id: int) {.exportc.} =\n");
    fprintf(file, "  console.log(\"Hover handler called for component: \" & $component_id)\n\n");

    fprintf(file, "proc handle_input(component_id: int, text: cstring) {.exportc.} =\n");
    fprintf(file, "  console.log(\"Input handler called for component \" & $component_id & \" with text: \" & text)\n\n");

    fprintf(file, "proc get_component_state(component_id: int): cstring {.exportc.} =\n");
    fprintf(file, "  result = \"state_for_\" & $component_id\n\n");

    fprintf(file, "# Module initialization\n");
    fprintf(file, "proc init_module() {.exportc.} =\n");
    fprintf(file, "  console.log(\"%s module initialized\")\n\n", module->module_id);

    fclose(file);
    return true;
}

bool wasm_bridge_compile_nim_to_wasm(WASMBridge* bridge, WASMModule* module) {
    if (!bridge || !module || !module->source_file || !module->wasm_output) return false;

    printf("🔧 Compiling Nim to WASM: %s → %s\n", module->source_file, module->wasm_output);

    // Build Nim compilation command
    char compile_cmd[1024];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "%s c -d:release -d:emscripten --os:emscripten --cpu:wasm32 -o:%s %s",
             bridge->nim_compiler_path, module->wasm_output, module->source_file);

    // For now, create a placeholder WASM file (nim → emscripten setup is complex)
    // In a real implementation, this would invoke the Nim compiler with emscripten
    FILE* wasm_file = fopen(module->wasm_output, "wb");
    if (!wasm_file) return false;

    // Write minimal WASM header (placeholder)
    const uint8_t minimal_wasm[] = {
        0x00, 0x61, 0x73, 0x6d, // Magic: "\0asm"
        0x01, 0x00, 0x00, 0x00  // Version: 1
    };
    fwrite(minimal_wasm, 1, sizeof(minimal_wasm), wasm_file);
    fclose(wasm_file);

    printf("✅ Created placeholder WASM: %s\n", module->wasm_output);
    return true;
}

bool wasm_bridge_generate_js_glue(WASMBridge* bridge, WASMModule* module) {
    if (!bridge || !module || !module->js_glue_code) return false;

    FILE* file = fopen(module->js_glue_code, "w");
    if (!file) return false;

    fprintf(file, "// Auto-generated JavaScript glue for %s WASM module\n\n", module->module_id);

    fprintf(file, "class %sWASM {\n", module->module_id);
    fprintf(file, "  constructor() {\n");
    fprintf(file, "    this.instance = null;\n");
    fprintf(file, "    this.initialized = false;\n");
    fprintf(file, "  }\n\n");

    fprintf(file, "  async initialize() {\n");
    fprintf(file, "    if (this.initialized) return;\n\n");
    fprintf(file, "    try {\n");
    fprintf(file, "      const response = await fetch('%s.wasm');\n", module->module_id);
    fprintf(file, "      const bytes = await response.arrayBuffer();\n");
    fprintf(file, "      const results = await WebAssembly.instantiate(bytes);\n");
    fprintf(file, "      this.instance = results.instance;\n");
    fprintf(file, "      this.initialized = true;\n");
    fprintf(file, "      console.log('%s WASM module loaded successfully');\n", module->module_id);
    fprintf(file, "    } catch (error) {\n");
    fprintf(file, "      console.error('Failed to load %s WASM:', error);\n", module->module_id);
    fprintf(file, "    }\n");
    fprintf(file, "  }\n\n");

    // Add function wrappers
    fprintf(file, "  handle_click(component_id) {\n");
    fprintf(file, "    if (!this.initialized) {\n");
    fprintf(file, "      throw new Error('%s WASM not initialized');\n", module->module_id);
    fprintf(file, "    }\n");
    fprintf(file, "    return this.instance.exports.handle_click(component_id);\n");
    fprintf(file, "  }\n\n");

    fprintf(file, "  handle_hover(component_id) {\n");
    fprintf(file, "    if (!this.initialized) {\n");
    fprintf(file, "      throw new Error('%s WASM not initialized');\n", module->module_id);
    fprintf(file, "    }\n");
    fprintf(file, "    return this.instance.exports.handle_hover(component_id);\n");
    fprintf(file, "  }\n\n");

    fprintf(file, "  handle_input(component_id, text) {\n");
    fprintf(file, "    if (!this.initialized) {\n");
    fprintf(file, "      throw new Error('%s WASM not initialized');\n", module->module_id);
    fprintf(file, "    }\n");
    fprintf(file, "    return this.instance.exports.handle_input(component_id, text);\n");
    fprintf(file, "  }\n\n");

    fprintf(file, "  get_component_state(component_id) {\n");
    fprintf(file, "    if (!this.initialized) {\n");
    fprintf(file, "      throw new Error('%s WASM not initialized');\n", module->module_id);
    fprintf(file, "    }\n");
    fprintf(file, "    return this.instance.exports.get_component_state(component_id);\n");
    fprintf(file, "  }\n");
    fprintf(file, "}\n\n");

    fprintf(file, "// Global instance\n");
    fprintf(file, "window.%sWASM = new %sWASM();\n", module->module_id, module->module_id);

    fclose(file);
    printf("✅ Generated JavaScript glue: %s\n", module->js_glue_code);
    return true;
}

bool wasm_bridge_extract_logic_from_ir(WASMBridge* bridge, IRComponent* root) {
    if (!bridge || !root) return false;

    printf("🔍 Extracting WASM modules from IR component tree...\n");

    // Look for components with logic or event handlers
    // This is a simplified version - in reality, we'd analyze the IR logic
    static int module_counter = 0;

    // Create a default module for now
    char module_id[64];
    snprintf(module_id, sizeof(module_id), "kryon_logic_%d", module_counter++);

    WASMModule* module = wasm_bridge_create_module(bridge, module_id, root);
    if (!module) {
        printf("❌ Failed to create WASM module\n");
        return false;
    }

    printf("✅ Created WASM module: %s\n", module_id);
    return true;
}

bool wasm_bridge_generate_web_bindings(WASMBridge* bridge, const char* output_dir) {
    if (!bridge || !output_dir) return false;

    printf("🌐 Generating web WASM bindings...\n");

    // Generate a main loader script
    char loader_path[512];
    snprintf(loader_path, sizeof(loader_path), "%s/kryon_wasm_loader.js", output_dir);

    FILE* loader_file = fopen(loader_path, "w");
    if (!loader_file) return false;

    fprintf(loader_file, "// Kryon WASM Module Loader\n");
    fprintf(loader_file, "// Auto-generated for loading all WASM modules\n\n");

    fprintf(loader_file, "class KryonWASMLoader {\n");
    fprintf(loader_file, "  constructor() {\n");
    fprintf(loader_file, "    this.modules = new Map();\n");
    fprintf(loader_file, "    this.initialized = false;\n");
    fprintf(loader_file, "  }\n\n");

    fprintf(loader_file, "  async initialize() {\n");
    fprintf(loader_file, "    if (this.initialized) return;\n\n");

    // Add module initializations
    WASMModule* module = bridge->modules;
    while (module) {
        fprintf(loader_file, "    // Load %s module\n", module->module_id);
        fprintf(loader_file, "    if (window.%sWASM) {\n", module->module_id);
        fprintf(loader_file, "      await window.%sWASM.initialize();\n", module->module_id);
        fprintf(loader_file, "      this.modules.set('%s', window.%sWASM);\n", module->module_id, module->module_id);
        fprintf(loader_file, "    }\n\n");
        module = module->next;
    }

    fprintf(loader_file, "    this.initialized = true;\n");
    fprintf(loader_file, "    console.log('All Kryon WASM modules loaded');\n");
    fprintf(loader_file, "  }\n\n");

    fprintf(loader_file, "  getModule(module_id) {\n");
    fprintf(loader_file, "    return this.modules.get(module_id);\n");
    fprintf(loader_file, "  }\n");
    fprintf(loader_file, "}\n\n");

    fprintf(loader_file, "// Global loader instance\n");
    fprintf(loader_file, "window.kryonWASMLoader = new KryonWASMLoader();\n");

    fprintf(loader_file, "// Auto-initialize when DOM is ready\n");
    fprintf(loader_file, "document.addEventListener('DOMContentLoaded', () => {\n");
    fprintf(loader_file, "  window.kryonWASMLoader.initialize();\n");
    fprintf(loader_file, "});\n");

    fclose(loader_file);
    printf("✅ Generated web bindings loader: %s\n", loader_path);
    return true;
}

bool wasm_bridge_write_runtime_loader(WASMBridge* bridge, const char* output_dir) {
    return wasm_bridge_generate_web_bindings(bridge, output_dir);
}

size_t wasm_bridge_get_module_count(WASMBridge* bridge) {
    return bridge ? bridge->module_count : 0;
}

WASMModule* wasm_bridge_get_module(WASMBridge* bridge, const char* module_id) {
    if (!bridge || !module_id) return NULL;

    WASMModule* module = bridge->modules;
    while (module) {
        if (strcmp(module->module_id, module_id) == 0) {
            return module;
        }
        module = module->next;
    }
    return NULL;
}

void wasm_bridge_print_stats(WASMBridge* bridge) {
    if (!bridge) return;

    printf("📊 WASM Bridge Statistics:\n");
    printf("   Output directory: %s\n", bridge->output_directory);
    printf("   Nim compiler: %s\n", bridge->nim_compiler_path);
    printf("   Optimization level: %d\n", bridge->optimization_level);
    printf("   Module count: %zu\n", bridge->module_count);

    printf("   Modules:\n");
    WASMModule* module = bridge->modules;
    while (module) {
        printf("     - %s (nim: %s, wasm: %s, js: %s)\n",
               module->module_id,
               module->source_file ? module->source_file : "N/A",
               module->wasm_output ? module->wasm_output : "N/A",
               module->js_glue_code ? module->js_glue_code : "N/A");
        module = module->next;
    }
}

bool wasm_process_ir_logic(IRComponent* root, const char* output_dir) {
    if (!root || !output_dir) return false;

    printf("🔄 Processing IR logic for WASM compilation...\n");

    WASMBridge* bridge = wasm_bridge_create();
    if (!bridge) return false;

    wasm_bridge_set_output_directory(bridge, output_dir);

    // Extract modules from IR
    if (!wasm_bridge_extract_logic_from_ir(bridge, root)) {
        printf("❌ Failed to extract logic from IR\n");
        wasm_bridge_destroy(bridge);
        return false;
    }

    // Process all modules
    WASMModule* module = bridge->modules;
    while (module) {
        // Write Nim source
        if (!wasm_bridge_write_nim_source(bridge, module)) {
            printf("❌ Failed to write Nim source for %s\n", module->module_id);
            wasm_bridge_destroy(bridge);
            return false;
        }

        // Compile to WASM
        if (!wasm_bridge_compile_nim_to_wasm(bridge, module)) {
            printf("❌ Failed to compile %s to WASM\n", module->module_id);
            wasm_bridge_destroy(bridge);
            return false;
        }

        // Generate JavaScript glue
        if (!wasm_bridge_generate_js_glue(bridge, module)) {
            printf("❌ Failed to generate JS glue for %s\n", module->module_id);
            wasm_bridge_destroy(bridge);
            return false;
        }

        module = module->next;
    }

    // Generate web bindings
    if (!wasm_bridge_generate_web_bindings(bridge, output_dir)) {
        printf("❌ Failed to generate web bindings\n");
        wasm_bridge_destroy(bridge);
        return false;
    }

    wasm_bridge_print_stats(bridge);
    wasm_bridge_destroy(bridge);

    printf("🎉 WASM processing complete!\n");
    return true;
}