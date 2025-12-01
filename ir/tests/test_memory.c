/*
 * Unit tests for IR memory allocators (ir_memory.c)
 * Tests component pool allocator and arena allocator
 */

#include "test_framework.h"
#include "../ir_core.h"
#include "../ir_memory.h"
#include <string.h>

// ============================================================================
// Component Pool Tests
// ============================================================================

// Test: Create and destroy pool
TEST(test_pool_create_destroy) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(stats.total_allocated, 0);
    ASSERT_EQ(stats.total_freed, 0);
    ASSERT_EQ(stats.current_in_use, 0);
    // Pool may or may not pre-allocate blocks (implementation-dependent)

    ir_pool_destroy(pool);
}

// Test: Allocate single component
TEST(test_pool_alloc_single) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comp = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp);

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(stats.total_allocated, 1);
    ASSERT_EQ(stats.total_freed, 0);
    ASSERT_EQ(stats.current_in_use, 1);

    ir_pool_destroy(pool);
}

// Test: Allocate multiple components
TEST(test_pool_alloc_multiple) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    const int NUM_ALLOCS = 10;
    IRComponent* components[NUM_ALLOCS];

    for (int i = 0; i < NUM_ALLOCS; i++) {
        components[i] = ir_pool_alloc_component(pool);
        ASSERT_NONNULL(components[i]);
    }

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(stats.total_allocated, NUM_ALLOCS);
    ASSERT_EQ(stats.current_in_use, NUM_ALLOCS);

    ir_pool_destroy(pool);
}

// Test: Allocate and free
TEST(test_pool_alloc_free) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comp = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp);

    IRPoolStats stats1;
    ir_pool_get_stats(pool, &stats1);
    ASSERT_EQ(stats1.current_in_use, 1);

    ir_pool_free_component(pool, comp);

    IRPoolStats stats2;
    ir_pool_get_stats(pool, &stats2);
    ASSERT_EQ(stats2.total_freed, 1);
    ASSERT_EQ(stats2.current_in_use, 0);

    ir_pool_destroy(pool);
}

// Test: Allocate, free, allocate again (reuse)
TEST(test_pool_alloc_free_reuse) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    // First allocation
    IRComponent* comp1 = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp1);
    ir_pool_free_component(pool, comp1);

    // Second allocation (should reuse freed component)
    IRComponent* comp2 = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp2);
    ASSERT_EQ((uintptr_t)comp1, (uintptr_t)comp2);  // Should be same memory

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(stats.total_allocated, 2);
    ASSERT_EQ(stats.total_freed, 1);
    ASSERT_EQ(stats.current_in_use, 1);

    ir_pool_destroy(pool);
}

// Test: Allocate many components (trigger block expansion)
TEST(test_pool_block_expansion) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    // Allocate more than one block's worth (blocks typically hold 64 components)
    const int NUM_ALLOCS = 128;
    IRComponent* components[NUM_ALLOCS];

    for (int i = 0; i < NUM_ALLOCS; i++) {
        components[i] = ir_pool_alloc_component(pool);
        ASSERT_NONNULL(components[i]);
    }

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(stats.total_allocated, NUM_ALLOCS);
    ASSERT_EQ(stats.current_in_use, NUM_ALLOCS);
    ASSERT_GTE(stats.blocks_allocated, 2);  // Should have allocated multiple blocks
    ASSERT_GTE(stats.total_capacity, NUM_ALLOCS);

    ir_pool_destroy(pool);
}

// Test: Free NULL component (should not crash)
TEST(test_pool_free_null) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    ir_pool_free_component(pool, NULL);  // Should not crash

    ir_pool_destroy(pool);
}

// Test: Pool statistics accuracy
TEST(test_pool_stats_accuracy) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comp1 = ir_pool_alloc_component(pool);
    IRComponent* comp2 = ir_pool_alloc_component(pool);
    IRComponent* comp3 = ir_pool_alloc_component(pool);

    IRPoolStats stats1;
    ir_pool_get_stats(pool, &stats1);
    ASSERT_EQ(stats1.total_allocated, 3);
    ASSERT_EQ(stats1.current_in_use, 3);

    ir_pool_free_component(pool, comp2);

    IRPoolStats stats2;
    ir_pool_get_stats(pool, &stats2);
    ASSERT_EQ(stats2.total_allocated, 3);
    ASSERT_EQ(stats2.total_freed, 1);
    ASSERT_EQ(stats2.current_in_use, 2);

    ir_pool_free_component(pool, comp1);
    ir_pool_free_component(pool, comp3);

    IRPoolStats stats3;
    ir_pool_get_stats(pool, &stats3);
    ASSERT_EQ(stats3.total_allocated, 3);
    ASSERT_EQ(stats3.total_freed, 3);
    ASSERT_EQ(stats3.current_in_use, 0);

    ir_pool_destroy(pool);
}

// ============================================================================
// Arena Allocator Tests
// ============================================================================

// Test: Create and destroy arena
TEST(test_arena_create_destroy) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    ASSERT_EQ(ir_arena_get_used(arena), 0);
    ASSERT_EQ(ir_arena_get_remaining(arena), 1024);

    ir_arena_destroy(arena);
}

// Test: Create arena with default size
TEST(test_arena_create_default_size) {
    IRArena* arena = ir_arena_create(0);  // 0 = use default size
    ASSERT_NONNULL(arena);

    ASSERT_GT(ir_arena_get_remaining(arena), 0);

    ir_arena_destroy(arena);
}

// Test: Allocate from arena
TEST(test_arena_alloc) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    void* ptr1 = ir_arena_alloc(arena, 16);
    ASSERT_NONNULL(ptr1);
    ASSERT_GTE(ir_arena_get_used(arena), 16);

    void* ptr2 = ir_arena_alloc(arena, 32);
    ASSERT_NONNULL(ptr2);
    ASSERT_GTE(ir_arena_get_used(arena), 48);

    // Pointers should be different
    ASSERT_NEQ((uintptr_t)ptr1, (uintptr_t)ptr2);

    ir_arena_destroy(arena);
}

// Test: Arena alignment
TEST(test_arena_alignment) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    // Default allocation should be 8-byte aligned
    void* ptr1 = ir_arena_alloc(arena, 1);
    ASSERT_EQ((uintptr_t)ptr1 % 8, 0);

    void* ptr2 = ir_arena_alloc(arena, 5);
    ASSERT_EQ((uintptr_t)ptr2 % 8, 0);

    // Custom 16-byte alignment
    void* ptr3 = ir_arena_alloc_aligned(arena, 10, 16);
    ASSERT_EQ((uintptr_t)ptr3 % 16, 0);

    ir_arena_destroy(arena);
}

// Test: Arena reset
TEST(test_arena_reset) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    // Allocate some memory
    ir_arena_alloc(arena, 100);
    ir_arena_alloc(arena, 200);

    size_t used = ir_arena_get_used(arena);
    ASSERT_GTE(used, 300);

    // Reset arena
    ir_arena_reset(arena);

    ASSERT_EQ(ir_arena_get_used(arena), 0);
    ASSERT_EQ(ir_arena_get_remaining(arena), 1024);

    // Should be able to allocate again
    void* ptr = ir_arena_alloc(arena, 50);
    ASSERT_NONNULL(ptr);

    ir_arena_destroy(arena);
}

// Test: Arena strdup
TEST(test_arena_strdup) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    const char* original = "Hello, Kryon!";
    char* copy = ir_arena_strdup(arena, original);

    ASSERT_NONNULL(copy);
    ASSERT_STR_EQ(copy, original);
    ASSERT_NEQ((uintptr_t)copy, (uintptr_t)original);  // Different pointers

    ir_arena_destroy(arena);
}

// Test: Arena strndup
TEST(test_arena_strndup) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    const char* original = "Hello, Kryon!";
    char* copy = ir_arena_strndup(arena, original, 5);

    ASSERT_NONNULL(copy);
    ASSERT_STR_EQ(copy, "Hello");
    ASSERT_EQ(strlen(copy), 5);

    ir_arena_destroy(arena);
}

// Test: Arena from buffer
TEST(test_arena_from_buffer) {
    uint8_t buffer[256];
    IRArena* arena = ir_arena_create_from_buffer(buffer, sizeof(buffer));
    ASSERT_NONNULL(arena);

    ASSERT_EQ(ir_arena_get_used(arena), 0);
    ASSERT_EQ(ir_arena_get_remaining(arena), 256);

    void* ptr = ir_arena_alloc(arena, 32);
    ASSERT_NONNULL(ptr);
    ASSERT_GTE((uintptr_t)ptr, (uintptr_t)buffer);
    ASSERT_LT((uintptr_t)ptr, (uintptr_t)buffer + sizeof(buffer));

    ir_arena_destroy(arena);
}

// Test: Arena overflow handling
TEST(test_arena_overflow) {
    IRArena* arena = ir_arena_create(64);
    ASSERT_NONNULL(arena);

    // Allocate more than capacity
    void* ptr1 = ir_arena_alloc(arena, 32);
    ASSERT_NONNULL(ptr1);

    void* ptr2 = ir_arena_alloc(arena, 32);
    ASSERT_NONNULL(ptr2);

    // This should fail (not enough space)
    void* ptr3 = ir_arena_alloc(arena, 32);
    ASSERT_NULL(ptr3);

    ir_arena_destroy(arena);
}

// Test suite runner
void run_memory_tests(void) {
    BEGIN_SUITE("IR Memory Tests");

    // Component pool tests
    RUN_TEST(test_pool_create_destroy);
    RUN_TEST(test_pool_alloc_single);
    RUN_TEST(test_pool_alloc_multiple);
    RUN_TEST(test_pool_alloc_free);
    RUN_TEST(test_pool_alloc_free_reuse);
    RUN_TEST(test_pool_block_expansion);
    RUN_TEST(test_pool_free_null);
    RUN_TEST(test_pool_stats_accuracy);

    // Arena allocator tests
    RUN_TEST(test_arena_create_destroy);
    RUN_TEST(test_arena_create_default_size);
    RUN_TEST(test_arena_alloc);
    RUN_TEST(test_arena_alignment);
    RUN_TEST(test_arena_reset);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_strndup);
    RUN_TEST(test_arena_from_buffer);
    RUN_TEST(test_arena_overflow);

    END_SUITE();
}

RUN_TEST_SUITE(run_memory_tests)
