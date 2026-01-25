/**
 * C Code Generator - Reactive Signal Utilities
 *
 * Functions for generating reactive state (signals) and event handlers.
 */

#ifndef IR_C_REACTIVE_H
#define IR_C_REACTIVE_H

#include "ir_c_internal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if a name matches any reactive variable
 *
 * @param ctx Code generation context
 * @param name Variable name to check
 * @return true if the name is in reactive_manifest.variables
 */
bool c_is_reactive_variable(CCodegenContext* ctx, const char* name);

/**
 * Get the scoped variable name for a given variable in current scope
 *
 * @param ctx Code generation context
 * @param name Base variable name
 * @return Scoped name (caller must free), or NULL if not found
 */
char* c_get_scoped_var_name(CCodegenContext* ctx, const char* name);

/**
 * Generate a unique C variable name from variable name and scope
 *
 * Input: name="value", scope="Counter#0"
 * Output: "value_Counter_0"
 *
 * @param name Variable name
 * @param scope Scope string (e.g., "Counter#0", "component")
 * @return Generated name (caller must free)
 */
char* c_generate_scoped_var_name(const char* name, const char* scope);

/**
 * Map KIR type to C signal creation function
 *
 * @param kir_type KIR type string (e.g., "int", "string", "bool")
 * @return Signal creator function name
 */
const char* c_get_signal_creator(const char* kir_type);

/**
 * Generate signal declarations from reactive_manifest.variables
 * Output: KryonSignal* value_signal;
 */
void c_generate_reactive_signal_declarations(CCodegenContext* ctx);

/**
 * Generate signal initialization code
 * Output: value_signal = kryon_signal_create(0);
 */
void c_generate_reactive_signal_initialization(CCodegenContext* ctx);

/**
 * Generate signal cleanup code
 * Output: kryon_signal_destroy(value_signal);
 */
void c_generate_reactive_signal_cleanup(CCodegenContext* ctx);

/**
 * Generate event handlers from logic_block
 * Transpiles KRY event handlers to C functions
 */
void c_generate_kry_event_handlers(CCodegenContext* ctx, cJSON* logic_block);

#ifdef __cplusplus
}
#endif

#endif // IR_C_REACTIVE_H
