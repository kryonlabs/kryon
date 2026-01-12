# Phase 7: Integration & Testing

**Duration**: 4 days
**Dependencies**: All previous phases
**Status**: Pending

## Overview

Phase 7 focuses on comprehensive integration testing, ensuring all components work together correctly. This includes end-to-end tests, backward compatibility verification, and real-world usage examples.

## Objectives

1. Create comprehensive test suite
2. Verify backward compatibility with existing code
3. Test round-trip conversion (TSX → KIR → load → execute)
4. Create example applications demonstrating new features
5. Performance benchmarking
6. Documentation completion

---

## Test Suite Structure

### 1.1 Test Organization

```
tests/expression-engine/
├── test_phase1_data_structures.c
├── test_phase2_parser.c
├── test_phase3_compiler.c
├── test_phase4_eval.c
├── test_phase5_cache.c
├── test_phase6_builtins.c
├── test_integration.c           <- NEW
├── test_backward_compat.c       <- NEW
├── test_performance.c           <- NEW
└── test_examples.c              <- NEW
```

---

## Integration Tests

### 2.1 End-to-End Expression Pipeline

**File**: `tests/expression-engine/test_integration.c`

```c
#include "ir/expression_engine.h"
#include "ir/ir_executor.h"

// Test 1: Simple variable reference through full pipeline
void test_integration_simple_var(void) {
    // Create expression AST
    IRExpression* expr = ir_expr_var_ref("count");

    // Compile to bytecode
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Create executor with variable
    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "count", 42, 0);

    // Evaluate
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(result.type == VAR_TYPE_INT);
    assert(result.int_val == 42);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 2: Nested member access
void test_integration_nested_access(void) {
    // user.profile.settings.theme
    IRExpression* expr = ir_expr_member_access(
        ir_expr_member_access(
            ir_expr_member_access(
                ir_expr_var_ref("user"),
                "profile"
            ),
            "settings"
        ),
        "theme"
    );

    // Create object: { profile: { settings: { theme: "dark" } } }
    IRObject* settings = ir_object_create(4);
    ir_object_set(settings, "theme", ir_value_string("dark"));

    IRObject* profile = ir_object_create(4);
    ir_object_set(profile, "settings", ir_value_object(settings));

    IRObject* user = ir_object_create(4);
    ir_object_set(user, "profile", ir_value_object(profile));

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "user", ir_value_object(user), 0);

    // Compile and evaluate
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(result.type == VAR_TYPE_STRING);
    assert(strcmp(result.string_val, "dark") == 0);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 3: Ternary with member access
void test_integration_ternary_member(void) {
    // user.isActive ? user.name : "Guest"
    IRExpression* expr = ir_expr_ternary(
        ir_expr_member_access(ir_expr_var_ref("user"), "isActive"),
        ir_expr_member_access(ir_expr_var_ref("user"), "name"),
        ir_expr_literal_string("Guest")
    );

    // Test with active user
    IRObject* user = ir_object_create(4);
    ir_object_set(user, "isActive", ir_value_bool(true));
    ir_object_set(user, "name", ir_value_string("Alice"));

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "user", ir_value_object(user), 0);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(strcmp(result.string_val, "Alice") == 0);
    ir_value_free(&result);

    // Test with inactive user
    ir_object_set(user, "isActive", ir_value_bool(false));
    result = ir_expr_eval(compiled, executor, 0);

    assert(strcmp(result.string_val, "Guest") == 0);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 4: Method call on array
void test_integration_array_method(void) {
    // items.push(newItem)
    IRExpression* expr = ir_expr_method_call(
        ir_expr_var_ref("items"),
        "push",
        (IRExpression*[]){
            ir_expr_var_ref("newItem")
        },
        1
    );

    IRExecutorContext* executor = ir_executor_create();

    // Create array
    IRValue array = ir_value_array_empty();
    ir_executor_set_var(executor, "items", array, 0);
    ir_executor_set_var_int(executor, "newItem", 42, 0);

    // Compile and evaluate
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    // Check array has new item
    IRValue check_array = ir_executor_get_var(executor, "items", 0);
    assert(check_array.array_val.count == 1);
    assert(check_array.array_val.items[0].int_val == 42);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Test 5: Complex expression with caching
void test_integration_caching(void) {
    IRExprCache* cache = ir_expr_cache_create(100);

    // Create complex expression
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("a"),
        ir_expr_binary(
            BINARY_OP_MUL,
            ir_expr_var_ref("b"),
            ir_expr_literal_int(2)
        )
    );

    // First lookup - should miss
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    assert(found == NULL);

    // Compile and cache
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Second lookup - should hit
    found = ir_expr_cache_lookup(cache, expr);
    assert(found == compiled);

    // Verify statistics
    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);
    assert(stats.hits == 1);
    assert(stats.misses == 1);

    // Cleanup
    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
}

// Test 6: Builtin function call
void test_integration_builtin(void) {
    // string.toUpperCase(name)
    IRExpression* expr = ir_expr_method_call(
        ir_expr_var_ref("name"),
        "toUpperCase",
        NULL,
        0
    );

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "name", ir_value_string("alice"), 0);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, executor, 0);

    assert(strcmp(result.string_val, "ALICE") == 0);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}
```

---

## Backward Compatibility Tests

### 3.1 Existing Expression Format

**File**: `tests/expression-engine/test_backward_compat.c`

```c
// Test that old JSON expression format still works
void test_backward_compat_json_format(void) {
    // Old format: {"type":0,"int_value":42} (EXPR_LITERAL_INT)
    const char* old_expr_json = "{\"type\":0,\"int_value\":42}";

    cJSON* json = cJSON_Parse(old_expr_json);
    IRExpression* expr = ir_expr_from_json(json);

    assert(expr->type == EXPR_LITERAL_INT);
    assert(expr->int_value == 42);

    // Should compile and execute
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    assert(result.int_val == 42);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    cJSON_Delete(json);
}

// Test old binary expression format
void test_backward_compat_binary(void) {
    // Old format for binary: {"type":5,"op":"add","left":{...},"right":{...}}
    const char* old_binary_json = "{"
        "\"type\":5,"
        "\"op\":\"add\","
        "\"left\":{\"type\":0,\"int_value\":10},"
        "\"right\":{\"type\":0,\"int_value\":32}"
    "}";

    cJSON* json = cJSON_Parse(old_binary_json);
    IRExpression* expr = ir_expr_from_json(json);

    assert(expr->type == EXPR_BINARY);
    assert(expr->binary.op == BINARY_OP_ADD);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    assert(result.int_val == 42);

    // Cleanup
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    cJSON_Delete(json);
}

// Test old KIR files load correctly
void test_backward_compat_kir_loading(void) {
    // Load an old KIR file
    const char* test_kir_path = "tests/data/old_format.kir";

    IRExecutorContext* executor = ir_executor_create();
    bool loaded = ir_executor_load_kir_file(executor, test_kir_path);

    assert(loaded);

    // Should be able to evaluate expressions
    IRValue count = ir_executor_get_var(executor, "count", 0);
    assert(count.type == VAR_TYPE_INT);

    ir_executor_destroy(executor);
}

// Test existing handler execution still works
void test_backward_compat_handlers(void) {
    // Create an old-style handler (simple assignment)
    IRStatement* stmt = ir_stmt_assign(
        "count",
        ir_expr_binary(
            BINARY_OP_ADD,
            ir_expr_var_ref("count"),
            ir_expr_literal_int(1)
        )
    );

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "count", 0, 0);

    // Execute statement
    exec_stmt(executor, stmt, 0);

    // Verify result
    int64_t new_count = ir_executor_get_var_int(executor, "count", 0);
    assert(new_count == 1);

    // Cleanup
    ir_stmt_free(stmt);
    ir_executor_destroy(executor);
}
```

---

## Real-World Examples

### 4.1 Example Application

**File**: `examples/expression-engine-demo/habits-tracker.kry`

```kry
// Habits Tracker demonstrating new expression features

app HabitsTracker {
    // State with nested objects
    state {
        user: {
            profile: {
                name: "Alice",
                preferences: {
                    theme: "dark",
                    notifications: true
                }
            },
            habits: [
                { id: 1, name: "Exercise", completed: false },
                { id: 2, name: "Read", completed: true },
                { id: 3, name: "Meditate", completed: false }
            ]
        }
    }

    // Using member access in text binding
    Column {
        gap: 16

        // Display user name from nested object
        Text {
            text: "Welcome, " + user.profile.name
            fontSize: 24
        }

        // Show theme from preferences
        Text {
            text: "Theme: " + user.profile.preferences.theme
        }

        // Display habits using ForEach
        ForEach(user.habits) { habit ->
            Row {
                gap: 8

                Text {
                    text: habit.name
                }

                Button {
                    text: habit.completed ? "✓ Done" : "Mark Done"
                    onClick: { setHabitCompleted(habit.id, !habit.completed) }
                }
            }
        }
    }

    // Handler using method call
    on addHabit {
        // Add new habit to array
        user.habits.push({
            id: user.habits.length + 1,
            name: newHabitName,
            completed: false
        })
    }
}
```

### 4.2 Performance Test

```kry
// Performance test comparing expressions

app PerformanceBenchmark {
    state {
        iterations: 100000
        results: {
            compiledTime: 0,
            interpretedTime: 0
        }
    }

    Column {
        Text {
            text: "Expression Engine Performance Test"
        }

        Text {
            text: "Compiled evaluation: " + results.compiledTime + "ms"
        }

        Text {
            text: "Improvement: " +
                   (results.interpretedTime / results.compiledTime) + "x"
        }

        Button {
            text: "Run Benchmark"
            onClick: { runBenchmark() }
        }
    }

    on runBenchmark {
        // Benchmark compiled evaluation
        start = getCurrentTime()
        for (i = 0; i < iterations; i++) {
            result = count + 1
        }
        results.compiledTime = getCurrentTime() - start
    }
}
```

---

## Performance Benchmarks

### 5.1 Benchmark Suite

**File**: `tests/expression-engine/test_performance.c`

```c
#include <time.h>

// Benchmark 1: Variable access
void benchmark_variable_access(int iterations) {
    IRExpression* expr = ir_expr_var_ref("count");
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "count", 42, 0);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, executor, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Variable access (%d iterations): %.4f seconds (%.2f µs/op)\n",
           iterations, elapsed, (elapsed * 1000000) / iterations);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Benchmark 2: Nested member access
void benchmark_member_access(int iterations) {
    IRExpression* expr = ir_expr_member_access(
        ir_expr_member_access(
            ir_expr_member_access(
                ir_expr_var_ref("user"),
                "profile"
            ),
            "settings"
        ),
        "theme"
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    IRObject* settings = ir_object_create(4);
    ir_object_set(settings, "theme", ir_value_string("dark"));

    IRObject* profile = ir_object_create(4);
    ir_object_set(profile, "settings", ir_value_object(settings));

    IRObject* user = ir_object_create(4);
    ir_object_set(user, "profile", ir_value_object(profile));

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var(executor, "user", ir_value_object(user), 0);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, executor, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Member access (%d iterations): %.4f seconds (%.2f µs/op)\n",
           iterations, elapsed, (elapsed * 1000000) / iterations);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Benchmark 3: Binary operations
void benchmark_binary_ops(int iterations) {
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("a"),
        ir_expr_binary(
            BINARY_OP_MUL,
            ir_expr_var_ref("b"),
            ir_expr_literal_int(2)
        )
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    IRExecutorContext* executor = ir_executor_create();
    ir_executor_set_var_int(executor, "a", 10, 0);
    ir_executor_set_var_int(executor, "b", 5, 0);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, executor, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Binary ops (%d iterations): %.4f seconds (%.2f µs/op)\n",
           iterations, elapsed, (elapsed * 1000000) / iterations);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_executor_destroy(executor);
}

// Benchmark 4: Cache effectiveness
void benchmark_cache_effectiveness(int unique_exprs, int total_lookups) {
    IRExprCache* cache = ir_expr_cache_create(unique_exprs);

    // Create and cache expressions
    IRExpression** exprs = (IRExpression**)malloc(unique_exprs * sizeof(IRExpression*));
    for (int i = 0; i < unique_exprs; i++) {
        exprs[i] = ir_expr_var_ref(alloc_var_name(i));
        IRCompiledExpr* compiled = ir_expr_compile(exprs[i]);
        ir_expr_cache_insert(cache, exprs[i], compiled);
    }

    // Random lookups
    srand(time(NULL));
    clock_t start = clock();
    for (int i = 0; i < total_lookups; i++) {
        int idx = rand() % unique_exprs;
        IRCompiledExpr* found = ir_expr_cache_lookup(cache, exprs[idx]);
        assert(found != NULL);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Cache lookup (%d unique, %d lookups): %.4f seconds (%.2f µs/op)\n",
           unique_exprs, total_lookups, elapsed, (elapsed * 1000000) / total_lookups);

    // Print cache stats
    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);
    printf("Cache hit rate: %.2f%%\n", stats.hit_rate * 100.0);

    // Cleanup
    for (int i = 0; i < unique_exprs; i++) {
        ir_expr_free(exprs[i]);
    }
    free(exprs);
    ir_expr_cache_destroy(cache);
}

// Run all benchmarks
void test_performance_all(void) {
    printf("\n=== Expression Engine Performance Benchmarks ===\n\n");

    benchmark_variable_access(1000000);
    benchmark_member_access(1000000);
    benchmark_binary_ops(1000000);
    benchmark_cache_effectiveness(100, 10000000);

    printf("\n=== Benchmarks Complete ===\n\n");

    // Assert performance targets
    // Variable access should be < 1 µs per operation
    // Member access should be < 2 µs per operation
    // Binary ops should be < 1 µs per operation
    // Cache lookup should be < 0.1 µs per operation
}
```

---

## Implementation Steps

### Day 1: Integration Tests

1. **Morning** (4 hours)
   - [ ] Create integration test file
   - [ ] Implement end-to-end expression tests
   - [ ] Test caching integration
   - [ ] Test builtin function integration

2. **Afternoon** (4 hours)
   - [ ] Create example applications
   - [ ] Test TSX → KIR → execute pipeline
   - [ ] Test Kry → KIR → execute pipeline

### Day 2: Backward Compatibility

1. **Morning** (4 hours)
   - [ ] Create backward compatibility test file
   - [ ] Test old JSON expression format
   - [ ] Test old KIR file loading
   - [ ] Test existing handler execution

2. **Afternoon** (4 hours)
   - [ ] Load and test existing example applications
   - [ ] Verify round-trip conversion
   - [ ] Fix any compatibility issues found

### Day 3: Performance Testing

1. **Morning** (4 hours)
   - [ ] Create performance benchmark suite
   - [ ] Benchmark each expression type
   - [ ] Measure cache effectiveness
   - [ ] Compare with old interpreter

2. **Afternoon** (4 hours)
   - [ ] Profile hot paths
   - [ ] Optimize slow operations
   - [ ] Verify performance targets met

### Day 4: Documentation & Polish

1. **Morning** (4 hours)
   - [ ] Update main README
   - [ ] Write expression engine documentation
   - [ ] Add examples to docs
   - [ ] Create migration guide

2. **Afternoon** (4 hours)
   - [ ] Final test run
   - [ ] Fix any remaining bugs
   - [ ] Code review preparation

---

## Acceptance Criteria

Phase 7 is complete when:

- [ ] All integration tests pass
- [ ] All backward compatibility tests pass
- [ ] Performance targets met:
  - [ ] Simple expression: < 1 µs
  - [ ] Member access: < 2 µs
  - [ ] Binary operation: < 1 µs
  - [ ] Cache lookup: < 0.1 µs
- [ ] Example applications run correctly
- [ ] No memory leaks (valgrind clean)
- [ ] Documentation complete

---

## Test Execution

```bash
# Run all expression engine tests
make test-expression-engine

# Run specific phase tests
make test-phase1
make test-phase2
# ...

# Run integration tests
make test-integration

# Run backward compatibility tests
make test-backward-compat

# Run performance benchmarks
make benchmark

# Check for memory leaks
make test-valgrind
```

---

## Next Phase

Once Phase 7 is complete, proceed to **[Phase 8: Performance Optimization](./phase-8-optimization.md)**.

---

**Last Updated**: 2026-01-11
