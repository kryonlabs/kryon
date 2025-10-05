#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

// Include Kryon headers
#include "runtime.h"

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ ASSERTION FAILED: %s at %s:%d\n", message, __FILE__, __LINE__); \
            return 0; \
        } else { \
            printf("✅ PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_SETUP(test_name) \
    printf("\n🧪 Running integration test: %s\n", test_name)

// Helper function to create test .kry files
int create_test_file(const char* filename, const char* content) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("❌ Failed to create test file: %s\n", filename);
        return 0;
    }
    
    fprintf(file, "%s", content);
    fclose(file);
    
    printf("📝 Created test file: %s\n", filename);
    return 1;
}

// Helper function to run kryon compile command
int compile_kryon_file(const char* input_file, const char* output_file) {
    char command[512];
    snprintf(command, sizeof(command), "../build/bin/kryon compile %s -o %s", input_file, output_file);
    
    printf("🔧 Compiling: %s\n", command);
    int result = system(command);
    
    if (result == 0) {
        printf("✅ Compilation successful\n");
        return 1;
    } else {
        printf("❌ Compilation failed with exit code: %d\n", result);
        return 0;
    }
}

// Helper function to check if file exists
int file_exists(const char* filename) {
    struct stat st;
    return stat(filename, &st) == 0;
}

// Test 1: Basic @for directive compilation
int test_basic_for_compilation() {
    TEST_SETUP("Basic @for compilation");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Basic For Test\"\n"
        "}\n\n"
        "@var items = [\"apple\", \"banana\"]\n\n"
        "Column {\n"
        "    @for item in items {\n"
        "        Text {\n"
        "            text = item\n"
        "        }\n"
        "    }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_basic_for.kry", test_content), "Test file creation");
    TEST_ASSERT(compile_kryon_file("test_basic_for.kry", "test_basic_for.krb"), "Basic @for compilation");
    TEST_ASSERT(file_exists("test_basic_for.krb"), "Output KRB file exists");
    
    // Cleanup
    unlink("test_basic_for.kry");
    unlink("test_basic_for.krb");
    
    return 1;
}

// Test 2: Empty array @for directive
int test_empty_array_for() {
    TEST_SETUP("Empty array @for");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Empty For Test\"\n"
        "}\n\n"
        "@var empty = []\n\n"
        "Column {\n"
        "    Text { text = \"Before\" }\n"
        "    @for item in empty {\n"
        "        Text { text = item }\n"
        "    }\n"
        "    Text { text = \"After\" }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_empty_for.kry", test_content), "Empty array test file creation");
    TEST_ASSERT(compile_kryon_file("test_empty_for.kry", "test_empty_for.krb"), "Empty array @for compilation");
    TEST_ASSERT(file_exists("test_empty_for.krb"), "Empty array output KRB file exists");
    
    // Cleanup
    unlink("test_empty_for.kry");
    unlink("test_empty_for.krb");
    
    return 1;
}

// Test 3: Complex @for with multiple properties
int test_complex_for_properties() {
    TEST_SETUP("Complex @for with multiple properties");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Complex For Test\"\n"
        "}\n\n"
        "@var colors = [\"red\", \"green\", \"blue\"]\n\n"
        "Column {\n"
        "    gap = 10\n"
        "    @for color in colors {\n"
        "        Container {\n"
        "            padding = 20\n"
        "            background = \"#FFFFFF\"\n"
        "            Text {\n"
        "                text = color\n"
        "                fontSize = 16\n"
        "                color = \"#000000\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_complex_for.kry", test_content), "Complex test file creation");
    TEST_ASSERT(compile_kryon_file("test_complex_for.kry", "test_complex_for.krb"), "Complex @for compilation");
    TEST_ASSERT(file_exists("test_complex_for.krb"), "Complex output KRB file exists");
    
    // Cleanup
    unlink("test_complex_for.kry");
    unlink("test_complex_for.krb");
    
    return 1;
}

// Test 4: Nested @for directives
int test_nested_for_directives() {
    TEST_SETUP("Nested @for directives");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Nested For Test\"\n"
        "}\n\n"
        "@var outer = [\"A\", \"B\"]\n"
        "@var inner = [\"1\", \"2\"]\n\n"
        "Column {\n"
        "    @for outer_item in outer {\n"
        "        Row {\n"
        "            Text { text = outer_item }\n"
        "            @for inner_item in inner {\n"
        "                Text { text = inner_item }\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_nested_for.kry", test_content), "Nested test file creation");
    TEST_ASSERT(compile_kryon_file("test_nested_for.kry", "test_nested_for.krb"), "Nested @for compilation");
    TEST_ASSERT(file_exists("test_nested_for.krb"), "Nested output KRB file exists");
    
    // Cleanup
    unlink("test_nested_for.kry");
    unlink("test_nested_for.krb");
    
    return 1;
}

// Test 5: @for with various element types
int test_for_with_various_elements() {
    TEST_SETUP("@for with various element types");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Various Elements For Test\"\n"
        "}\n\n"
        "@var items = [\"Button\", \"Input\", \"Text\"]\n\n"
        "Column {\n"
        "    @for item in items {\n"
        "        Button {\n"
        "            text = item\n"
        "            width = 100\n"
        "            height = 30\n"
        "        }\n"
        "    }\n"
        "    @for item in items {\n"
        "        Text {\n"
        "            text = item\n"
        "            fontSize = 14\n"
        "        }\n"
        "    }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_various_elements_for.kry", test_content), "Various elements test file creation");
    TEST_ASSERT(compile_kryon_file("test_various_elements_for.kry", "test_various_elements_for.krb"), "Various elements @for compilation");
    TEST_ASSERT(file_exists("test_various_elements_for.krb"), "Various elements output KRB file exists");
    
    // Cleanup
    unlink("test_various_elements_for.kry");
    unlink("test_various_elements_for.krb");
    
    return 1;
}

// Test 6: @for stress test with many items
int test_for_stress_test() {
    TEST_SETUP("@for stress test");
    
    const char* test_content = 
        "@metadata {\n"
        "    title = \"Stress Test\"\n"
        "}\n\n"
        "@var many_items = [\"item1\", \"item2\", \"item3\", \"item4\", \"item5\", \"item6\", \"item7\", \"item8\", \"item9\", \"item10\", \"item11\", \"item12\", \"item13\", \"item14\", \"item15\"]\n\n"
        "Column {\n"
        "    @for item in many_items {\n"
        "        Text { text = item }\n"
        "    }\n"
        "}\n";
    
    TEST_ASSERT(create_test_file("test_stress_for.kry", test_content), "Stress test file creation");
    TEST_ASSERT(compile_kryon_file("test_stress_for.kry", "test_stress_for.krb"), "Stress test @for compilation");
    TEST_ASSERT(file_exists("test_stress_for.krb"), "Stress test output KRB file exists");
    
    // Cleanup
    unlink("test_stress_for.kry");
    unlink("test_stress_for.krb");
    
    return 1;
}

int main() {
    printf("🧪 Starting @for Directive Integration Test Suite\n");
    printf("=================================================\n");
    
    int tests_passed = 0;
    int total_tests = 6;
    
    // Change to tests directory for relative paths to work
    if (chdir("tests") != 0) {
        printf("❌ Failed to change to tests directory\n");
        return 1;
    }
    
    // Run all integration tests
    if (test_basic_for_compilation()) tests_passed++;
    if (test_empty_array_for()) tests_passed++;
    if (test_complex_for_properties()) tests_passed++;
    if (test_nested_for_directives()) tests_passed++;
    if (test_for_with_various_elements()) tests_passed++;
    if (test_for_stress_test()) tests_passed++;
    
    printf("\n📊 Integration Test Results:\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests) {
        printf("🎉 All @for directive integration tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed. Check output above.\n");
        return 1;
    }
}
