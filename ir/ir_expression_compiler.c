#define _POSIX_C_SOURCE 200809L
#include "ir_expression_compiler.h"
#include "ir_builtin_registry.h"
#include "ir_log.h"
#include "../third_party/cJSON/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// Forward declarations for executor types (avoid including ir_executor.h due to type conflicts)
typedef struct IRExecutorContext IRExecutorContext;

// ============================================================================
// OPCODE NAMES
// ============================================================================

const char* ir_opcode_name(IROpcode opcode) {
    switch (opcode) {
        case OP_NOP: return "NOP";
        case OP_PUSH_INT: return "PUSH_INT";
        case OP_PUSH_FLOAT: return "PUSH_FLOAT";
        case OP_PUSH_STRING: return "PUSH_STRING";
        case OP_PUSH_BOOL: return "PUSH_BOOL";
        case OP_PUSH_NULL: return "PUSH_NULL";
        case OP_DUP: return "DUP";
        case OP_POP: return "POP";
        case OP_SWAP: return "SWAP";
        case OP_LOAD_VAR: return "LOAD_VAR";
        case OP_LOAD_VAR_LOCAL: return "LOAD_VAR_LOCAL";
        case OP_LOAD_VAR_GLOBAL: return "LOAD_VAR_GLOBAL";
        case OP_STORE_VAR: return "STORE_VAR";
        case OP_GET_PROP: return "GET_PROP";
        case OP_GET_PROP_COMPUTED: return "GET_PROP_COMPUTED";
        case OP_SET_PROP: return "SET_PROP";
        case OP_CALL_METHOD: return "CALL_METHOD";
        case OP_CALL_BUILTIN: return "CALL_BUILTIN";
        case OP_CALL_FUNCTION: return "CALL_FUNCTION";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_CONCAT: return "CONCAT";
        case OP_EQ: return "EQ";
        case OP_NEQ: return "NEQ";
        case OP_LT: return "LT";
        case OP_LTE: return "LTE";
        case OP_GT: return "GT";
        case OP_GTE: return "GTE";
        case OP_AND: return "AND";
        case OP_OR: return "OR";
        case OP_NOT: return "NOT";
        case OP_NEGATE: return "NEGATE";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OP_GET_INDEX: return "GET_INDEX";
        case OP_HALT: return "HALT";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// VALUE CONSTRUCTION
// ============================================================================

IRValue ir_value_string(const char* value) {
    IRValue v;
    v.type = VAR_TYPE_STRING;
    v.string_val = value ? strdup(value) : NULL;
    return v;
}

IRValue ir_value_array_empty(void) {
    IRValue v;
    v.type = VAR_TYPE_ARRAY;
    v.array_val = ir_array_create(4);
    return v;
}

IRValue ir_value_object_empty(void) {
    IRValue v;
    v.type = VAR_TYPE_OBJECT;
    v.object_val = ir_object_create(4);
    return v;
}

// ============================================================================
// VALUE OPERATIONS
// ============================================================================

IRValue ir_value_copy(const IRValue* value) {
    if (!value) return ir_value_null();

    IRValue result;
    result.type = value->type;

    switch (value->type) {
        case VAR_TYPE_NULL:
            break;

        case VAR_TYPE_INT:
            result.int_val = value->int_val;
            break;

        case VAR_TYPE_FLOAT:
            result.float_val = value->float_val;
            break;

        case VAR_TYPE_BOOL:
            result.bool_val = value->bool_val;
            break;

        case VAR_TYPE_STRING:
            result.string_val = value->string_val ? strdup(value->string_val) : NULL;
            break;

        case VAR_TYPE_ARRAY:
            if (value->array_val) {
                result.array_val = ir_array_clone(value->array_val);
            } else {
                result.array_val = NULL;
            }
            break;

        case VAR_TYPE_OBJECT:
            if (value->object_val) {
                result.object_val = ir_object_clone(value->object_val);
            } else {
                result.object_val = NULL;
            }
            break;

        default:
            result.type = VAR_TYPE_NULL;
            break;
    }

    return result;
}

void ir_value_free(IRValue* value) {
    if (!value) return;

    switch (value->type) {
        case VAR_TYPE_STRING:
            free(value->string_val);
            value->string_val = NULL;
            break;

        case VAR_TYPE_ARRAY:
            ir_array_free(value->array_val);
            value->array_val = NULL;
            break;

        case VAR_TYPE_OBJECT:
            ir_object_free(value->object_val);
            value->object_val = NULL;
            break;

        default:
            break;
    }

    value->type = VAR_TYPE_NULL;
}

const char* ir_value_type_name(VarType type) {
    switch (type) {
        case VAR_TYPE_NULL: return "null";
        case VAR_TYPE_INT: return "int";
        case VAR_TYPE_FLOAT: return "float";
        case VAR_TYPE_STRING: return "string";
        case VAR_TYPE_BOOL: return "bool";
        case VAR_TYPE_ARRAY: return "array";
        case VAR_TYPE_OBJECT: return "object";
        case VAR_TYPE_FUNCTION: return "function";
        default: return "unknown";
    }
}

char* ir_value_to_string(const IRValue* value) {
    if (!value) return strdup("null");

    static char buffer[256];

    switch (value->type) {
        case VAR_TYPE_NULL:
            return strdup("null");

        case VAR_TYPE_INT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int_val);
            return strdup(buffer);

        case VAR_TYPE_FLOAT:
            snprintf(buffer, sizeof(buffer), "%g", value->float_val);
            return strdup(buffer);

        case VAR_TYPE_BOOL:
            return strdup(value->bool_val ? "true" : "false");

        case VAR_TYPE_STRING:
            return strdup(value->string_val ? value->string_val : "");

        case VAR_TYPE_ARRAY:
            snprintf(buffer, sizeof(buffer), "[array with %u items]",
                     value->array_val ? value->array_val->count : 0);
            return strdup(buffer);

        case VAR_TYPE_OBJECT:
            snprintf(buffer, sizeof(buffer), "[object with %u entries]",
                     value->object_val ? value->object_val->count : 0);
            return strdup(buffer);

        default:
            return strdup("unknown");
    }
}

bool ir_value_equals(const IRValue* a, const IRValue* b) {
    if (!a || !b) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case VAR_TYPE_NULL:
            return true;

        case VAR_TYPE_INT:
            return a->int_val == b->int_val;

        case VAR_TYPE_FLOAT:
            return a->float_val == b->float_val;

        case VAR_TYPE_BOOL:
            return a->bool_val == b->bool_val;

        case VAR_TYPE_STRING:
            if (!a->string_val && !b->string_val) return true;
            if (!a->string_val || !b->string_val) return false;
            return strcmp(a->string_val, b->string_val) == 0;

        case VAR_TYPE_ARRAY:
            // Array equality: same length, same elements in order
            if (!a->array_val && !b->array_val) return true;
            if (!a->array_val || !b->array_val) return false;
            if (a->array_val->count != b->array_val->count) return false;
            for (uint32_t i = 0; i < a->array_val->count; i++) {
                if (!ir_value_equals(&a->array_val->items[i], &b->array_val->items[i])) {
                    return false;
                }
            }
            return true;

        case VAR_TYPE_OBJECT:
            // Object equality: same keys, same values
            if (!a->object_val && !b->object_val) return true;
            if (!a->object_val || !b->object_val) return false;
            if (a->object_val->count != b->object_val->count) return false;
            for (uint32_t i = 0; i < a->object_val->count; i++) {
                const char* key = a->object_val->entries[i].key;
                IRValue b_val = ir_object_get(b->object_val, key);
                if (!ir_value_equals(&a->object_val->entries[i].value, &b_val)) {
                    ir_value_free(&b_val);
                    return false;
                }
                ir_value_free(&b_val);
            }
            return true;

        default:
            return false;
    }
}

bool ir_value_is_truthy(const IRValue* value) {
    if (!value) return false;

    switch (value->type) {
        case VAR_TYPE_NULL:
            return false;

        case VAR_TYPE_INT:
            return value->int_val != 0;

        case VAR_TYPE_FLOAT:
            return value->float_val != 0.0;

        case VAR_TYPE_BOOL:
            return value->bool_val;

        case VAR_TYPE_STRING:
            return value->string_val != NULL && strlen(value->string_val) > 0;

        case VAR_TYPE_ARRAY:
            return value->array_val != NULL && value->array_val->count > 0;

        case VAR_TYPE_OBJECT:
            return value->object_val != NULL && value->object_val->count > 0;

        default:
            return false;
    }
}

// ============================================================================
// OBJECT HELPERS
// ============================================================================

IRObject* ir_object_create(uint32_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 4;

    IRObject* obj = (IRObject*)calloc(1, sizeof(IRObject));
    if (!obj) return NULL;

    obj->entries = (IRObjectEntry*)calloc(initial_capacity, sizeof(IRObjectEntry));
    if (!obj->entries) {
        free(obj);
        return NULL;
    }

    obj->capacity = initial_capacity;
    obj->count = 0;

    return obj;
}

void ir_object_free(IRObject* obj) {
    if (!obj) return;

    // Free all entries
    for (uint32_t i = 0; i < obj->count; i++) {
        free(obj->entries[i].key);
        ir_value_free(&obj->entries[i].value);
    }

    free(obj->entries);
    free(obj);
}

IRValue ir_object_get(IRObject* obj, const char* key) {
    if (!obj || !key) return ir_value_null();

    for (uint32_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            return ir_value_copy(&obj->entries[i].value);
        }
    }

    return ir_value_null();
}

void ir_object_set(IRObject* obj, const char* key, IRValue value) {
    if (!obj || !key) return;

    // Check if key already exists
    for (uint32_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            // Free old value
            ir_value_free(&obj->entries[i].value);
            // Set new value
            obj->entries[i].value = value;
            return;
        }
    }

    // Need to add new entry - check capacity
    if (obj->count >= obj->capacity) {
        uint32_t new_capacity = obj->capacity * 2;
        IRObjectEntry* new_entries = (IRObjectEntry*)realloc(
            obj->entries,
            new_capacity * sizeof(IRObjectEntry)
        );
        if (!new_entries) return;

        obj->entries = new_entries;
        obj->capacity = new_capacity;
    }

    // Add new entry
    obj->entries[obj->count].key = strdup(key);
    obj->entries[obj->count].value = value;
    obj->count++;
}

bool ir_object_has(IRObject* obj, const char* key) {
    if (!obj || !key) return false;

    for (uint32_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            return true;
        }
    }

    return false;
}

bool ir_object_delete(IRObject* obj, const char* key) {
    if (!obj || !key) return false;

    for (uint32_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            // Free this entry
            free(obj->entries[i].key);
            ir_value_free(&obj->entries[i].value);

            // Shift remaining entries
            for (uint32_t j = i; j < obj->count - 1; j++) {
                obj->entries[j] = obj->entries[j + 1];
            }

            obj->count--;
            return true;
        }
    }

    return false;
}

char** ir_object_keys(IRObject* obj, uint32_t* out_count) {
    if (!obj) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    char** keys = (char**)calloc(obj->count, sizeof(char*));
    if (!keys) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    for (uint32_t i = 0; i < obj->count; i++) {
        keys[i] = strdup(obj->entries[i].key);
    }

    if (out_count) *out_count = obj->count;
    return keys;
}

IRObject* ir_object_clone(IRObject* src) {
    if (!src) return NULL;

    IRObject* clone = ir_object_create(src->count);
    if (!clone) return NULL;

    for (uint32_t i = 0; i < src->count; i++) {
        IRValue value_copy = ir_value_copy(&src->entries[i].value);
        ir_object_set(clone, src->entries[i].key, value_copy);
    }

    return clone;
}

// ============================================================================
// ARRAY HELPERS
// ============================================================================

IRArray* ir_array_create(uint32_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = 4;

    IRArray* arr = (IRArray*)calloc(1, sizeof(IRArray));
    if (!arr) return NULL;

    arr->items = (IRValue*)calloc(initial_capacity, sizeof(IRValue));
    if (!arr->items) {
        free(arr);
        return NULL;
    }

    arr->capacity = initial_capacity;
    arr->count = 0;

    return arr;
}

void ir_array_free(IRArray* arr) {
    if (!arr) return;

    // Free all items
    for (uint32_t i = 0; i < arr->count; i++) {
        ir_value_free(&arr->items[i]);
    }

    free(arr->items);
    free(arr);
}

void ir_array_push(IRArray* arr, IRValue value) {
    if (!arr) return;

    // Check capacity
    if (arr->count >= arr->capacity) {
        uint32_t new_capacity = arr->capacity * 2;
        IRValue* new_items = (IRValue*)realloc(
            arr->items,
            new_capacity * sizeof(IRValue)
        );
        if (!new_items) return;

        arr->items = new_items;
        arr->capacity = new_capacity;
    }

    // Add value
    arr->items[arr->count++] = value;
}

IRValue ir_array_pop(IRArray* arr) {
    if (!arr || arr->count == 0) {
        return ir_value_null();
    }

    IRValue result = arr->items[arr->count - 1];
    arr->count--;
    return result;
}

IRValue ir_array_get(IRArray* arr, uint32_t index) {
    if (!arr || index >= arr->count) {
        return ir_value_null();
    }

    return ir_value_copy(&arr->items[index]);
}

void ir_array_set(IRArray* arr, uint32_t index, IRValue value) {
    if (!arr || index >= arr->count) {
        ir_value_free(&value);
        return;
    }

    // Free old value
    ir_value_free(&arr->items[index]);

    // Set new value
    arr->items[index] = value;
}

IRArray* ir_array_clone(IRArray* src) {
    if (!src) return NULL;

    IRArray* clone = ir_array_create(src->count);
    if (!clone) return NULL;

    for (uint32_t i = 0; i < src->count; i++) {
        IRValue value_copy = ir_value_copy(&src->items[i]);
        ir_array_push(clone, value_copy);
    }

    return clone;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint64_t ir_hash_string(const char* str) {
    if (!str) return 0;

    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;

    while (*str) {
        hash ^= (uint64_t)(unsigned char)*str++;
        hash *= 1099511628211ULL;
    }

    return hash;
}

uint32_t ir_next_power_of_2(uint32_t value) {
    if (value == 0) return 1;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return value;
}

// ============================================================================
// COMPILER CONTEXT
// ============================================================================

// Internal compiler context
typedef struct {
    IRInstruction* code;        // Output bytecode buffer
    uint32_t code_size;         // Current size
    uint32_t code_capacity;     // Allocated capacity

    // String/identifier pool
    struct {
        char** strings;
        uint32_t* hashes;
        uint32_t count;
        uint32_t capacity;
    } pool;

    // Large integer pool (for values that don't fit in operand3)
    struct {
        int64_t* values;
        uint32_t count;
        uint32_t capacity;
    } int_pool;

    // Metadata
    uint32_t max_stack_depth;   // For stack allocation
    uint32_t current_stack_depth;
    uint32_t temp_count;        // Number of temporary values needed

    // Error handling
    char error_msg[256];
    bool has_error;

    // Original source for debugging
    const char* source_expr;
} IRExprCompiler;

// Create compiler instance
static IRExprCompiler* ir_expr_compiler_create(const char* source) {
    IRExprCompiler* comp = (IRExprCompiler*)calloc(1, sizeof(IRExprCompiler));
    if (!comp) return NULL;

    comp->code_capacity = 256;
    comp->code = (IRInstruction*)calloc(comp->code_capacity, sizeof(IRInstruction));
    if (!comp->code) {
        free(comp);
        return NULL;
    }

    comp->pool.capacity = 64;
    comp->pool.strings = (char**)calloc(comp->pool.capacity, sizeof(char*));
    comp->pool.hashes = (uint32_t*)calloc(comp->pool.capacity, sizeof(uint32_t));

    comp->int_pool.capacity = 16;
    comp->int_pool.values = (int64_t*)calloc(comp->int_pool.capacity, sizeof(int64_t));

    comp->source_expr = source ? strdup(source) : NULL;

    return comp;
}

// Destroy compiler instance
static void ir_expr_compiler_destroy(IRExprCompiler* comp) {
    if (!comp) return;

    // Free string pool
    for (uint32_t i = 0; i < comp->pool.count; i++) {
        free(comp->pool.strings[i]);
    }
    free(comp->pool.strings);
    free(comp->pool.hashes);

    // Free int pool
    free(comp->int_pool.values);

    // Free code
    free(comp->code);

    // Free source
    free((void*)comp->source_expr);

    free(comp);
}

// ============================================================================
// BYTECODE EMISSION
// ============================================================================

// Emit a single instruction
static uint32_t emit(IRExprCompiler* comp, IROpcode opcode,
                     uint8_t op1, uint16_t op2, int32_t op3) {
    if (comp->has_error) return 0;

    // Grow buffer if needed
    if (comp->code_size >= comp->code_capacity) {
        uint32_t new_capacity = comp->code_capacity * 2;
        IRInstruction* new_code = (IRInstruction*)realloc(
            comp->code,
            new_capacity * sizeof(IRInstruction)
        );
        if (!new_code) {
            snprintf(comp->error_msg, sizeof(comp->error_msg),
                     "Failed to allocate code buffer (size: %u)", new_capacity);
            comp->has_error = true;
            return 0;
        }
        comp->code = new_code;
        comp->code_capacity = new_capacity;
    }

    uint32_t pc = comp->code_size++;
    IRInstruction* instr = &comp->code[pc];

    instr->opcode = (uint8_t)opcode;
    instr->operand1 = op1;
    instr->operand2 = op2;
    instr->operand3 = op3;

    return pc;  // Return program counter for patching
}

// Add string to pool and return index
static uint32_t pool_add(IRExprCompiler* comp, const char* str) {
    if (!str) str = "";

    uint64_t hash = ir_hash_string(str);
    uint32_t hash32 = (uint32_t)hash;

    // Check if already exists
    for (uint32_t i = 0; i < comp->pool.count; i++) {
        if (comp->pool.hashes[i] == hash32 &&
            comp->pool.strings[i] &&
            strcmp(comp->pool.strings[i], str) == 0) {
            return i;
        }
    }

    // Grow pool if needed
    if (comp->pool.count >= comp->pool.capacity) {
        uint32_t new_capacity = comp->pool.capacity * 2;
        char** new_strings = (char**)realloc(
            comp->pool.strings,
            new_capacity * sizeof(char*)
        );
        if (!new_strings) return 0;

        uint32_t* new_hashes = (uint32_t*)realloc(
            comp->pool.hashes,
            new_capacity * sizeof(uint32_t)
        );
        if (!new_hashes) return 0;

        comp->pool.strings = new_strings;
        comp->pool.hashes = new_hashes;
        comp->pool.capacity = new_capacity;
    }

    // Add new entry
    uint32_t idx = comp->pool.count++;
    comp->pool.strings[idx] = strdup(str);
    comp->pool.hashes[idx] = hash32;
    return idx;
}

// Add large integer to pool and return index
static uint32_t int_pool_add(IRExprCompiler* comp, int64_t value) {
    // Check if already exists
    for (uint32_t i = 0; i < comp->int_pool.count; i++) {
        if (comp->int_pool.values[i] == value) {
            return i;
        }
    }

    // Grow pool if needed
    if (comp->int_pool.count >= comp->int_pool.capacity) {
        uint32_t new_capacity = comp->int_pool.capacity * 2;
        int64_t* new_values = (int64_t*)realloc(
            comp->int_pool.values,
            new_capacity * sizeof(int64_t)
        );
        if (!new_values) return 0;

        comp->int_pool.values = new_values;
        comp->int_pool.capacity = new_capacity;
    }

    // Add new entry
    uint32_t idx = comp->int_pool.count++;
    comp->int_pool.values[idx] = value;
    return idx;
}

// Patch jump instruction at pc with new offset
static void patch_jump(IRExprCompiler* comp, uint32_t pc, int32_t offset) {
    if (pc < comp->code_size) {
        comp->code[pc].operand3 = offset;
    }
}

// ============================================================================
// COMPILATION FUNCTIONS
// ============================================================================

// Forward declarations
static void compile_expr(IRExprCompiler* comp, IRExpression* expr);
static void compile_literal_int(IRExprCompiler* comp, int64_t value);
static void compile_literal_float(IRExprCompiler* comp, double value);
static void compile_literal_string(IRExprCompiler* comp, const char* value);
static void compile_var_ref(IRExprCompiler* comp, const char* name);
static void compile_member_access(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_computed_member(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_binary(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_unary(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_ternary(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_method_call(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_call(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_index_access(IRExprCompiler* comp, struct IRExpression* expr);
static void compile_property(IRExprCompiler* comp, struct IRExpression* expr);

// Main compilation dispatcher
static void compile_expr(IRExprCompiler* comp, IRExpression* expr) {
    if (!expr) {
        emit(comp, OP_PUSH_NULL, 0, 0, 0);
        return;
    }

    comp->current_stack_depth++;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            compile_literal_int(comp, expr->int_value);
            break;

        case EXPR_LITERAL_FLOAT:
            compile_literal_float(comp, expr->float_value);
            break;

        case EXPR_LITERAL_STRING:
            compile_literal_string(comp, expr->string_value);
            break;

        case EXPR_LITERAL_BOOL:
            emit(comp, OP_PUSH_BOOL, expr->bool_value ? 1 : 0, 0, 0);
            break;

        case EXPR_LITERAL_NULL:
            emit(comp, OP_PUSH_NULL, 0, 0, 0);
            break;

        case EXPR_VAR_REF:
            compile_var_ref(comp, expr->var_ref.name);
            break;

        case EXPR_MEMBER_ACCESS:
            compile_member_access(comp, expr);
            break;

        case EXPR_COMPUTED_MEMBER:
            compile_computed_member(comp, expr);
            break;

        case EXPR_METHOD_CALL:
            compile_method_call(comp, expr);
            break;

        case EXPR_CALL:
            compile_call(comp, expr);
            break;

        case EXPR_BINARY:
            compile_binary(comp, expr);
            break;

        case EXPR_UNARY:
            compile_unary(comp, expr);
            break;

        case EXPR_TERNARY:
            compile_ternary(comp, expr);
            break;

        case EXPR_GROUP:
            compile_expr(comp, expr->group.inner);
            break;

        case EXPR_INDEX:
            compile_index_access(comp, expr);
            break;

        case EXPR_PROPERTY:
            compile_property(comp, expr);
            break;

        default:
            emit(comp, OP_PUSH_NULL, 0, 0, 0);
            break;
    }

    comp->current_stack_depth--;
    if (comp->current_stack_depth > comp->max_stack_depth) {
        comp->max_stack_depth = comp->current_stack_depth;
    }
}

// Compile integer literal
static void compile_literal_int(IRExprCompiler* comp, int64_t value) {
    // Small integers fit in operand3
    if (value >= INT32_MIN && value <= INT32_MAX) {
        emit(comp, OP_PUSH_INT, 0, 0, (int32_t)value);
    } else {
        // Large integers need int pool
        uint32_t idx = int_pool_add(comp, value);
        emit(comp, OP_PUSH_INT, 0, (uint16_t)idx, 0);
    }
}

// Compile float literal
static void compile_literal_float(IRExprCompiler* comp, double value) {
    // Convert float to string and store in pool
    // Use OP_PUSH_FLOAT to differentiate from regular strings
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.15g", value);
    uint32_t idx = pool_add(comp, buffer);
    emit(comp, OP_PUSH_FLOAT, 0, (uint16_t)idx, 0);
}

// Compile string literal
static void compile_literal_string(IRExprCompiler* comp, const char* value) {
    uint32_t idx = pool_add(comp, value ? value : "");
    emit(comp, OP_PUSH_STRING, 0, (uint16_t)idx, 0);
}

// Compile variable reference
static void compile_var_ref(IRExprCompiler* comp, const char* name) {
    uint32_t idx = pool_add(comp, name);
    emit(comp, OP_LOAD_VAR, 0, (uint16_t)idx, 0);
}

// Compile old-style property access (static object name)
static void compile_property(IRExprCompiler* comp, struct IRExpression* expr) {
    // Load the object variable
    compile_var_ref(comp, expr->property.object);

    // Get the property
    uint32_t prop_idx = pool_add(comp, expr->property.field);
    emit(comp, OP_GET_PROP, 0, (uint16_t)prop_idx, 0);
}

// Compile member access (obj.prop where obj is an expression)
static void compile_member_access(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile object expression first (pushes object on stack)
    compile_expr(comp, expr->member_access.object);

    // Get property (object is on stack)
    uint32_t prop_idx = pool_add(comp, expr->member_access.property);
    emit(comp, OP_GET_PROP, 0, (uint16_t)prop_idx, 0);
}

// Compile computed member access (obj[key])
static void compile_computed_member(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile object expression
    compile_expr(comp, expr->computed_member.object);

    // Compile key expression (pushes key on stack)
    compile_expr(comp, expr->computed_member.key);

    // Get computed property (object and key are on stack)
    emit(comp, OP_GET_PROP_COMPUTED, 0, 0, 0);
}

// Compile index access (array[index])
static void compile_index_access(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile array expression
    compile_expr(comp, expr->index_access.array);

    // Compile index expression
    compile_expr(comp, expr->index_access.index);

    // Array indexing
    emit(comp, OP_GET_INDEX, 0, 0, 0);
}

// Compile method call (obj.method(args))
static void compile_method_call(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile receiver (pushes object on stack)
    compile_expr(comp, expr->method_call.receiver);

    // Compile arguments in reverse order (for correct stack order)
    for (int32_t i = (int32_t)expr->method_call.arg_count - 1; i >= 0; i--) {
        compile_expr(comp, expr->method_call.args[i]);
    }

    // Emit method call
    uint32_t method_idx = pool_add(comp, expr->method_call.method_name);
    emit(comp, OP_CALL_METHOD, 0, (uint16_t)method_idx, (int32_t)expr->method_call.arg_count);
}

// Compile function call (func(args))
static void compile_call(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile arguments in reverse order (for correct stack order)
    for (int32_t i = (int32_t)expr->call.arg_count - 1; i >= 0; i--) {
        compile_expr(comp, expr->call.args[i]);
    }

    // Determine if this is a builtin function call
    // Builtins follow the pattern: category_function (e.g., string_toUpper, array_length)
    const char* func_name = expr->call.function;
    bool is_builtin = false;

    if (func_name) {
        // Check for known builtin prefixes
        if (strncmp(func_name, "string_", 7) == 0 ||
            strncmp(func_name, "array_", 6) == 0 ||
            strncmp(func_name, "math_", 5) == 0 ||
            strncmp(func_name, "type_", 5) == 0) {
            is_builtin = true;
        }
    }

    uint32_t func_idx = pool_add(comp, expr->call.function);

    if (is_builtin) {
        emit(comp, OP_CALL_BUILTIN, 0, (uint16_t)func_idx, (int32_t)expr->call.arg_count);
    } else {
        emit(comp, OP_CALL_FUNCTION, 0, (uint16_t)func_idx, (int32_t)expr->call.arg_count);
    }
}

// Compile binary operation
static void compile_binary(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile left operand (pushes on stack)
    compile_expr(comp, expr->binary.left);

    // Compile right operand (pushes on stack)
    compile_expr(comp, expr->binary.right);

    // Emit appropriate operator
    IROpcode op;
    switch (expr->binary.op) {
        case BINARY_OP_ADD:    op = OP_ADD; break;
        case BINARY_OP_SUB:    op = OP_SUB; break;
        case BINARY_OP_MUL:    op = OP_MUL; break;
        case BINARY_OP_DIV:    op = OP_DIV; break;
        case BINARY_OP_MOD:    op = OP_MOD; break;
        case BINARY_OP_CONCAT: op = OP_CONCAT; break;
        case BINARY_OP_EQ:     op = OP_EQ; break;
        case BINARY_OP_NEQ:    op = OP_NEQ; break;
        case BINARY_OP_LT:     op = OP_LT; break;
        case BINARY_OP_LTE:    op = OP_LTE; break;
        case BINARY_OP_GT:     op = OP_GT; break;
        case BINARY_OP_GTE:    op = OP_GTE; break;
        case BINARY_OP_AND:    op = OP_AND; break;
        case BINARY_OP_OR:     op = OP_OR; break;
        default:
            op = OP_NOP;
            break;
    }

    emit(comp, op, 0, 0, 0);
}

// Compile unary operation
static void compile_unary(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile operand
    compile_expr(comp, expr->unary.operand);

    // Emit operator
    switch (expr->unary.op) {
        case UNARY_OP_NEG:
            emit(comp, OP_NEGATE, 0, 0, 0);
            break;
        case UNARY_OP_NOT:
            emit(comp, OP_NOT, 0, 0, 0);
            break;
    }
}

// Compile ternary operator
static void compile_ternary(IRExprCompiler* comp, struct IRExpression* expr) {
    // Compile condition
    compile_expr(comp, expr->ternary.condition);

    // Jump if false to else branch
    uint32_t jump_else = emit(comp, OP_JUMP_IF_FALSE, 0, 0, 0);

    // Compile then branch
    compile_expr(comp, expr->ternary.then_expr);

    // Jump past else branch
    uint32_t jump_end = emit(comp, OP_JUMP, 0, 0, 0);

    // Patch else jump target (current position is start of else branch)
    // Offset is relative: target - current_position
    patch_jump(comp, jump_else, (int32_t)comp->code_size - (int32_t)jump_else);

    // Compile else branch
    compile_expr(comp, expr->ternary.else_expr);

    // Patch end jump target
    // Offset is relative: target - current_position
    patch_jump(comp, jump_end, (int32_t)comp->code_size - (int32_t)jump_end);
}

// ============================================================================
// OPTIMIZATION PASS: CONSTANT FOLDING
// ============================================================================

// Deep copy an expression (for optimization)
static IRExpression* ir_expr_copy(IRExpression* expr) {
    if (!expr) return NULL;

    IRExpression* copy = (IRExpression*)calloc(1, sizeof(IRExpression));
    if (!copy) return NULL;

    copy->type = expr->type;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            copy->int_value = expr->int_value;
            break;

        case EXPR_LITERAL_FLOAT:
            copy->float_value = expr->float_value;
            break;

        case EXPR_LITERAL_STRING:
            copy->string_value = expr->string_value ? strdup(expr->string_value) : NULL;
            break;

        case EXPR_LITERAL_BOOL:
            copy->bool_value = expr->bool_value;
            break;

        case EXPR_LITERAL_NULL:
            break;

        case EXPR_VAR_REF:
            copy->var_ref.name = expr->var_ref.name ? strdup(expr->var_ref.name) : NULL;
            copy->var_ref.scope = expr->var_ref.scope ? strdup(expr->var_ref.scope) : NULL;
            break;

        case EXPR_PROPERTY:
            copy->property.object = expr->property.object ? strdup(expr->property.object) : NULL;
            copy->property.field = expr->property.field ? strdup(expr->property.field) : NULL;
            break;

        case EXPR_BINARY:
            copy->binary.op = expr->binary.op;
            copy->binary.left = ir_expr_copy(expr->binary.left);
            copy->binary.right = ir_expr_copy(expr->binary.right);
            break;

        case EXPR_UNARY:
            copy->unary.op = expr->unary.op;
            copy->unary.operand = ir_expr_copy(expr->unary.operand);
            break;

        case EXPR_TERNARY:
            copy->ternary.condition = ir_expr_copy(expr->ternary.condition);
            copy->ternary.then_expr = ir_expr_copy(expr->ternary.then_expr);
            copy->ternary.else_expr = ir_expr_copy(expr->ternary.else_expr);
            break;

        case EXPR_CALL:
            copy->call.function = expr->call.function ? strdup(expr->call.function) : NULL;
            copy->call.arg_count = expr->call.arg_count;
            copy->call.args = (IRExpression**)calloc(expr->call.arg_count, sizeof(IRExpression*));
            for (int i = 0; i < expr->call.arg_count; i++) {
                copy->call.args[i] = ir_expr_copy(expr->call.args[i]);
            }
            break;

        case EXPR_MEMBER_ACCESS:
            copy->member_access.object = ir_expr_copy(expr->member_access.object);
            copy->member_access.property = expr->member_access.property ? strdup(expr->member_access.property) : NULL;
            copy->member_access.property_hash = expr->member_access.property_hash;
            break;

        case EXPR_COMPUTED_MEMBER:
            copy->computed_member.object = ir_expr_copy(expr->computed_member.object);
            copy->computed_member.key = ir_expr_copy(expr->computed_member.key);
            break;

        case EXPR_METHOD_CALL:
            copy->method_call.receiver = ir_expr_copy(expr->method_call.receiver);
            copy->method_call.method_name = expr->method_call.method_name ? strdup(expr->method_call.method_name) : NULL;
            copy->method_call.arg_count = expr->method_call.arg_count;
            copy->method_call.args = (IRExpression**)calloc(expr->method_call.arg_count, sizeof(IRExpression*));
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                copy->method_call.args[i] = ir_expr_copy(expr->method_call.args[i]);
            }
            break;

        case EXPR_GROUP:
            copy->group.inner = ir_expr_copy(expr->group.inner);
            break;

        case EXPR_INDEX:
            copy->index_access.array = ir_expr_copy(expr->index_access.array);
            copy->index_access.index = ir_expr_copy(expr->index_access.index);
            break;

        default:
            break;
    }

    return copy;
}

// Check if expression is a constant (literal)
static bool is_constant(IRExpression* expr) {
    if (!expr) return false;
    return expr->type == EXPR_LITERAL_INT ||
           expr->type == EXPR_LITERAL_FLOAT ||
           expr->type == EXPR_LITERAL_STRING ||
           expr->type == EXPR_LITERAL_BOOL ||
           expr->type == EXPR_LITERAL_NULL;
}

// Evaluate binary constant expression at compile time
static IRExpression* eval_binary_const(IRExpression* left, IRExpression* right, IRBinaryOp op) {
    IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
    if (!result) return NULL;

    // Integer operations
    if (left->type == EXPR_LITERAL_INT && right->type == EXPR_LITERAL_INT) {
        int64_t l = left->int_value;
        int64_t r = right->int_value;

        result->type = EXPR_LITERAL_INT;

        switch (op) {
            case BINARY_OP_ADD:    result->int_value = l + r; break;
            case BINARY_OP_SUB:    result->int_value = l - r; break;
            case BINARY_OP_MUL:    result->int_value = l * r; break;
            case BINARY_OP_DIV:    result->int_value = (r != 0) ? l / r : 0; break;
            case BINARY_OP_MOD:    result->int_value = (r != 0) ? l % r : 0; break;
            case BINARY_OP_EQ:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l == r); break;
            case BINARY_OP_NEQ:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l != r); break;
            case BINARY_OP_LT:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l < r); break;
            case BINARY_OP_LTE:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l <= r); break;
            case BINARY_OP_GT:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l > r); break;
            case BINARY_OP_GTE:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l >= r); break;
            case BINARY_OP_AND:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l && r); break;
            case BINARY_OP_OR:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l || r); break;
            default:
                result->int_value = 0;
                break;
        }
        return result;
    }

    // Float operations
    if (left->type == EXPR_LITERAL_FLOAT && right->type == EXPR_LITERAL_FLOAT) {
        double l = left->float_value;
        double r = right->float_value;

        result->type = EXPR_LITERAL_FLOAT;

        switch (op) {
            case BINARY_OP_ADD:    result->float_value = l + r; break;
            case BINARY_OP_SUB:    result->float_value = l - r; break;
            case BINARY_OP_MUL:    result->float_value = l * r; break;
            case BINARY_OP_DIV:    result->float_value = (r != 0.0) ? l / r : 0.0; break;
            case BINARY_OP_MOD:    result->float_value = fmod(l, r); break;
            case BINARY_OP_EQ:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l == r); break;
            case BINARY_OP_NEQ:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l != r); break;
            case BINARY_OP_LT:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l < r); break;
            case BINARY_OP_LTE:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l <= r); break;
            case BINARY_OP_GT:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l > r); break;
            case BINARY_OP_GTE:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l >= r); break;
            case BINARY_OP_AND:    result->type = EXPR_LITERAL_BOOL; result->bool_value = (l && r); break;
            case BINARY_OP_OR:     result->type = EXPR_LITERAL_BOOL; result->bool_value = (l || r); break;
            default:
                result->float_value = 0.0;
                break;
        }
        return result;
    }

    // Boolean operations
    if (left->type == EXPR_LITERAL_BOOL && right->type == EXPR_LITERAL_BOOL) {
        bool l = left->bool_value;
        bool r = right->bool_value;

        result->type = EXPR_LITERAL_BOOL;

        switch (op) {
            case BINARY_OP_EQ:     result->bool_value = (l == r); break;
            case BINARY_OP_NEQ:    result->bool_value = (l != r); break;
            case BINARY_OP_AND:    result->bool_value = (l && r); break;
            case BINARY_OP_OR:     result->bool_value = (l || r); break;
            default:
                result->bool_value = false;
                break;
        }
        return result;
    }

    // String concatenation
    if (left->type == EXPR_LITERAL_STRING && right->type == EXPR_LITERAL_STRING &&
        (op == BINARY_OP_ADD || op == BINARY_OP_CONCAT)) {
        size_t len = strlen(left->string_value) + strlen(right->string_value) + 1;
        result->string_value = (char*)malloc(len);
        if (result->string_value) {
            snprintf(result->string_value, len, "%s%s", left->string_value, right->string_value);
            result->type = EXPR_LITERAL_STRING;
        }
        return result;
    }

    free(result);
    return NULL;
}

// Evaluate unary constant expression at compile time
static IRExpression* eval_unary_const(IRExpression* operand, IRUnaryOp op) {
    IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
    if (!result) return NULL;

    switch (op) {
        case UNARY_OP_NEG:
            if (operand->type == EXPR_LITERAL_INT) {
                result->type = EXPR_LITERAL_INT;
                result->int_value = -operand->int_value;
                return result;
            }
            if (operand->type == EXPR_LITERAL_FLOAT) {
                result->type = EXPR_LITERAL_FLOAT;
                result->float_value = -operand->float_value;
                return result;
            }
            break;

        case UNARY_OP_NOT:
            if (operand->type == EXPR_LITERAL_BOOL) {
                result->type = EXPR_LITERAL_BOOL;
                result->bool_value = !operand->bool_value;
                return result;
            }
            if (operand->type == EXPR_LITERAL_INT) {
                result->type = EXPR_LITERAL_BOOL;
                result->bool_value = (operand->int_value == 0);
                return result;
            }
            if (operand->type == EXPR_LITERAL_NULL) {
                result->type = EXPR_LITERAL_BOOL;
                result->bool_value = true;
                return result;
            }
            break;
    }

    free(result);
    return NULL;
}

// Constant folding on expression AST
IRExpression* ir_expr_const_fold(IRExpression* expr) {
    if (!expr) return NULL;

    switch (expr->type) {
        case EXPR_BINARY: {
            IRExpression* left = ir_expr_const_fold(expr->binary.left);
            IRExpression* right = ir_expr_const_fold(expr->binary.right);

            // If both operands are constants, evaluate now
            if (is_constant(left) && is_constant(right)) {
                IRExpression* result = eval_binary_const(left, right, expr->binary.op);
                // Free the folded expressions
                ir_expr_free(left);
                ir_expr_free(right);
                return result;
            }

            // Otherwise, build optimized binary expression
            IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
            result->type = EXPR_BINARY;
            result->binary.op = expr->binary.op;
            result->binary.left = left;
            result->binary.right = right;
            return result;
        }

        case EXPR_UNARY: {
            IRExpression* operand = ir_expr_const_fold(expr->unary.operand);

            // If operand is constant, evaluate now
            if (is_constant(operand)) {
                IRExpression* result = eval_unary_const(operand, expr->unary.op);
                ir_expr_free(operand);
                return result;
            }

            // Otherwise, build optimized unary expression
            IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
            result->type = EXPR_UNARY;
            result->unary.op = expr->unary.op;
            result->unary.operand = operand;
            return result;
        }

        case EXPR_TERNARY: {
            IRExpression* condition = ir_expr_const_fold(expr->ternary.condition);

            // If condition is constant, fold to the appropriate branch
            if (condition->type == EXPR_LITERAL_BOOL) {
                IRExpression* result;
                if (condition->bool_value) {
                    result = ir_expr_const_fold(expr->ternary.then_expr);
                } else {
                    result = ir_expr_const_fold(expr->ternary.else_expr);
                }
                ir_expr_free(condition);
                return result;
            }

            // Otherwise, recursively optimize branches
            IRExpression* then_expr = ir_expr_const_fold(expr->ternary.then_expr);
            IRExpression* else_expr = ir_expr_const_fold(expr->ternary.else_expr);

            IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
            result->type = EXPR_TERNARY;
            result->ternary.condition = condition;
            result->ternary.then_expr = then_expr;
            result->ternary.else_expr = else_expr;
            return result;
        }

        case EXPR_MEMBER_ACCESS:
        case EXPR_COMPUTED_MEMBER:
        case EXPR_METHOD_CALL: {
            // For complex expressions, we need to recursively optimize children
            // This is simplified - a full implementation would handle all cases
            return ir_expr_copy(expr);
        }

        case EXPR_GROUP: {
            return ir_expr_const_fold(expr->group.inner);
        }

        case EXPR_CALL: {
            // Optimize all arguments
            IRExpression** optimized_args = (IRExpression**)calloc(
                expr->call.arg_count, sizeof(IRExpression*)
            );
            for (int i = 0; i < expr->call.arg_count; i++) {
                optimized_args[i] = ir_expr_const_fold(expr->call.args[i]);
            }

            IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
            result->type = EXPR_CALL;
            result->call.function = strdup(expr->call.function);
            result->call.args = optimized_args;
            result->call.arg_count = expr->call.arg_count;
            return result;
        }

        case EXPR_INDEX: {
            IRExpression* array = ir_expr_const_fold(expr->index_access.array);
            IRExpression* index = ir_expr_const_fold(expr->index_access.index);

            // If both are constant, could potentially fold...
            // For now, just rebuild with optimized children

            IRExpression* result = (IRExpression*)calloc(1, sizeof(IRExpression));
            result->type = EXPR_INDEX;
            result->index_access.array = array;
            result->index_access.index = index;
            return result;
        }

        // Constants are already optimal
        case EXPR_LITERAL_INT:
        case EXPR_LITERAL_FLOAT:
        case EXPR_LITERAL_STRING:
        case EXPR_LITERAL_BOOL:
        case EXPR_LITERAL_NULL:
        case EXPR_VAR_REF:
        case EXPR_PROPERTY:
        default:
            return ir_expr_copy(expr);
    }
}

// Main optimization entry point
IRExpression* ir_expr_optimize(IRExpression* expr) {
    return ir_expr_const_fold(expr);
}

// ============================================================================
// OPTIMIZATION PASS: DEAD CODE ELIMINATION
// ============================================================================

void ir_bytecode_dce(IRCompiledExpr* compiled) {
    if (!compiled || !compiled->code) return;

    // Mark unreachable instructions
    bool* reachable = (bool*)calloc(compiled->code_size, sizeof(bool));
    if (!reachable) return;

    // First pass: mark reachable instructions
    reachable[0] = true;
    for (uint32_t pc = 0; pc < compiled->code_size; pc++) {
        if (!reachable[pc]) continue;

        IRInstruction* instr = &compiled->code[pc];

        switch (instr->opcode) {
            case OP_JUMP: {
                uint32_t target = (uint32_t)((int64_t)pc + instr->operand3);
                if (target < compiled->code_size) {
                    reachable[target] = true;
                }
                // Instructions after jump are not reachable
                for (uint32_t i = pc + 1; i < compiled->code_size && i < (uint32_t)((int64_t)pc + instr->operand3); i++) {
                    // Keep as-is for now (could be optimized)
                }
                break;
            }

            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE: {
                uint32_t target = (uint32_t)((int64_t)pc + instr->operand3);
                if (target < compiled->code_size) {
                    reachable[target] = true;
                }
                // Fallthrough is also reachable
                if (pc + 1 < compiled->code_size) {
                    reachable[pc + 1] = true;
                }
                break;
            }

            default:
                // Fallthrough to next instruction
                if (pc + 1 < compiled->code_size) {
                    reachable[pc + 1] = true;
                }
                break;
        }
    }

    // Second pass: replace unreachable instructions with NOP
    for (uint32_t pc = 0; pc < compiled->code_size; pc++) {
        if (!reachable[pc]) {
            compiled->code[pc].opcode = OP_NOP;
            compiled->code[pc].operand1 = 0;
            compiled->code[pc].operand2 = 0;
            compiled->code[pc].operand3 = 0;
        }
    }

    free(reachable);
}

// ============================================================================
// DISASSEMBLY
// ============================================================================

char* ir_bytecode_disassemble(IRCompiledExpr* compiled) {
    if (!compiled || !compiled->code) {
        return strdup("; No bytecode");
    }

    // Estimate buffer size
    size_t buffer_size = compiled->code_size * 80 + 100;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) return NULL;

    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "; Disassembly of %u instructions\n",
                      compiled->code_size);

    for (uint32_t pc = 0; pc < compiled->code_size; pc++) {
        IRInstruction* instr = &compiled->code[pc];
        const char* opcode_name = ir_opcode_name((IROpcode)instr->opcode);

        offset += snprintf(buffer + offset, buffer_size - offset,
                          "%4u: %-18s", pc, opcode_name);

        // Print operands based on opcode
        switch (instr->opcode) {
            case OP_PUSH_INT:
                if (instr->operand2 == 0) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " %d", instr->operand3);
                } else {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " [pool=%u]", instr->operand2);
                }
                break;

            case OP_PUSH_FLOAT:
                if (instr->operand2 < compiled->string_pool_count) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " %s", compiled->string_pool[instr->operand2]);
                } else {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " [pool=%u]", instr->operand2);
                }
                break;

            case OP_PUSH_STRING:
            case OP_LOAD_VAR:
            case OP_GET_PROP:
            case OP_CALL_FUNCTION:
                if (instr->operand2 < compiled->string_pool_count) {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " \"%s\"", compiled->string_pool[instr->operand2]);
                } else {
                    offset += snprintf(buffer + offset, buffer_size - offset,
                                      " [pool=%u]", instr->operand2);
                }
                break;

            case OP_PUSH_BOOL:
                offset += snprintf(buffer + offset, buffer_size - offset,
                                  " %s", instr->operand1 ? "true" : "false");
                break;

            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE:
                offset += snprintf(buffer + offset, buffer_size - offset,
                                  " [offset=%d]", instr->operand3);
                break;

            case OP_CALL_METHOD:
            case OP_CALL_BUILTIN:
                offset += snprintf(buffer + offset, buffer_size - offset,
                                  " [args=%d]", instr->operand3);
                break;

            default:
                // No additional operands
                break;
        }

        offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    }

    return buffer;
}

// ============================================================================
// PUBLIC COMPILATION ENTRY POINT
// ============================================================================

IRCompiledExpr* ir_expr_compile(IRExpression* expr) {
    if (!expr) {
        IR_LOG_ERROR("EXPR_COMPILER", "ir_expr_compile: null expression");
        return NULL;
    }

    // Get source string for debugging (if available)
    char* source = NULL;
    cJSON* json = ir_expr_to_json(expr);
    if (json) {
        char* json_str = cJSON_PrintUnformatted(json);
        if (json_str) {
            source = json_str;
        }
        cJSON_Delete(json);
    }

    IRExprCompiler* comp = ir_expr_compiler_create(source);
    if (!comp) {
        free(source);
        IR_LOG_ERROR("EXPR_COMPILER", "Failed to create compiler instance");
        return NULL;
    }

    // Compile the expression
    compile_expr(comp, expr);

    // Emit final halt
    emit(comp, OP_HALT, 0, 0, 0);

    // Build result
    IRCompiledExpr* result = (IRCompiledExpr*)calloc(1, sizeof(IRCompiledExpr));
    if (!result) {
        ir_expr_compiler_destroy(comp);
        return NULL;
    }

    result->code = comp->code;
    result->code_size = comp->code_size;
    result->code_capacity = comp->code_capacity;
    result->temp_count = comp->temp_count;
    result->max_stack_depth = comp->max_stack_depth + 1;  // Add 1 for safety

    // Transfer ownership of string pool
    result->string_pool = comp->pool.strings;
    result->string_pool_count = comp->pool.count;
    result->string_pool_capacity = comp->pool.capacity;

    // Transfer ownership of int pool
    result->int_pool = comp->int_pool.values;
    result->int_pool_count = comp->int_pool.count;
    result->int_pool_capacity = comp->int_pool.capacity;

    // Store source for debugging
    result->source_expr = comp->source_expr;
    result->has_debug_info = (source != NULL);

    // Free compiler but keep the arrays we transferred
    comp->code = NULL;
    comp->pool.strings = NULL;
    comp->pool.hashes = NULL;
    comp->pool.count = 0;  // Important: prevent double-free in destroy
    comp->int_pool.values = NULL;
    comp->int_pool.count = 0;  // Important: prevent double-free in destroy
    comp->source_expr = NULL;

    ir_expr_compiler_destroy(comp);
    free(source);

    return result;
}

void ir_expr_compiled_free(IRCompiledExpr* compiled) {
    if (!compiled) return;

    free(compiled->code);
    free((void*)compiled->source_expr);
    free(compiled->debug_lines);

    // Free string pool
    for (uint32_t i = 0; i < compiled->string_pool_count; i++) {
        free(compiled->string_pool[i]);
    }
    free(compiled->string_pool);

    // Free int pool
    free(compiled->int_pool);

    free(compiled);
}

// ============================================================================
// EVALUATION ENGINE (Phase 4)
// ============================================================================

// Evaluation context - contains runtime state for bytecode execution
typedef struct IREvalContext {
    // Stack for bytecode execution
    IRValue* stack;
    uint32_t stack_top;
    uint32_t stack_capacity;

    // Builtin function registry
    IRBuiltinRegistry* builtin_registry;

    // Variable scope for local variables (method arguments, etc.)
    struct {
        const char** names;
        IRValue* values;
        uint32_t count;
        uint32_t capacity;
    } local_scope;

    // Access to executor context for global variables (optional)
    IRExecutorContext* executor;
    uint32_t instance_id;

    // Error handling
    char error_msg[256];
    bool has_error;
} IREvalContext;

// Forward declarations for eval functions
static IRValue eval_loop(IREvalContext* ctx, IRCompiledExpr* compiled);

// ============================================================================
// STACK OPERATIONS
// ============================================================================

static void eval_stack_push(IREvalContext* ctx, IRValue value) {
    if (ctx->stack_top >= ctx->stack_capacity) {
        // Grow stack
        uint32_t new_capacity = ctx->stack_capacity * 2;
        IRValue* new_stack = (IRValue*)realloc(ctx->stack, new_capacity * sizeof(IRValue));
        if (!new_stack) {
            snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Stack overflow");
            ctx->has_error = true;
            ir_value_free(&value);
            return;
        }
        ctx->stack = new_stack;
        ctx->stack_capacity = new_capacity;
    }

    ctx->stack[ctx->stack_top++] = value;
}

static IRValue eval_stack_pop(IREvalContext* ctx) {
    if (ctx->stack_top == 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Stack underflow");
        ctx->has_error = true;
        return ir_value_null();
    }

    return ctx->stack[--ctx->stack_top];
}

static IRValue eval_stack_peek(IREvalContext* ctx, uint32_t offset) {
    if (ctx->stack_top <= offset) {
        return ir_value_null();
    }

    return ctx->stack[ctx->stack_top - 1 - offset];
}

static void eval_stack_dup(IREvalContext* ctx) {
    if (ctx->stack_top > 0) {
        IRValue top = ctx->stack[ctx->stack_top - 1];
        eval_stack_push(ctx, ir_value_copy(&top));
    }
}

static void eval_stack_swap(IREvalContext* ctx) {
    if (ctx->stack_top >= 2) {
        IRValue temp = ctx->stack[ctx->stack_top - 1];
        ctx->stack[ctx->stack_top - 1] = ctx->stack[ctx->stack_top - 2];
        ctx->stack[ctx->stack_top - 2] = temp;
    }
}

// ============================================================================
// EVALUATION CONTEXT MANAGEMENT
// ============================================================================

static IREvalContext* ir_eval_context_create(IRExecutorContext* executor, uint32_t instance_id, IRBuiltinRegistry* builtin_registry) {
    IREvalContext* ctx = (IREvalContext*)calloc(1, sizeof(IREvalContext));
    if (!ctx) return NULL;

    ctx->stack_capacity = 64;
    ctx->stack = (IRValue*)calloc(ctx->stack_capacity, sizeof(IRValue));
    if (!ctx->stack) {
        free(ctx);
        return NULL;
    }
    ctx->stack_top = 0;

    ctx->local_scope.capacity = 16;
    ctx->local_scope.names = (const char**)calloc(ctx->local_scope.capacity, sizeof(const char*));
    ctx->local_scope.values = (IRValue*)calloc(ctx->local_scope.capacity, sizeof(IRValue));
    ctx->local_scope.count = 0;

    ctx->executor = executor;
    ctx->instance_id = instance_id;
    ctx->builtin_registry = builtin_registry;
    ctx->has_error = false;

    return ctx;
}

static void ir_eval_context_destroy(IREvalContext* ctx) {
    if (!ctx) return;

    // Free stack values
    for (uint32_t i = 0; i < ctx->stack_top; i++) {
        ir_value_free(&ctx->stack[i]);
    }
    free(ctx->stack);

    // Free local scope names (strings are borrowed, no need to free)
    free(ctx->local_scope.names);

    // Free local scope values
    for (uint32_t i = 0; i < ctx->local_scope.count; i++) {
        ir_value_free(&ctx->local_scope.values[i]);
    }
    free(ctx->local_scope.values);

    free(ctx);
}

// ============================================================================
// VARIABLE RESOLUTION
// ============================================================================

// Convert executor IRValue to compiler IRValue
// NOTE: This is a simplified version that doesn't directly access executor types
// to avoid circular dependencies. Full integration would require accessor functions.
static IRValue executor_value_to_compiler_value(void* exec_val) {
    (void)exec_val;
    // TODO: Implement proper conversion when executor integration is needed
    return ir_value_null();
}

static IRValue resolve_variable(IREvalContext* ctx, const char* name) {
    // First, check local scope
    for (uint32_t i = 0; i < ctx->local_scope.count; i++) {
        if (strcmp(ctx->local_scope.names[i], name) == 0) {
            return ir_value_copy(&ctx->local_scope.values[i]);
        }
    }

    // Then, check executor variables (simplified - returns null for now)
    // Full implementation would use accessor functions from ir_executor.h
    (void)ctx->executor;
    (void)ctx->instance_id;

    // Variable not found, return null
    return ir_value_null();
}

// ============================================================================
// PROPERTY ACCESS
// ============================================================================

static IRValue get_property(IRValue object, const char* property) {
    if (object.type == VAR_TYPE_OBJECT && object.object_val) {
        return ir_object_get(object.object_val, property);
    }

    // If object is null, return null
    if (object.type == VAR_TYPE_NULL) {
        return ir_value_null();
    }

    // For arrays, check for length property
    if (object.type == VAR_TYPE_ARRAY && strcmp(property, "length") == 0) {
        return ir_value_int(ir_array_length(object.array_val));
    }

    // For strings, check for length property
    if (object.type == VAR_TYPE_STRING && strcmp(property, "length") == 0) {
        return ir_value_int(strlen(object.string_val));
    }

    // Property not found
    return ir_value_null();
}

static IRValue get_computed_property(IRValue object, IRValue key) {
    if (object.type == VAR_TYPE_NULL || key.type == VAR_TYPE_NULL) {
        return ir_value_null();
    }

    // Array indexing
    if (object.type == VAR_TYPE_ARRAY && key.type == VAR_TYPE_INT) {
        int64_t index = key.int_val;
        if (index >= 0 && index < (int64_t)object.array_val->count) {
            return ir_value_copy(&object.array_val->items[index]);
        }
        return ir_value_null();
    }

    // String indexing
    if (object.type == VAR_TYPE_STRING && key.type == VAR_TYPE_INT) {
        int64_t index = key.int_val;
        size_t len = strlen(object.string_val);
        if (index >= 0 && index < (int64_t)len) {
            char str[2] = {object.string_val[index], '\0'};
            return ir_value_string(str);
        }
        return ir_value_string("");
    }

    // Object property access with string key
    if (object.type == VAR_TYPE_OBJECT && object.object_val && key.type == VAR_TYPE_STRING) {
        return ir_object_get(object.object_val, key.string_val);
    }

    return ir_value_null();
}

// ============================================================================
// OPERATION IMPLEMENTATIONS
// ============================================================================

static IRValue op_add(IRValue left, IRValue right) {
    // String concatenation
    if (left.type == VAR_TYPE_STRING || right.type == VAR_TYPE_STRING) {
        char* left_str = ir_value_to_string(&left);
        char* right_str = ir_value_to_string(&right);

        size_t len = strlen(left_str) + strlen(right_str) + 1;
        char* result = (char*)malloc(len);
        snprintf(result, len, "%s%s", left_str, right_str);

        free(left_str);
        free(right_str);

        IRValue retval;
        retval.type = VAR_TYPE_STRING;
        retval.string_val = result;
        return retval;
    }

    // Integer addition
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val + right.int_val);
    }

    // Float addition
    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_float(left.float_val + right.float_val);
    }

    // Mixed int/float
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_float((double)left.int_val + right.float_val);
    }
    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_INT) {
        return ir_value_float(left.float_val + (double)right.int_val);
    }

    return ir_value_null();
}

static IRValue op_sub(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val - right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_float(left.float_val - right.float_val);
    }

    return ir_value_null();
}

static IRValue op_mul(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val * right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_float(left.float_val * right.float_val);
    }

    return ir_value_null();
}

static IRValue op_div(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        if (right.int_val == 0) return ir_value_null();
        return ir_value_int(left.int_val / right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        if (right.float_val == 0.0) return ir_value_null();
        return ir_value_float(left.float_val / right.float_val);
    }

    return ir_value_null();
}

static IRValue op_mod(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        if (right.int_val == 0) return ir_value_null();
        return ir_value_int(left.int_val % right.int_val);
    }

    return ir_value_null();
}

static IRValue op_eq(IRValue left, IRValue right) {
    bool result = ir_value_equals(&left, &right);
    return ir_value_bool(result);
}

static IRValue op_neq(IRValue left, IRValue right) {
    bool result = !ir_value_equals(&left, &right);
    return ir_value_bool(result);
}

static IRValue op_lt(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_bool(left.int_val < right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_bool(left.float_val < right.float_val);
    }

    if (left.type == VAR_TYPE_STRING && right.type == VAR_TYPE_STRING) {
        return ir_value_bool(strcmp(left.string_val, right.string_val) < 0);
    }

    return ir_value_bool(false);
}

static IRValue op_lte(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_bool(left.int_val <= right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_bool(left.float_val <= right.float_val);
    }

    return ir_value_bool(false);
}

static IRValue op_gt(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_bool(left.int_val > right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_bool(left.float_val > right.float_val);
    }

    if (left.type == VAR_TYPE_STRING && right.type == VAR_TYPE_STRING) {
        return ir_value_bool(strcmp(left.string_val, right.string_val) > 0);
    }

    return ir_value_bool(false);
}

static IRValue op_gte(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_bool(left.int_val >= right.int_val);
    }

    if (left.type == VAR_TYPE_FLOAT && right.type == VAR_TYPE_FLOAT) {
        return ir_value_bool(left.float_val >= right.float_val);
    }

    return ir_value_bool(false);
}

static IRValue op_and(IRValue left, IRValue right) {
    bool left_val = ir_value_is_truthy(&left);
    bool right_val = ir_value_is_truthy(&right);
    return ir_value_bool(left_val && right_val);
}

static IRValue op_or(IRValue left, IRValue right) {
    bool left_val = ir_value_is_truthy(&left);
    bool right_val = ir_value_is_truthy(&right);
    return ir_value_bool(left_val || right_val);
}

static IRValue op_not(IRValue operand) {
    return ir_value_bool(!ir_value_is_truthy(&operand));
}

static IRValue op_negate(IRValue operand) {
    if (operand.type == VAR_TYPE_INT) {
        return ir_value_int(-operand.int_val);
    }

    if (operand.type == VAR_TYPE_FLOAT) {
        return ir_value_float(-operand.float_val);
    }

    return ir_value_null();
}

// ============================================================================
// METHOD CALLS
// ============================================================================

static IRValue call_array_method(IREvalContext* ctx, IRValue array_val, const char* method, uint32_t arg_count) {
    if (array_val.type != VAR_TYPE_ARRAY || !array_val.array_val) {
        return ir_value_null();
    }

    // Pop arguments from stack
    IRValue* args = NULL;
    if (arg_count > 0) {
        args = (IRValue*)malloc(arg_count * sizeof(IRValue));
        for (int i = (int)arg_count - 1; i >= 0; i--) {
            args[i] = eval_stack_pop(ctx);
        }
    }

    IRValue result = ir_value_null();

    if (strcmp(method, "push") == 0 && arg_count >= 1) {
        // array.push(value) - pushes value and returns new length
        ir_array_push(array_val.array_val, args[0]);
        result = ir_value_int(ir_array_length(array_val.array_val));
    } else if (strcmp(method, "pop") == 0) {
        // array.pop() - removes and returns last element
        result = ir_array_pop(array_val.array_val);
    } else if (strcmp(method, "length") == 0) {
        // array.length - return length
        result = ir_value_int(ir_array_length(array_val.array_val));
    } else if (strcmp(method, "reverse") == 0) {
        // array.reverse() - reverse in place
        IRArray* arr = array_val.array_val;
        for (uint32_t i = 0; i < arr->count / 2; i++) {
            IRValue temp = arr->items[i];
            arr->items[i] = arr->items[arr->count - 1 - i];
            arr->items[arr->count - 1 - i] = temp;
        }
        result = ir_value_object(array_val.object_val);
    }

    // Free argument values that weren't used
    if (args) {
        for (uint32_t i = 0; i < arg_count; i++) {
            ir_value_free(&args[i]);
        }
        free(args);
    }

    return result;
}

static IRValue call_string_method(IREvalContext* ctx, IRValue str_val, const char* method, uint32_t arg_count) {
    if (str_val.type != VAR_TYPE_STRING || !str_val.string_val) {
        return ir_value_null();
    }

    // Pop arguments from stack
    IRValue* args = NULL;
    if (arg_count > 0) {
        args = (IRValue*)malloc(arg_count * sizeof(IRValue));
        for (int i = (int)arg_count - 1; i >= 0; i--) {
            args[i] = eval_stack_pop(ctx);
        }
    }

    IRValue result = ir_value_null();

    if (strcmp(method, "length") == 0) {
        result = ir_value_int(strlen(str_val.string_val));
    } else if (strcmp(method, "toUpperCase") == 0) {
        char* upper = strdup(str_val.string_val);
        for (size_t i = 0; upper[i]; i++) {
            upper[i] = (char)toupper((unsigned char)upper[i]);
        }
        result.type = VAR_TYPE_STRING;
        result.string_val = upper;
    } else if (strcmp(method, "toLowerCase") == 0) {
        char* lower = strdup(str_val.string_val);
        for (size_t i = 0; lower[i]; i++) {
            lower[i] = (char)tolower((unsigned char)lower[i]);
        }
        result.type = VAR_TYPE_STRING;
        result.string_val = lower;
    } else if (strcmp(method, "trim") == 0) {
        const char* s = str_val.string_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                          s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
        char* trimmed = strndup(s, len);
        result.type = VAR_TYPE_STRING;
        result.string_val = trimmed;
    } else if (strcmp(method, "substring") == 0 && arg_count >= 1) {
        size_t start = 0;
        size_t end = strlen(str_val.string_val);

        if (args[0].type == VAR_TYPE_INT) {
            start = (size_t)args[0].int_val;
        }
        if (arg_count >= 2 && args[1].type == VAR_TYPE_INT) {
            end = (size_t)args[1].int_val;
        }

        size_t len = strlen(str_val.string_val);
        if (start > len) start = len;
        if (end > len) end = len;
        if (end < start) end = start;

        char* substr = strndup(str_val.string_val + start, end - start);
        result.type = VAR_TYPE_STRING;
        result.string_val = substr;
    }

    // Free argument values
    if (args) {
        for (uint32_t i = 0; i < arg_count; i++) {
            ir_value_free(&args[i]);
        }
        free(args);
    }

    return result;
}

static IRValue call_method(IREvalContext* ctx, IRValue receiver, const char* method, uint32_t arg_count) {
    if (receiver.type == VAR_TYPE_NULL) {
        return ir_value_null();
    }

    // Array methods
    if (receiver.type == VAR_TYPE_ARRAY) {
        return call_array_method(ctx, receiver, method, arg_count);
    }

    // String methods
    if (receiver.type == VAR_TYPE_STRING) {
        return call_string_method(ctx, receiver, method, arg_count);
    }

    // Object methods (not yet implemented)
    if (receiver.type == VAR_TYPE_OBJECT) {
        return ir_value_null();
    }

    return ir_value_null();
}

// ============================================================================
// MAIN EVALUATION LOOP
// ============================================================================

static IRValue eval_loop(IREvalContext* ctx, IRCompiledExpr* compiled) {
    uint32_t pc = 0;  // Program counter

    while (pc < compiled->code_size && !ctx->has_error) {
        IRInstruction* instr = &compiled->code[pc];
        IROpcode opcode = (IROpcode)instr->opcode;

        switch (opcode) {
            case OP_NOP:
                pc++;
                break;

            case OP_HALT:
                goto halt;

            // Stack operations
            case OP_PUSH_INT:
                if (instr->operand2 == 0) {
                    // Small integer in operand3
                    eval_stack_push(ctx, ir_value_int(instr->operand3));
                } else {
                    // Large integer in pool
                    uint32_t idx = instr->operand2;
                    if (idx < compiled->int_pool_count) {
                        eval_stack_push(ctx, ir_value_int(compiled->int_pool[idx]));
                    } else {
                        eval_stack_push(ctx, ir_value_null());
                    }
                }
                pc++;
                break;

            case OP_PUSH_STRING: {
                uint32_t idx = instr->operand2;
                if (idx < compiled->string_pool_count) {
                    const char* str = compiled->string_pool[idx];
                    eval_stack_push(ctx, ir_value_string(str));
                } else {
                    eval_stack_push(ctx, ir_value_string(""));
                }
                pc++;
                break;
            }

            case OP_PUSH_FLOAT: {
                uint32_t idx = instr->operand2;
                if (idx < compiled->string_pool_count) {
                    const char* str = compiled->string_pool[idx];
                    // Parse the string back to a float
                    double val = strtod(str, NULL);
                    eval_stack_push(ctx, ir_value_float(val));
                } else {
                    eval_stack_push(ctx, ir_value_float(0.0));
                }
                pc++;
                break;
            }

            case OP_PUSH_BOOL:
                eval_stack_push(ctx, ir_value_bool(instr->operand1 != 0));
                pc++;
                break;

            case OP_PUSH_NULL:
                eval_stack_push(ctx, ir_value_null());
                pc++;
                break;

            case OP_DUP:
                eval_stack_dup(ctx);
                pc++;
                break;

            case OP_POP: {
                IRValue popped = eval_stack_pop(ctx);
                ir_value_free(&popped);
                pc++;
                break;
            }

            case OP_SWAP:
                eval_stack_swap(ctx);
                pc++;
                break;

            // Variable access
            case OP_LOAD_VAR: {
                uint32_t idx = instr->operand2;
                if (idx < compiled->string_pool_count) {
                    const char* name = compiled->string_pool[idx];
                    IRValue value = resolve_variable(ctx, name);
                    eval_stack_push(ctx, value);
                } else {
                    eval_stack_push(ctx, ir_value_null());
                }
                pc++;
                break;
            }

            // Property access
            case OP_GET_PROP: {
                IRValue object = eval_stack_pop(ctx);
                uint32_t idx = instr->operand2;
                if (idx < compiled->string_pool_count) {
                    const char* prop = compiled->string_pool[idx];
                    IRValue result = get_property(object, prop);
                    ir_value_free(&object);
                    eval_stack_push(ctx, result);
                } else {
                    ir_value_free(&object);
                    eval_stack_push(ctx, ir_value_null());
                }
                pc++;
                break;
            }

            case OP_GET_PROP_COMPUTED: {
                IRValue key = eval_stack_pop(ctx);
                IRValue object = eval_stack_pop(ctx);
                IRValue result = get_computed_property(object, key);
                ir_value_free(&key);
                ir_value_free(&object);
                eval_stack_push(ctx, result);
                pc++;
                break;
            }

            case OP_GET_INDEX: {
                IRValue index = eval_stack_pop(ctx);
                IRValue array = eval_stack_pop(ctx);
                IRValue result = get_computed_property(array, index);
                ir_value_free(&index);
                ir_value_free(&array);
                eval_stack_push(ctx, result);
                pc++;
                break;
            }

            // Binary operations
            case OP_ADD: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_add(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_SUB: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_sub(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_MUL: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_mul(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_DIV: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_div(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_MOD: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_mod(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_CONCAT: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_add(left, right));  // Same as ADD for string concatenation
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Comparison operations
            case OP_EQ: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_eq(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_NEQ: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_neq(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_LT: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_lt(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_LTE: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_lte(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_GT: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_gt(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_GTE: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_gte(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Logical operations
            case OP_AND: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_and(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_OR: {
                IRValue right = eval_stack_pop(ctx);
                IRValue left = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_or(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Unary operations
            case OP_NOT: {
                IRValue operand = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_not(operand));
                ir_value_free(&operand);
                pc++;
                break;
            }

            case OP_NEGATE: {
                IRValue operand = eval_stack_pop(ctx);
                eval_stack_push(ctx, op_negate(operand));
                ir_value_free(&operand);
                pc++;
                break;
            }

            // Control flow
            case OP_JUMP:
                pc += (uint32_t)instr->operand3;
                break;

            case OP_JUMP_IF_FALSE: {
                IRValue condition = eval_stack_pop(ctx);
                bool is_false = !ir_value_is_truthy(&condition);
                ir_value_free(&condition);

                if (is_false) {
                    pc += (uint32_t)instr->operand3;
                } else {
                    pc++;
                }
                break;
            }

            case OP_JUMP_IF_TRUE: {
                IRValue condition = eval_stack_pop(ctx);
                bool is_true = ir_value_is_truthy(&condition);
                ir_value_free(&condition);

                if (is_true) {
                    pc += (uint32_t)instr->operand3;
                } else {
                    pc++;
                }
                break;
            }

            // Method calls
            case OP_CALL_METHOD: {
                uint32_t method_idx = instr->operand2;
                uint32_t arg_count = (uint32_t)instr->operand3;

                if (method_idx < compiled->string_pool_count) {
                    const char* method = compiled->string_pool[method_idx];
                    IRValue receiver = eval_stack_pop(ctx);
                    IRValue result = call_method(ctx, receiver, method, arg_count);
                    ir_value_free(&receiver);
                    eval_stack_push(ctx, result);
                } else {
                    eval_stack_push(ctx, ir_value_null());
                }
                pc++;
                break;
            }

            case OP_CALL_FUNCTION:
                // Not yet implemented - return null
                eval_stack_push(ctx, ir_value_null());
                pc++;
                break;

            case OP_CALL_BUILTIN: {
                uint32_t name_idx = instr->operand2;
                uint32_t arg_count = instr->operand3;

                // Get builtin function name from string pool
                if (name_idx >= compiled->string_pool_count || !ctx->builtin_registry) {
                    eval_stack_push(ctx, ir_value_null());
                    pc++;
                    break;
                }

                const char* builtin_name = compiled->string_pool[name_idx];

                // Pop arguments from stack (in reverse order)
                IRValue* args = NULL;
                if (arg_count > 0) {
                    args = (IRValue*)calloc(arg_count, sizeof(IRValue));
                    if (!args) {
                        eval_stack_push(ctx, ir_value_null());
                        pc++;
                        break;
                    }

                    for (uint32_t i = 0; i < arg_count; i++) {
                        args[arg_count - 1 - i] = eval_stack_pop(ctx);
                    }
                }

                // Call the builtin function
                IRValue result = ir_builtin_call(ctx->builtin_registry, builtin_name, ctx, args, arg_count);

                // Free args array (but not the values themselves, those are on the stack)
                free(args);

                eval_stack_push(ctx, result);
                pc++;
                break;
            }

            default:
                snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                    "Unknown opcode: %d", opcode);
                ctx->has_error = true;
                pc++;
                break;
        }
    }

halt:
    // Return top of stack as result (or null if empty)
    if (ctx->stack_top > 0) {
        return eval_stack_pop(ctx);
    }

    return ir_value_null();
}

// ============================================================================
// PUBLIC EVALUATION ENTRY POINT
// ============================================================================

IRValue ir_expr_eval(IRCompiledExpr* compiled, IRExecutorContext* executor, uint32_t instance_id) {
    if (!compiled || !compiled->code) {
        return ir_value_null();
    }

    // Get or create global builtin registry
    IRBuiltinRegistry* builtin_registry = ir_builtin_global_get();
    if (!builtin_registry) {
        ir_builtin_global_init();
        builtin_registry = ir_builtin_global_get();
    }

    IREvalContext* ctx = ir_eval_context_create(executor, instance_id, builtin_registry);
    if (!ctx) {
        return ir_value_null();
    }

    // Main evaluation loop
    IRValue result = eval_loop(ctx, compiled);

    // Cleanup
    ir_eval_context_destroy(ctx);

    return result;
}
