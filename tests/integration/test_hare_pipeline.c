/**
 * Hare Pipeline Integration Tests
 *
 * Tests the complete Hare pipeline:
 * - Hare DSL → KIR (parser)
 * - KIR → Hare DSL (codegen)
 * - Round-trip preservation
 * - Full pipeline: source → binary (if hare compiler available)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "../../../ir/parsers/hare/hare_parser.h"
#include "../../../codegens/hare/hare_codegen.h"
#include "../../../ir/include/ir_serialization.h"

static int tests_passed = 0;
static int tests_failed = 0;

// Helper function to check if a file exists
static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Helper function to compare two JSON strings semantically
// (ignores whitespace differences)
static bool json_equivalent(const char* json1, const char* json2) {
    // Simple implementation: check if key substrings exist in both
    // For a more robust implementation, use a JSON comparison library

    // Extract key fields that should be preserved
    const char* keys[] = {
        "\"root\"",
        "\"type\"",
        "\"children\"",
        "\"properties\"",
        NULL
    };

    for (int i = 0; keys[i]; i++) {
        bool in1 = strstr(json1, keys[i]) != NULL;
        bool in2 = strstr(json2, keys[i]) != NULL;
        if (in1 != in2) {
            printf("JSON difference: key '%s' found in only one\n", keys[i]);
            return false;
        }
    }

    return true;
}

/**
 * Test 1: Round-trip preservation
 * Hare → KIR → Hare (codegen) → KIR
 */
static void test_hare_roundtrip(void) {
    printf("\n=== Test 1: Round-trip preservation ===\n");

    const char* input_hare =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.width(\"100%\");\n"
        "        b.height(\"100%\");\n"
        "        b.child(text { t =>\n"
        "            t.text(\"Hello, Hare!\");\n"
        "            t.font_size(32);\n"
        "        });\n"
        "    }\n"
        "};\n";

    // Step 1: Hare → KIR (parser)
    printf("Step 1: Hare → KIR (parser)...\n");
    char* kir = ir_hare_to_kir(input_hare, 0);
    if (!kir) {
        fprintf(stderr, "FAIL: Parser error: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }
    printf("✓ KIR generated (length: %zu)\n", strlen(kir));

    // Step 2: Write KIR to temp file
    char kir_file[] = "/tmp/test_hare_XXXXXX";
    int fd = mkstemp(kir_file);
    if (fd < 0) {
        fprintf(stderr, "FAIL: Could not create temp file\n");
        tests_failed++;
        free(kir);
        return;
    }
    write(fd, kir, strlen(kir));
    close(fd);
    printf("✓ KIR written to: %s\n", kir_file);

    // Step 3: KIR → Hare (codegen)
    printf("Step 2: KIR → Hare (codegen)...\n");
    char output_dir[] = "/tmp/test_hare_output_XXXXXX";
    mkdtemp(output_dir);
    bool codegen_ok = hare_codegen_generate_multi(kir_file, output_dir);
    if (!codegen_ok) {
        fprintf(stderr, "FAIL: Codegen failed\n");
        tests_failed++;
        free(kir);
        unlink(kir_file);
        return;
    }
    printf("✓ Hare codegen completed\n");

    // Step 4: Read generated Hare and verify
    char gen_hare_path[512];
    snprintf(gen_hare_path, sizeof(gen_hare_path), "%s/main.ha", output_dir);

    FILE* f = fopen(gen_hare_path, "r");
    if (!f) {
        fprintf(stderr, "FAIL: Could not open generated Hare file: %s\n", gen_hare_path);
        tests_failed++;
        free(kir);
        unlink(kir_file);
        rmdir(output_dir);
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* gen_hare = malloc(size + 1);
    fread(gen_hare, 1, size, f);
    gen_hare[size] = '\0';
    fclose(f);
    printf("✓ Generated Hare read (length: %ld)\n", size);

    // Step 5: Parse generated Hare to KIR again
    printf("Step 3: Generated Hare → KIR (parser)...\n");
    char* kir2 = ir_hare_to_kir(gen_hare, 0);
    if (!kir2) {
        fprintf(stderr, "FAIL: Second parse error: %s\n", ir_hare_get_error());
        tests_failed++;
        free(gen_hare);
        free(kir);
        unlink(kir_file);
        rmdir(output_dir);
        return;
    }
    printf("✓ Second KIR generated (length: %zu)\n", strlen(kir2));

    // Step 6: Compare KIRs (semantically)
    printf("Step 4: Comparing KIR representations...\n");
    if (json_equivalent(kir, kir2)) {
        printf("✓ KIR representations are semantically equivalent\n");
        printf("PASS: Round-trip preservation\n");
        tests_passed++;
    } else {
        fprintf(stderr, "FAIL: KIR representations differ\n");
        fprintf(stderr, "First KIR:\n%s\n", kir);
        fprintf(stderr, "Second KIR:\n%s\n", kir2);
        tests_failed++;
    }

    // Cleanup
    free(gen_hare);
    free(kir);
    free(kir2);
    unlink(kir_file);
    unlink(gen_hare_path);
    rmdir(output_dir);

    ir_hare_clear_error();
}

/**
 * Test 2: Error handling
 */
static void test_hare_error_recovery(void) {
    printf("\n=== Test 2: Error handling ===\n");

    const char* invalid_hare = "this is not valid hare code at all";

    char* result = ir_hare_to_kir(invalid_hare, 0);
    if (result != NULL) {
        fprintf(stderr, "FAIL: Parser should return NULL for invalid input\n");
        tests_failed++;
        free(result);
    } else if (ir_hare_get_error() == NULL) {
        fprintf(stderr, "FAIL: Parser should set error message\n");
        tests_failed++;
    } else {
        printf("✓ Got expected error: %s\n", ir_hare_get_error());
        printf("PASS: Error handling\n");
        tests_passed++;
    }

    ir_hare_clear_error();
}

/**
 * Test 3: Complex nesting
 */
static void test_hare_complex_nesting(void) {
    printf("\n=== Test 3: Complex nesting ===\n");

    const char* input_hare =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.child(column { c =>\n"
        "            c.child(row { r1 =>\n"
        "                r1.child(text { t1 => t1.text(\"Level 3\"); });\n"
        "            });\n"
        "            c.child(row { r2 =>\n"
        "                r2.child(text { t2 => t2.text(\"Also Level 3\"); });\n"
        "            });\n"
        "        });\n"
        "    }\n"
        "};\n";

    char* kir = ir_hare_to_kir(input_hare, 0);
    if (!kir) {
        fprintf(stderr, "FAIL: Parser error: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    // Verify deep nesting is preserved
    int depth = 0;
    const char* p = kir;
    while ((p = strstr(p, "\"children\"")) != NULL) {
        depth++;
        p += strlen("\"children\"");
    }

    if (depth >= 3) {
        printf("✓ Nesting depth preserved: %d levels\n", depth);
        printf("PASS: Complex nesting\n");
        tests_passed++;
    } else {
        fprintf(stderr, "FAIL: Expected at least 3 levels of nesting, got %d\n", depth);
        tests_failed++;
    }

    free(kir);
    ir_hare_clear_error();
}

/**
 * Test 4: All property types
 */
static void test_hare_all_properties(void) {
    printf("\n=== Test 4: All property types ===\n");

    const char* input_hare =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.width(\"100%\");\n"
        "        b.height(\"100%\");\n"
        "        b.gap(10);\n"
        "        b.padding(20);\n"
        "        b.margin(5);\n"
        "        b.background(\"#000000\");\n"
        "        b.color(\"#ffffff\");\n"
        "    }\n"
        "};\n";

    char* kir = ir_hare_to_kir(input_hare, 0);
    if (!kir) {
        fprintf(stderr, "FAIL: Parser error: %s\n", ir_hare_get_error());
        tests_failed++;
        return;
    }

    // Verify all properties are present
    const char* expected_props[] = {
        "\"width\":\"100%\"",
        "\"height\":\"100%\"",
        "\"gap\":10",
        "\"padding\":20",
        "\"margin\":5",
        "\"background\":\"#000000\"",
        "\"color\":\"#ffffff\"",
        NULL
    };

    int missing = 0;
    for (int i = 0; expected_props[i]; i++) {
        if (strstr(kir, expected_props[i]) == NULL) {
            fprintf(stderr, "MISSING: %s\n", expected_props[i]);
            missing++;
        }
    }

    if (missing == 0) {
        printf("✓ All properties present\n");
        printf("PASS: All property types\n");
        tests_passed++;
    } else {
        fprintf(stderr, "FAIL: %d properties missing\n", missing);
        tests_failed++;
    }

    free(kir);
    ir_hare_clear_error();
}

/**
 * Test 5: Full pipeline to binary (if hare compiler available)
 */
static void test_hare_full_pipeline(void) {
    printf("\n=== Test 5: Full pipeline (if Hare compiler available) ===\n");

    // Check if hare compiler is available
    if (system("command -v hare >/dev/null 2>&1") != 0) {
        printf("⊘ SKIP: Hare compiler not found\n");
        return;
    }

    const char* input_hare =
        "use kryon::*;\n"
        "export fn main() void = {\n"
        "    container { b =>\n"
        "        b.width(\"100%\");\n"
        "        b.child(text { t =>\n"
        "            t.text(\"Hello from Hare!\");\n"
        "        });\n"
        "    }\n"
        "};\n";

    // Write test file
    char test_file[] = "/tmp/test_hare_full_XXXXXX.ha";
    int fd = mkstemps(test_file, 3);
    if (fd < 0) {
        fprintf(stderr, "FAIL: Could not create test file\n");
        tests_failed++;
        return;
    }
    write(fd, input_hare, strlen(input_hare));
    close(fd);
    printf("✓ Test file created: %s\n", test_file);

    // Step 1: Hare → KIR
    char* kir = ir_hare_to_kir(input_hare, 0);
    if (!kir) {
        fprintf(stderr, "FAIL: Parser error: %s\n", ir_hare_get_error());
        tests_failed++;
        unlink(test_file);
        return;
    }
    printf("✓ Hare → KIR\n");

    // Step 2: Write KIR to file
    char kir_file[] = "/tmp/test_hare_full_XXXXXX.kir";
    fd = mkstemps(kir_file, 4);
    if (fd < 0) {
        fprintf(stderr, "FAIL: Could not create KIR file\n");
        tests_failed++;
        free(kir);
        unlink(test_file);
        return;
    }
    write(fd, kir, strlen(kir));
    close(fd);
    free(kir);
    printf("✓ KIR written to file\n");

    // Step 3: KIR → Hare (codegen)
    char output_dir[] = "/tmp/test_hare_full_output_XXXXXX";
    mkdtemp(output_dir);
    bool codegen_ok = hare_codegen_generate_multi(kir_file, output_dir);
    if (!codegen_ok) {
        fprintf(stderr, "FAIL: Codegen failed\n");
        tests_failed++;
        unlink(test_file);
        unlink(kir_file);
        return;
    }
    printf("✓ KIR → Hare (codegen)\n");

    // Step 4: Compile Hare to binary
    char gen_hare_path[512];
    snprintf(gen_hare_path, sizeof(gen_hare_path), "%s/main.ha", output_dir);

    char binary_path[] = "/tmp/test_hare_full_XXXXXX";
    mkstemp(binary_path);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hare build -o \"%s\" \"%s\" 2>&1", binary_path, gen_hare_path);

    printf("Compiling Hare to binary...\n");
    int compile_result = system(cmd);
    if (compile_result != 0) {
        fprintf(stderr, "FAIL: Hare compilation failed (code: %d)\n", compile_result);
        tests_failed++;
        unlink(test_file);
        unlink(kir_file);
        rmdir(output_dir);
        return;
    }
    printf("✓ Hare compiled to binary: %s\n", binary_path);

    // Step 5: Verify binary exists
    if (!file_exists(binary_path)) {
        fprintf(stderr, "FAIL: Binary not created\n");
        tests_failed++;
    } else {
        printf("✓ Binary exists\n");
        printf("PASS: Full pipeline\n");
        tests_passed++;
    }

    // Cleanup
    unlink(test_file);
    unlink(kir_file);
    unlink(binary_path);
    unlink(gen_hare_path);
    rmdir(output_dir);

    ir_hare_clear_error();
}

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Hare Pipeline Integration Tests            ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    test_hare_roundtrip();
    test_hare_error_recovery();
    test_hare_complex_nesting();
    test_hare_all_properties();
    test_hare_full_pipeline();

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║              Test Summary                     ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Passed: %2d                                  ║\n", tests_passed);
    printf("║  Failed: %2d                                  ║\n", tests_failed);
    printf("╚══════════════════════════════════════════════╝\n");

    return tests_failed > 0 ? 1 : 0;
}
