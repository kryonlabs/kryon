/**
 * @file kry_codegen_adapter.c
 * @brief KRY Codegen Adapter for Codegen Interface
 *
 * Adapts the existing KRY codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "kry_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* kry_extensions[] = {".kry", NULL};

static const CodegenInfo kry_codegen_info_impl = {
    .name = "KRY",
    .version = "1.0.0",
    .description = "KRY DSL code generator (round-trip format)",
    .file_extensions = kry_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* kry_codegen_get_info(void) {
    return &kry_codegen_info_impl;
}

static bool kry_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to KRY codegen\n");
        return false;
    }

    return kry_codegen_generate_multi(kir_path, output_path);
}

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface kry_codegen_interface = {
    .get_info = kry_codegen_get_info,
    .generate = kry_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
