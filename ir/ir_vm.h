#ifndef IR_VM_H
#define IR_VM_H

#include <stdint.h>
#include <stdbool.h>

// Bytecode VM for Kryon IR
// Minimal prototype with 4 basic opcodes to validate architecture

// Opcode definitions (full instruction set)
typedef enum {
    // Stack operations (0x01-0x0F)
    OP_PUSH_INT = 0x01,     // Push integer constant onto stack
    OP_PUSH_FLOAT = 0x02,   // Push float constant onto stack
    OP_PUSH_STRING = 0x03,  // Push string constant onto stack
    OP_PUSH_BOOL = 0x04,    // Push boolean constant onto stack
    OP_POP = 0x05,          // Pop top value from stack
    OP_DUP = 0x06,          // Duplicate top of stack

    // Arithmetic operations (0x10-0x1F)
    OP_ADD = 0x10,          // Pop 2 values, push sum
    OP_SUB = 0x11,          // Pop 2 values, push difference
    OP_MUL = 0x12,          // Pop 2 values, push product
    OP_DIV = 0x13,          // Pop 2 values, push quotient
    OP_MOD = 0x14,          // Pop 2 values, push remainder
    OP_NEG = 0x15,          // Negate top of stack

    // Comparison operations (0x20-0x2F)
    OP_EQ = 0x20,           // Pop 2 values, push equality result
    OP_NE = 0x21,           // Pop 2 values, push inequality result
    OP_LT = 0x22,           // Pop 2 values, push less-than result
    OP_GT = 0x23,           // Pop 2 values, push greater-than result
    OP_LE = 0x24,           // Pop 2 values, push less-equal result
    OP_GE = 0x25,           // Pop 2 values, push greater-equal result

    // Logical operations (0x30-0x3F)
    OP_AND = 0x30,          // Pop 2 values, push logical AND
    OP_OR = 0x31,           // Pop 2 values, push logical OR
    OP_NOT = 0x32,          // Pop 1 value, push logical NOT

    // String operations (0x40-0x4F)
    OP_CONCAT = 0x40,       // Pop 2 strings, push concatenation

    // State management (0x50-0x5F)
    OP_GET_STATE = 0x50,    // Push state value onto stack
    OP_SET_STATE = 0x51,    // Pop value, store in state
    OP_GET_LOCAL = 0x52,    // Push local variable onto stack
    OP_SET_LOCAL = 0x53,    // Pop value, store in local variable

    // Control flow (0x60-0x6F)
    OP_JUMP = 0x60,         // Unconditional jump to offset
    OP_JUMP_IF_FALSE = 0x61, // Jump if top of stack is false
    OP_CALL = 0x62,         // Call internal function
    OP_RETURN = 0x63,       // Return from function

    // Host interaction (0x70-0x7F)
    OP_CALL_HOST = 0x70,    // Call host-provided function
    OP_GET_PROP = 0x71,     // Get component property
    OP_SET_PROP = 0x72,     // Set component property

    // System (0xFF)
    OP_HALT = 0xFF          // Stop execution
} IROpcode;

// Value types
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL
} IRValueType;

// Stack value (tagged union)
typedef struct {
    IRValueType type;
    union {
        int64_t i;
        double f;
        char* s;
        bool b;
    } as;
} IRValue;

// VM Stack and storage limits
#define VM_STACK_MAX 256
#define VM_STATE_MAX 64
#define VM_LOCAL_MAX 32
#define VM_HOST_FUNC_MAX 128

typedef struct {
    IRValue stack[VM_STACK_MAX];
    int sp;  // Stack pointer (-1 = empty)
} IRStack;

// State storage (reactive states - key-value pairs)
typedef struct {
    uint32_t id;     // State ID
    IRValue value;
} IRStateEntry;

typedef struct {
    IRStateEntry entries[VM_STATE_MAX];
    int count;
} IRStateTable;

// Local variable storage (function locals)
typedef struct {
    IRValue locals[VM_LOCAL_MAX];
    int count;
} IRLocalTable;

// Forward declaration
typedef struct IRVM IRVM;

// Host function callback type
typedef void (*IRHostFunctionCallback)(IRVM* vm, void* user_data);

// Host function registry entry
typedef struct {
    uint32_t id;
    const char* name;
    IRHostFunctionCallback callback;
    void* user_data;
} IRHostFunction;

typedef struct {
    IRHostFunction functions[VM_HOST_FUNC_MAX];
    int count;
} IRHostRegistry;

// VM execution context
struct IRVM {
    const uint8_t* code;      // Bytecode buffer
    int pc;                    // Program counter
    int code_size;             // Bytecode length
    IRStack stack;             // Execution stack
    IRStateTable state;        // Reactive state storage
    IRLocalTable locals;       // Local variables
    IRHostRegistry host_funcs; // Host function registry
    bool halted;               // Execution stopped
    char error[256];           // Error message
};

// VM lifecycle
IRVM* ir_vm_create(void);
void ir_vm_destroy(IRVM* vm);

// Execution
bool ir_vm_execute(IRVM* vm, const uint8_t* bytecode, int size);
bool ir_vm_step(IRVM* vm);  // Execute one instruction

// Stack operations
void ir_vm_push_int(IRVM* vm, int64_t value);
void ir_vm_push_float(IRVM* vm, double value);
void ir_vm_push_string(IRVM* vm, const char* value);
void ir_vm_push_bool(IRVM* vm, bool value);
void ir_vm_push(IRVM* vm, IRValue value);
bool ir_vm_pop(IRVM* vm, IRValue* out);
IRValue* ir_vm_peek(IRVM* vm);

// State operations
bool ir_vm_get_state(IRVM* vm, uint32_t state_id, IRValue* out);
bool ir_vm_set_state(IRVM* vm, uint32_t state_id, IRValue value);

// Local variable operations
bool ir_vm_get_local(IRVM* vm, uint32_t local_id, IRValue* out);
bool ir_vm_set_local(IRVM* vm, uint32_t local_id, IRValue value);

// Host function registry
bool ir_vm_register_host_function(IRVM* vm, uint32_t id, const char* name,
                                   IRHostFunctionCallback callback, void* user_data);
bool ir_vm_call_host_function(IRVM* vm, uint32_t func_id);

// Debugging
void ir_vm_print_stack(IRVM* vm);
void ir_vm_print_state(IRVM* vm);
const char* ir_vm_opcode_name(IROpcode op);
const char* ir_vm_value_type_name(IRValueType type);

#endif // IR_VM_H
