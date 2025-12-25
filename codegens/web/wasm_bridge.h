#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#include "../../ir/ir_core.h"
#include <stdbool.h>
#include <stddef.h>

// WASM Bridge Context (opaque)
typedef struct WASMBridge WASMBridge;

// WASM module info
typedef struct WASMModule {
    char* module_id;
    char* source_file;
    char* wasm_output;
    char* js_glue_code;
    struct WASMModule* next;
} WASMModule;

// WASM Bridge Functions
WASMBridge* wasm_bridge_create(void);
void wasm_bridge_destroy(WASMBridge* bridge);

// Configuration
void wasm_bridge_set_output_directory(WASMBridge* bridge, const char* directory);
void wasm_bridge_set_nim_compiler(WASMBridge* bridge, const char* nim_compiler_path);
void wasm_bridge_set_optimization_level(WASMBridge* bridge, int level);

// Module management
WASMModule* wasm_bridge_create_module(WASMBridge* bridge, const char* module_id, IRComponent* component);
bool wasm_bridge_compile_nim_to_wasm(WASMBridge* bridge, WASMModule* module);
bool wasm_bridge_generate_js_glue(WASMBridge* bridge, WASMModule* module);

// Logic extraction from IR
bool wasm_bridge_extract_logic_from_ir(WASMBridge* bridge, IRComponent* root);
bool wasm_bridge_write_nim_source(WASMBridge* bridge, WASMModule* module);

// Web integration
bool wasm_bridge_generate_web_bindings(WASMBridge* bridge, const char* output_dir);
bool wasm_bridge_write_runtime_loader(WASMBridge* bridge, const char* output_dir);

// Utility functions
size_t wasm_bridge_get_module_count(WASMBridge* bridge);
WASMModule* wasm_bridge_get_module(WASMBridge* bridge, const char* module_id);
void wasm_bridge_print_stats(WASMBridge* bridge);

// Convenience function for complete WASM processing
bool wasm_process_ir_logic(IRComponent* root, const char* output_dir);

#endif // WASM_BRIDGE_H