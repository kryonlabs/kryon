/**
 * KRY Struct to IR Converter Implementation
 *
 * Converts struct declarations and instances from KRY AST to IR structures.
 *
 * Note: kry_value_to_json is implemented in kry_to_ir.c to avoid duplication
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_struct_to_ir.h"
#include "../include/ir_builder.h"
#include "../include/ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// External reference to kry_value_to_json (implemented in kry_to_ir.c)
extern char* kry_value_to_json(KryValue* value);

// ============================================================================
// Struct Declaration to IR Conversion
// ============================================================================

// Convert struct declaration to IRStructType
IRStructType* kry_convert_struct_decl(ConversionContext* ctx, KryNode* node) {
    if (!node || node->type != KRY_NODE_STRUCT_DECL) {
        return NULL;
    }

    IRStructType* struct_type = (IRStructType*)calloc(1, sizeof(IRStructType));
    if (!struct_type) return NULL;

    struct_type->name = strdup(node->struct_name);
    struct_type->line = node->line;
    struct_type->field_count = node->field_count;

    // Allocate fields array
    struct_type->fields = (IRStructField**)calloc(node->field_count, sizeof(IRStructField*));

    // Convert each field
    for (int i = 0; i < node->field_count; i++) {
        KryNode* field_node = node->struct_fields[i];
        if (!field_node || field_node->type != KRY_NODE_STRUCT_FIELD) continue;

        IRStructField* field = (IRStructField*)calloc(1, sizeof(IRStructField));
        field->name = strdup(field_node->name);
        field->type = strdup(field_node->field_type);
        field->line = field_node->line;

        // Convert default value
        if (field_node->field_default) {
            field->default_value = kry_value_to_json(field_node->field_default);
        } else {
            field->default_value = strdup("");
        }

        struct_type->fields[i] = field;
    }

    fprintf(stderr, "[STRUCT] Converted struct '%s' with %d fields\n",
            struct_type->name, struct_type->field_count);

    return struct_type;
}

// ============================================================================
// Struct Instance to IR Conversion
// ============================================================================

// Convert struct instance to IRStructInstance
IRStructInstance* kry_convert_struct_inst(ConversionContext* ctx, KryNode* node) {
    if (!node || node->type != KRY_NODE_STRUCT_INST) {
        return NULL;
    }

    IRStructInstance* instance = (IRStructInstance*)calloc(1, sizeof(IRStructInstance));
    if (!instance) return NULL;

    instance->struct_type = strdup(node->instance_type);
    instance->line = node->line;
    instance->field_count = node->field_value_count;

    // Allocate field arrays
    instance->field_names = (char**)calloc(node->field_value_count, sizeof(char*));
    instance->field_values = (char**)calloc(node->field_value_count, sizeof(char*));

    // Convert field values
    for (int i = 0; i < node->field_value_count; i++) {
        instance->field_names[i] = strdup(node->field_names[i]);

        if (node->field_values[i]) {
            instance->field_values[i] = kry_value_to_json(node->field_values[i]);
        }
    }

    fprintf(stderr, "[STRUCT] Converted instance of type '%s' with %d fields\n",
            instance->struct_type, instance->field_count);

    return instance;
}
