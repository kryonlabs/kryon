/**
 * @file test_ir_memory.c
 * @brief Tests for memory allocators - pool and arena
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_memory.h"
#include "ir_core.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

// Pool Allocator Tests

TEST(test_pool_create) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);
    ir_pool_destroy(pool);
}

TEST(test_pool_alloc_component) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comp = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp);
    ASSERT_EQ(IR_COMPONENT_CONTAINER, comp->type);

    ir_pool_free_component(pool, comp);
    ir_pool_destroy(pool);
}

TEST(test_pool_alloc_multiple) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comps[10];
    for (int i = 0; i < 10; i++) {
        comps[i] = ir_pool_alloc_component(pool);
        ASSERT_NONNULL(comps[i]);
    }

    // Free all
    for (int i = 0; i < 10; i++) {
        ir_pool_free_component(pool, comps[i]);
    }

    ir_pool_destroy(pool);
}

TEST(test_pool_stats) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRPoolStats stats;
    ir_pool_get_stats(pool, &stats);

    ASSERT_EQ(0, stats.current_in_use);

    // Allocate some components
    IRComponent* comp1 = ir_pool_alloc_component(pool);
    IRComponent* comp2 = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp1);
    ASSERT_NONNULL(comp2);

    ir_pool_get_stats(pool, &stats);
    ASSERT_EQ(2, stats.current_in_use);

    ir_pool_free_component(pool, comp1);
    ir_pool_free_component(pool, comp2);

    ir_pool_destroy(pool);
}

TEST(test_pool_reuse_after_free) {
    IRComponentPool* pool = ir_pool_create();
    ASSERT_NONNULL(pool);

    IRComponent* comp1 = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp1);

    // Set a value to verify we get the same memory back
    comp1->id = 12345;

    IRPoolStats stats_before;
    ir_pool_get_stats(pool, &stats_before);

    ir_pool_free_component(pool, comp1);

    IRPoolStats stats_after;
    ir_pool_get_stats(pool, &stats_after);
    ASSERT_GT(stats_after.total_freed, stats_before.total_freed);

    // Allocate again - should reuse memory
    IRComponent* comp2 = ir_pool_alloc_component(pool);
    ASSERT_NONNULL(comp2);

    ir_pool_free_component(pool, comp2);
    ir_pool_destroy(pool);
}

// Arena Allocator Tests

TEST(test_arena_create_default_size) {
    IRArena* arena = ir_arena_create(0);
    ASSERT_NONNULL(arena);
    ASSERT_EQ(0, ir_arena_get_used(arena));

    ir_arena_destroy(arena);
}

TEST(test_arena_create_custom_size) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    // Verify we can allocate from it
    void* ptr = ir_arena_alloc(arena, 100);
    ASSERT_NONNULL(ptr);

    ir_arena_destroy(arena);
}

TEST(test_arena_alloc) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    void* ptr1 = ir_arena_alloc(arena, 100);
    ASSERT_NONNULL(ptr1);

    void* ptr2 = ir_arena_alloc(arena, 200);
    ASSERT_NONNULL(ptr2);

    // Pointers should be different
    ASSERT_TRUE(ptr1 != ptr2);

    // Check used bytes
    ASSERT_GT(ir_arena_get_used(arena), 300);

    ir_arena_destroy(arena);
}

TEST(test_arena_alloc_aligned) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    // Allocate with 16-byte alignment
    void* ptr = ir_arena_alloc_aligned(arena, 64, 16);
    ASSERT_NONNULL(ptr);
    ASSERT_EQ(0, (uintptr_t)ptr % 16);

    ir_arena_destroy(arena);
}

TEST(test_arena_alloc_zero) {
    IRArena* arena = ir_arena_create(1024);
    ASSERT_NONNULL(arena);

    // Allocating 0 bytes returns NULL
    void* ptr = ir_arena_alloc(arena, 0);
    ASSERT_NULL(ptr);

    ir_arena_destroy(arena);
}

TEST(test_arena_remaining) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    size_t initial_remaining = ir_arena_get_remaining(arena);

    ir_arena_alloc(arena, 100);

    size_t new_remaining = ir_arena_get_remaining(arena);
    ASSERT_LT(new_remaining, initial_remaining);

    ir_arena_destroy(arena);
}

TEST(test_arena_reset) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    // Allocate some memory
    void* ptr1 = ir_arena_alloc(arena, 100);
    void* ptr2 = ir_arena_alloc(arena, 200);
    ASSERT_NONNULL(ptr1);
    ASSERT_NONNULL(ptr2);

    ASSERT_GT(ir_arena_get_used(arena), 300);

    // Reset
    ir_arena_reset(arena);

    ASSERT_EQ(0, ir_arena_get_used(arena));

    // Can still allocate after reset
    void* ptr3 = ir_arena_alloc(arena, 150);
    ASSERT_NONNULL(ptr3);

    ir_arena_destroy(arena);
}

TEST(test_arena_strdup) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    char* str = ir_arena_strdup(arena, "hello world");
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("hello world", str);

    ir_arena_destroy(arena);
}

TEST(test_arena_strdup_empty) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    char* str = ir_arena_strdup(arena, "");
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("", str);

    ir_arena_destroy(arena);
}

TEST(test_arena_strndup) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    char* str = ir_arena_strndup(arena, "hello world", 5);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("hello", str);

    ir_arena_destroy(arena);
}

TEST(test_arena_strndup_zero_length) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    char* str = ir_arena_strndup(arena, "hello", 0);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("", str);

    ir_arena_destroy(arena);
}

TEST(test_arena_create_from_buffer) {
    // Create a buffer
    char buffer[1024];
    strcpy(buffer, "test data");

    // Create arena from buffer
    IRArena* arena = ir_arena_create_from_buffer(buffer, sizeof(buffer));
    ASSERT_NONNULL(arena);

    // Allocate from arena
    char* str = ir_arena_strdup(arena, "arena string");
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("arena string", str);

    // Note: arena created from buffer doesn't own the buffer
    // so we don't call ir_arena_destroy
}

TEST(test_arena_large_allocation) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    // Request larger allocation than capacity - fails (no auto-grow)
    void* ptr = ir_arena_alloc(arena, 2000);
    ASSERT_NULL(ptr);

    ir_arena_destroy(arena);
}

TEST(test_arena_multiple_small_allocations) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    // Many small allocations
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        ptrs[i] = ir_arena_alloc(arena, 10);
        ASSERT_NONNULL(ptrs[i]);
    }

    ASSERT_GT(ir_arena_get_used(arena), 200);

    ir_arena_destroy(arena);
}

TEST(test_arena_alignment_preserved) {
    IRArena* arena = ir_arena_create(1000);
    ASSERT_NONNULL(arena);

    // Test 8-byte alignment (commonly needed)
    void* ptr8 = ir_arena_alloc_aligned(arena, 64, 8);
    ASSERT_NONNULL(ptr8);
    ASSERT_TRUE(((uintptr_t)ptr8 & 7) == 0);  // 8-byte aligned

    // Test 16-byte alignment (SSE/AVX needs this)
    void* ptr16 = ir_arena_alloc_aligned(arena, 64, 16);
    ASSERT_NONNULL(ptr16);
    ASSERT_TRUE(((uintptr_t)ptr16 & 15) == 0);  // 16-byte aligned

    ir_arena_destroy(arena);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRMemory Tests");

    // Pool allocator tests
    RUN_TEST(test_pool_create);
    RUN_TEST(test_pool_alloc_component);
    RUN_TEST(test_pool_alloc_multiple);
    RUN_TEST(test_pool_stats);
    RUN_TEST(test_pool_reuse_after_free);

    // Arena allocator tests
    RUN_TEST(test_arena_create_default_size);
    RUN_TEST(test_arena_create_custom_size);
    RUN_TEST(test_arena_alloc);
    RUN_TEST(test_arena_alloc_aligned);
    RUN_TEST(test_arena_alloc_zero);
    RUN_TEST(test_arena_remaining);
    RUN_TEST(test_arena_reset);
    RUN_TEST(test_arena_strdup);
    RUN_TEST(test_arena_strdup_empty);
    RUN_TEST(test_arena_strndup);
    RUN_TEST(test_arena_strndup_zero_length);
    RUN_TEST(test_arena_create_from_buffer);
    RUN_TEST(test_arena_large_allocation);
    RUN_TEST(test_arena_multiple_small_allocations);
    RUN_TEST(test_arena_alignment_preserved);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
