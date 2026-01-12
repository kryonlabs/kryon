#include "ir_expression_cache.h"
#include "ir_expression.h"
#include "ir_expression_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

// ============================================================================
// TEST HELPERS
// ============================================================================

#define TEST(name) printf("Test: %s...\n", name)
#define PASS() printf("  PASS\n")
#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s\n", msg); \
            exit(1); \
        } \
    } while(0)

// ============================================================================
// CACHE CREATION AND DESTRUCTION
// ============================================================================

static void test_cache_create_destroy(void) {
    TEST("cache_create_destroy");

    IRExprCache* cache = ir_expr_cache_create(100);

    ASSERT_TRUE(cache != NULL, "cache should not be NULL");
    ASSERT_TRUE(cache->max_entries == 100, "max_entries should be 100");
    ASSERT_TRUE(cache->entry_count == 0, "entry_count should be 0");
    ASSERT_TRUE(cache->bucket_count >= 16, "bucket_count should be at least 16");
    ASSERT_TRUE(cache->hits == 0, "hits should be 0");
    ASSERT_TRUE(cache->misses == 0, "misses should be 0");
    ASSERT_TRUE(cache->evictions == 0, "evictions should be 0");

    ir_expr_cache_destroy(cache);
    PASS();
}

static void test_cache_global_init_cleanup(void) {
    TEST("cache_global_init_cleanup");

    ASSERT_TRUE(!ir_expr_cache_global_is_initialized(), "global cache should not be initialized");

    ir_expr_cache_global_init(50);
    ASSERT_TRUE(ir_expr_cache_global_is_initialized(), "global cache should be initialized");

    IRExprCache* cache = ir_expr_cache_global_get();
    ASSERT_TRUE(cache != NULL, "global cache should not be NULL");
    ASSERT_TRUE(cache->max_entries == 50, "max_entries should be 50");

    ir_expr_cache_global_cleanup();
    ASSERT_TRUE(!ir_expr_cache_global_is_initialized(), "global cache should not be initialized");

    PASS();
}

// ============================================================================
// CACHE INSERT AND LOOKUP
// ============================================================================

static void test_cache_insert_lookup(void) {
    TEST("cache_insert_lookup");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create a simple expression: 42
    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Insert into cache
    ir_expr_cache_insert(cache, expr, compiled);

    // Verify entry count
    ASSERT_TRUE(cache->entry_count == 1, "entry_count should be 1");

    // Lookup should find the compiled expression
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found != NULL, "lookup should find the compiled expression");
    ASSERT_TRUE(found == compiled, "lookup should return the same compiled expression");

    // Verify stats
    ASSERT_TRUE(cache->hits == 1, "hits should be 1");
    ASSERT_TRUE(cache->misses == 0, "misses should be 0");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_cache_miss(void) {
    TEST("cache_miss");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create an expression
    IRExpression* expr = ir_expr_int(42);

    // Lookup without insert - should miss
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found == NULL, "lookup should miss");
    ASSERT_TRUE(cache->misses == 1, "misses should be 1");
    ASSERT_TRUE(cache->hits == 0, "hits should be 0");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// LRU EVICTION
// ============================================================================

static void test_cache_lru_eviction(void) {
    TEST("cache_lru_eviction");

    // Create cache with small size
    IRExprCache* cache = ir_expr_cache_create(3);

    // Insert 4 different expressions
    for (int i = 0; i < 4; i++) {
        IRExpression* expr = ir_expr_int(i);
        IRCompiledExpr* compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(cache, expr, compiled);
        ir_expr_free(expr);
    }

    // Should have evicted one entry
    ASSERT_TRUE(cache->entry_count == 3, "entry_count should be 3");
    ASSERT_TRUE(cache->evictions == 1, "evictions should be 1");

    // The first expression (0) should have been evicted
    IRExpression* expr0 = ir_expr_int(0);
    IRCompiledExpr* found0 = ir_expr_cache_lookup(cache, expr0);
    ASSERT_TRUE(found0 == NULL, "first expression should have been evicted");
    ir_expr_free(expr0);

    // The other expressions should still be there
    IRExpression* expr3 = ir_expr_int(3);
    IRCompiledExpr* found3 = ir_expr_cache_lookup(cache, expr3);
    ASSERT_TRUE(found3 != NULL, "fourth expression should be in cache");
    ir_expr_free(expr3);

    ir_expr_cache_destroy(cache);
    PASS();
}

static void test_cache_lru_access_order(void) {
    TEST("cache_lru_access_order");

    IRExprCache* cache = ir_expr_cache_create(3);

    // Insert 3 expressions
    IRExpression* expr0 = ir_expr_int(0);
    IRExpression* expr1 = ir_expr_int(1);
    IRExpression* expr2 = ir_expr_int(2);

    IRCompiledExpr* compiled0 = ir_expr_compile(expr0);
    IRCompiledExpr* compiled1 = ir_expr_compile(expr1);
    IRCompiledExpr* compiled2 = ir_expr_compile(expr2);

    ir_expr_cache_insert(cache, expr0, compiled0);
    ir_expr_cache_insert(cache, expr1, compiled1);
    ir_expr_cache_insert(cache, expr2, compiled2);

    // Access expr0 to move it to front
    ir_expr_cache_lookup(cache, expr0);

    // Insert one more - should evict expr1 (least recently used)
    IRExpression* expr3 = ir_expr_int(3);
    IRCompiledExpr* compiled3 = ir_expr_compile(expr3);
    ir_expr_cache_insert(cache, expr3, compiled3);

    // expr0 should still be in cache (was accessed)
    IRCompiledExpr* found0 = ir_expr_cache_lookup(cache, expr0);
    ASSERT_TRUE(found0 != NULL, "expr0 should still be in cache");

    // expr1 should have been evicted
    IRCompiledExpr* found1 = ir_expr_cache_lookup(cache, expr1);
    ASSERT_TRUE(found1 == NULL, "expr1 should have been evicted");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr0);
    ir_expr_free(expr1);
    ir_expr_free(expr2);
    ir_expr_free(expr3);
    PASS();
}

// ============================================================================
// CACHE RESULT CACHING
// ============================================================================

static void test_cache_result_store(void) {
    TEST("cache_result_store");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create a constant expression: 42
    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ir_expr_cache_insert(cache, expr, compiled);

    // Store a result
    IRValue result = ir_value_int(42);
    ir_expr_cache_store_result(cache, expr, result);

    // Get the cached result
    IRValue* cached = ir_expr_cache_get_result(cache, expr);
    ASSERT_TRUE(cached != NULL, "should have cached result");
    ASSERT_TRUE(cached->type == VAR_TYPE_INT, "cached result should be int");
    ASSERT_TRUE(cached->int_val == 42, "cached result should be 42");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_cache_result_no_store(void) {
    TEST("cache_result_no_store");

    IRExprCache* cache = ir_expr_cache_create(100);

    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ir_expr_cache_insert(cache, expr, compiled);

    // Try to get result without storing
    IRValue* cached = ir_expr_cache_get_result(cache, expr);
    ASSERT_TRUE(cached == NULL, "should not have cached result");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// CACHE INVALIDATION
// ============================================================================

static void test_cache_var_invalidate(void) {
    TEST("cache_var_invalidate");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create expression: count + 1
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var("count"),
        ir_expr_int(1)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Store a result
    IRValue result = ir_value_int(5);
    ir_expr_cache_store_result(cache, expr, result);

    // Verify result is cached
    IRValue* cached = ir_expr_cache_get_result(cache, expr);
    ASSERT_TRUE(cached != NULL, "should have cached result before invalidation");

    // Invalidate the variable
    ir_expr_cache_invalidate_var(cache, "count");

    // Result should be cleared
    cached = ir_expr_cache_get_result(cache, expr);
    ASSERT_TRUE(cached == NULL, "should not have cached result after invalidation");

    // Compiled bytecode should still be there
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found != NULL, "compiled bytecode should still be in cache");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_cache_invalidate_all(void) {
    TEST("cache_invalidate_all");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create multiple expressions
    IRExpression* expr1 = ir_expr_int(42);
    IRExpression* expr2 = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var("x"),
        ir_expr_int(1)
    );

    IRCompiledExpr* compiled1 = ir_expr_compile(expr1);
    IRCompiledExpr* compiled2 = ir_expr_compile(expr2);

    ir_expr_cache_insert(cache, expr1, compiled1);
    ir_expr_cache_insert(cache, expr2, compiled2);

    // Store results
    ir_expr_cache_store_result(cache, expr1, ir_value_int(42));
    ir_expr_cache_store_result(cache, expr2, ir_value_int(5));

    // Invalidate all
    ir_expr_cache_invalidate_all(cache);

    // All results should be cleared
    IRValue* cached1 = ir_expr_cache_get_result(cache, expr1);
    IRValue* cached2 = ir_expr_cache_get_result(cache, expr2);
    ASSERT_TRUE(cached1 == NULL, "expr1 result should be cleared");
    ASSERT_TRUE(cached2 == NULL, "expr2 result should be cleared");

    // But bytecode should still be there
    ASSERT_TRUE(cache->entry_count == 2, "entry_count should still be 2");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr1);
    ir_expr_free(expr2);
    PASS();
}

static void test_cache_clear(void) {
    TEST("cache_clear");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Insert some expressions
    for (int i = 0; i < 5; i++) {
        IRExpression* expr = ir_expr_int(i);
        IRCompiledExpr* compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(cache, expr, compiled);
        ir_expr_free(expr);
    }

    ASSERT_TRUE(cache->entry_count == 5, "entry_count should be 5");

    // Clear the cache
    ir_expr_cache_clear(cache);

    ASSERT_TRUE(cache->entry_count == 0, "entry_count should be 0");
    ASSERT_TRUE(cache->lru_head == NULL, "lru_head should be NULL");
    ASSERT_TRUE(cache->lru_tail == NULL, "lru_tail should be NULL");

    ir_expr_cache_destroy(cache);
    PASS();
}

// ============================================================================
// CACHE STATISTICS
// ============================================================================

static void test_cache_stats(void) {
    TEST("cache_stats");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Insert some entries
    for (int i = 0; i < 5; i++) {
        IRExpression* expr = ir_expr_int(i);
        IRCompiledExpr* compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(cache, expr, compiled);
        ir_expr_free(expr);
    }

    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);

    ASSERT_TRUE(stats.entry_count == 5, "entry_count should be 5");
    ASSERT_TRUE(stats.max_entries == 100, "max_entries should be 100");
    ASSERT_TRUE(stats.hits == 0, "hits should be 0");
    ASSERT_TRUE(stats.misses == 0, "misses should be 0");

    // Do some lookups
    IRExpression* expr = ir_expr_int(0);
    ir_expr_cache_lookup(cache, expr);  // hit
    ir_expr_cache_lookup(cache, expr);  // hit

    IRExpression* expr_miss = ir_expr_int(999);
    ir_expr_cache_lookup(cache, expr_miss);  // miss

    ir_expr_cache_get_stats(cache, &stats);
    ASSERT_TRUE(stats.hits == 2, "hits should be 2");
    ASSERT_TRUE(stats.misses == 1, "misses should be 1");

    // Hit rate should be 2/3 ≈ 0.67
    double expected_rate = 2.0 / 3.0;
    double diff = stats.hit_rate - expected_rate;
    ASSERT_TRUE(diff < 0.001, "hit_rate should be ~0.67");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    ir_expr_free(expr_miss);
    PASS();
}

static void test_cache_reset_stats(void) {
    TEST("cache_reset_stats");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Generate some stats
    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Force eviction
    for (int i = 0; i < 110; i++) {
        IRExpression* e = ir_expr_int(i + 1000);
        IRCompiledExpr* c = ir_expr_compile(e);
        ir_expr_cache_insert(cache, e, c);
        ir_expr_free(e);
    }

    ASSERT_TRUE(cache->evictions > 0, "should have evictions");

    // Reset stats
    ir_expr_cache_reset_stats(cache);

    ASSERT_TRUE(cache->hits == 0, "hits should be 0 after reset");
    ASSERT_TRUE(cache->misses == 0, "misses should be 0 after reset");
    ASSERT_TRUE(cache->evictions == 0, "evictions should be 0 after reset");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// COMPLEX EXPRESSIONS
// ============================================================================

static void test_cache_complex_expr(void) {
    TEST("cache_complex_expr");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create a complex expression: (a + b) * c
    IRExpression* expr = ir_expr_mul(
        ir_expr_binary(BINARY_OP_ADD, ir_expr_var("a"), ir_expr_var("b")),
        ir_expr_var("c")
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Insert and lookup
    ir_expr_cache_insert(cache, expr, compiled);
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);

    ASSERT_TRUE(found != NULL, "should find complex expression");
    ASSERT_TRUE(found == compiled, "should return same compiled expression");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

static void test_cache_ternary_expr(void) {
    TEST("cache_ternary_expr");

    IRExprCache* cache = ir_expr_cache_create(100);

    // Create a ternary expression: flag ? 1 : 0
    IRExpression* expr = ir_expr_ternary(
        ir_expr_var("flag"),
        ir_expr_int(1),
        ir_expr_int(0)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Insert and lookup
    ir_expr_cache_insert(cache, expr, compiled);
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);

    ASSERT_TRUE(found != NULL, "should find ternary expression");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// PERFORMANCE TEST
// ============================================================================

static void test_cache_performance(void) {
    TEST("cache_performance");

    IRExprCache* cache = ir_expr_cache_create(1000);

    // Create an expression
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var("a"),
        ir_expr_var("b")
    );

    // First lookup - should miss
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    ASSERT_TRUE(found == NULL, "first lookup should miss");

    // Insert
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Benchmark cached lookups
    clock_t start = clock();
    const int iterations = 100000;
    for (int i = 0; i < iterations; i++) {
        IRCompiledExpr* result = ir_expr_cache_lookup(cache, expr);
        ASSERT_TRUE(result != NULL, "cached lookup should succeed");
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double per_lookup = elapsed / iterations * 1e9;  // nanoseconds

    printf("  (%d lookups in %.4f seconds, %.0f ns/lookup)",
           iterations, elapsed, per_lookup);

    // Should be very fast (< 1 microsecond per lookup)
    ASSERT_TRUE(per_lookup < 1000.0, "cached lookup should be fast (< 1us)");

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("============================================================================\n");
    printf("EXPRESSION CACHE TEST\n");
    printf("============================================================================\n\n");

    // Cache creation and destruction
    printf("\nCache Lifecycle Tests:\n");
    test_cache_create_destroy();
    test_cache_global_init_cleanup();

    // Cache insert and lookup
    printf("\nCache Insert/Lookup Tests:\n");
    test_cache_insert_lookup();
    test_cache_miss();

    // LRU eviction
    printf("\nLRU Eviction Tests:\n");
    test_cache_lru_eviction();
    test_cache_lru_access_order();

    // Result caching
    printf("\nResult Caching Tests:\n");
    test_cache_result_store();
    test_cache_result_no_store();

    // Invalidation
    printf("\nCache Invalidation Tests:\n");
    test_cache_var_invalidate();
    test_cache_invalidate_all();
    test_cache_clear();

    // Statistics
    printf("\nStatistics Tests:\n");
    test_cache_stats();
    test_cache_reset_stats();

    // Complex expressions
    printf("\nComplex Expression Tests:\n");
    test_cache_complex_expr();
    test_cache_ternary_expr();

    // Performance
    printf("\nPerformance Tests:\n");
    test_cache_performance();

    printf("\n============================================================================\n");
    printf("ALL TESTS PASSED\n");
    printf("============================================================================\n");

    return 0;
}
