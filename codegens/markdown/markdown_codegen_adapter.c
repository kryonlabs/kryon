/**
 * @file markdown_codegen_adapter.c
 * @brief Markdown Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Markdown codegen to implement the unified CodegenInterface.
 * Uses IMPLEMENT_CODEGEN_ADAPTER macro to eliminate boilerplate.
 */

#include "../codegen_adapter_macros.h"
#include "markdown_codegen.h"

/* ============================================================================
 * Codegen Adapter
 * ============================================================================ */

/**
 * Implementation note: We use the markdown_codegen_generate_multi function.
 */
IMPLEMENT_CODEGEN_ADAPTER(Markdown, "Markdown documentation generator", markdown_codegen_generate_multi, ".md", ".markdown", NULL)
