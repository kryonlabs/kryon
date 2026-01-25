/**
 * C Code Generator - Main Function and Struct Generation
 *
 * Functions for generating the main() function and struct definitions.
 */

#ifndef IR_C_MAIN_H
#define IR_C_MAIN_H

#include "ir_c_internal.h"
#include "../../third_party/cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate C struct definitions from source_structures.struct_types
 *
 * @param ctx Codegen context
 * @param struct_types Array of struct definition JSON objects
 */
void c_generate_struct_definitions(CCodegenContext* ctx, cJSON* struct_types);

/**
 * Generate the main() function for a Kryon application
 *
 * Generates:
 * - kryon_init() call with app metadata
 * - KRYON_APP() macro with component tree
 * - KRYON_RUN() call
 * - Signal cleanup
 *
 * @param ctx Codegen context with output file and state
 */
void c_generate_main_function(CCodegenContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // IR_C_MAIN_H
