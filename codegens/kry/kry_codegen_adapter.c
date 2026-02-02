/**
 * @file kry_codegen_adapter.c
 * @brief KRY Codegen Adapter for Codegen Interface
 *
 * Adapts the existing KRY codegen to implement the unified CodegenInterface.
 * Uses IMPLEMENT_CODEGEN_ADAPTER macro to eliminate boilerplate.
 */

#include "../codegen_adapter_macros.h"
#include "kry_codegen.h"

/* ============================================================================
 * Codegen Adapter
 * ============================================================================ */

/**
 * Implementation note: We use the kry_codegen_generate_multi function.
 */
IMPLEMENT_CODEGEN_ADAPTER(KRY, "KRY DSL code generator (round-trip format)", kry_codegen_generate_multi, ".kry", NULL)
