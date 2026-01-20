/**
 * KRY Struct to IR Converter
 *
 * Converts struct declarations and instances from KRY AST to IR structures.
 */

#ifndef IR_KRY_STRUCT_TO_IR_H
#define IR_KRY_STRUCT_TO_IR_H

#include "kry_ast.h"
#include "../include/ir_serialization.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration (opaque type)
typedef struct ConversionContext ConversionContext;

// Convert struct declaration to IRStructType
IRStructType* kry_convert_struct_decl(ConversionContext* ctx, KryNode* node);

// Convert struct instance to IRStructInstance
IRStructInstance* kry_convert_struct_inst(ConversionContext* ctx, KryNode* node);

// Convert KryValue to JSON string expression
char* kry_value_to_json(KryValue* value);

#ifdef __cplusplus
}
#endif

#endif // IR_KRY_STRUCT_TO_IR_H
