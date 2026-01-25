/**
 * C Code Generator - Component Tree Generation
 *
 * Functions for generating C code from KIR component trees.
 */

#ifndef IR_C_COMPONENTS_H
#define IR_C_COMPONENTS_H

#include "ir_c_internal.h"
#include "../../third_party/cJSON/cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate C code for a component and its children recursively
 *
 * Handles all component types including:
 * - For loops (emits FOR_EACH macro)
 * - Container, Row, Column layouts
 * - Button, Text, Input, Image, Canvas
 * - All style and event properties
 *
 * @param ctx Codegen context with output file and state
 * @param component The component JSON to generate
 * @param is_root Whether this is the root KRYON_APP component
 */
void c_generate_component_recursive(CCodegenContext* ctx, cJSON* component, bool is_root);

/**
 * Generate a property macro for a component
 *
 * Handles all property types:
 * - Dimensions (width, height)
 * - Colors (bg_color, color)
 * - Layout (gap, padding, margin, align, justify)
 * - Font properties
 * - Events (onClick, onChange)
 * - Reactive bindings (bind_*)
 *
 * @param ctx Codegen context
 * @param key Property name
 * @param value Property value (JSON)
 * @param first_prop Pointer to flag tracking if this is first property
 * @return true on success, false on error
 */
bool c_generate_property_macro(CCodegenContext* ctx, const char* key, cJSON* value, bool* first_prop);

/**
 * Generate C code for component definitions (for module files)
 *
 * Used for non-app modules that define reusable components.
 *
 * @param ctx Codegen context
 * @param component_defs Array of component definition JSON objects
 */
void c_generate_component_definitions(CCodegenContext* ctx, cJSON* component_defs);

#ifdef __cplusplus
}
#endif

#endif // IR_C_COMPONENTS_H
