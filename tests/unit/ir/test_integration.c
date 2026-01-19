#define _POSIX_C_SOURCE 200809L
// ============================================================================
// INTEGRATION TEST
// ============================================================================
// End-to-end tests for the expression engine pipeline

#include "ir_expression_compiler.h"
#include "ir_expression_cache.h"
#include "ir_builtin_registry.h"
#include "../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("Test %d: %s...", tests_run, name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf(" PASS\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf(" FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

// ============================================================================
// END-TO-END EXPRESSION PIPELINE TESTS
// ============================================================================

static void test_integration_literal(void) {
    TEST("integration_literal");

    // Create expression AST: 42
    IRExpression* expr = ir_expr_int(42);

    // Compile to bytecode
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");

    // Evaluate
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "value should be 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_binary(void) {
    TEST("integration_binary");

    // Create: 10 + 32
    IRExpression* expr = ir_expr_add(ir_expr_int(10), ir_expr_int(32));

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");

    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "value should be 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_string_concat(void) {
    TEST("integration_string_concat");

    // Create: "Hello, " + "World"
    IRExpression* expr = ir_expr_add(ir_expr_string("Hello, "), ir_expr_string("World"));

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "Hello, World") == 0, "value should be 'Hello, World'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_ternary(void) {
    TEST("integration_ternary");

    // Create: true ? "yes" : "no"
    IRExpression* expr = ir_expr_ternary(
        ir_expr_bool(1),
        ir_expr_string("yes"),
        ir_expr_string("no")
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "yes") == 0, "value should be 'yes'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_comparison(void) {
    TEST("integration_comparison");

    // Create: 5 > 3
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_GT,
        ir_expr_int(5),
        ir_expr_int(3)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should return bool");
    ASSERT_TRUE(result.bool_val == 1, "value should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_complex(void) {
    TEST("integration_complex");

    // Create: (10 + 5) > 12 ? "many" : "few"
    IRExpression* expr = ir_expr_ternary(
        ir_expr_binary(
            BINARY_OP_GT,
            ir_expr_add(ir_expr_int(10), ir_expr_int(5)),
            ir_expr_int(12)
        ),
        ir_expr_string("many"),
        ir_expr_string("few")
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "many") == 0, "value should be 'many'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// CACHE INTEGRATION TESTS
// ============================================================================

static void test_integration_cache_compile(void) {
    TEST("integration_cache_compile");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create expression
    IRExpression* expr = ir_expr_add(ir_expr_int(10), ir_expr_int(32));

    // First lookup - should miss
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found == NULL, "first lookup should miss");

    // Compile and cache
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Second lookup - should hit
    found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found == compiled, "second lookup should hit");

    // Verify statistics
    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);
    ASSERT_TRUE(stats.hits == 1, "should have 1 hit");
    ASSERT_TRUE(stats.misses == 1, "should have 1 miss");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_cache_eval(void) {
    TEST("integration_cache_eval");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create and compile expression
    IRExpression* expr = ir_expr_mul(ir_expr_int(6), ir_expr_int(7));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Cache it
    ir_expr_cache_insert(cache, expr, compiled);

    // Look up from cache and evaluate
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found != NULL, "should find in cache");

    IRValue result = ir_expr_eval(found, NULL, 0);
    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "6 * 7 should be 42");

    ir_value_free(&result);
    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_cache_invalidation(void) {
    TEST("integration_cache_invalidation");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create expression with variable: count + 1
    IRExpression* expr = ir_expr_add(ir_expr_var("count"), ir_expr_int(1));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Lookup should work
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found != NULL, "should find in cache");

    // Invalidate for variable "count"
    ir_expr_cache_invalidate_var(cache, "count");

    // Lookup should still work (compiled bytecode not invalidated, only cached results)
    // Note: Our cache implementation only invalidates cached results, not compiled bytecode
    found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found != NULL, "should still find compiled bytecode");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// BUILTIN FUNCTION INTEGRATION TESTS
// ============================================================================

static void test_integration_builtin_string(void) {
    TEST("integration_builtin_string");

    // Initialize global builtin registry
    ir_builtin_global_init();

    // Create: string_toUpper("hello")
    IRExpression* args[] = { ir_expr_string("hello") };
    IRExpression* expr = ir_expr_call("string_toUpper", args, 1);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "HELLO") == 0, "should be uppercase");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_builtin_math(void) {
    TEST("integration_builtin_math");

    // Create: math_max(10, 20, 15)
    IRExpression* args[] = {
        ir_expr_int(10),
        ir_expr_int(20),
        ir_expr_int(15)
    };
    IRExpression* expr = ir_expr_call("math_max", args, 3);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 20, "max should be 20");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_builtin_type(void) {
    TEST("integration_builtin_type");

    // Create: type_typeof(42)
    IRExpression* args[] = { ir_expr_int(42) };
    IRExpression* expr = ir_expr_call("type_typeof", args, 1);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "int") == 0, "type should be 'int'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_integration_builtin_nested(void) {
    TEST("integration_builtin_nested");

    // Create: string_length(string_toUpper("hello"))
    // Should return 5 (length of "HELLO")
    IRExpression* inner_args[] = { ir_expr_string("hello") };
    IRExpression* inner = ir_expr_call("string_toUpper", inner_args, 1);

    IRExpression* outer_args[] = { inner };
    IRExpression* expr = ir_expr_call("string_length", outer_args, 1);

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 5, "length should be 5");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    // Note: expr owns inner, so only free expr
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// JSON SERIALIZATION INTEGRATION TESTS
// ============================================================================

static void test_integration_json_serialize(void) {
    TEST("integration_json_serialize");

    // Create expression: 42 + 58
    IRExpression* expr = ir_expr_add(ir_expr_int(42), ir_expr_int(58));

    // Serialize to JSON
    cJSON* json = ir_expr_to_json(expr);
    ASSERT_TRUE(json != NULL, "should serialize to JSON");

    char* json_str = cJSON_Print(json);
    ASSERT_TRUE(json_str != NULL, "should print JSON");

    // Deserialize back
    IRExpression* deserialized = ir_expr_from_json(json);
    ASSERT_TRUE(deserialized != NULL, "should deserialize from JSON");

    // Compile and evaluate deserialized expression
    IRCompiledExpr* compiled = ir_expr_compile(deserialized);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 100, "value should be 100");

    // Cleanup
    free(json_str);
    cJSON_Delete(json);
    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    ir_expr_free(deserialized);
    PASS();
}

// ============================================================================
// PERFORMANCE BENCHMARKS
// ============================================================================

static void benchmark_literal_eval(int iterations) {
    printf("\n  Benchmark: literal evaluation (%d iterations)\n", iterations);

    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, NULL, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double us_per_op = (elapsed * 1000000.0) / iterations;
    printf("    Time: %.4f seconds (%.2f µs/op)\n", elapsed, us_per_op);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);

    ASSERT_TRUE(us_per_op < 1.0, "literal eval should be < 1 µs per operation");
}

static void benchmark_binary_eval(int iterations) {
    printf("\n  Benchmark: binary operation (%d iterations)\n", iterations);

    IRExpression* expr = ir_expr_add(ir_expr_var("a"), ir_expr_var("b"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, NULL, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double us_per_op = (elapsed * 1000000.0) / iterations;
    printf("    Time: %.4f seconds (%.2f µs/op)\n", elapsed, us_per_op);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);

    ASSERT_TRUE(us_per_op < 2.0, "binary eval should be < 2 µs per operation");
}

static void benchmark_builtin_call(int iterations) {
    printf("\n  Benchmark: builtin call (%d iterations)\n", iterations);

    IRExpression* args[] = { ir_expr_string("hello") };
    IRExpression* expr = ir_expr_call("string_length", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRValue result = ir_expr_eval(compiled, NULL, 0);
        ir_value_free(&result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double us_per_op = (elapsed * 1000000.0) / iterations;
    printf("    Time: %.4f seconds (%.2f µs/op)\n", elapsed, us_per_op);

    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);

    ASSERT_TRUE(us_per_op < 5.0, "builtin call should be < 5 µs per operation");
}

static void benchmark_cache_lookup(int iterations) {
    printf("\n  Benchmark: cache lookup (%d iterations)\n", iterations);

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create and cache expressions
    IRExpression* expr = ir_expr_var("count");
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
        (void)found; // Suppress unused warning
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double us_per_op = (elapsed * 1000000.0) / iterations;
    printf("    Time: %.4f seconds (%.2f µs/op)\n", elapsed, us_per_op);

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);

    ASSERT_TRUE(us_per_op < 0.5, "cache lookup should be < 0.5 µs per operation");
}

static void test_performance_benchmarks(void) {
    TEST("performance_benchmarks");

    printf("\n");
    benchmark_literal_eval(1000000);
    benchmark_binary_eval(1000000);
    benchmark_builtin_call(100000);
    benchmark_cache_lookup(10000000);

    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("EXPRESSION ENGINE INTEGRATION TEST\n");
    printf("============================================================================\n\n");

    // End-to-end expression pipeline tests
    printf("End-to-End Tests:\n");
    test_integration_literal();
    test_integration_binary();
    test_integration_string_concat();
    test_integration_ternary();
    test_integration_comparison();
    test_integration_complex();

    // Cache integration tests
    printf("\nCache Integration Tests:\n");
    test_integration_cache_compile();
    test_integration_cache_eval();
    test_integration_cache_invalidation();

    // Builtin function integration tests
    printf("\nBuiltin Integration Tests:\n");
    test_integration_builtin_string();
    test_integration_builtin_math();
    test_integration_builtin_type();
    test_integration_builtin_nested();

    // JSON serialization tests
    printf("\nJSON Serialization Tests:\n");
    test_integration_json_serialize();

    // Performance benchmarks
    printf("\nPerformance Benchmarks:\n");
    test_performance_benchmarks();

    // Cleanup global registry
    ir_builtin_global_cleanup();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
