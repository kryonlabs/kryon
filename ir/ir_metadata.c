#define _GNU_SOURCE
#include "ir_metadata.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Lifecycle
// ============================================================================

IRMetadata* ir_metadata_create(void) {
    IRMetadata* metadata = (IRMetadata*)calloc(1, sizeof(IRMetadata));
    if (!metadata) return NULL;

    metadata->version = strdup("2.1");
    metadata->function_capacity = 16;
    metadata->state_capacity = 16;
    metadata->host_function_capacity = 16;

    metadata->functions = (IRBytecodeFunction*)calloc(metadata->function_capacity, sizeof(IRBytecodeFunction));
    metadata->states = (IRStateDefinition*)calloc(metadata->state_capacity, sizeof(IRStateDefinition));
    metadata->host_functions = (IRHostFunctionDeclaration*)calloc(metadata->host_function_capacity, sizeof(IRHostFunctionDeclaration));

    return metadata;
}

void ir_metadata_destroy(IRMetadata* metadata) {
    if (!metadata) return;

    free(metadata->version);

    // Free functions
    for (int i = 0; i < metadata->function_count; i++) {
        free(metadata->functions[i].name);
        for (int j = 0; j < metadata->functions[i].instruction_count; j++) {
            if (metadata->functions[i].instructions[j].opcode == OP_PUSH_STRING &&
                metadata->functions[i].instructions[j].arg.string_arg) {
                free(metadata->functions[i].instructions[j].arg.string_arg);
            }
        }
        free(metadata->functions[i].instructions);
    }
    free(metadata->functions);

    // Free states
    for (int i = 0; i < metadata->state_count; i++) {
        free(metadata->states[i].name);
        if (metadata->states[i].type == IR_STATE_TYPE_STRING &&
            metadata->states[i].initial_value.string_value) {
            free(metadata->states[i].initial_value.string_value);
        }
    }
    free(metadata->states);

    // Free host functions
    for (int i = 0; i < metadata->host_function_count; i++) {
        free(metadata->host_functions[i].name);
        free(metadata->host_functions[i].signature);
    }
    free(metadata->host_functions);

    free(metadata);
}

// ============================================================================
// Bytecode Function Management
// ============================================================================

IRBytecodeFunction* ir_metadata_add_function(IRMetadata* metadata, uint32_t id, const char* name) {
    if (!metadata) return NULL;

    // Expand if needed
    if (metadata->function_count >= metadata->function_capacity) {
        metadata->function_capacity *= 2;
        metadata->functions = (IRBytecodeFunction*)realloc(metadata->functions,
                                metadata->function_capacity * sizeof(IRBytecodeFunction));
    }

    IRBytecodeFunction* func = &metadata->functions[metadata->function_count++];
    memset(func, 0, sizeof(IRBytecodeFunction));

    func->id = id;
    func->name = name ? strdup(name) : NULL;
    func->instruction_capacity = 16;
    func->instructions = (IRBytecodeInstruction*)calloc(func->instruction_capacity, sizeof(IRBytecodeInstruction));

    return func;
}

bool ir_function_add_instruction(IRBytecodeFunction* func, IRBytecodeInstruction instr) {
    if (!func) return false;

    // Expand if needed
    if (func->instruction_count >= func->instruction_capacity) {
        func->instruction_capacity *= 2;
        func->instructions = (IRBytecodeInstruction*)realloc(func->instructions,
                            func->instruction_capacity * sizeof(IRBytecodeInstruction));
    }

    func->instructions[func->instruction_count++] = instr;
    return true;
}

// Instruction helpers

IRBytecodeInstruction ir_instr_simple(IROpcode opcode) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.has_arg = false;
    return instr;
}

IRBytecodeInstruction ir_instr_int(IROpcode opcode, int64_t arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.int_arg = arg;
    instr.has_arg = true;
    return instr;
}

IRBytecodeInstruction ir_instr_float(IROpcode opcode, double arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.float_arg = arg;
    instr.has_arg = true;
    return instr;
}

IRBytecodeInstruction ir_instr_string(IROpcode opcode, const char* arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.string_arg = arg ? strdup(arg) : NULL;
    instr.has_arg = true;
    return instr;
}

IRBytecodeInstruction ir_instr_bool(IROpcode opcode, bool arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.bool_arg = arg;
    instr.has_arg = true;
    return instr;
}

IRBytecodeInstruction ir_instr_id(IROpcode opcode, uint32_t arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.id_arg = arg;
    instr.has_arg = true;
    return instr;
}

IRBytecodeInstruction ir_instr_offset(IROpcode opcode, int32_t arg) {
    IRBytecodeInstruction instr = {0};
    instr.opcode = opcode;
    instr.arg.offset_arg = arg;
    instr.has_arg = true;
    return instr;
}

// ============================================================================
// State Definition Management
// ============================================================================

static IRStateDefinition* add_state_internal(IRMetadata* metadata, uint32_t id, const char* name, IRStateType type) {
    if (!metadata) return NULL;

    // Expand if needed
    if (metadata->state_count >= metadata->state_capacity) {
        metadata->state_capacity *= 2;
        metadata->states = (IRStateDefinition*)realloc(metadata->states,
                          metadata->state_capacity * sizeof(IRStateDefinition));
    }

    IRStateDefinition* state = &metadata->states[metadata->state_count++];
    memset(state, 0, sizeof(IRStateDefinition));

    state->id = id;
    state->name = name ? strdup(name) : NULL;
    state->type = type;

    return state;
}

IRStateDefinition* ir_metadata_add_state_int(IRMetadata* metadata, uint32_t id,
                                              const char* name, int64_t initial_value) {
    IRStateDefinition* state = add_state_internal(metadata, id, name, IR_STATE_TYPE_INT);
    if (state) {
        state->initial_value.int_value = initial_value;
    }
    return state;
}

IRStateDefinition* ir_metadata_add_state_float(IRMetadata* metadata, uint32_t id,
                                                const char* name, double initial_value) {
    IRStateDefinition* state = add_state_internal(metadata, id, name, IR_STATE_TYPE_FLOAT);
    if (state) {
        state->initial_value.float_value = initial_value;
    }
    return state;
}

IRStateDefinition* ir_metadata_add_state_string(IRMetadata* metadata, uint32_t id,
                                                 const char* name, const char* initial_value) {
    IRStateDefinition* state = add_state_internal(metadata, id, name, IR_STATE_TYPE_STRING);
    if (state) {
        state->initial_value.string_value = initial_value ? strdup(initial_value) : NULL;
    }
    return state;
}

IRStateDefinition* ir_metadata_add_state_bool(IRMetadata* metadata, uint32_t id,
                                               const char* name, bool initial_value) {
    IRStateDefinition* state = add_state_internal(metadata, id, name, IR_STATE_TYPE_BOOL);
    if (state) {
        state->initial_value.bool_value = initial_value;
    }
    return state;
}

// ============================================================================
// Host Function Declaration Management
// ============================================================================

IRHostFunctionDeclaration* ir_metadata_add_host_function(IRMetadata* metadata, uint32_t id,
                                                          const char* name, const char* signature,
                                                          bool required) {
    if (!metadata) return NULL;

    // Expand if needed
    if (metadata->host_function_count >= metadata->host_function_capacity) {
        metadata->host_function_capacity *= 2;
        metadata->host_functions = (IRHostFunctionDeclaration*)realloc(metadata->host_functions,
                                   metadata->host_function_capacity * sizeof(IRHostFunctionDeclaration));
    }

    IRHostFunctionDeclaration* host_func = &metadata->host_functions[metadata->host_function_count++];
    memset(host_func, 0, sizeof(IRHostFunctionDeclaration));

    host_func->id = id;
    host_func->name = name ? strdup(name) : NULL;
    host_func->signature = signature ? strdup(signature) : NULL;
    host_func->required = required;

    return host_func;
}

// ============================================================================
// Lookup
// ============================================================================

IRBytecodeFunction* ir_metadata_find_function(IRMetadata* metadata, uint32_t id) {
    if (!metadata) return NULL;

    for (int i = 0; i < metadata->function_count; i++) {
        if (metadata->functions[i].id == id) {
            return &metadata->functions[i];
        }
    }
    return NULL;
}

IRStateDefinition* ir_metadata_find_state(IRMetadata* metadata, uint32_t id) {
    if (!metadata) return NULL;

    for (int i = 0; i < metadata->state_count; i++) {
        if (metadata->states[i].id == id) {
            return &metadata->states[i];
        }
    }
    return NULL;
}

IRHostFunctionDeclaration* ir_metadata_find_host_function(IRMetadata* metadata, uint32_t id) {
    if (!metadata) return NULL;

    for (int i = 0; i < metadata->host_function_count; i++) {
        if (metadata->host_functions[i].id == id) {
            return &metadata->host_functions[i];
        }
    }
    return NULL;
}
