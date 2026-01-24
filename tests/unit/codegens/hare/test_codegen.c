/**
 * Hare Codegen Test
 * Tests the Hare code generator with a sample KIR file
 */

#define _POSIX_C_SOURCE 200809L
#include "hare_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

// Simple test KIR JSON
// Note: Hare codegen expects "root" or "component" at top level, not a wrapper
static const char* test_kir =
"{"
"  \"root\": {"
"    \"type\": \"Container\","
"    \"width\": \"100%\","
"    \"height\": \"100%\","
"    \"backgroundColor\": \"#1a1a1a\","
"    \"children\": ["
"      {"
"        \"type\": \"Row\","
"        \"gap\": 10,"
"        \"children\": ["
"          {"
"            \"type\": \"Text\","
"            \"text\": \"Hello, Hare!\","
"            \"fontSize\": 32,"
"            \"fontWeight\": 700,"
"            \"color\": \"#00ff00\""
"          },"
"          {"
"            \"type\": \"Button\","
"            \"text\": \"Click Me\","
"            \"width\": \"200px\","
"            \"height\": \"50px\""
"          }"
"        ]"
"      }"
"    ]"
"  }"
"}";

// Helper function to create a temporary file
static char* create_temp_file(const char* content, size_t len) {
    char template[] = "/tmp/kryon_hare_test_XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        perror("mkstemp");
        return NULL;
    }

    ssize_t written = write(fd, content, len);
    if (written < 0 || (size_t)written != len) {
        perror("write");
        close(fd);
        return NULL;
    }

    close(fd);
    return strdup(template);
}

// Helper function to create a temporary directory
static char* create_temp_dir(void) {
    char template[] = "/tmp/kryon_hare_test_dir_XXXXXX";
    char* result = mkdtemp(template);
    if (result == NULL) {
        perror("mkdtemp");
        return NULL;
    }
    return strdup(result);
}

// Helper function to check if a file exists
static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Test 1: Generate Hare code from KIR JSON
static bool test_codegen_from_json(void) {
    printf("Test 1: Generate Hare code from KIR JSON... ");

    char* hare_code = hare_codegen_from_json(test_kir);
    if (!hare_code) {
        printf("FAILED (hare_codegen_from_json returned NULL)\n");
        return false;
    }

    // Check that the output contains expected Hare keywords
    bool has_use = strstr(hare_code, "use") != NULL;
    bool has_fn = strstr(hare_code, "fn") != NULL;
    bool has_container = strstr(hare_code, "container") != NULL;

    free(hare_code);

    if (!has_use || !has_fn || !has_container) {
        printf("FAILED (output missing expected Hare keywords)\n");
        return false;
    }

    printf("PASSED\n");
    return true;
}

// Test 2: Generate Hare code from KIR file
static bool test_codegen_from_file(void) {
    printf("Test 2: Generate Hare code from KIR file... ");

    // Create temporary KIR file
    char* kir_file = create_temp_file(test_kir, strlen(test_kir));
    if (!kir_file) {
        printf("FAILED (could not create temp KIR file)\n");
        return false;
    }

    // Create temporary output directory
    char* output_dir = create_temp_dir();
    if (!output_dir) {
        free(kir_file);
        printf("FAILED (could not create temp output dir)\n");
        return false;
    }

    // Generate Hare code
    bool success = hare_codegen_generate(kir_file, output_dir);

    // Check if main.ha was created
    char main_ha[512];
    snprintf(main_ha, sizeof(main_ha), "%s/main.ha", output_dir);
    bool main_exists = file_exists(main_ha);

    // Cleanup
    unlink(kir_file);
    rmdir(output_dir);
    free(kir_file);
    free(output_dir);

    if (!success || !main_exists) {
        printf("FAILED (codegen=%d, main.ha exists=%d)\n", success, main_exists);
        return false;
    }

    printf("PASSED\n");
    return true;
}

// Test 3: Test cJSON_to_hare_syntax function
static bool test_cjson_to_hare_syntax(void) {
    printf("Test 3: Test cJSON_to_hare_syntax... ");

    // Test with proper KIR structure
    char* hare_code = hare_codegen_from_json("{\"root\": {\"type\": \"Text\", \"text\": \"test\"}}");
    if (!hare_code) {
        printf("FAILED (hare_codegen_from_json returned NULL)\n");
        return false;
    }

    // Check that the generated code contains Hare keywords
    bool has_main = strstr(hare_code, "fn main") != NULL;

    free(hare_code);

    if (!has_main) {
        printf("FAILED (output missing main function)\n");
        return false;
    }

    printf("PASSED\n");
    return true;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=== Hare Codegen Tests ===\n\n");

    int passed = 0;
    int total = 0;

    // Run tests
    total++;
    if (test_codegen_from_json()) passed++;

    total++;
    if (test_codegen_from_file()) passed++;

    total++;
    if (test_cjson_to_hare_syntax()) passed++;

    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d/%d\n", passed, total);

    return (passed == total) ? 0 : 1;
}
