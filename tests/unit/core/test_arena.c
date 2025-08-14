#include "unity.h"
#include "memory.h"
#include <stdint.h>

#define KRYON_ARENA_DEFAULT_ALIGNMENT 8

// Define a setup function to run before each test
void setUp(void) {
    // This is run before EACH test
}

// Define a teardown function to run after each test
void tearDown(void) {
    // This is run after EACH test
}

void test_KryonArena_CreateAndDestroy(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(arena);
    TEST_ASSERT_EQUAL(1024, arena->size);
    TEST_ASSERT_EQUAL(0, arena->used);
    TEST_ASSERT_NOT_NULL(arena->memory);
    kryon_arena_destroy(arena);
}

void test_KryonArena_Alloc(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);
    void* ptr = kryon_arena_alloc(arena, 128, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL(128, arena->used);

    void* ptr2 = kryon_arena_alloc(arena, 256, 0);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_EQUAL(128 + 256, arena->used);

    kryon_arena_destroy(arena);
}

void test_KryonArena_Alloc_Alignment(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);

    // Allocate a small amount to misalign the offset
    kryon_arena_alloc(arena, 1, 1);
    TEST_ASSERT_EQUAL(1, arena->used);

    // Allocate with default alignment
    void* ptr = kryon_arena_alloc(arena, 16, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    // Check if the pointer is aligned to KRYON_ARENA_DEFAULT_ALIGNMENT
    TEST_ASSERT_EQUAL(0, (uintptr_t)ptr % KRYON_ARENA_DEFAULT_ALIGNMENT);

    kryon_arena_destroy(arena);
}


void test_KryonArena_Alloc_ZeroSize(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);
    void* ptr = kryon_arena_alloc(arena, 0, 0);
    TEST_ASSERT_NULL(ptr);
    TEST_ASSERT_EQUAL(0, arena->used);
    kryon_arena_destroy(arena);
}

void test_KryonArena_Alloc_TooLarge(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);
    void* ptr = kryon_arena_alloc(arena, 2048, 0);
    TEST_ASSERT_NULL(ptr);
    TEST_ASSERT_EQUAL(0, arena->used);
    kryon_arena_destroy(arena);
}

void test_KryonArena_Reset(void) {
    KryonArena* arena = kryon_arena_create(1024, KRYON_ARENA_DEFAULT_ALIGNMENT);
    kryon_arena_alloc(arena, 512, 0);
    TEST_ASSERT_EQUAL(512, arena->used);

    kryon_arena_reset(arena);
    TEST_ASSERT_EQUAL(0, arena->used);

    // Ensure we can still allocate after reset
    void* ptr = kryon_arena_alloc(arena, 256, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL(256, arena->used);

    kryon_arena_destroy(arena);
}

// Main function to run the tests
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_KryonArena_CreateAndDestroy);
    RUN_TEST(test_KryonArena_Alloc);
    RUN_TEST(test_KryonArena_Alloc_Alignment);
    RUN_TEST(test_KryonArena_Alloc_ZeroSize);
    RUN_TEST(test_KryonArena_Alloc_TooLarge);
    RUN_TEST(test_KryonArena_Reset);
    return UNITY_END();
}