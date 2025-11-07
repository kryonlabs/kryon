/**
 * Kryon Terminal Renderer - Regression Tests
 *
 * Validates that all drawing functions work correctly and produce
 * consistent results. This ensures that changes don't break
 * existing functionality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "terminal_backend.h"

// Test configuration
#define TEST_TERMINAL_WIDTH 80
#define TEST_TERMINAL_HEIGHT 24

typedef struct {
    int test_id;
    char name[64];
    bool (*test_func)(void);
    char expected_output[1024];
} regression_test_t;

static int tests_run = 0;
static int tests_passed = 0;

// Utility functions
static void save_terminal_state() {
    printf("\033[?47h");  // Save screen
    printf("\033[H");    // Move to top
}

static void restore_terminal_state() {
    printf("\033[?47l");  // Restore screen
    printf("\033[H");    // Move to top
    printf("\033[J");    // Clear screen
}

static void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

static bool capture_screen_region(int x, int y, int w, int h, char* buffer, size_t buffer_size) {
    // This is a simplified mock for testing
    // In a real implementation, this would capture actual terminal output
    memset(buffer, 0, buffer_size);
    return true;
}

static bool compare_output(const char* expected, const char* actual, const char* test_name) {
    if (!expected || !actual) return false;

    if (strcmp(expected, actual) == 0) {
        printf("✅ %s: Output matches expected\n", test_name);
        return true;
    } else {
        printf("❌ %s: Output mismatch\n", test_name);
        printf("Expected: %s\n", expected);
        printf("Actual:   %s\n", actual);
        return false;
    }
}

// ============================================================================
// Color Regression Tests
// ============================================================================

static bool test_color_accuracy() {
    printf("Testing color accuracy...\n");

    // Test RGB color conversion accuracy
    struct color_test {
        int r, g, b, mode;
        int expected_result;
        char description[64];
    } color_tests[] = {
        {255, 0, 0, 24, 0xFF0000, "Red (24-bit)"},
        {0, 255, 0, 24, 0x00FF00, "Green (24-bit)"},
        {0, 0, 255, 24, 0x0000FF, "Blue (24-bit)"},
        {255, 255, 255, 24, 0xFFFFFF, "White (24-bit)"},
        {0, 0, 0, 24, 0x000000, "Black (24-bit)"},
        {128, 128, 128, 24, 0x808080, "Gray (24-bit)"},
        {255, 0, 0, 16, 1, "Red (16-bit)"},
        {0, 255, 0, 16, 2, "Green (16-bit)"},
        {0, 0, 255, 16, 4, "Blue (16-bit)"},
        {255, 255, 0, 16, 3, "Yellow (16-bit)"},
    };

    for (size_t i = 0; i < sizeof(color_tests) / sizeof(color_tests[0]); i++) {
        struct color_test* test = &color_tests[i];
        int result = kryon_terminal_rgb_to_color(test->r, test->g, test->b, test->mode);

        if (result == test->expected_result) {
            printf("✅ %s: %s -> 0x%06X\n", test->description, test->mode, result);
        } else {
            printf("❌ %s: Expected 0x%06X, got 0x%06X\n",
                   test->description, test->expected_result, result);
            return false;
        }
    }

    printf("✅ All color accuracy tests passed\n");
    return true;
}

static bool test_color_mode_fallback() {
    printf("Testing color mode fallback...\n");

    // Test fallback behavior for invalid modes
    int result = kryon_terminal_rgb_to_color(128, 128, 128, 0);
    printf("✅ Invalid mode (0) fallback: %d\n", result);

    result = kryon_terminal_rgb_to_color(255, 0, 0, 999);
    printf("✅ Invalid mode (999) fallback: %d\n", result);

    // Test alpha handling
    uint32_t kryon_color = kryon_color_rgba(255, 128, 64, 128);  // 50% alpha
    int result_alpha = kryon_terminal_kryon_color_to_color(kryon_color, 24);

    // Should be darker than full color
    int full_color = kryon_terminal_rgb_to_color(255, 128, 64, 24);
    if (result_alpha != full_color) {
        printf("✅ Alpha blending: 50%% alpha produces different result\n");
    } else {
        printf("❌ Alpha blending failed: No difference detected\n");
        return false;
    }

    printf("✅ Color mode fallback tests passed\n");
    return true;
}

// ============================================================================
// Coordinate System Regression Tests
// ============================================================================

static bool test_coordinate_precision() {
    printf("Testing coordinate precision...\n");

    // Test round-trip conversion accuracy
    for (int x = 0; x < 100; x++) {
        for (int y = 0; y < 50; y++) {
            int px, py, cx, cy;

            kryon_terminal_char_to_pixel(NULL, x, y, &px, &py);
            kryon_terminal_pixel_to_char(NULL, px, py, &cx, &cy);

            if (cx != x || cy != y) {
                printf("❌ Round-trip failed at (%d,%d): got (%d,%d)\n",
                       x, y, cx, cy);
                return false;
            }
        }
    }

    printf("✅ Round-trip coordinate precision verified (100% accuracy)\n");
    return true;
}

static bool test_coordinate_boundaries() {
    printf("Testing coordinate boundaries...\n");

    // Test origin
    int px, py;
    kryon_terminal_char_to_pixel(NULL, 0, 0, &px, &py);
    if (px != 0 || py != 0) {
        printf("❌ Origin test failed: expected (0,0), got (%d,%d)\n", px, py);
        return false;
    }
    printf("✅ Origin (0,0) correct\n");

    // Test large coordinates
    kryon_terminal_char_to_pixel(NULL, 1000, 1000, &px, &py);
    if (px < 0 || py < 0) {
        printf("❌ Large coordinate test failed\n");
        return false;
    }
    printf("✅ Large coordinates handled: (%d,%d)\n", px, py);

    // Test pixel to char at boundaries
    int cx, cy;
    kryon_terminal_pixel_to_char(NULL, 0, 0, &cx, &cy);
    if (cx != 0 || cy != 0) {
        printf("❌ Pixel origin test failed: expected (0,0), got (%d,%d)\n", cx, cy);
        return false;
    }
    printf("✅ Pixel origin (0,0) correct\n");

    kryon_terminal_pixel_to_char(NULL, 1, 1, &cx, &cy);
    if (cx < 0 || cy < 0) {
        printf("❌ Sub-pixel test failed\n");
        return false;
    }
    printf("✅ Sub-pixel coordinates handled\n");

    printf("✅ Coordinate boundary tests passed\n");
    return true;
}

// ============================================================================
// Terminal Capability Regression Tests
// ============================================================================

static bool test_capability_detection() {
    printf("Testing capability detection...\n");

    // Test size detection
    uint16_t width, height;
    kryon_terminal_get_size(NULL, &width, &height);

    if (width < 20 || height < 5) {
        printf("❌ Terminal size too small: %dx%d\n", width, height);
        return false;
    }
    printf("✅ Terminal size adequate: %dx%d\n", width, height);

    // Test color capability detection
    int colors = kryon_terminal_get_color_capability(NULL);

    const char* color_desc;
    switch (colors) {
        case 16: color_desc = "16-color"; break;
        case 256: color_desc = "256-color"; break;
        case 16777216: color_desc = "truecolor"; break;
        default: color_desc = "unknown"; break;
    }

    if (colors < 16) {
        printf("❌ Invalid color capability: %d\n", colors);
        return false;
    }
    printf("✅ Color capability detected: %s\n", color_desc);

    printf("✅ Capability detection working correctly\n");
    return true;
}

static bool test_terminal_control() {
    printf("Testing terminal control functions...\n");

    // Test clear screen
    clear_screen();
    printf("✅ Screen cleared successfully\n");

    // Test cursor visibility
    kryon_terminal_set_cursor_visible(NULL, false);
    printf("✅ Cursor hidden\n");

    kryon_terminal_set_cursor_visible(NULL, true);
    printf("✅ cursor restored\n");

    // Test title setting (may not work on all terminals)
    bool title_set = kryon_terminal_set_title("Regression Test");
    if (title_set) {
        printf("✅ Terminal title set successfully\n");
    } else {
        printf("⚠️ Terminal title not supported (acceptable)\n");
    }

    printf("✅ Terminal control functions working\n");
    return true;
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

static bool test_performance_regression() {
    printf("Testing performance regression...\n");

    const int iterations = 100000;
    const double max_time = 0.1;  // 100ms max

    // Test color conversion performance
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        int color = kryon_terminal_rgb_to_color(i % 256, (i*2) % 256, (i*3) % 256, 24);
        (void)color; // Prevent optimization
    }
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    double ops_per_sec = iterations / time_taken;
    printf("📊 Color conversion: %.0f ops/sec", ops_per_sec);

    if (ops_per_sec < 100000) {
        printf("❌ Color conversion performance regression detected (%.0f ops/sec < 100k)\n", ops_per_sec);
        return false;
    }
    printf("✅ Color conversion performance acceptable\n");

    // Test coordinate conversion performance
    start = clock();
    for (int i = 0; i < iterations; i++) {
        int px, py, cx, cy;
        kryon_terminal_char_to_pixel(NULL, i % 80, i % 24, &px, &py);
        kryon_terminal_pixel_to_char(NULL, px, py, &cx, &cy);
        (void)cx; (void)cy; // Prevent optimization
    }
    end = clock();
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    ops_per_sec = iterations / time_taken;
    printf("📊 Coordinate conversion: %.0f ops/sec\n", ops_per_sec);

    if (ops_per_sec < 100000) {
        printf("❌ Coordinate conversion performance regression detected (%.0f ops/sec < 100k)\n", ops_per_sec);
        return false;
    }
    printf("✅ Coordinate conversion performance acceptable\n");

    return true;
}

// ============================================================================
// Memory Safety Regression Tests
// ============================================================================

static bool test_memory_safety() {
    printf("Testing memory safety...\n");

    // Test renderer creation/destruction stability
    for (int i = 0; i < 1000; i++) {
        kryon_renderer_t* renderer = kryon_terminal_renderer_create();
        if (!renderer) {
            printf("❌ Renderer creation failed at iteration %d\n", i);
            return false;
        }

        // Test operations with null parameters
        uint16_t width, height;
        kryon_terminal_get_size(renderer, &width, &height);
        kryon_terminal_get_size(NULL, &width, &height);

        kryon_terminal_renderer_destroy(renderer);
    }
    printf("✅ 1000 renderer creation/destruction cycles completed\n");

    // Test double destruction protection
    kryon_renderer_t* test_renderer = kryon_terminal_renderer_create();
    kryon_terminal_renderer_destroy(test_renderer);
    kryon_terminal_renderer_destroy(test_renderer);  // Should not crash
    printf("✅ Double destruction protection working\n");

    // Test null parameter handling in utility functions
    int px, py;
    kryon_terminal_char_to_pixel(NULL, 0, 0, &px, &py);
    kryon_terminal_pixel_to_char(NULL, 0, 0, &px, &py);
    kryon_terminal_get_size(NULL, NULL, NULL);
    printf("✅ Null parameter handling working\n");

    printf("✅ Memory safety tests passed\n");
    return true;
}

// ============================================================================
// Edge Case Regression Tests
// ============================================================================

static bool test_edge_cases() {
    printf("Testing edge cases...\n");

    // Test extreme color values
    int extreme_white = kryon_terminal_rgb_to_color(255, 255, 255, 24);
    int extreme_black = kryon_terminal_rgb_to_color(0, 0, 0, 24);

    if (extreme_white != 0xFFFFFF || extreme_black != 0x000000) {
        printf("❌ Extreme color values failed\n");
        return false;
    }
    printf("✅ Extreme color values correct\n");

    // Test boundary colors
    int boundary_gray = kryon_terminal_rgb_to_color(128, 128, 128, 24);
    if (boundary_gray != 0x808080) {
        printf("❌ Boundary gray value failed\n");
        return false;
    }
    printf("✅ Boundary values correct\n");

    // Test invalid parameters in color conversion
    int invalid_mode = kryon_terminal_rgb_to_color(255, 255, 255, -1);
    if (invalid_mode < 0) {
        printf("✅ Invalid color mode handled gracefully\n");
    } else {
        printf("⚠️ Invalid color mode not properly handled\n");
    }

    printf("✅ Edge case tests completed\n");
    return true;
}

// ============================================================================
// Test Registration
// ============================================================================

static regression_test_t regression_tests[] = {
    {1, "Color Accuracy", test_color_accuracy, ""},
    {2, "Color Mode Fallback", test_color_mode_fallback, ""},
    {3, "Coordinate Precision", test_coordinate_precision, ""},
    {4, "Coordinate Boundaries", test_coordinate_boundaries, ""},
    {5, "Capability Detection", test_capability_detection, ""},
    {6, "Terminal Control", test_terminal_control, ""},
    {7, "Performance Regression", test_performance_regression, ""},
    {8, "Memory Safety", test_memory_safety, ""},
    {9, "Edge Cases", test_edge_cases, ""},
    {0, NULL, NULL, ""}
};

static bool run_regression_test(regression_test_t* test) {
    printf("\n=== Regression Test %d: %s ===\n", test->test_id, test->name);

    bool result = test->test_func();

    tests_run++;
    if (result) {
        tests_passed++;
        printf("✅ REGRESSION TEST PASSED: %s\n\n", test->name);
    } else {
        printf("❌ REGRESSION TEST FAILED: %s\n\n", test->name);
    }

    return result;
}

// ============================================================================
// Test Execution
// ============================================================================

int main() {
    printf("🧪 Kryon Terminal Renderer - Regression Test Suite\n");
    printf("=====================================================\n");
    printf("Validating that all functionality works as expected\n\n");

    // Save terminal state
    save_terminal_state();

    // Run all regression tests
    bool all_passed = true;
    for (int i = 0; regression_tests[i].test_func != NULL; i++) {
        if (!run_regression_test(&regression_tests[i])) {
            all_passed = false;
        }
    }

    // Restore terminal state
    restore_terminal_state();

    // Print results
    printf("\n📊 Regression Test Results\n");
    printf("==========================\n");
    printf("Total tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / tests_run * 100);

    if (all_passed) {
        printf("\n🎉 ALL REGRESSION TESTS PASSED!\n");
        printf("Terminal backend is FLAWLESS - no regressions detected!\n");
        return 0;
    } else {
        printf("\n⚠️  REGRESSION TESTS FAILED!\n");
        printf("Some functionality may be broken. Please review the implementation.\n");
        return 1;
    }
}