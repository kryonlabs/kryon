/**
 * @file c_codegen_adapter.c
 * @brief C Codegen Adapter for Codegen Interface
 *
 * Adapts the existing C codegen to implement the unified CodegenInterface.
 * Uses IMPLEMENT_CODEGEN_ADAPTER macro to eliminate boilerplate.
 */

#include "../codegen_adapter_macros.h"
#include "ir_c_codegen.h"

/* ============================================================================
 * Codegen Adapter
 * ============================================================================ */

/**
 * Implementation note: We use the ir_generate_c_code_multi function which
 * handles both single-file and multi-file output based on output_path.
 */
IMPLEMENT_CODEGEN_ADAPTER(C, "C code generator", ir_generate_c_code_multi, ".c", ".h", NULL)
