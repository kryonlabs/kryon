/**
 * @file tcltk_codegen_adapter.c
 * @brief Tcl/Tk Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Tcl/Tk codegen to implement the unified CodegenInterface.
 */

#include "../codegen_interface.h"
#include "tcltk_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Codegen Metadata
 * ============================================================================ */

static const char* tcltk_extensions[] = {".tcl", NULL};

static const CodegenInfo tcltk_codegen_info_impl = {
    .name = "Tcl/Tk",
    .version = "1.0.0",
    .description = "Tcl/Tk code generator",
    .file_extensions = tcltk_extensions
};

/* ============================================================================
 * Codegen Interface Implementation
 * ============================================================================ */

static const CodegenInfo* tcltk_codegen_get_info(void) {
    return &tcltk_codegen_info_impl;
}

static bool tcltk_codegen_generate_impl(const char* kir_path, const char* output_path) {
    if (!kir_path || !output_path) {
        fprintf(stderr, "Error: Invalid arguments to Tcl/Tk codegen\n");
        return false;
    }

    return tcltk_codegen_generate_multi(kir_path, output_path);
}

/* ============================================================================
 * Codegen Interface Export
 * ============================================================================ */

const CodegenInterface tcltk_codegen_interface = {
    .get_info = tcltk_codegen_get_info,
    .generate = tcltk_codegen_generate_impl,
    .validate = NULL,
    .cleanup = NULL
};
