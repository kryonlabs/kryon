/**
 * @file c_codegen_adapter.c
 * @brief C Codegen Adapter for Codegen Interface
 *
 * Adapts the existing C codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "ir_c_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* c_extensions[] = {".c", ".h", NULL};

static const CodegenInfo c_codegen_info_impl = {
    .name = "C",
    .version = "1.0.0",
    .description = "C code generator",
    .file_extensions = c_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* c_codegen_get_info(void) {
    return &c_codegen_info_impl;
}

static bool c_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to C codegen\n");
        return false;
    }

    return ir_generate_c_code_multi(kir_path, output_path);
}

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface c_codegen_interface = {
    .get_info = c_codegen_get_info,
    .generate = c_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
