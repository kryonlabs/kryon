/**
 * @file limbo_codegen_adapter.c
 * @brief Limbo Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Limbo codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "limbo_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* limbo_extensions[] = {".b", ".m", NULL};

static const CodegenInfo limbo_codegen_info_impl = {
    .name = "Limbo",
    .version = "1.0.0",
    .description = "Limbo code generator (Inferno/TaijiOS)",
    .file_extensions = limbo_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* limbo_codegen_get_info(void) {
    return &limbo_codegen_info_impl;
}

static bool limbo_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to Limbo codegen\n");
        return false;
    }

    return limbo_codegen_generate_multi(kir_path, output_path);
}

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface limbo_codegen_interface = {
    .get_info = limbo_codegen_get_info,
    .generate = limbo_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
