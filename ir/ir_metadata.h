#ifndef IR_METADATA_H
#define IR_METADATA_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_vm.h"

// Forward declarations
typedef struct IRMetadata IRMetadata;

// IR v2.1 Metadata Structures
// These structures hold metadata for bytecode functions, reactive states,
// and host function declarations for serialization to .kir files

// ============================================================================
// Bytecode Instruction
// ============================================================================

typedef struct {
    IROpcode opcode;
    union {
        int64_t int_arg;
        double float_arg;
        char* string_arg;
        bool bool_arg;
        uint32_t id_arg;
        int32_t offset_arg;
    } arg;
    bool has_arg;
} IRBytecodeInstruction;

// ============================================================================
// Bytecode Function
// ============================================================================

typedef struct {
    uint32_t id;
    char* name;
    IRBytecodeInstruction* instructions;
    int instruction_count;
    int instruction_capacity;
} IRBytecodeFunction;

// ============================================================================
// Reactive State Definition
// ============================================================================

typedef enum {
    IR_STATE_TYPE_INT,
    IR_STATE_TYPE_FLOAT,
    IR_STATE_TYPE_STRING,
    IR_STATE_TYPE_BOOL
} IRStateType;

typedef struct {
    uint32_t id;
    char* name;
    IRStateType type;
    union {
        int64_t int_value;
        double float_value;
        char* string_value;
        bool bool_value;
    } initial_value;
} IRStateDefinition;

// ============================================================================
// Host Function Declaration
// ============================================================================

typedef struct {
    uint32_t id;
    char* name;
    char* signature;  // e.g., "(int, string) -> bool"
    bool required;    // If true, app fails if not available
} IRHostFunctionDeclaration;

// ============================================================================
// IR Metadata Container
// ============================================================================

struct IRMetadata {
    // Version
    char* version;  // e.g., "2.1"

    // Bytecode functions
    IRBytecodeFunction* functions;
    int function_count;
    int function_capacity;

    // Reactive states
    IRStateDefinition* states;
    int state_count;
    int state_capacity;

    // Host functions
    IRHostFunctionDeclaration* host_functions;
    int host_function_count;
    int host_function_capacity;
};

// ============================================================================
// Lifecycle
// ============================================================================

IRMetadata* ir_metadata_create(void);
void ir_metadata_destroy(IRMetadata* metadata);

// ============================================================================
// Bytecode Function Management
// ============================================================================

IRBytecodeFunction* ir_metadata_add_function(IRMetadata* metadata, uint32_t id, const char* name);
bool ir_function_add_instruction(IRBytecodeFunction* func, IRBytecodeInstruction instr);

// Helper to create instructions
IRBytecodeInstruction ir_instr_simple(IROpcode opcode);
IRBytecodeInstruction ir_instr_int(IROpcode opcode, int64_t arg);
IRBytecodeInstruction ir_instr_float(IROpcode opcode, double arg);
IRBytecodeInstruction ir_instr_string(IROpcode opcode, const char* arg);
IRBytecodeInstruction ir_instr_bool(IROpcode opcode, bool arg);
IRBytecodeInstruction ir_instr_id(IROpcode opcode, uint32_t arg);
IRBytecodeInstruction ir_instr_offset(IROpcode opcode, int32_t arg);

// ============================================================================
// State Definition Management
// ============================================================================

IRStateDefinition* ir_metadata_add_state_int(IRMetadata* metadata, uint32_t id,
                                              const char* name, int64_t initial_value);
IRStateDefinition* ir_metadata_add_state_float(IRMetadata* metadata, uint32_t id,
                                                const char* name, double initial_value);
IRStateDefinition* ir_metadata_add_state_string(IRMetadata* metadata, uint32_t id,
                                                 const char* name, const char* initial_value);
IRStateDefinition* ir_metadata_add_state_bool(IRMetadata* metadata, uint32_t id,
                                               const char* name, bool initial_value);

// ============================================================================
// Host Function Declaration Management
// ============================================================================

IRHostFunctionDeclaration* ir_metadata_add_host_function(IRMetadata* metadata, uint32_t id,
                                                          const char* name, const char* signature,
                                                          bool required);

// ============================================================================
// Lookup
// ============================================================================

IRBytecodeFunction* ir_metadata_find_function(IRMetadata* metadata, uint32_t id);
IRStateDefinition* ir_metadata_find_state(IRMetadata* metadata, uint32_t id);
IRHostFunctionDeclaration* ir_metadata_find_host_function(IRMetadata* metadata, uint32_t id);

#endif // IR_METADATA_H
