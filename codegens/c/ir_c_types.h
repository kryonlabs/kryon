/**
 * C Code Generator - Type Mapping Utilities
 *
 * Maps KIR types to C types and provides component macro lookups.
 */

#ifndef IR_C_TYPES_H
#define IR_C_TYPES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Map KIR type to C type string
 *
 * @param kir_type KIR type name (e.g., "string", "int", "bool")
 * @return C type string (e.g., "const char*", "int", "bool")
 */
const char* kir_type_to_c(const char* kir_type);

/**
 * Get component macro name for a component type
 *
 * @param type Component type name (e.g., "Container", "Button")
 * @return C DSL macro name (e.g., "CONTAINER", "BUTTON")
 */
const char* get_component_macro(const char* type);

/**
 * Check if a string is a numeric literal
 *
 * @param str String to check
 * @return true if string represents a number
 */
bool is_numeric(const char* str);

#ifdef __cplusplus
}
#endif

#endif // IR_C_TYPES_H
