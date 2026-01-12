# Phase 3: Bytecode Compiler

**Duration**: 5 days
**Dependencies**: Phase 1
**Status**: Pending

## Overview

Phase 3 implements the bytecode compiler that converts AST expressions into optimized bytecode. This is the core performance enhancement - compiled bytecode executes 10-100x faster than AST traversal.

## Objectives

1. Design bytecode instruction set and encoding
2. Implement AST-to-bytecode compiler
3. Add optimization passes (constant folding, dead code elimination)
4. Support all expression types from Phase 1
5. Generate position-independent bytecode for caching

---

## Bytecode Architecture

### 1.1 Virtual Machine Design

The expression VM is a **stack-based** virtual machine (similar to JVM or Lua). Stack-based design was chosen because:

- Simpler bytecode generation (no register allocation)
- Smaller bytecode size
- Easier to implement optimized evaluation
- Suitable for expression evaluation (short-lived computations)

### 1.2 Stack Layout

```
┌────────────────────────────────────────────────────┐
│                   Operand Stack                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │   ...    │ │  value3  │ │  value2  │ │ value1  │
│  └──────────┘ └──────────┘ └──────────┘           │
│                                              ▲     │
│                                      grows upward  │
└────────────────────────────────────────────────────┘

Stack Pointer (SP) points to top of stack
```

### 1.3 Instruction Encoding

Each instruction is 8 bytes (64 bits):

```
┌──────────────┬──────────────┬──────────────────────────────────┐
│  Opcode      │  Operand 1   │          Operand 2 + 3            │
│  (1 byte)    │  (1 byte)    │          (6 bytes)                │
├──────────────┼──────────────┼──────────────────────────────────┤
│  OP_LOAD_VAR │  reg_index   │  [2 byte pool_id] [4 byte offset]│
│  OP_GET_PROP │  prop_index  │  [padding]                       │
│  OP_CALL     │  builtin_idx │  [arg_count]                     │
│  OP_ADD      │  [unused]    │  [padding]                       │
└──────────────┴──────────────┴──────────────────────────────────┘
```

---

## Compiler Implementation

### 2.1 Compiler Context

**File**: `ir/ir_expression_compiler.c`

```c
// Compiler context
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

    // Metadata
    uint32_t max_stack_depth;   // For stack allocation
    uint32_t temp_count;        // Number of temporaries needed

    // Error handling
    char error_msg[256];
    bool has_error;
} IRExprCompiler;

// Initialize compiler
IRExprCompiler* ir_expr_compiler_create(void) {
    IRExprCompiler* comp = (IRExprCompiler*)calloc(1, sizeof(IRExprCompiler));
    comp->code_capacity = 256;
    comp->code = (IRInstruction*)calloc(comp->code_capacity, sizeof(IRInstruction));
    comp->pool.capacity = 64;
    comp->pool.strings = (char**)calloc(comp->pool.capacity, sizeof(char*));
    comp->pool.hashes = (uint32_t*)calloc(comp->pool.capacity, sizeof(uint32_t));
    return comp;
}

void ir_expr_compiler_destroy(IRExprCompiler* comp) {
    if (!comp) return;

    // Free string pool
    for (uint32_t i = 0; i < comp->pool.count; i++) {
        free(comp->pool.strings[i]);
    }
    free(comp->pool.strings);
    free(comp->pool.hashes);

    // Free code
    free(comp->code);
    free(comp);
}
```

### 2.2 Bytecode Emission

```c
// Emit a single instruction
static uint32_t emit(IRExprCompiler* comp, IROpcode opcode,
                     uint8_t op1, uint16_t op2, int32_t op3) {
    // Grow buffer if needed
    if (comp->code_size >= comp->code_capacity) {
        comp->code_capacity *= 2;
        comp->code = (IRInstruction*)realloc(comp->code,
            comp->code_capacity * sizeof(IRInstruction));
    }

    uint32_t pc = comp->code_size++;
    IRInstruction* instr = &comp->code[pc];

    instr->opcode = opcode;
    instr->operand1 = op1;
    instr->operand2 = op2;
    instr->operand3 = op3;

    return pc;  // Return program counter for patching
}

// Add string to pool and return index
static uint32_t pool_add(IRExprCompiler* comp, const char* str) {
    uint32_t hash = hash_string(str);

    // Check if already exists
    for (uint32_t i = 0; i < comp->pool.count; i++) {
        if (comp->pool.hashes[i] == hash &&
            strcmp(comp->pool.strings[i], str) == 0) {
            return i;
        }
    }

    // Add new entry
    if (comp->pool.count >= comp->pool.capacity) {
        comp->pool.capacity *= 2;
        comp->pool.strings = (char**)realloc(comp->pool.strings,
            comp->pool.capacity * sizeof(char*));
        comp->pool.hashes = (uint32_t*)realloc(comp->pool.hashes,
            comp->pool.capacity * sizeof(uint32_t));
    }

    uint32_t idx = comp->pool.count++;
    comp->pool.strings[idx] = strdup(str);
    comp->pool.hashes[idx] = hash;
    return idx;
}
```

### 2.3 Compilation Functions

```c
// Compile expression to bytecode
IRCompiledExpr* ir_expr_compile(IRExpression* expr) {
    IRExprCompiler* comp = ir_expr_compiler_create();

    // Compile the expression
    compile_expr(comp, expr);

    // Emit final return (implicit)
    emit(comp, OP_HALT, 0, 0, 0);

    // Build result
    IRCompiledExpr* result = (IRCompiledExpr*)calloc(1, sizeof(IRCompiledExpr));
    result->code = comp->code;
    result->code_size = comp->code_size;
    result->code_capacity = comp->code_capacity;
    result->temp_count = comp->temp_count;
    result->max_stack_depth = comp->max_stack_depth;

    // Transfer ownership of string pool
    result->string_pool = comp->pool.strings;
    result->string_pool_count = comp->pool.count;

    // Free compiler but keep code
    free(comp->pool.hashes);
    free(comp);

    return result;
}

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

        case EXPR_LITERAL_STRING:
            compile_literal_string(comp, expr->string_value);
            break;

        case EXPR_LITERAL_BOOL:
            emit(comp, OP_PUSH_BOOL, expr->bool_value ? 1 : 0, 0, 0);
            break;

        case EXPR_VAR_REF:
            compile_var_ref(comp, expr->var_ref.name);
            break;

        case EXPR_MEMBER_ACCESS:
            compile_member_access(comp, &expr->member_access);
            break;

        case EXPR_COMPUTED_MEMBER:
            compile_computed_member(comp, &expr->computed_member);
            break;

        case EXPR_METHOD_CALL:
            compile_method_call(comp, &expr->method_call);
            break;

        case EXPR_BINARY:
            compile_binary(comp, &expr->binary);
            break;

        case EXPR_UNARY:
            compile_unary(comp, &expr->unary);
            break;

        case EXPR_TERNARY:
            compile_ternary(comp, &expr->ternary);
            break;

        case EXPR_GROUP:
            compile_expr(comp, expr->group.inner);
            break;

        case EXPR_INDEX:
            compile_index_access(comp, &expr->index_access);
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
```

### 2.4 Literal Compilation

```c
static void compile_literal_int(IRExprCompiler* comp, int64_t value) {
    // Small integers fit in operand3
    if (value >= INT32_MIN && value <= INT32_MAX) {
        emit(comp, OP_PUSH_INT, 0, 0, (int32_t)value);
    } else {
        // Large integers need special handling
        uint32_t idx = pool_add(comp, "");  // Placeholder for large int pool
        emit(comp, OP_PUSH_INT, 0, idx, 0);
    }
}

static void compile_literal_string(IRExprCompiler* comp, const char* value) {
    uint32_t idx = pool_add(comp, value);
    emit(comp, OP_PUSH_STRING, 0, (uint16_t)idx, 0);
}
```

### 2.5 Variable Reference Compilation

```c
static void compile_var_ref(IRExprCompiler* comp, const char* name) {
    uint32_t idx = pool_add(comp, name);

    // Try to determine if this is a local or global variable
    // For now, assume local and emit LOAD_VAR
    // The evaluator will handle scope resolution
    emit(comp, OP_LOAD_VAR, 0, (uint16_t)idx, 0);
}
```

### 2.6 Member Access Compilation

```c
static void compile_member_access(IRExprCompiler* comp, IRMemberAccess* access) {
    // Compile object expression first (pushes object on stack)
    compile_expr(comp, access->object);

    // Get property (object is on stack)
    uint32_t prop_idx = pool_add(comp, access->property);
    emit(comp, OP_GET_PROP, 0, (uint16_t)prop_idx, 0);
}

static void compile_computed_member(IRExprCompiler* comp, IRComputedMemberAccess* access) {
    // Compile object expression
    compile_expr(comp, access->object);

    // Compile key expression (pushes key on stack)
    compile_expr(comp, access->key);

    // Get computed property (object and key are on stack)
    emit(comp, OP_GET_PROP_COMPUTED, 0, 0, 0);
}
```

### 2.7 Binary Operation Compilation

```c
static void compile_binary(IRExprCompiler* comp, IRBinaryOp* binary) {
    // Compile left operand (pushes on stack)
    compile_expr(comp, binary->left);

    // Compile right operand (pushes on stack)
    compile_expr(comp, binary->right);

    // Emit appropriate operator
    IROpcode op;
    switch (binary->op) {
        case BINARY_OP_ADD:    op = OP_ADD; break;
        case BINARY_OP_SUB:    op = OP_SUB; break;
        case BINARY_OP_MUL:    op = OP_MUL; break;
        case BINARY_OP_DIV:    op = OP_DIV; break;
        case BINARY_OP_MOD:    op = OP_MOD; break;
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
```

### 2.8 Ternary Compilation

```c
static void compile_ternary(IRExprCompiler* comp, IRTernaryExpression* ternary) {
    // Compile condition
    compile_expr(comp, ternary->condition);

    // Jump if false
    uint32_t jump_else = emit(comp, OP_JUMP_IF_FALSE, 0, 0, 0);

    // Compile then branch
    compile_expr(comp, ternary->then_expr);

    // Jump past else branch
    uint32_t jump_end = emit(comp, OP_JUMP, 0, 0, 0);

    // Patch else jump target
    comp->code[jump_else].operand3 = (int32_t)(comp->code_size);

    // Compile else branch
    compile_expr(comp, ternary->else_expr);

    // Patch end jump target
    comp->code[jump_end].operand3 = (int32_t)(comp->code_size);
}
```

### 2.9 Method Call Compilation

```c
static void compile_method_call(IRExprCompiler* comp, IRMethodCall* call) {
    // Compile receiver (pushes object on stack)
    compile_expr(comp, call->receiver);

    // Compile arguments in reverse order (for correct stack order)
    for (int32_t i = (int32_t)call->arg_count - 1; i >= 0; i--) {
        compile_expr(comp, call->args[i]);
    }

    // Emit method call
    uint32_t method_idx = pool_add(comp, call->method_name);
    emit(comp, OP_CALL_METHOD, 0, (uint16_t)method_idx, (int32_t)call->arg_count);
}
```

---

## Optimization Passes

### 3.1 Constant Folding

**File**: `ir/ir_expression_optimizer.c`

```c
// Constant folding: evaluate constant expressions at compile time
static IRExpression* optimize_const_fold(IRExpression* expr) {
    if (!expr) return NULL;

    switch (expr->type) {
        case EXPR_BINARY: {
            IRExpression* left = optimize_const_fold(expr->binary.left);
            IRExpression* right = optimize_const_fold(expr->binary.right);

            // If both operands are constants, evaluate now
            if (is_constant(left) && is_constant(right)) {
                return eval_binary_const(left, right, expr->binary.op);
            }

            // Otherwise, optimize children and return
            IRExpression* result = ir_expr_alloc();
            result->type = EXPR_BINARY;
            result->binary.op = expr->binary.op;
            result->binary.left = left;
            result->binary.right = right;
            return result;
        }

        case EXPR_LITERAL_INT:
        case EXPR_LITERAL_STRING:
        case EXPR_LITERAL_BOOL:
            // Constants are already optimal
            return expr_copy(expr);

        default:
            // For other types, recursively optimize children
            return optimize_children(expr);
    }
}

static bool is_constant(IRExpression* expr) {
    return expr && (expr->type == EXPR_LITERAL_INT ||
                    expr->type == EXPR_LITERAL_STRING ||
                    expr->type == EXPR_LITERAL_BOOL);
}

static IRExpression* eval_binary_const(IRExpression* left, IRExpression* right, BinaryOp op) {
    IRExpression* result = ir_expr_alloc();

    if (left->type == EXPR_LITERAL_INT && right->type == EXPR_LITERAL_INT) {
        result->type = EXPR_LITERAL_INT;
        int64_t l = left->int_value;
        int64_t r = right->int_value;

        switch (op) {
            case BINARY_OP_ADD: result->int_value = l + r; break;
            case BINARY_OP_SUB: result->int_value = l - r; break;
            case BINARY_OP_MUL: result->int_value = l * r; break;
            case BINARY_OP_DIV: result->int_value = r != 0 ? l / r : 0; break;
            case BINARY_OP_MOD: result->int_value = r != 0 ? l % r : 0; break;
            case BINARY_OP_EQ:  result->int_value = l == r; break;
            case BINARY_OP_NEQ: result->int_value = l != r; break;
            case BINARY_OP_LT:  result->int_value = l < r; break;
            case BINARY_OP_LTE: result->int_value = l <= r; break;
            case BINARY_OP_GT:  result->int_value = l > r; break;
            case BINARY_OP_GTE: result->int_value = l >= r; break;
            case BINARY_OP_AND: result->int_value = l && r; break;
            case BINARY_OP_OR:  result->int_value = l || r; break;
        }
    }

    // String concatenation
    if (left->type == EXPR_LITERAL_STRING && right->type == EXPR_LITERAL_STRING && op == BINARY_OP_ADD) {
        result->type = EXPR_LITERAL_STRING;
        size_t len = strlen(left->string_value) + strlen(right->string_value) + 1;
        result->string_value = (char*)malloc(len);
        snprintf(result->string_value, len, "%s%s", left->string_value, right->string_value);
    }

    return result;
}
```

### 3.2 Dead Code Elimination

```c
// Eliminate code after unconditional jumps
static void optimize_dead_code(IRExprCompiler* comp) {
    for (uint32_t pc = 0; pc < comp->code_size; pc++) {
        IRInstruction* instr = &comp->code[pc];

        if (instr->opcode == OP_JUMP) {
            // Mark instructions after jump as dead until we hit the jump target
            uint32_t target = (uint32_t)instr->operand3;
            for (uint32_t i = pc + 1; i < target && i < comp->code_size; i++) {
                if (comp->code[i].opcode != OP_NOP) {
                    comp->code[i].opcode = OP_NOP;
                }
            }
        }
    }
}
```

---

## Implementation Steps

### Day 1: Compiler Framework

1. **Morning** (4 hours)
   - [ ] Create `ir/ir_expression_compiler.h` and `.c`
   - [ ] Implement `IRExprCompiler` structure
   - [ ] Implement `emit()` and `pool_add()` functions
   - [ ] Write basic unit tests

2. **Afternoon** (4 hours)
   - [ ] Implement `ir_expr_compile()` entry point
   - [ ] Implement `compile_expr()` dispatcher
   - [ ] Add literal compilation (int, string, bool)
   - [ ] Test literal compilation

### Day 2: Variable & Member Access

1. **Morning** (4 hours)
   - [ ] Implement variable reference compilation
   - [ ] Implement static member access compilation
   - [ ] Implement computed member access compilation
   - [ ] Write tests

2. **Afternoon** (4 hours)
   - [ ] Implement array index compilation
   - [ ] Implement chained member access optimization
   - [ ] Test complex property access

### Day 3: Operations

1. **Morning** (4 hours)
   - [ ] Implement binary operation compilation
   - [ ] Implement unary operation compilation
   - [ ] Implement ternary operator compilation
   - [ ] Write tests

2. **Afternoon** (4 hours)
   - [ ] Implement method call compilation
   - [ ] Implement function call compilation
   - [ ] Test method chaining

### Day 4: Optimization

1. **Morning** (4 hours)
   - [ ] Implement constant folding pass
   - [ ] Implement dead code elimination
   - [ ] Add peephole optimization

2. **Afternoon** (4 hours)
   - [ ] Implement optimization pipeline
   - [ ] Benchmark optimization effectiveness
   - [ ] Test with real expressions

### Day 5: Testing & Polish

1. **Morning** (4 hours)
   - [ ] Comprehensive test suite
   - [ ] Memory leak detection (valgrind)
   - [ ] Edge case testing

2. **Afternoon** (4 hours)
   - [ ] Documentation
   - [ ] Code review
   - [ ] Fix any remaining bugs

---

## Testing Strategy

**File**: `tests/expression-engine/test_phase3_compiler.c`

```c
// Test 1: Simple literal compilation
void test_compile_literal(void) {
    IRExpression expr = {
        .type = EXPR_LITERAL_INT,
        .int_value = 42
    };

    IRCompiledExpr* compiled = ir_expr_compile(&expr);

    assert(compiled != NULL);
    assert(compiled->code_size == 2);  // PUSH_INT, HALT
    assert(compiled->code[0].opcode == OP_PUSH_INT);
    assert(compiled->code[0].operand3 == 42);

    ir_expr_compiled_free(compiled);
}

// Test 2: Variable reference compilation
void test_compile_var_ref(void) {
    IRExpression expr = {
        .type = EXPR_VAR_REF,
        .var_ref = {.name = "count"}
    };

    IRCompiledExpr* compiled = ir_expr_compile(&expr);

    assert(compiled->code[0].opcode == OP_LOAD_VAR);
    assert(strcmp(compiled->string_pool[compiled->code[0].operand2], "count") == 0);

    ir_expr_compiled_free(compiled);
}

// Test 3: Member access compilation
void test_compile_member_access(void) {
    // Create: user.name
    IRExpression* var_expr = ir_expr_var_ref("user");
    IRExpression* member_expr = ir_expr_member_access(var_expr, "name");

    IRCompiledExpr* compiled = ir_expr_compile(member_expr);

    // Should be: LOAD_VAR "user", GET_PROP "name"
    assert(compiled->code[0].opcode == OP_LOAD_VAR);
    assert(compiled->code[1].opcode == OP_GET_PROP);

    ir_expr_compiled_free(compiled);
    ir_expr_free(member_expr);
}

// Test 4: Binary operation compilation
void test_compile_binary(void) {
    // Create: a + b
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("a"),
        ir_expr_var_ref("b")
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Should be: LOAD_VAR "a", LOAD_VAR "b", ADD
    assert(compiled->code[0].opcode == OP_LOAD_VAR);
    assert(compiled->code[1].opcode == OP_LOAD_VAR);
    assert(compiled->code[2].opcode == OP_ADD);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
}

// Test 5: Ternary compilation
void test_compile_ternary(void) {
    // Create: isValid ? 1 : 0
    IRExpression* expr = ir_expr_ternary(
        ir_expr_var_ref("isValid"),
        ir_expr_literal_int(1),
        ir_expr_literal_int(0)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Should be: LOAD_VAR, JUMP_IF_FALSE, PUSH_INT 1, JUMP, PUSH_INT 0
    assert(compiled->code[0].opcode == OP_LOAD_VAR);
    assert(compiled->code[1].opcode == OP_JUMP_IF_FALSE);
    assert(compiled->code[2].opcode == OP_PUSH_INT);
    assert(compiled->code[2].operand3 == 1);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
}

// Test 6: Constant folding
void test_const_folding(void) {
    // Create: 2 + 3 (should fold to 5)
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_literal_int(2),
        ir_expr_literal_int(3)
    );

    IRExpression* optimized = optimize_const_fold(expr);
    IRCompiledExpr* compiled = ir_expr_compile(optimized);

    // Should compile to single PUSH_INT 5
    assert(compiled->code_size == 2);
    assert(compiled->code[0].opcode == OP_PUSH_INT);
    assert(compiled->code[0].operand3 == 5);

    ir_expr_compiled_free(compiled);
    ir_expr_free(optimized);
    ir_expr_free(expr);
}
```

---

## Acceptance Criteria

Phase 3 is complete when:

- [x] All expression types compile to valid bytecode
- [x] Bytecode is position-independent (can be cached)
- [x] Constant folding works for arithmetic and logical operations
- [x] Dead code elimination removes unreachable code
- [x] Unit test coverage > 90%
- [x] No memory leaks (verified with valgrind)
- [x] Bytecode can be disassembled for debugging

---

## Completion Summary

**Status**: ✅ Complete (2026-01-11)

**Implementation Details**:

1. **Compiler Framework** (`ir/ir_expression_compiler.c`):
   - `IRExprCompiler` structure with code buffer, string pool, and int pool
   - `emit()` function for bytecode emission
   - `pool_add()` function for string deduplication
   - `int_pool_add()` function for large integer literals
   - `patch_jump()` for back-patching jump targets

2. **Compilation Functions** (all expression types):
   - Literals: int, float, string, bool, null
   - Variables: `OP_LOAD_VAR`
   - Member access: `OP_GET_PROP` (static), `OP_GET_PROP_COMPUTED` (dynamic)
   - Index access: `OP_GET_INDEX`
   - Binary operations: 12 operators supported
   - Unary operations: `OP_NOT`, `OP_NEGATE`
   - Ternary: conditional branching with `OP_JUMP_IF_FALSE`
   - Method/function calls: `OP_CALL_METHOD`, `OP_CALL_FUNCTION`

3. **Optimization Passes**:
   - Constant folding: evaluates constant expressions at compile time
   - Dead code elimination: removes unreachable instructions
   - Works for binary, unary, and ternary operations

4. **Disassembly**: `ir_bytecode_disassemble()` for debugging

5. **Test Coverage**: 27 unit tests, 100% passing

**Files Modified/Created**:
- `ir/ir_expression_compiler.c` - Compiler implementation (1600+ lines)
- `ir/ir_expression_compiler.h` - Added optimization and disassembly declarations
- `ir/test_expression_compiler.c` - 27 unit tests
- `ir/Makefile` - Added test targets

---

## Bytecode Examples

### Example 1: Simple Variable

```
Expression: count
Bytecode:
  0: LOAD_VAR    [pool_idx=0]  ; "count"
  1: HALT
```

### Example 2: Member Access

```
Expression: user.profile.name
Bytecode:
  0: LOAD_VAR    [pool_idx=0]  ; "user"
  1: GET_PROP    [pool_idx=1]  ; "profile"
  2: GET_PROP    [pool_idx=2]  ; "name"
  3: HALT
```

### Example 3: Binary Operation

```
Expression: a + b * 2
Bytecode:
  0: LOAD_VAR    [pool_idx=0]  ; "a"
  1: LOAD_VAR    [pool_idx=1]  ; "b"
  2: PUSH_INT    [value=2]
  3: MUL
  4: ADD
  5: HALT
```

### Example 4: Ternary

```
Expression: isValid ? 1 : 0
Bytecode:
  0: LOAD_VAR    [pool_idx=0]  ; "isValid"
  1: JUMP_IF_FALSE [offset=3]
  2: PUSH_INT    [value=1]
  3: JUMP        [offset=2]
  4: PUSH_INT    [value=0]
  5: HALT
```

---

## Next Phase

Once Phase 3 is complete, proceed to **[Phase 4: Evaluation Engine](./phase-4-evaluation-engine.md)**, which implements the bytecode interpreter.

---

**Last Updated**: 2026-01-11
