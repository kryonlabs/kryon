/**
 * Kryon .kry Property Handlers
 *
 * Property dispatch system for mapping .kry properties to IR components.
 * Extracted from kry_to_ir.c for modularity.
 */

#ifndef KRY_TO_IR_PROPERTIES_H
#define KRY_TO_IR_PROPERTIES_H

#include "kry_ast.h"
#include "kry_to_ir_internal.h"
#include "../../include/ir_core.h"
#include "../../include/ir_style.h"

// Component type lookup
IRComponentType kry_get_component_type(const char* name);

// Property application (main entry point)
bool kry_apply_property(ConversionContext* ctx, IRComponent* component, const char* name, KryValue* value);

// Helper functions exposed for other modules
const char* kry_resolve_value_as_string(ConversionContext* ctx, KryValue* value, bool* is_unresolved);
bool kry_get_value_as_bool(KryValue* value, bool* out);
uint32_t kry_parse_color_value(ConversionContext* ctx, KryValue* value, const char* property_name, IRComponent* component);
IRAlignment kry_parse_alignment(const char* align_str);

#endif // KRY_TO_IR_PROPERTIES_H
