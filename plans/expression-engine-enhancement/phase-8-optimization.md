# Phase 8: Performance Optimization

**Duration**: 3 days
**Dependencies**: Phase 5, Phase 7
**Status**: Pending

## Overview

Phase 8 focuses on final performance optimizations based on profiling results from Phase 7. This includes hot path optimization, memory usage reduction, and CPU cache improvements.

## Objectives

1. Profile and identify hot paths
2. Optimize bytecode execution loop
3. Reduce memory allocations
4. Improve CPU cache locality
5. Add specialized fast paths for common operations
6. Final documentation

---

## Profiling Results

### 1.1 Expected Hot Paths

Based on the expression engine design, the hot paths are:

1. **Variable lookup** - Most expressions access variables
2. **Stack operations** - Every bytecode operation uses the stack
3. **Property access** - Common in UI bindings
4. **Cache lookup** - Every expression evaluation starts here

---

## Optimization Strategies

### 2.1 Inline Critical Functions

**File**: `ir/ir_expression_eval.c`

```c
// Before: Function call overhead
static void stack_push(IREvalContext* ctx, IRValue value) {
    if (ctx->stack_top >= ctx->stack_capacity) {
        ctx->stack_capacity *= 2;
        ctx->stack = (IRValue*)realloc(ctx->stack, ctx->stack_capacity * sizeof(IRValue));
    }
    ctx->stack[ctx->stack_top++] = value;
}

// After: Inline hot path with separate slow path
static inline void stack_push_fast(IREvalContext* ctx, IRValue value) {
    // Fast path - no bounds check (caller ensures space)
    ctx->stack[ctx->stack_top++] = value;
}

static void stack_push(IREvalContext* ctx, IRValue value) {
    // Check if we need to grow
    if (ctx->stack_top >= ctx->stack_capacity) {
        // Grow by 50% instead of 2x for better memory usage
        uint32_t new_capacity = ctx->stack_capacity + (ctx->stack_capacity >> 1);
        if (new_capacity < 64) new_capacity = 64;
        ctx->stack = (IRValue*)realloc(ctx->stack, new_capacity * sizeof(IRValue));
        ctx->stack_capacity = new_capacity;
    }
    ctx->stack[ctx->stack_top++] = value;
}
```

### 2.2 Specialized Bytecode Execution

```c
// Specialized evaluation for simple expressions
static IRValue eval_simple_literal(IRCompiledExpr* compiled) {
    // For expressions that are just a literal, skip the whole loop
    if (compiled->code_size == 2 && compiled->code[0].opcode == OP_PUSH_INT) {
        return ir_value_int(compiled->code[0].operand3);
    }
    if (compiled->code_size == 2 && compiled->code[0].opcode == OP_PUSH_STRING) {
        return ir_value_string(compiled->string_pool[compiled->code[0].operand2]);
    }
    if (compiled->code_size == 2 && compiled->code[0].opcode == OP_PUSH_BOOL) {
        return ir_value_int(compiled->code[0].operand1 ? 1 : 0);
    }

    // Fallback to normal evaluation
    return eval_loop_normal(NULL, compiled);
}

// Specialized evaluation for single variable reference
static IRValue eval_single_var(IRCompiledExpr* compiled, IRExecutorContext* executor, uint32_t instance_id) {
    if (compiled->code_size == 2 && compiled->code[0].opcode == OP_LOAD_VAR) {
        const char* name = compiled->string_pool[compiled->code[0].operand2];
        return ir_executor_get_var(executor, name, instance_id);
    }

    return eval_loop_normal(executor, compiled, instance_id);
}
```

### 2.3 Direct Threaded Interpretation

**Note**: This is an advanced optimization using computed gotos (GCC extension)

```c
// Direct threading optimization (GCC/Clang specific)
#if defined(__GNUC__) || defined(__clang__)
#define USE_DIRECT_THREADED 1

// Labels for each opcode
static void* opcode_labels[] = {
    &&op_nop,
    &&op_push_int,
    &&op_push_string,
    &&op_push_bool,
    &&op_load_var,
    &&op_get_prop,
    &&op_add,
    // ... etc
};

IRValue eval_loop_threaded(IREvalContext* ctx, IRCompiledExpr* compiled) {
    uint32_t pc = 0;
    void** opcode_table = opcode_labels;

    // Fetch first instruction
    IRInstruction* instr = &compiled->code[pc];
    goto *opcode_table[instr->opcode];

op_push_int:
    stack_push_fast(ctx, ir_value_int(instr->operand3));
    instr = &compiled->code[++pc];
    goto *opcode_table[instr->opcode];

op_load_var: {
    const char* name = compiled->string_pool[instr->operand2];
    IRValue value = resolve_variable(ctx, name);
    stack_push_fast(ctx, value);
    instr = &compiled->code[++pc];
    goto *opcode_table[instr->opcode];
}

op_add: {
    IRValue right = stack_pop(ctx);
    IRValue left = stack_pop(ctx);
    stack_push_fast(ctx, op_add(left, right));
    ir_value_free(&left);
    ir_value_free(&right);
    instr = &compiled->code[++pc];
    goto *opcode_table[instr->opcode];
}

// ... other opcodes

op_halt:
    return stack_pop(ctx);
}

#endif
```

### 2.4 String Pool Optimization

```c
// Use string interning for frequently accessed property names
static const char* g_interned_strings[] = {
    "length", "name", "id", "value", "text",
    "toString", "toUpperCase", "toLowerCase",
    "push", "pop", "indexOf",
    // ... more common strings
};

// Fast string comparison using interned strings
static inline bool is_interned_string_equal(const char* a, const char* b) {
    // If pointers are equal, strings are equal (interned)
    if (a == b) return true;

    // Fall back to strcmp for non-interned strings
    return strcmp(a, b) == 0;
}

// Property access with interned strings
static IRValue get_property_fast(IRValue object, const char* property) {
    // Fast path for known properties
    if (property == g_interned_strings[0]) {  // "length"
        if (object.type == VAR_TYPE_ARRAY) {
            return ir_value_int(object.array_val.count);
        }
        if (object.type == VAR_TYPE_STRING) {
            return ir_value_int(strlen(object.string_val));
        }
    }

    // Normal path
    return get_property(object, property);
}
```

### 2.5 Memory Pool for Values

```c
// Memory pool for frequently allocated small values
typedef struct {
    IRValue* free_list;
    uint32_t free_count;
    uint32_t total_allocated;
} IRValuePool;

static IRValuePool* g_value_pool = NULL;

void ir_value_pool_init(void) {
    g_value_pool = (IRValuePool*)calloc(1, sizeof(IRValuePool));
    g_value_pool->free_count = 1000;
    g_value_pool->free_list = (IRValue*)calloc(g_value_pool->free_count, sizeof(IRValue));
}

IRValue ir_value_int_pool(int64_t value) {
    IRValue v = {.type = VAR_TYPE_INT, .int_val = value};
    return v;  // Small values don't need pooling
}

IRValue ir_value_string_pool(const char* str) {
    // Use pool for small strings
    size_t len = strlen(str);
    if (len < 32 && g_value_pool && g_value_pool->free_count > 0) {
        // Try to reuse from pool
        // (Implementation depends on use case)
    }
    return ir_value_string(str);
}
```

### 2.6 Inline Caching for Property Access

```c
// Inline cache for property access
typedef struct {
    const char* property_name;
    uint32_t last_offset;     // Last known offset in object
    IRObject* last_object;    // Last object type seen
    uint32_t hit_count;       // Cache hits
    uint32_t miss_count;      // Cache misses
} IRPropertyCache;

#define IC_CACHE_SIZE 4

typedef struct {
    IRPropertyCache caches[IC_CACHE_SIZE];
    uint32_t lru_counter;
} IRInlineCache;

static inline IRValue get_property_with_ic(IRValue object, const char* property, IRInlineCache* ic) {
    if (object.type != VAR_TYPE_OBJECT || !object.object_val) {
        return ir_value_null();
    }

    // Check inline cache
    for (int i = 0; i < IC_CACHE_SIZE; i++) {
        if (ic->caches[i].property_name == property) {
            ic->caches[i].hit_count++;

            // Fast path: use cached offset
            // (Implementation requires object layout tracking)
            return ir_object_get(object.object_val, property);
        }
    }

    // Cache miss - update cache
    uint32_t lru_idx = ic->lru_counter % IC_CACHE_SIZE;
    ic->caches[lru_idx].property_name = property;
    ic->caches[lru_idx].last_object = object.object_val;
    ic->caches[lru_idx].miss_count++;
    ic->lru_counter++;

    return ir_object_get(object.object_val, property);
}
```

---

## Memory Optimization

### 3.1 Reduce IRValue Size

```c
// Before: IRValue is 32+ bytes
typedef struct {
    VarType type;
    union {
        int64_t int_val;
        bool bool_val;
        char* string_val;
        struct {
            IRValue* items;
            uint32_t count;
            uint32_t capacity;
        } array_val;
        IRObject* object_val;
    };
} IRValue;

// After: Use smaller representation where possible
typedef struct {
    // Packed type + flags in single byte
    uint8_t type_and_flags;

    // Compact value representation
    union {
        int64_t int_val;           // 8 bytes
        const char* string_ptr;     // 8 bytes (pointer to interned string)
        struct {
            void* ptr;              // 8 bytes
            uint32_t size;          // 4 bytes
            uint16_t flags;         // 2 bytes
        } complex_val;
    };
} IRValue;

// Type extraction helpers
static inline VarType ir_value_get_type(IRValue* v) {
    return v->type_and_flags & 0x0F;
}

static inline bool ir_value_is_inline(IRValue* v) {
    return v->type_and_flags & 0x10;
}
```

### 3.2 Expression AST Recycling

```c
// Pool for reusing AST nodes
typedef struct {
    IRExpression* free_list;
    uint32_t free_count;
} IRExprPool;

static IRExprPool g_expr_pool = {NULL, 0};

IRExpression* ir_expr_alloc_pooled(void) {
    if (g_expr_pool.free_list) {
        IRExpression* expr = g_expr_pool.free_list;
        g_expr_pool.free_list = *(IRExpression**)expr;
        g_expr_pool.free_count--;
        return expr;
    }

    return (IRExpression*)calloc(1, sizeof(IRExpression));
}

void ir_expr_free_pooled(IRExpression* expr) {
    // Recursively free children
    // Then add to pool
    *(IRExpression**)expr = g_expr_pool.free_list;
    g_expr_pool.free_list = expr;
    g_expr_pool.free_count++;
}
```

---

## Implementation Steps

### Day 1: Profiling & Hot Path Optimization

1. **Morning** (4 hours)
   - [ ] Run profiler on test suite
   - [ ] Identify top 10 hot functions
   - [ ] Implement stack operation inlining
   - [ ] Benchmark improvements

2. **Afternoon** (4 hours)
   - [ ] Implement specialized eval paths
   - [ ] Add direct threading (if available)
   - [ ] Benchmark improvements

### Day 2: Memory Optimization

1. **Morning** (4 hours)
   - [ ] Implement string interning
   - [ ] Add inline caching for property access
   - [ ] Benchmark improvements

2. **Afternoon** (4 hours)
   - [ ] Optimize IRValue size
   - [ ] Implement expression pooling
   - [ ] Measure memory reduction

### Day 3: Final Polish & Documentation

1. **Morning** (4 hours)
   - [ ] Run full test suite with optimizations
   - [ ] Verify no regressions
   - [ ] Document optimization techniques

2. **Afternoon** (4 hours)
   - [ ] Create performance comparison report
   - [ ] Update README with performance numbers
   - [ ] Final code review

---

## Testing Strategy

### 4.1 Optimization Validation

```c
// Test that optimizations don't change behavior
void test_optimization_correctness(void) {
    // Create various expressions
    IRExpression* exprs[] = {
        ir_expr_literal_int(42),
        ir_expr_var_ref("count"),
        ir_expr_member_access(ir_expr_var_ref("user"), "name"),
        ir_expr_binary(BINARY_OP_ADD, ir_expr_var_ref("a"), ir_expr_var_ref("b")),
        ir_expr_ternary(ir_expr_var_ref("flag"), ir_expr_literal_int(1), ir_expr_literal_int(0)),
    };

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "count", 42, 0);
    ir_executor_set_var_int(executor, "a", 10, 0);
    ir_executor_set_var_int(executor, "b", 32, 0);

    for (size_t i = 0; i < sizeof(exprs)/sizeof(exprs[0]); i++) {
        // Compile with optimizations off
        IRCompiledExpr* normal = ir_expr_compile(exprs[i]);

        // Evaluate both
        IRValue result_normal = ir_expr_eval(normal, executor, 0);
        IRValue result_optimized = ir_expr_eval_optimized(exprs[i], executor, 0);

        // Results should be identical
        assert(values_equal(&result_normal, &result_optimized));

        ir_value_free(&result_normal);
        ir_value_free(&result_optimized);
        ir_expr_compiled_free(normal);
    }

    ir_executor_destroy(executor);
}

// Memory usage test
void test_optimization_memory(void) {
    size_t baseline = get_current_memory();

    // Create and compile many expressions
    for (int i = 0; i < 1000; i++) {
        IRExpression* expr = ir_expr_var_ref("var" + i);
        IRCompiledExpr* compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(g_cache, expr, compiled);
    }

    size_t after = get_current_memory();
    size_t used = after - baseline;

    // Should use less than 10MB for 1000 expressions
    assert(used < 10 * 1024 * 1024);
}
```

---

## Performance Targets

### 5.1 Target Metrics

| Operation | Before | Target | After |
|-----------|--------|--------|-------|
| Literal | 50 ns | < 10 ns | TBD |
| Variable | 100 ns | < 20 ns | TBD |
| Member access (1 deep) | 200 ns | < 50 ns | TBD |
| Member access (3 deep) | 500 ns | < 100 ns | TBD |
| Binary op | 150 ns | < 30 ns | TBD |
| Ternary | 300 ns | < 80 ns | TBD |
| Cache lookup | 200 ns | < 20 ns | TBD |

### 5.2 Memory Targets

| Metric | Before | Target | After |
|--------|--------|--------|-------|
| IRValue size | 32 bytes | < 16 bytes | TBD |
| Compiled expr (simple) | 200 bytes | < 100 bytes | TBD |
| Cache entry | 400 bytes | < 200 bytes | TBD |

---

## Acceptance Criteria

Phase 8 is complete when:

- [ ] All performance targets met
- [ ] All memory targets met
- [ ] No correctness regressions
- [ ] Valgrind clean
- [ ] Test suite passes
- [ ] Documentation complete
- [ ] Performance report generated

---

## Performance Report Template

```markdown
# Expression Engine Performance Report

## Summary

| Metric | Value |
|--------|-------|
| Total Tests | XXX |
| Pass Rate | 100% |
| Memory Leaks | 0 |
| Code Coverage | XX% |

## Performance Results

### Expression Evaluation

| Operation | Time (ns/op) | Target | Status |
|-----------|--------------|--------|--------|
| Literal | XX | < 10 | ✓/✗ |
| Variable | XX | < 20 | ✓/✗ |
| Member Access (1) | XX | < 50 | ✓/✗ |
| Member Access (3) | XX | < 100 | ✓/✗ |
| Binary Op | XX | < 30 | ✓/✗ |
| Ternary | XX | < 80 | ✓/✗ |
| Cache Lookup | XX | < 20 | ✓/✗ |

### Memory Usage

| Component | Size (bytes) | Target | Status |
|-----------|--------------|--------|--------|
| IRValue | XX | < 16 | ✓/✗ |
| Compiled Expr | XX | < 100 | ✓/✗ |
| Cache Entry | XX | < 200 | ✓/✗ |

### Cache Effectiveness

| Metric | Value |
|--------|-------|
| Hit Rate | XX% |
| Eviction Rate | XX% |
| Avg Lookup Time | XX ns |

## Comparison with Previous Implementation

| Scenario | Old (µs) | New (µs) | Speedup |
|----------|----------|----------|---------|
| Simple variable | X | Y | Zx |
| Member access | X | Y | Zx |
| Complex expression | X | Y | Zx |

## Optimizations Applied

1. Inline critical functions
2. Specialized evaluation paths
3. Direct threading
4. String interning
5. Inline caching
6. Value pooling
```

---

## Completion Checklist

- [ ] All performance targets met
- [ ] All memory targets met
- [ ] Test suite passes 100%
- [ ] No memory leaks
- [ ] Documentation complete
- [ ] Performance report generated
- [ ] Code reviewed
- [ ] Ready for merge

---

## Project Completion

Upon completion of Phase 8:

1. **Create Pull Request** with all changes
2. **Update CHANGELOG.md** with new features
3. **Update CLAUDE.md** with new capabilities
4. **Tag release** (e.g., v1.1.0)

---

**Last Updated**: 2026-01-11
