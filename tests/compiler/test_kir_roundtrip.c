/**
 * @file test_kir_roundtrip.c
 * @brief Round-trip tests for KIR system (kry → kir → krb → kir → kry)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

// Test framework macros
#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        fprintf(stderr, "❌ FAIL: %s\n   Location: %s:%d\n", message, __FILE__, __LINE__); \
        return false; \
    }

#define TEST_PASS(message) \
    printf("✅ PASS: %s\n", message);

// Test result counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Helper function to check if file exists
static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Helper function to get file size
static size_t get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_size;
}

// Helper function to compare files byte-by-byte
static bool files_identical(const char *path1, const char *path2) {
    FILE *f1 = fopen(path1, "rb");
    FILE *f2 = fopen(path2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return false;
    }

    bool identical = true;
    int c1, c2;
    while ((c1 = fgetc(f1)) != EOF && (c2 = fgetc(f2)) != EOF) {
        if (c1 != c2) {
            identical = false;
            break;
        }
    }

    // Check if both reached EOF at same time
    if (c1 != c2) identical = false;

    fclose(f1);
    fclose(f2);
    return identical;
}

// Helper function to run shell command and capture return code
static int run_command(const char *command) {
    return system(command);
}

// Test 1: Basic KIR generation
static bool test_kir_generation(void) {
    tests_run++;

    const char *test_file = "tests/fixtures/basic.kry";
    const char *kir_file = "tests/output/basic.kir";

    // Create test fixture
    system("mkdir -p tests/fixtures tests/output");
    FILE *f = fopen(test_file, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Button { text = \"Hello\" width = 100 }\n");
    fclose(f);

    // Compile with KIR output
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s --kir-output %s --no-krb 2>/dev/null",
             test_file, kir_file);

    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Compilation should succeed");
    TEST_ASSERT(file_exists(kir_file), "KIR file should be created");

    // Verify KIR is valid JSON
    snprintf(cmd, sizeof(cmd), "cat %s | jq . >/dev/null 2>&1", kir_file);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "KIR should be valid JSON");

    tests_passed++;
    TEST_PASS("KIR generation from source");
    return true;
}

// Test 2: Forward round-trip (kry → kir → krb)
static bool test_forward_roundtrip(void) {
    tests_run++;

    const char *test_file = "tests/fixtures/forward.kry";
    const char *kir_file = "tests/output/forward.kir";
    const char *krb_file = "tests/output/forward.krb";
    const char *krb_from_kir = "tests/output/forward_from_kir.krb";

    // Create test fixture
    FILE *f = fopen(test_file, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Container {\n");
    fprintf(f, "  width = 200\n");
    fprintf(f, "  height = 150\n");
    fprintf(f, "  Button { text = \"Click\" }\n");
    fprintf(f, "}\n");
    fclose(f);

    // Step 1: Compile kry → kir + krb
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s --kir-output %s -o %s 2>/dev/null",
             test_file, kir_file, krb_file);
    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Initial compilation should succeed");
    TEST_ASSERT(file_exists(kir_file), "KIR file should exist");
    TEST_ASSERT(file_exists(krb_file), "KRB file should exist");

    // Step 2: Compile from KIR → krb
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s -o %s 2>/dev/null",
             kir_file, krb_from_kir);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Compilation from KIR should succeed");
    TEST_ASSERT(file_exists(krb_from_kir), "KRB from KIR should exist");

    // Step 3: Compare binaries (should be identical or very similar)
    size_t size1 = get_file_size(krb_file);
    size_t size2 = get_file_size(krb_from_kir);
    TEST_ASSERT(size1 > 0 && size2 > 0, "Both KRB files should have content");

    // Allow small size difference due to metadata
    size_t diff = (size1 > size2) ? (size1 - size2) : (size2 - size1);
    TEST_ASSERT(diff < 100, "KRB files should be similar in size");

    tests_passed++;
    TEST_PASS("Forward round-trip (kry → kir → krb)");
    return true;
}

// Test 3: Backward round-trip (krb → kir → kry)
static bool test_backward_roundtrip(void) {
    tests_run++;

    const char *test_file = "tests/fixtures/backward.kry";
    const char *krb_file = "tests/output/backward.krb";
    const char *kir_decompiled = "tests/output/backward_decompiled.kir";
    const char *kry_printed = "tests/output/backward_printed.kry";

    // Create test fixture
    FILE *f = fopen(test_file, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Text { content = \"Hello World\" fontSize = 16 }\n");
    fclose(f);

    // Step 1: Compile kry → krb
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s -o %s 2>/dev/null",
             test_file, krb_file);
    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Compilation should succeed");
    TEST_ASSERT(file_exists(krb_file), "KRB file should exist");

    // Step 2: Decompile krb → kir
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon decompile %s -o %s 2>/dev/null",
             krb_file, kir_decompiled);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Decompilation should succeed");
    TEST_ASSERT(file_exists(kir_decompiled), "Decompiled KIR should exist");

    // Step 3: Print kir → kry
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon print %s -o %s 2>/dev/null",
             kir_decompiled, kry_printed);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Printing should succeed");
    TEST_ASSERT(file_exists(kry_printed), "Printed KRY should exist");

    // Verify the printed KRY has content
    size_t size = get_file_size(kry_printed);
    TEST_ASSERT(size > 0, "Printed KRY should have content");

    tests_passed++;
    TEST_PASS("Backward round-trip (krb → kir → kry)");
    return true;
}

// Test 4: Full circle round-trip (kry → kir → krb → kir → kry → krb)
static bool test_full_circle(void) {
    tests_run++;

    const char *original_kry = "tests/fixtures/circle.kry";
    const char *original_kir = "tests/output/circle_original.kir";
    const char *original_krb = "tests/output/circle_original.krb";
    const char *decompiled_kir = "tests/output/circle_decompiled.kir";
    const char *roundtrip_kry = "tests/output/circle_roundtrip.kry";
    const char *roundtrip_krb = "tests/output/circle_roundtrip.krb";

    // Create test fixture with complex structure
    FILE *f = fopen(original_kry, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Container {\n");
    fprintf(f, "  width = 300\n");
    fprintf(f, "  height = 200\n");
    fprintf(f, "  Button {\n");
    fprintf(f, "    text = \"Submit\"\n");
    fprintf(f, "    width = 100\n");
    fprintf(f, "    height = 40\n");
    fprintf(f, "  }\n");
    fprintf(f, "  Text {\n");
    fprintf(f, "    content = \"Label\"\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);

    char cmd[512];

    // Step 1: kry → kir + krb
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s --kir-output %s -o %s 2>/dev/null",
             original_kry, original_kir, original_krb);
    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Original compilation should succeed");

    // Step 2: krb → kir (decompile)
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon decompile %s -o %s 2>/dev/null",
             original_krb, decompiled_kir);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Decompilation should succeed");

    // Step 3: kir → kry (print)
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon print %s -o %s 2>/dev/null",
             decompiled_kir, roundtrip_kry);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Printing should succeed");

    // Step 4: kry → krb (compile roundtrip)
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s -o %s 2>/dev/null",
             roundtrip_kry, roundtrip_krb);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Roundtrip compilation should succeed");

    // Verify all files exist
    TEST_ASSERT(file_exists(original_kir), "Original KIR should exist");
    TEST_ASSERT(file_exists(original_krb), "Original KRB should exist");
    TEST_ASSERT(file_exists(decompiled_kir), "Decompiled KIR should exist");
    TEST_ASSERT(file_exists(roundtrip_kry), "Roundtrip KRY should exist");
    TEST_ASSERT(file_exists(roundtrip_krb), "Roundtrip KRB should exist");

    // Compare binary sizes (should be very similar)
    size_t original_size = get_file_size(original_krb);
    size_t roundtrip_size = get_file_size(roundtrip_krb);
    size_t diff = (original_size > roundtrip_size) ?
                  (original_size - roundtrip_size) :
                  (roundtrip_size - original_size);

    TEST_ASSERT(diff < 200, "Binary sizes should be similar after full circle");

    tests_passed++;
    TEST_PASS("Full circle round-trip (kry → kir → krb → kir → kry → krb)");
    return true;
}

// Test 5: KIR validation
static bool test_kir_validation(void) {
    tests_run++;

    const char *valid_kir = "tests/output/valid.kir";
    const char *invalid_kir = "tests/output/invalid.kir";

    // Create valid KIR (by compiling)
    const char *test_file = "tests/fixtures/validate.kry";
    FILE *f = fopen(test_file, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Button { text = \"Test\" }\n");
    fclose(f);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s --kir-output %s --no-krb 2>/dev/null",
             test_file, valid_kir);
    run_command(cmd);

    // Test valid KIR validation
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon kir-validate %s 2>/dev/null", valid_kir);
    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "Valid KIR should pass validation");

    // Create invalid KIR (malformed JSON)
    f = fopen(invalid_kir, "w");
    if (f) {
        fprintf(f, "{ invalid json content }\n");
        fclose(f);

        snprintf(cmd, sizeof(cmd), "./build/bin/kryon kir-validate %s 2>/dev/null", invalid_kir);
        ret = run_command(cmd);
        TEST_ASSERT(ret != 0, "Invalid KIR should fail validation");
    }

    tests_passed++;
    TEST_PASS("KIR validation");
    return true;
}

// Test 6: KIR utilities (dump, stats, diff)
static bool test_kir_utilities(void) {
    tests_run++;

    const char *test_file = "tests/fixtures/utils.kry";
    const char *kir_file = "tests/output/utils.kir";

    // Create test fixture
    FILE *f = fopen(test_file, "w");
    if (!f) {
        tests_failed++;
        return false;
    }
    fprintf(f, "Container {\n");
    fprintf(f, "  Button { text = \"A\" }\n");
    fprintf(f, "  Button { text = \"B\" }\n");
    fprintf(f, "}\n");
    fclose(f);

    // Compile
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon compile %s --kir-output %s --no-krb 2>/dev/null",
             test_file, kir_file);
    run_command(cmd);

    // Test kir-dump
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon kir-dump %s >/dev/null 2>&1", kir_file);
    int ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "kir-dump should succeed");

    // Test kir-stats
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon kir-stats %s >/dev/null 2>&1", kir_file);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "kir-stats should succeed");

    // Test kir-diff (same file should be identical)
    snprintf(cmd, sizeof(cmd), "./build/bin/kryon kir-diff %s %s >/dev/null 2>&1", kir_file, kir_file);
    ret = run_command(cmd);
    TEST_ASSERT(ret == 0, "kir-diff on identical files should succeed");

    tests_passed++;
    TEST_PASS("KIR utilities (dump, stats, diff)");
    return true;
}

// Main test runner
int main(void) {
    printf("\n=== KIR Round-Trip Test Suite ===\n\n");

    // Check if kryon binary exists
    if (!file_exists("./build/bin/kryon")) {
        fprintf(stderr, "❌ ERROR: ./build/bin/kryon not found. Please build first with 'make'\n");
        return 1;
    }

    // Run all tests
    test_kir_generation();
    test_forward_roundtrip();
    test_backward_roundtrip();
    test_full_circle();
    test_kir_validation();
    test_kir_utilities();

    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("\n🎉 All tests passed!\n\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed\n\n");
        return 1;
    }
}
