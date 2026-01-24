/**
 * Hare Parser Unit Tests
 *
 * Tests the Hare DSL parser that converts Hare source to KIR JSON.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../../../ir/parsers/hare/hare_parser.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void test_simple_component(void) {
    const char* source =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b => b.width(\"100%\"); }\n"
        "};\n";

    char* result = ir_hare_to_kir(source, 0);
    if (!result) {
        fprintf(stderr, "FAIL: Simple component - NULL result: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    printf("Test 1 - Simple component output:\n%s\n\n", result);

    // Verify basic structure
    if (strstr(result, "\"root\"") == NULL) {
        fprintf(stderr, "FAIL: Simple component - missing 'root' key\n");
        tests_failed++;
    } else if (strstr(result, "\"type\":\"Container\"") == NULL) {
        fprintf(stderr, "FAIL: Simple component - missing component type\n");
        tests_failed++;
    } else if (strstr(result, "\"width\"") == NULL) {
        fprintf(stderr, "FAIL: Simple component - missing 'width' property\n");
        tests_failed++;
    } else if (strstr(result, "\"100%\"") == NULL) {
        fprintf(stderr, "FAIL: Simple component - missing width value\n");
        tests_failed++;
    } else {
        printf("PASS: Simple component\n");
        tests_passed++;
    }

    free(result);
    ir_hare_clear_error();
}

static void test_nested_children(void) {
    const char* source =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.child(row { r =>\n"
        "            r.gap(10);\n"
        "        });\n"
        "    }\n"
        "};\n";

    char* result = ir_hare_to_kir(source, 0);
    if (!result) {
        fprintf(stderr, "FAIL: Nested children - NULL result: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    printf("Test 2 - Nested children output:\n%s\n\n", result);

    // Verify nested structure
    if (strstr(result, "\"children\"") == NULL) {
        fprintf(stderr, "FAIL: Nested children - missing 'children' array\n");
        tests_failed++;
    } else if (strstr(result, "\"type\":\"Container\"") == NULL) {
        fprintf(stderr, "FAIL: Nested children - missing Container type\n");
        tests_failed++;
    } else if (strstr(result, "\"type\":\"Row\"") == NULL) {
        fprintf(stderr, "FAIL: Nested children - missing Row type\n");
        tests_failed++;
    } else if (strstr(result, "\"gap\"") == NULL) {
        fprintf(stderr, "FAIL: Nested children - missing 'gap' property\n");
        tests_failed++;
    } else {
        printf("PASS: Nested children\n");
        tests_passed++;
    }

    free(result);
    ir_hare_clear_error();
}

static void test_multiple_properties(void) {
    const char* source =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    text { t =>\n"
        "        t.text(\"Hello, Hare!\");\n"
        "        t.font_size(32);\n"
        "    }\n"
        "};\n";

    char* result = ir_hare_to_kir(source, 0);
    if (!result) {
        fprintf(stderr, "FAIL: Multiple properties - NULL result: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    printf("Test 3 - Multiple properties output:\n%s\n\n", result);

    // Verify multiple properties
    if (strstr(result, "\"text\"") == NULL) {
        fprintf(stderr, "FAIL: Multiple properties - missing 'text' property\n");
        tests_failed++;
    } else if (strstr(result, "\"font_size\"") == NULL) {
        fprintf(stderr, "FAIL: Multiple properties - missing 'font_size' property\n");
        tests_failed++;
    } else if (strstr(result, "\"Hello, Hare!\"") == NULL) {
        fprintf(stderr, "FAIL: Multiple properties - missing text value\n");
        tests_failed++;
    } else if (strstr(result, "\"font_size\":32") == NULL) {
        fprintf(stderr, "FAIL: Multiple properties - missing font size value\n");
        tests_failed++;
    } else {
        printf("PASS: Multiple properties\n");
        tests_passed++;
    }

    free(result);
    ir_hare_clear_error();
}

static void test_full_app(void) {
    const char* source =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.width(\"100%\");\n"
        "        b.height(\"100%\");\n"
        "        b.child(row { r =>\n"
        "            r.gap(10);\n"
        "            r.child(text { t =>\n"
        "                t.text(\"Hello, Hare!\");\n"
        "                t.font_size(32);\n"
        "            });\n"
        "        });\n"
        "    }\n"
        "};\n";

    char* result = ir_hare_to_kir(source, 0);
    if (!result) {
        fprintf(stderr, "FAIL: Full app - NULL result: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    printf("Test 4 - Full app output:\n%s\n\n", result);

    // Verify complete structure
    int checks = 0;
    int errors = 0;

    if (strstr(result, "\"root\"")) checks++; else errors++;
    if (strstr(result, "\"type\":\"Container\"")) checks++; else errors++;
    if (strstr(result, "\"width\":\"100%\"")) checks++; else errors++;
    if (strstr(result, "\"height\":\"100%\"")) checks++; else errors++;
    if (strstr(result, "\"children\"")) checks++; else errors++;
    if (strstr(result, "\"type\":\"Row\"")) checks++; else errors++;
    if (strstr(result, "\"gap\":10")) checks++; else errors++;
    if (strstr(result, "\"type\":\"Text\"")) checks++; else errors++;
    if (strstr(result, "\"text\":\"Hello, Hare!\"")) checks++; else errors++;
    if (strstr(result, "\"font_size\":32")) checks++; else errors++;

    if (errors > 0) {
        fprintf(stderr, "FAIL: Full app - %d/%d checks passed\n", checks, checks + errors);
        tests_failed++;
    } else {
        printf("PASS: Full app - all %d checks passed\n", checks);
        tests_passed++;
    }

    free(result);
    ir_hare_clear_error();
}

static void test_error_handling(void) {
    const char* invalid = "not valid hare code at all";

    char* result = ir_hare_to_kir(invalid, 0);
    if (result != NULL) {
        fprintf(stderr, "FAIL: Error handling - should return NULL for invalid input\n");
        tests_failed++;
        free(result);
    } else if (ir_hare_get_error() == NULL) {
        fprintf(stderr, "FAIL: Error handling - should set error message\n");
        tests_failed++;
    } else {
        printf("PASS: Error handling - got expected error: %s\n", ir_hare_get_error());
        tests_passed++;
    }

    ir_hare_clear_error();
}

int main(void) {
    printf("=== Hare Parser Unit Tests ===\n\n");

    test_simple_component();
    test_nested_children();
    test_multiple_properties();
    test_full_app();
    test_error_handling();

    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
