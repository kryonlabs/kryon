/**
 * @file markdown_codegen_adapter.c
 * @brief Markdown Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Markdown codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "markdown_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* markdown_extensions[] = {".md", ".markdown", NULL};

static const CodegenInfo markdown_codegen_info_impl = {
    .name = "Markdown",
    .version = "1.0.0",
    .description = "Markdown documentation generator",
    .file_extensions = markdown_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* markdown_codegen_get_info(void) {
    return &markdown_codegen_info_impl;
}

static bool markdown_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to Markdown codegen\n");
        return false;
    }

    return markdown_codegen_generate_multi(kir_path, output_path);
}

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface markdown_codegen_interface = {
    .get_info = markdown_codegen_get_info,
    .generate = markdown_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
