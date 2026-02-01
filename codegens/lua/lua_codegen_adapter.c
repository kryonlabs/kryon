/**
 * @file lua_codegen_adapter.c
 * @brief Lua Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Lua codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "lua_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* lua_extensions[] = {".lua", NULL};

static const CodegenInfo lua_codegen_info_impl = {
    .name = "Lua",
    .version = "1.0.0",
    .description = "Lua DSL code generator",
    .file_extensions = lua_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* lua_codegen_get_info(void) {
    return &lua_codegen_info_impl;
}

static bool lua_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to Lua codegen\n");
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

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface lua_codegen_interface = {
    .get_info = lua_codegen_get_info,
    .generate = lua_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
