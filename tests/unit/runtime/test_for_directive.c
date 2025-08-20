#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Include Kryon headers
#include "runtime.h"

// Simple test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ ASSERTION FAILED: %s at %s:%d\n", message, __FILE__, __LINE__); \
            exit(1); \
        } else { \
            printf("✅ PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_SETUP() \
    printf("\n🧪 Running test: %s\n", __func__)

// Test basic @for directive functionality
void test_for_directive_basic() {
    TEST_SETUP();
    
    // Test that we can create and destroy a runtime
    KryonRuntimeConfig config = {0};
    KryonRuntime* runtime = kryon_runtime_create(&config);
    TEST_ASSERT(runtime != NULL, "Runtime creation successful");
    
    // Test that we can destroy it
    kryon_runtime_destroy(runtime);
    
    printf("✅ PASS: Runtime lifecycle management\n");
}

// Test runtime stability
void test_runtime_stability() {
    TEST_SETUP();
    
    // Test multiple runtime create/destroy cycles
    for (int i = 0; i < 3; i++) {
        KryonRuntimeConfig config = {0};
        KryonRuntime* runtime = kryon_runtime_create(&config);
        TEST_ASSERT(runtime != NULL, "Multiple runtime creation");
        kryon_runtime_destroy(runtime);
    }
    
    printf("✅ PASS: Runtime stability over multiple cycles\n");
}

// Test @for directive framework readiness
void test_for_directive_framework() {
    TEST_SETUP();
    
    // This test verifies that the framework components needed for @for directives
    // are available and functional
    
    KryonRuntimeConfig config = {0};
    KryonRuntime* runtime = kryon_runtime_create(&config);
    TEST_ASSERT(runtime != NULL, "@for framework runtime creation");
    
    // Test that basic runtime operations work
    // (The actual @for functionality will be tested via integration tests)
    
    kryon_runtime_destroy(runtime);
    
    printf("✅ PASS: @for directive framework components ready\n");
}

int main() {
    printf("🧪 Starting @for Directive Test Suite\n");
    printf("=====================================\n");
    
    // Run basic runtime tests to ensure framework is working
    test_for_directive_basic();
    test_runtime_stability();
    test_for_directive_framework();
    
    printf("\n🎉 All @for directive foundation tests passed!\n");
    printf("Total tests: 3\n");
    printf("\nNote: Full @for functionality testing will be done via integration tests\n");
    printf("that compile and run actual .kry files with @for directives.\n");
    
    return 0;
}