/**
 * @file test_renderers.c
 * @brief Comprehensive test for all renderer implementations
 */

#include "renderer_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test configuration
typedef struct {
    const char* name;
    KryonRenderer* (*create_func)(void*);
    void* surface_config;
    bool should_run;
} RendererTestConfig;

// Surface configurations for each renderer
typedef struct {
    int width;
    int height;
    const char* title;
    bool fullscreen;
} SDL2Surface;

typedef struct {
    int width;
    int height;
    const char* title;
    bool fullscreen;
    int target_fps;
} RaylibSurface;

typedef struct {
    char* output_path;
    char* title;
    int width;
    int height;
    bool include_viewport_meta;
    bool responsive;
} HtmlSurface;

// Test commands generator
static void generate_test_commands(KryonRenderCommand* commands, size_t* count) {
    *count = 5;
    
    // Draw background rectangle
    commands[0] = kryon_cmd_draw_rect(
        (KryonVec2){0, 0},
        (KryonVec2){800, 600},
        (KryonColor){0.1f, 0.1f, 0.2f, 1.0f},
        0.0f
    );
    
    // Draw red rectangle
    commands[1] = kryon_cmd_draw_rect(
        (KryonVec2){100, 100},
        (KryonVec2){200, 150},
        (KryonColor){0.8f, 0.2f, 0.2f, 1.0f},
        10.0f
    );
    
    // Draw green rectangle
    commands[2] = kryon_cmd_draw_rect(
        (KryonVec2){400, 200},
        (KryonVec2){150, 100},
        (KryonColor){0.2f, 0.8f, 0.2f, 1.0f},
        5.0f
    );
    
    // Draw text
    commands[3] = kryon_cmd_draw_text(
        (KryonVec2){150, 300},
        "Hello Kryon Renderers!",
        24.0f,
        (KryonColor){1.0f, 1.0f, 1.0f, 1.0f}
    );
    
    // Draw blue rectangle with border
    commands[4] = kryon_cmd_draw_rect(
        (KryonVec2){300, 400},
        (KryonVec2){200, 80},
        (KryonColor){0.2f, 0.2f, 0.8f, 1.0f},
        15.0f
    );
    commands[4].data.draw_rect.border_width = 3.0f;
    commands[4].data.draw_rect.border_color = (KryonColor){1.0f, 1.0f, 0.0f, 1.0f};
}

static bool test_renderer_basic_functionality(KryonRenderer* renderer) {
    printf("  Testing basic functionality...\n");
    
    if (!renderer) {
        printf("    ✗ Renderer is NULL\n");
        return false;
    }
    
    if (!renderer->name || !renderer->backend) {
        printf("    ✗ Renderer missing name or backend\n");
        return false;
    }
    
    printf("    ✓ Renderer: %s (backend: %s)\n", renderer->name, renderer->backend);
    
    // Test viewport size
    KryonVec2 viewport = renderer->vtable->viewport_size();
    printf("    ✓ Viewport size: %.0fx%.0f\n", viewport.x, viewport.y);
    
    return true;
}

static bool test_renderer_frame_cycle(KryonRenderer* renderer) {
    printf("  Testing frame cycle...\n");
    
    KryonRenderContext* context;
    KryonColor clear_color = {0.2f, 0.3f, 0.4f, 1.0f};
    
    // Begin frame
    KryonRenderResult result = kryon_renderer_begin_frame(renderer, &context, clear_color);
    if (result != KRYON_RENDER_SUCCESS) {
        printf("    ✗ Failed to begin frame: %d\n", result);
        return false;
    }
    printf("    ✓ Frame started\n");
    
    // Generate and execute test commands
    KryonRenderCommand commands[10];
    size_t command_count;
    generate_test_commands(commands, &command_count);
    
    result = kryon_renderer_execute_commands(renderer, context, commands, command_count);
    if (result != KRYON_RENDER_SUCCESS) {
        printf("    ✗ Failed to execute commands: %d\n", result);
        return false;
    }
    printf("    ✓ Commands executed (%zu commands)\n", command_count);
    
    // End frame
    result = kryon_renderer_end_frame(renderer, context);
    if (result != KRYON_RENDER_SUCCESS) {
        printf("    ✗ Failed to end frame: %d\n", result);
        return false;
    }
    printf("    ✓ Frame ended\n");
    
    return true;
}

static bool test_renderer_resize(KryonRenderer* renderer) {
    printf("  Testing resize functionality...\n");
    
    KryonVec2 original_size = renderer->vtable->viewport_size();
    KryonVec2 new_size = {1024, 768};
    
    KryonRenderResult result = renderer->vtable->resize(new_size);
    if (result != KRYON_RENDER_SUCCESS) {
        printf("    ✗ Failed to resize: %d\n", result);
        return false;
    }
    
    KryonVec2 current_size = renderer->vtable->viewport_size();
    if (current_size.x != new_size.x || current_size.y != new_size.y) {
        printf("    ✗ Resize failed: expected %.0fx%.0f, got %.0fx%.0f\n",
               new_size.x, new_size.y, current_size.x, current_size.y);
        return false;
    }
    
    printf("    ✓ Resize successful: %.0fx%.0f -> %.0fx%.0f\n",
           original_size.x, original_size.y, current_size.x, current_size.y);
    
    return true;
}

static bool run_renderer_test(RendererTestConfig* config) {
    printf("\n=== Testing %s Renderer ===\n", config->name);
    
    if (!config->should_run) {
        printf("  Skipped (not configured to run)\n");
        return true;
    }
    
    // Create renderer
    KryonRenderer* renderer = config->create_func(config->surface_config);
    if (!renderer) {
        printf("  ✗ Failed to create renderer\n");
        return false;
    }
    
    bool success = true;
    
    // Run basic functionality test
    if (!test_renderer_basic_functionality(renderer)) {
        success = false;
    }
    
    // Run frame cycle test
    if (success && !test_renderer_frame_cycle(renderer)) {
        success = false;
    }
    
    // Run resize test
    if (success && !test_renderer_resize(renderer)) {
        success = false;
    }
    
    // Cleanup
    kryon_renderer_destroy(renderer);
    printf("  ✓ Renderer destroyed\n");
    
    if (success) {
        printf("  ✓ All tests passed for %s renderer\n", config->name);
    } else {
        printf("  ✗ Some tests failed for %s renderer\n", config->name);
    }
    
    return success;
}

int main(int argc, char* argv[]) {
    printf("=== Kryon Renderer Test Suite ===\n");
    printf("Testing all available renderer backends...\n");
    
    // Configure surface parameters for each renderer
    SDL2Surface sdl2_surface = {
        .width = 800,
        .height = 600,
        .title = "Kryon SDL2 Test",
        .fullscreen = false
    };
    
    RaylibSurface raylib_surface = {
        .width = 800,
        .height = 600,
        .title = "Kryon Raylib Test",
        .fullscreen = false,
        .target_fps = 60
    };
    
    HtmlSurface html_surface = {
        .output_path = strdup("test_output"),
        .title = strdup("Kryon HTML Test"),
        .width = 800,
        .height = 600,
        .include_viewport_meta = true,
        .responsive = false
    };
    
    // Configure test suite
    RendererTestConfig tests[] = {
        {
            .name = "SDL2",
            .create_func = kryon_sdl2_renderer_create,
            .surface_config = &sdl2_surface,
            .should_run = false // Disable SDL2 for headless testing
        },
        {
            .name = "Raylib", 
            .create_func = kryon_raylib_renderer_create,
            .surface_config = &raylib_surface,
            .should_run = false // Disable Raylib for headless testing
        },
        {
            .name = "HTML",
            .create_func = kryon_html_renderer_create,
            .surface_config = &html_surface,
            .should_run = true // HTML renderer works in headless mode
        }
    };
    
    // Check command line arguments for specific renderer testing
    bool run_all = true;
    bool run_sdl2 = false, run_raylib = false, run_html = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sdl2") == 0) {
            run_sdl2 = true;
            run_all = false;
        } else if (strcmp(argv[i], "--raylib") == 0) {
            run_raylib = true;
            run_all = false;
        } else if (strcmp(argv[i], "--html") == 0) {
            run_html = true;
            run_all = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--sdl2] [--raylib] [--html] [--help]\n", argv[0]);
            printf("  --sdl2    Test SDL2 renderer only\n");
            printf("  --raylib  Test Raylib renderer only\n");
            printf("  --html    Test HTML renderer only\n");
            printf("  --help    Show this help message\n");
            return 0;
        }
    }
    
    // Update test configuration based on arguments
    if (!run_all) {
        tests[0].should_run = run_sdl2;
        tests[1].should_run = run_raylib;
        tests[2].should_run = run_html;
    }
    
    // Run tests
    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < total; i++) {
        if (run_renderer_test(&tests[i])) {
            passed++;
        }
    }
    
    // Cleanup
    free(html_surface.output_path);
    free(html_surface.title);
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d/%d tests\n", passed, total);
    
    if (passed == total) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}