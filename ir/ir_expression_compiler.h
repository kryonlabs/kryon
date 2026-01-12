#ifndef IR_EXPRESSION_COMPILER_H
#define IR_EXPRESSION_COMPILER_H

#include "ir_expression.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// BYTECODE OPCODES
// ============================================================================
// Bytecode opcodes for expression evaluation
// These form the instruction set for the stack-based expression VM

typedef enum {
    // No-operation
    OP_NOP = 0,

    // Stack manipulation
    OP_PUSH_INT,             // Push integer literal (operand3: value)
    OP_PUSH_FLOAT,           // Push float literal (operand2: string pool index, encoded as string)
    OP_PUSH_STRING,          // Push string literal (operand2: string pool index)
    OP_PUSH_BOOL,            // Push boolean literal (operand1: 0/1)
    OP_PUSH_NULL,            // Push null value
    OP_DUP,                  // Duplicate top of stack
    OP_POP,                  // Pop and discard top value
    OP_SWAP,                 // Swap top two values

    // Variable access
    OP_LOAD_VAR,             // Load variable by name (operand2: pool index)
    OP_LOAD_VAR_LOCAL,       // Load from local scope
    OP_LOAD_VAR_GLOBAL,      // Load from global scope
    OP_STORE_VAR,            // Store to variable

    // Property access
    OP_GET_PROP,             // Get static property (operand2: property index)
    OP_GET_PROP_COMPUTED,    // Get computed property (pop key from stack)
    OP_SET_PROP,             // Set property value

    // Method and function calls
    OP_CALL_METHOD,          // Call method (operand2: method index, operand3: arg count)
    OP_CALL_BUILTIN,         // Call builtin function (operand2: builtin index, operand3: arg count)
    OP_CALL_FUNCTION,        // Call regular function

    // Binary operations
    OP_ADD,                  // Addition (number or string concatenation)
    OP_SUB,                  // Subtraction
    OP_MUL,                  // Multiplication
    OP_DIV,                  // Division
    OP_MOD,                  // Modulo
    OP_CONCAT,               // String concatenation

    // Comparison operations
    OP_EQ,                   // Equality
    OP_NEQ,                  // Inequality
    OP_LT,                   // Less than
    OP_LTE,                  // Less than or equal
    OP_GT,                   // Greater than
    OP_GTE,                  // Greater than or equal

    // Logical operations
    OP_AND,                  // Logical AND (short-circuit)
    OP_OR,                   // Logical OR (short-circuit)
    OP_NOT,                  // Logical NOT

    // Unary operations
    OP_NEGATE,               // Arithmetic negation

    // Control flow
    OP_JUMP,                 // Unconditional jump (operand3: offset)
    OP_JUMP_IF_FALSE,        // Jump if top of stack is false (operand3: offset)
    OP_JUMP_IF_TRUE,         // Jump if top of stack is true (operand3: offset)

    // Array operations
    OP_GET_INDEX,            // Array indexing: arr[index]

    // Special
    OP_HALT,                 // Stop execution

    // Count of opcodes (for validation)
    OP_COUNT
} IROpcode;

// Opcode name for debugging
const char* ir_opcode_name(IROpcode opcode);

// ============================================================================
// VALUE TYPES (Enhanced)
// ============================================================================

// Extended value type enum
typedef enum {
    VAR_TYPE_NULL = 0,
    VAR_TYPE_INT,
    VAR_TYPE_FLOAT,
    VAR_TYPE_STRING,
    VAR_TYPE_BOOL,
    VAR_TYPE_ARRAY,
    VAR_TYPE_OBJECT,        // NEW: Object/map type
    VAR_TYPE_FUNCTION,      // Reserved for future
} VarType;

// Forward declarations
typedef struct IRObject IRObject;
typedef struct IRArray IRArray;

// Extended value union - must be defined before being used
typedef struct IRValue {
    VarType type;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        char* string_val;
        IRArray* array_val;
        IRObject* object_val;
    };
} IRValue;

// Object entry for map-like objects
typedef struct {
    char* key;
    IRValue value;
} IRObjectEntry;

// Object structure (map/dictionary)
struct IRObject {
    IRObjectEntry* entries;
    uint32_t count;
    uint32_t capacity;
};

// Array structure (dynamic array)
struct IRArray {
    IRValue* items;
    uint32_t count;
    uint32_t capacity;
};

// ============================================================================
// BYTECODE INSTRUCTION
// ============================================================================

// Single bytecode instruction (fixed 8-byte size for cache efficiency)
// Layout: [opcode:1][operand1:1][operand2:2][operand3:4]
typedef struct {
    uint8_t opcode;      // 1 byte - use uint8_t to ensure 1-byte size
    uint8_t operand1;    // 1 byte (small operand or register)
    uint16_t operand2;   // 2 bytes (index or offset)
    int32_t operand3;    // 4 bytes (large immediate value or offset)
} IRInstruction;

// Note: Instruction size should be 8 bytes for cache efficiency
// This is validated at compile time if the compiler supports _Static_assert
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(IRInstruction) == 8, "IRInstruction must be exactly 8 bytes");
#endif
#endif

// ============================================================================
// COMPILED EXPRESSION
// ============================================================================

// Compiled expression bytecode
typedef struct {
    IRInstruction* code;       // Bytecode array
    uint32_t code_size;        // Number of instructions
    uint32_t code_capacity;    // Allocated capacity

    // String pool for identifiers and literals
    char** string_pool;
    uint32_t string_pool_count;
    uint32_t string_pool_capacity;

    // Large integer pool (for values that don't fit in operand3)
    int64_t* int_pool;
    uint32_t int_pool_count;
    uint32_t int_pool_capacity;

    // Metadata
    uint32_t max_stack_depth;  // Maximum stack usage (for allocation)
    uint32_t temp_count;       // Number of temporary values needed

    // Debug info (optional, included in debug builds)
    const char* source_expr;   // Original expression string (for debugging)
    uint32_t* debug_lines;     // Line numbers per instruction
    bool has_debug_info;
} IRCompiledExpr;

// ============================================================================
// VALUE CONSTRUCTION HELPERS
// ============================================================================

// Create null value
static inline IRValue ir_value_null(void) {
    IRValue v;
    v.type = VAR_TYPE_NULL;
    return v;
}

// Create integer value
static inline IRValue ir_value_int(int64_t value) {
    IRValue v;
    v.type = VAR_TYPE_INT;
    v.int_val = value;
    return v;
}

// Create float value
static inline IRValue ir_value_float(double value) {
    IRValue v;
    v.type = VAR_TYPE_FLOAT;
    v.float_val = value;
    return v;
}

// Create string value (copies the string)
IRValue ir_value_string(const char* value);

// Create boolean value
static inline IRValue ir_value_bool(bool value) {
    IRValue v;
    v.type = VAR_TYPE_BOOL;
    v.bool_val = value;
    return v;
}

// Create empty array value
IRValue ir_value_array_empty(void);

// Create empty object value
IRValue ir_value_object_empty(void);

// Create object value from existing object
static inline IRValue ir_value_object(struct IRObject* obj) {
    IRValue v;
    v.type = VAR_TYPE_OBJECT;
    v.object_val = obj;
    return v;
}

// Create array value from existing array
static inline IRValue ir_value_array(struct IRArray* arr) {
    IRValue v;
    v.type = VAR_TYPE_ARRAY;
    v.array_val = arr;
    return v;
}

// Copy a value (deep copy for strings, arrays, objects)
IRValue ir_value_copy(const IRValue* value);

// Free a value's resources
void ir_value_free(IRValue* value);

// Get type name as string
const char* ir_value_type_name(VarType type);

// Convert value to string representation
char* ir_value_to_string(const IRValue* value);

// Compare two values for equality
bool ir_value_equals(const IRValue* a, const IRValue* b);

// Check if value is truthy
bool ir_value_is_truthy(const IRValue* value);

// Check if value is falsey
static inline bool ir_value_is_falsey(const IRValue* value) {
    return !ir_value_is_truthy(value);
}

// ============================================================================
// OBJECT HELPERS
// ============================================================================

// Create a new object with specified initial capacity
struct IRObject* ir_object_create(uint32_t initial_capacity);

// Free an object
void ir_object_free(struct IRObject* obj);

// Get value from object by key
// Returns null value if key not found
IRValue ir_object_get(struct IRObject* obj, const char* key);

// Set value in object by key
// Takes ownership of the value (makes a copy)
void ir_object_set(struct IRObject* obj, const char* key, IRValue value);

// Check if object has key
bool ir_object_has(struct IRObject* obj, const char* key);

// Remove key from object
bool ir_object_delete(struct IRObject* obj, const char* key);

// Get all keys from object
char** ir_object_keys(struct IRObject* obj, uint32_t* out_count);

// Clone an object
struct IRObject* ir_object_clone(struct IRObject* src);

// ============================================================================
// ARRAY HELPERS
// ============================================================================

// Create a new array with specified initial capacity
struct IRArray* ir_array_create(uint32_t initial_capacity);

// Free an array
void ir_array_free(struct IRArray* arr);

// Push value to end of array
void ir_array_push(struct IRArray* arr, IRValue value);

// Pop value from end of array
IRValue ir_array_pop(struct IRArray* arr);

// Get value at index
IRValue ir_array_get(struct IRArray* arr, uint32_t index);

// Set value at index
void ir_array_set(struct IRArray* arr, uint32_t index, IRValue value);

// Get array length
static inline uint32_t ir_array_length(struct IRArray* arr) {
    return arr ? arr->count : 0;
}

// Clone an array
struct IRArray* ir_array_clone(struct IRArray* src);

// ============================================================================
// COMPILER FUNCTIONS
// ============================================================================

// Forward declaration of executor context
typedef struct IRExecutorContext IRExecutorContext;

// Compile expression AST to bytecode
// Returns a compiled expression that can be evaluated multiple times
IRCompiledExpr* ir_expr_compile(IRExpression* expr);

// Free compiled expression
void ir_expr_compiled_free(IRCompiledExpr* compiled);

// Evaluate compiled expression and return result
// The executor context provides variable values and other runtime state
IRValue ir_expr_eval(IRCompiledExpr* compiled, IRExecutorContext* executor, uint32_t instance_id);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// String hash (FNV-1a) for use in caches and lookups
uint64_t ir_hash_string(const char* str);

// Combine two hashes (for composite keys)
static inline uint64_t ir_hash_combine(uint64_t a, uint64_t b) {
    return a ^ (b + 0x9e3779b9ull + (a << 6) + (a >> 2));
}

// Get next power of 2
uint32_t ir_next_power_of_2(uint32_t value);

// ============================================================================
// OPTIMIZATION FUNCTIONS
// ============================================================================

// Optimize expression AST before compilation
// Returns a new optimized expression (caller should free the original)
IRExpression* ir_expr_optimize(IRExpression* expr);

// Constant folding: evaluate constant expressions at compile time
// Returns a new expression with constants folded
IRExpression* ir_expr_const_fold(IRExpression* expr);

// Dead code elimination: remove unreachable code
// This operates on the bytecode level
void ir_bytecode_dce(IRCompiledExpr* compiled);

// ============================================================================
// DISASSEMBLY (for debugging)
// ============================================================================

// Disassemble bytecode to human-readable format
// Returns a string that caller must free
char* ir_bytecode_disassemble(IRCompiledExpr* compiled);

#endif // IR_EXPRESSION_COMPILER_H
