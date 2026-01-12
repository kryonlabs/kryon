# Phase 4: Evaluation Engine

**Duration**: 4 days
**Dependencies**: Phase 3
**Status**: Pending

## Overview

Phase 4 implements the bytecode evaluation engine (interpreter) that executes compiled bytecode at runtime. This is the runtime component that makes compiled expressions useful.

## Objectives

1. Implement stack-based bytecode interpreter
2. Add variable scope resolution (local, parent, global)
3. Support all bytecode opcodes from Phase 3
4. Handle errors gracefully
5. Optimize hot paths for common operations

---

## Interpreter Architecture

### 1.1 Evaluation Context

**File**: `ir/ir_expression_eval.c`

```c
// Evaluation context - contains runtime state
typedef struct {
    // Stack for bytecode execution
    IRValue* stack;
    uint32_t stack_top;
    uint32_t stack_capacity;

    // Variable scopes (linked list for nested scopes)
    struct {
        const char** names;
        IRValue* values;
        uint32_t count;
        uint32_t capacity;
    } local_scope;

    // Access to executor context for global variables
    IRExecutorContext* executor;
    uint32_t instance_id;

    // Error handling
    char error_msg[256];
    bool has_error;

    // Builtin function registry
    const IRBuiltinRegistry* builtins;
} IREvalContext;

// Initialize evaluation context
IREvalContext* ir_eval_context_create(IRExecutorContext* executor, uint32_t instance_id) {
    IREvalContext* ctx = (IREvalContext*)calloc(1, sizeof(IREvalContext));

    ctx->stack_capacity = 64;
    ctx->stack = (IRValue*)calloc(ctx->stack_capacity, sizeof(IRValue));
    ctx->stack_top = 0;

    ctx->local_scope.capacity = 16;
    ctx->local_scope.names = (const char**)calloc(ctx->local_scope.capacity, sizeof(const char*));
    ctx->local_scope.values = (IRValue*)calloc(ctx->local_scope.capacity, sizeof(IRValue));
    ctx->local_scope.count = 0;

    ctx->executor = executor;
    ctx->instance_id = instance_id;

    return ctx;
}

void ir_eval_context_destroy(IREvalContext* ctx) {
    if (!ctx) return;

    // Free stack values
    for (uint32_t i = 0; i < ctx->stack_top; i++) {
        ir_value_free(&ctx->stack[i]);
    }
    free(ctx->stack);

    // Free local scope
    free(ctx->local_scope.names);
    for (uint32_t i = 0; i < ctx->local_scope.count; i++) {
        ir_value_free(&ctx->local_scope.values[i]);
    }
    free(ctx->local_scope.values);

    free(ctx);
}
```

### 1.2 Stack Operations

```c
// Push value onto stack
static void stack_push(IREvalContext* ctx, IRValue value) {
    if (ctx->stack_top >= ctx->stack_capacity) {
        // Grow stack
        ctx->stack_capacity *= 2;
        ctx->stack = (IRValue*)realloc(ctx->stack, ctx->stack_capacity * sizeof(IRValue));
    }

    ctx->stack[ctx->stack_top++] = value;
}

// Pop value from stack
static IRValue stack_pop(IREvalContext* ctx) {
    if (ctx->stack_top == 0) {
        // Stack underflow - return null
        return ir_value_null();
    }

    return ctx->stack[--ctx->stack_top];
}

// Peek at top of stack without popping
static IRValue stack_peek(IREvalContext* ctx, uint32_t offset) {
    if (ctx->stack_top <= offset) {
        return ir_value_null();
    }

    return ctx->stack[ctx->stack_top - 1 - offset];
}

// Duplicate top of stack
static void stack_dup(IREvalContext* ctx) {
    if (ctx->stack_top > 0) {
        IRValue top = ctx->stack[ctx->stack_top - 1];
        stack_push(ctx, ir_value_copy(&top));
    }
}
```

---

## Main Evaluation Loop

### 2.1 Entry Point

```c
// Evaluate compiled expression and return result
IRValue ir_expr_eval(IRCompiledExpr* compiled, IRExecutorContext* executor, uint32_t instance_id) {
    if (!compiled || !compiled->code) {
        return ir_value_null();
    }

    IREvalContext* ctx = ir_eval_context_create(executor, instance_id);

    // Main evaluation loop
    IRValue result = eval_loop(ctx, compiled);

    // Cleanup and return
    ir_eval_context_destroy(ctx);
    return result;
}

// Main evaluation loop
static IRValue eval_loop(IREvalContext* ctx, IRCompiledExpr* compiled) {
    uint32_t pc = 0;  // Program counter

    while (pc < compiled->code_size && !ctx->has_error) {
        IRInstruction* instr = &compiled->code[pc];
        IROpcode opcode = instr->opcode;

        // Execute instruction
        switch (opcode) {
            case OP_NOP:
                pc++;
                break;

            case OP_HALT:
                goto halt;

            // Stack operations
            case OP_PUSH_INT:
                stack_push(ctx, ir_value_int(instr->operand3));
                pc++;
                break;

            case OP_PUSH_STRING: {
                const char* str = compiled->string_pool[instr->operand2];
                stack_push(ctx, ir_value_string(str));
                pc++;
                break;
            }

            case OP_PUSH_BOOL:
                stack_push(ctx, ir_value_int(instr->operand1 ? 1 : 0));
                pc++;
                break;

            case OP_PUSH_NULL:
                stack_push(ctx, ir_value_null());
                pc++;
                break;

            case OP_DUP:
                stack_dup(ctx);
                pc++;
                break;

            case OP_POP:
                stack_pop(ctx);
                pc++;
                break;

            // Variable access
            case OP_LOAD_VAR: {
                const char* name = compiled->string_pool[instr->operand2];
                IRValue value = resolve_variable(ctx, name);
                stack_push(ctx, value);
                pc++;
                break;
            }

            // Property access
            case OP_GET_PROP: {
                IRValue object = stack_pop(ctx);
                const char* prop = compiled->string_pool[instr->operand2];
                IRValue result = get_property(object, prop);
                ir_value_free(&object);
                stack_push(ctx, result);
                pc++;
                break;
            }

            case OP_GET_PROP_COMPUTED: {
                IRValue key = stack_pop(ctx);
                IRValue object = stack_pop(ctx);
                IRValue result = get_computed_property(object, key);
                ir_value_free(&key);
                ir_value_free(&object);
                stack_push(ctx, result);
                pc++;
                break;
            }

            // Binary operations
            case OP_ADD: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_add(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_SUB: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_sub(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_MUL: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_mul(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_DIV: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_div(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Comparison operations
            case OP_EQ: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_eq(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_LT: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_lt(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Logical operations
            case OP_AND: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_and(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            case OP_OR: {
                IRValue right = stack_pop(ctx);
                IRValue left = stack_pop(ctx);
                stack_push(ctx, op_or(left, right));
                ir_value_free(&left);
                ir_value_free(&right);
                pc++;
                break;
            }

            // Unary operations
            case OP_NEGATE: {
                IRValue operand = stack_pop(ctx);
                stack_push(ctx, op_negate(operand));
                ir_value_free(&operand);
                pc++;
                break;
            }

            case OP_NOT: {
                IRValue operand = stack_pop(ctx);
                stack_push(ctx, op_not(operand));
                ir_value_free(&operand);
                pc++;
                break;
            }

            // Control flow
            case OP_JUMP:
                pc += instr->operand3;
                break;

            case OP_JUMP_IF_FALSE: {
                IRValue condition = stack_pop(ctx);
                bool is_false = value_is_falsey(condition);
                ir_value_free(&condition);

                if (is_false) {
                    pc += instr->operand3;
                } else {
                    pc++;
                }
                break;
            }

            case OP_JUMP_IF_TRUE: {
                IRValue condition = stack_pop(ctx);
                bool is_true = !value_is_falsey(condition);
                ir_value_free(&condition);

                if (is_true) {
                    pc += instr->operand3;
                } else {
                    pc++;
                }
                break;
            }

            // Method calls
            case OP_CALL_METHOD: {
                const char* method = compiled->string_pool[instr->operand2];
                uint32_t arg_count = instr->operand3;

                IRValue receiver = stack_pop(ctx);
                IRValue result = call_method(ctx, receiver, method, arg_count);
                ir_value_free(&receiver);
                stack_push(ctx, result);
                pc++;
                break;
            }

            case OP_CALL_BUILTIN: {
                uint32_t builtin_idx = instr->operand2;
                uint32_t arg_count = instr->operand3;

                IRValue result = call_builtin(ctx, builtin_idx, arg_count);
                stack_push(ctx, result);
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
        return stack_pop(ctx);
    }

    return ir_value_null();
}
```

---

## Variable Resolution

### 3.1 Scope Lookup

```c
// Resolve variable by searching scopes in order:
// 1. Local scope (component instance)
// 2. Executor variables (global/app state)
static IRValue resolve_variable(IREvalContext* ctx, const char* name) {
    // First, check local scope
    for (uint32_t i = 0; i < ctx->local_scope.count; i++) {
        if (strcmp(ctx->local_scope.names[i], name) == 0) {
            return ir_value_copy(&ctx->local_scope.values[i]);
        }
    }

    // Then, check executor variables
    if (ctx->executor) {
        return ir_executor_get_var(ctx->executor, name, ctx->instance_id);
    }

    return ir_value_null();
}

// Set local variable (for method call arguments, etc.)
static void set_local_variable(IREvalContext* ctx, const char* name, IRValue value) {
    // Check if already exists
    for (uint32_t i = 0; i < ctx->local_scope.count; i++) {
        if (strcmp(ctx->local_scope.names[i], name) == 0) {
            ir_value_free(&ctx->local_scope.values[i]);
            ctx->local_scope.values[i] = value;
            return;
        }
    }

    // Add new variable
    if (ctx->local_scope.count >= ctx->local_scope.capacity) {
        ctx->local_scope.capacity *= 2;
        ctx->local_scope.names = (const char**)realloc(ctx->local_scope.names,
            ctx->local_scope.capacity * sizeof(const char*));
        ctx->local_scope.values = (IRValue*)realloc(ctx->local_scope.values,
            ctx->local_scope.capacity * sizeof(IRValue));
    }

    ctx->local_scope.names[ctx->local_scope.count] = strdup(name);
    ctx->local_scope.values[ctx->local_scope.count] = value;
    ctx->local_scope.count++;
}
```

---

## Property Access

### 4.1 Static Property Access

```c
// Get property from object value
static IRValue get_property(IRValue object, const char* property) {
    if (object.type == VAR_TYPE_OBJECT && object.object_val) {
        IRValue result = ir_object_get(object.object_val, property);
        if (result.type != VAR_TYPE_NULL) {
            return result;
        }
    }

    // If object is null/undefined, return null
    if (object.type == VAR_TYPE_NULL) {
        return ir_value_null();
    }

    // For arrays, check for length property
    if (object.type == VAR_TYPE_ARRAY && strcmp(property, "length") == 0) {
        return ir_value_int(object.array_val.count);
    }

    // For strings, check for length property
    if (object.type == VAR_TYPE_STRING && strcmp(property, "length") == 0) {
        return ir_value_int(strlen(object.string_val));
    }

    // Property not found
    return ir_value_null();
}

// Computed property access: obj[key]
static IRValue get_computed_property(IRValue object, IRValue key) {
    if (object.type == VAR_TYPE_NULL || key.type == VAR_TYPE_NULL) {
        return ir_value_null();
    }

    // Convert key to string
    char key_str[256];
    const char* key_ptr = key_str;

    if (key.type == VAR_TYPE_STRING) {
        key_ptr = key.string_val;
    } else if (key.type == VAR_TYPE_INT) {
        snprintf(key_str, sizeof(key_str), "%ld", (long)key.int_val);
    } else {
        return ir_value_null();
    }

    // Array indexing
    if (object.type == VAR_TYPE_ARRAY && key.type == VAR_TYPE_INT) {
        int64_t index = key.int_val;
        if (index >= 0 && index < object.array_val.count) {
            return ir_value_copy(&object.array_val.items[index]);
        }
        return ir_value_null();
    }

    // String indexing
    if (object.type == VAR_TYPE_STRING && key.type == VAR_TYPE_INT) {
        int64_t index = key.int_val;
        size_t len = strlen(object.string_val);
        if (index >= 0 && index < len) {
            char str[2] = {object.string_val[index], '\0'};
            return ir_value_string(str);
        }
        return ir_value_string("");
    }

    // Object property access
    if (object.type == VAR_TYPE_OBJECT && object.object_val) {
        return ir_object_get(object.object_val, key_ptr);
    }

    return ir_value_null();
}
```

---

## Operation Implementations

### 5.1 Arithmetic Operations

```c
static IRValue op_add(IRValue left, IRValue right) {
    // String concatenation
    if (left.type == VAR_TYPE_STRING || right.type == VAR_TYPE_STRING) {
        char left_str[256];
        char right_str[256];

        value_to_string(left, left_str, sizeof(left_str));
        value_to_string(right, right_str, sizeof(right_str));

        size_t len = strlen(left_str) + strlen(right_str) + 1;
        char* result = (char*)malloc(len);
        snprintf(result, len, "%s%s", left_str, right_str);
        return ir_value_string_nocopy(result);
    }

    // Integer addition
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val + right.int_val);
    }

    return ir_value_null();
}

static IRValue op_sub(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val - right.int_val);
    }
    return ir_value_null();
}

static IRValue op_mul(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val * right.int_val);
    }
    return ir_value_null();
}

static IRValue op_div(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        if (right.int_val == 0) return ir_value_null();
        return ir_value_int(left.int_val / right.int_val);
    }
    return ir_value_null();
}
```

### 5.2 Comparison Operations

```c
static IRValue op_eq(IRValue left, IRValue right) {
    bool result = false;

    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        result = left.int_val == right.int_val;
    } else if (left.type == VAR_TYPE_STRING && right.type == VAR_TYPE_STRING) {
        result = strcmp(left.string_val, right.string_val) == 0;
    } else if (left.type == VAR_TYPE_NULL && right.type == VAR_TYPE_NULL) {
        result = true;
    } else {
        result = false;
    }

    return ir_value_int(result ? 1 : 0);
}

static IRValue op_lt(IRValue left, IRValue right) {
    if (left.type == VAR_TYPE_INT && right.type == VAR_TYPE_INT) {
        return ir_value_int(left.int_val < right.int_val ? 1 : 0);
    }
    if (left.type == VAR_TYPE_STRING && right.type == VAR_TYPE_STRING) {
        return ir_value_int(strcmp(left.string_val, right.string_val) < 0 ? 1 : 0);
    }
    return ir_value_int(0);
}
```

### 5.3 Logical Operations

```c
static IRValue op_and(IRValue left, IRValue right) {
    bool left_val = !value_is_falsey(left);
    bool right_val = !value_is_falsey(right);
    return ir_value_int(left_val && right_val ? 1 : 0);
}

static IRValue op_or(IRValue left, IRValue right) {
    bool left_val = !value_is_falsey(left);
    bool right_val = !value_is_falsey(right);
    return ir_value_int(left_val || right_val ? 1 : 0);
}

static bool value_is_falsey(IRValue value) {
    switch (value.type) {
        case VAR_TYPE_NULL:
            return true;
        case VAR_TYPE_INT:
            return value.int_val == 0;
        case VAR_TYPE_BOOL:
            return value.bool_val == false;
        case VAR_TYPE_STRING:
            return value.string_val == NULL || strlen(value.string_val) == 0;
        case VAR_TYPE_ARRAY:
            return value.array_val.count == 0;
        case VAR_TYPE_OBJECT:
            return value.object_val == NULL || value.object_val->count == 0;
        default:
            return true;
    }
}
```

### 5.4 Unary Operations

```c
static IRValue op_negate(IRValue operand) {
    if (operand.type == VAR_TYPE_INT) {
        return ir_value_int(-operand.int_val);
    }
    return ir_value_null();
}

static IRValue op_not(IRValue operand) {
    return ir_value_int(value_is_falsey(operand) ? 1 : 0);
}
```

---

## Method Calls

### 6.1 Method Dispatch

```c
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

    // Object methods
    if (receiver.type == VAR_TYPE_OBJECT) {
        return call_object_method(ctx, receiver, method, arg_count);
    }

    return ir_value_null();
}

// Array methods
static IRValue call_array_method(IREvalContext* ctx, IRValue array, const char* method, uint32_t arg_count) {
    if (strcmp(method, "push") == 0) {
        // array.push(value) - returns void
        if (arg_count >= 1) {
            IRValue value = stack_pop(ctx);
            // Add to array (modifies in place)
            if (array.array_val.count >= array.array_val.capacity) {
                array.array_val.capacity *= 2;
                array.array_val.items = (IRValue*)realloc(array.array_val.items,
                    array.array_val.capacity * sizeof(IRValue));
            }
            array.array_val.items[array.array_val.count++] = value;
        }
        return ir_value_null();
    }

    if (strcmp(method, "pop") == 0) {
        if (array.array_val.count > 0) {
            IRValue result = array.array_val.items[--array.array_val.count];
            return result;
        }
        return ir_value_null();
    }

    if (strcmp(method, "length") == 0) {
        return ir_value_int(array.array_val.count);
    }

    return ir_value_null();
}

// String methods
static IRValue call_string_method(IREvalContext* ctx, IRValue str, const char* method, uint32_t arg_count) {
    if (strcmp(method, "toUpperCase") == 0) {
        char* upper = strdup(str.string_val);
        for (size_t i = 0; upper[i]; i++) {
            upper[i] = toupper(upper[i]);
        }
        return ir_value_string_nocopy(upper);
    }

    if (strcmp(method, "toLowerCase") == 0) {
        char* lower = strdup(str.string_val);
        for (size_t i = 0; lower[i]; i++) {
            lower[i] = tolower(lower[i]);
        }
        return ir_value_string_nocopy(lower);
    }

    if (strcmp(method, "length") == 0) {
        return ir_value_int(strlen(str.string_val));
    }

    return ir_value_null();
}
```

---

## Implementation Steps

### Day 1: Core Interpreter

1. **Morning** (4 hours)
   - [ ] Create `ir/ir_expression_eval.h` and `.c`
   - [ ] Implement `IREvalContext` structure
   - [ ] Implement stack operations (push, pop, peek, dup)
   - [ ] Write unit tests

2. **Afternoon** (4 hours)
   - [ ] Implement main evaluation loop
   - [ ] Implement basic opcodes (PUSH, POP, HALT)
   - [ ] Implement LOAD_VAR with executor integration
   - [ ] Test simple expressions

### Day 2: Operations

1. **Morning** (4 hours)
   - [ ] Implement all arithmetic opcodes
   - [ ] Implement comparison opcodes
   - [ ] Implement logical opcodes
   - [ ] Implement unary opcodes

2. **Afternoon** (4 hours)
   - [ ] Implement property access (GET_PROP, GET_PROP_COMPUTED)
   - [ ] Implement array indexing
   - [ ] Test complex expressions

### Day 3: Control Flow & Methods

1. **Morning** (4 hours)
   - [ ] Implement jump opcodes
   - [ ] Implement method call dispatch
   - [ ] Implement array methods

2. **Afternoon** (4 hours)
   - [ ] Implement string methods
   - [ ] Test ternary operators
   - [ ] Test method chaining

### Day 4: Testing & Integration

1. **Morning** (4 hours)
   - [ ] Comprehensive test suite
   - [ ] Integration with executor
   - [ ] Error handling tests

2. **Afternoon** (4 hours)
   - [ ] Performance benchmarking
   - [ ] Memory profiling
   - [ ] Documentation

---

## Testing Strategy

**File**: `tests/expression-engine/test_phase4_eval.c`

```c
// Test 1: Simple literal evaluation
void test_eval_literal(void) {
    IRExpression* expr = ir_expr_literal_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    IRValue result = ir_expr_eval(compiled, NULL, 0);

    assert(result.type == VAR_TYPE_INT);
    assert(result.int_val == 42);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
}

// Test 2: Variable reference evaluation
void test_eval_var_ref(void) {
    IRExpression* expr = ir_expr_var_ref("count");

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "count", 100, 0);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(result.int_val == 100);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 3: Member access evaluation
void test_eval_member_access(void) {
    // Create an object with nested properties
    IRObject* user = ir_object_create(4);
    ir_object_set(user, "name", ir_value_string("Alice"));

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "user", ir_value_object(user), 0);

    IRExpression* expr = ir_expr_member_access(ir_expr_var_ref("user"), "name");
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(result.type == VAR_TYPE_STRING);
    assert(strcmp(result.string_val, "Alice") == 0);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 4: Binary operation evaluation
void test_eval_binary(void) {
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_literal_int(10),
        ir_expr_literal_int(32)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    assert(result.int_val == 42);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
}

// Test 5: Ternary evaluation
void test_eval_ternary(void) {
    IRExpression* expr = ir_expr_ternary(
        ir_expr_literal_bool(true),
        ir_expr_literal_int(1),
        ir_expr_literal_int(0)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    assert(result.int_val == 1);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
}

// Test 6: Method call evaluation
void test_eval_method_call(void) {
    IRValue array_val = ir_value_array_empty();
    ir_object_set(array_val.array_val, "items", ir_value_int(1));
    ir_object_set(array_val.array_val, "items", ir_value_int(2));
    ir_object_set(array_val.array_val, "items", ir_value_int(3));

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "arr", array_val, 0);

    IRExpression* expr = ir_expr_method_call(
        ir_expr_var_ref("arr"),
        "length",
        NULL,
        0
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(result.int_val == 3);

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Performance test
void test_eval_performance(void) {
    // Compare compiled vs interpreted performance
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("a"),
        ir_expr_var_ref("b")
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "a", 1000, 0);
    ir_executor_set_var_int(executor, "b", 2000, 0);

    // Benchmark compiled evaluation
    clock_t start = clock();
    for (int i = 0; i < 100000; i++) {
        IRValue result = ir_expr_eval(compiled, executor, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double compiled_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Compiled evaluation: %.4f seconds for 100k iterations\n", compiled_time);

    // Cleanup
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);

    // Should be significantly faster than interpreted
    assert(compiled_time < 1.0);  // Less than 1 second for 100k iterations
}
```

---

## Acceptance Criteria

Phase 4 is complete when:

- [ ] All bytecode opcodes are implemented and tested
- [ ] Variable resolution works across scopes
- [ ] Property access works for objects, arrays, and strings
- [ ] Method calls work for built-in types
- [ ] Control flow (jumps) works correctly
- [ ] Unit test coverage > 90%
- [ ] Performance: compiled evaluation is 10x+ faster than interpreted
- [ ] No memory leaks (verified with valgrind)
- [ ] Error handling prevents crashes on invalid expressions

---

## Next Phase

Once Phase 4 is complete, proceed to **[Phase 5: Expression Cache](./phase-5-expression-cache.md)**.

---

**Last Updated**: 2026-01-11
