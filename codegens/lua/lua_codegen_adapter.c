/**
 * @file lua_codegen_adapter.c
 * @brief Lua Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Lua codegen to implement the unified CodegenInterface.
 * Uses a custom implementation to handle file vs directory output detection.
 */

#include "../codegen_adapter_macros.h"
#include "../codegen_common.h"
#include "lua_codegen.h"
#include <string.h>

/* ============================================================================
 * Codegen Adapter - Custom Implementation
 * ============================================================================ */

/**
 * Custom generate implementation that detects file vs directory output.
 * Lua codegen supports both:
 * - Single file output: "output.lua"
 * - Directory output: "output_dir/"
 */
static bool lua_codegen_generate_with_detection(const char* kir_path, const char* output_path) {
    codegen_set_error_prefix("Lua");

    if (!kir_path || !output_path) {
        codegen_error("Invalid arguments to Lua codegen");
        return false;
    }

    // Determine if output_path is a file or directory
    // Check if output_path ends with .lua extension
    size_t output_len = strlen(output_path);
    bool is_file = (output_len > 4 && strcmp(output_path + output_len - 4, ".lua") == 0);

    if (is_file) {
        // Single file output
        return lua_codegen_generate(kir_path, output_path);
    } else {
        // Directory output - use multi-file generation
        return lua_codegen_generate_multi(kir_path, output_path);
    }
}

/* Use macro with custom generate function */
IMPLEMENT_CODEGEN_ADAPTER(Lua, "Lua DSL code generator", lua_codegen_generate_with_detection, ".lua", NULL)
