/**
 * @file limbo_codegen_adapter.c
 * @brief Limbo Codegen Adapter for Codegen Interface
 *
 * Adapts the existing Limbo codegen to implement the unified CodegenInterface.
 * Uses IMPLEMENT_CODEGEN_ADAPTER macro to eliminate boilerplate.
 */

#include "../codegen_adapter_macros.h"
#include "limbo_codegen.h"

/* ============================================================================
 * Codegen Adapter
 * ============================================================================ */

/**
 * Implementation note: We use the limbo_codegen_generate_multi function which
 * handles both single-file and multi-file output based on output_path.
 */
IMPLEMENT_CODEGEN_ADAPTER(Limbo, "Limbo code generator (Inferno/TaijiOS)", limbo_codegen_generate_multi, ".b", ".m", NULL)
