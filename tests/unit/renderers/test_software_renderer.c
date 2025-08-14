/**
 * @file test_software_renderer.c
 * @brief Unit tests for Kryon software renderer
 */

#include "software_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: " #name "... "); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAILED\n  Assertion failed: " #condition "\n"); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Test renderer creation and destruction
TEST(renderer_lifecycle) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(800, 600);
    ASSERT(renderer != NULL);
    ASSERT(renderer->backbuffer != NULL);
    ASSERT(renderer->backbuffer->width == 800);
    ASSERT(renderer->backbuffer->height == 600);
    ASSERT(renderer->target == renderer->backbuffer);
    
    kryon_software_renderer_destroy(renderer);
    
    // Test invalid dimensions
    renderer = kryon_software_renderer_create(0, 100);
    ASSERT(renderer == NULL);
    
    renderer = kryon_software_renderer_create(100, -1);
    ASSERT(renderer == NULL);
}

// Test pixel buffer management
TEST(pixel_buffer_management) {
    // Create buffer
    KryonPixelBuffer *buffer = kryon_pixel_buffer_create(100, 100);
    ASSERT(buffer != NULL);
    ASSERT(buffer->width == 100);
    ASSERT(buffer->height == 100);
    ASSERT(buffer->pixels != NULL);
    ASSERT(buffer->owns_memory == true);
    
    kryon_pixel_buffer_destroy(buffer);
    
    // Create from existing memory
    uint32_t pixels[10 * 10];
    buffer = kryon_pixel_buffer_create_from_memory(pixels, 10, 10, 10 * sizeof(uint32_t));
    ASSERT(buffer != NULL);
    ASSERT(buffer->pixels == pixels);
    ASSERT(buffer->owns_memory == false);
    
    kryon_pixel_buffer_destroy(buffer);
}

// Test color utilities
TEST(color_utilities) {
    // Test color creation
    KryonColor red = kryon_color_rgba(255, 0, 0, 255);
    ASSERT(red.r == 255);
    ASSERT(red.g == 0);
    ASSERT(red.b == 0);
    ASSERT(red.a == 255);
    
    // Test hex color
    KryonColor blue = kryon_color_from_hex(0x0000FFFF);
    ASSERT(blue.r == 0);
    ASSERT(blue.g == 0);
    ASSERT(blue.b == 255);
    ASSERT(blue.a == 255);
    
    // Test pixel conversion
    uint32_t pixel = kryon_color_to_pixel(red);
    ASSERT(pixel == 0xFF0000FF);
    
    // Test color blending
    KryonColor semi_red = kryon_color_rgba(255, 0, 0, 128);
    KryonColor white = KRYON_COLOR_WHITE;
    KryonColor blended = kryon_color_blend(semi_red, white);
    ASSERT(blended.r > 127); // Should be pinkish
    ASSERT(blended.g > 127);
    ASSERT(blended.b > 127);
}

// Test basic drawing operations
TEST(drawing_primitives) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Clear buffer
    kryon_software_renderer_clear(renderer);
    
    // Test pixel drawing
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_RED);
    kryon_software_renderer_draw_pixel(renderer, 50, 50);
    
    // Check if pixel was drawn (simplified check)
    uint32_t pixel = renderer->target->pixels[50 * 100 + 50];
    ASSERT(pixel != 0); // Should not be black
    
    // Test line drawing
    kryon_software_renderer_draw_line(renderer, 10, 10, 20, 20);
    ASSERT(renderer->stats.primitives_drawn > 0);
    
    // Test rectangle drawing
    KryonRect rect = {25, 25, 50, 30};
    kryon_software_renderer_draw_rect(renderer, rect);
    kryon_software_renderer_fill_rect(renderer, rect);
    
    ASSERT(renderer->stats.pixels_drawn > 0);
    ASSERT(renderer->stats.primitives_drawn > 0);
    
    kryon_software_renderer_destroy(renderer);
}

// Test text rendering
TEST(text_rendering) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(200, 100);
    ASSERT(renderer != NULL);
    
    // Test text measurement
    int width, height;
    kryon_software_renderer_measure_text(renderer, "Hello", &width, &height);
    ASSERT(width > 0);
    ASSERT(height > 0);
    
    // Test text drawing
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_WHITE);
    kryon_software_renderer_draw_text(renderer, "Test", 10, 10);
    ASSERT(renderer->stats.primitives_drawn > 0);
    
    // Test multiline text measurement
    kryon_software_renderer_measure_text(renderer, "Line 1\nLine 2", &width, &height);
    ASSERT(height > 8); // Should be at least 2 lines
    
    kryon_software_renderer_destroy(renderer);
}

// Test rendering target switching
TEST(render_target_switching) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Create custom buffer
    KryonPixelBuffer *custom_buffer = kryon_pixel_buffer_create(50, 50);
    ASSERT(custom_buffer != NULL);
    
    // Switch to custom buffer
    kryon_software_renderer_set_target(renderer, custom_buffer);
    ASSERT(renderer->target == custom_buffer);
    
    // Draw something
    kryon_software_renderer_clear(renderer);
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_GREEN);
    kryon_software_renderer_draw_pixel(renderer, 25, 25);
    
    // Switch back to backbuffer
    kryon_software_renderer_set_target(renderer, NULL);
    ASSERT(renderer->target == renderer->backbuffer);
    
    kryon_pixel_buffer_destroy(custom_buffer);
    kryon_software_renderer_destroy(renderer);
}

// Test clipping
TEST(clipping_operations) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Test initial clip rect
    ASSERT(renderer->clip_rect.x == 0);
    ASSERT(renderer->clip_rect.y == 0);
    ASSERT(renderer->clip_rect.width == 100);
    ASSERT(renderer->clip_rect.height == 100);
    ASSERT(renderer->clipping_enabled == true);
    
    // Draw outside clip area (should be clipped)
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_RED);
    kryon_software_renderer_draw_pixel(renderer, -1, -1);
    kryon_software_renderer_draw_pixel(renderer, 101, 101);
    
    // These should not crash and should not affect stats significantly
    
    kryon_software_renderer_destroy(renderer);
}

// Test performance stats
TEST(performance_stats) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Initial stats should be zero
    ASSERT(renderer->stats.pixels_drawn == 0);
    ASSERT(renderer->stats.primitives_drawn == 0);
    
    // Draw some primitives
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_BLUE);
    kryon_software_renderer_draw_pixel(renderer, 10, 10);
    ASSERT(renderer->stats.pixels_drawn == 1);
    
    KryonRect rect = {20, 20, 10, 10};
    kryon_software_renderer_fill_rect(renderer, rect);
    ASSERT(renderer->stats.pixels_drawn > 1);
    ASSERT(renderer->stats.primitives_drawn > 0);
    
    kryon_software_renderer_destroy(renderer);
}

// Test transformation
TEST(transformation_operations) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Test initial transformation
    ASSERT(renderer->scale_x == 1.0f);
    ASSERT(renderer->scale_y == 1.0f);
    ASSERT(renderer->offset_x == 0);
    ASSERT(renderer->offset_y == 0);
    
    // Modify transformation
    renderer->scale_x = 2.0f;
    renderer->scale_y = 2.0f;
    renderer->offset_x = 10;
    renderer->offset_y = 10;
    
    // Draw pixel with transformation applied
    kryon_software_renderer_set_color(renderer, KRYON_COLOR_WHITE);
    kryon_software_renderer_draw_pixel(renderer, 5, 5);
    
    // Pixel should be drawn at (5*2+10, 5*2+10) = (20, 20)
    uint32_t pixel = renderer->target->pixels[20 * 100 + 20];
    ASSERT(pixel != 0);
    
    kryon_software_renderer_destroy(renderer);
}

// Test buffer presentation
TEST(buffer_presentation) {
    KryonSoftwareRenderer *renderer = kryon_software_renderer_create(100, 100);
    ASSERT(renderer != NULL);
    
    // Present should return backbuffer
    KryonPixelBuffer *presented = kryon_software_renderer_present(renderer);
    ASSERT(presented == renderer->backbuffer);
    
    kryon_software_renderer_destroy(renderer);
}

// Main test runner
int main(void) {
    printf("=== Kryon Software Renderer Unit Tests ===\n\n");
    
    run_test_renderer_lifecycle();
    run_test_pixel_buffer_management();
    run_test_color_utilities();
    run_test_drawing_primitives();
    run_test_text_rendering();
    run_test_render_target_switching();
    run_test_clipping_operations();
    run_test_performance_stats();
    run_test_transformation_operations();
    run_test_buffer_presentation();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All tests PASSED! ✅\n");
        return 0;
    } else {
        printf("Some tests FAILED! ❌\n");
        return 1;
    }
}