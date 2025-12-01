/*
 * Asset Management System Test
 *
 * Tests asset loading, virtual paths, and hot reload
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// SDL3 headers
#if defined(__has_include)
  #if __has_include(<SDL3/SDL.h>)
    #include <SDL3/SDL.h>
  #elif __has_include(<SDL.h>)
    #include <SDL.h>
  #else
    #error "SDL3 headers not found"
  #endif
#else
  #include <SDL3/SDL.h>
#endif

// Kryon asset system
#include "../../ir/ir_asset.h"
#include "../../ir/ir_sprite.h"
#include "../../renderers/sdl3/sdl3_sprite_backend.h"

// Window dimensions
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// ============================================================================
// Helper: Create colored square sprite data (RGBA)
// ============================================================================

static uint8_t* create_colored_square(uint32_t size, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t* pixels = (uint8_t*)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t i = (y * size + x) * 4;
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = 255;  // Opaque
        }
    }

    return pixels;
}

// ============================================================================
// Main Program
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Kryon Asset Management System Test ===\n\n");

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Asset Test",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );

    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize sprite backend
    printf("Initializing sprite backend...\n");
    sdl3_sprite_backend_init(renderer);

    // Initialize asset system
    printf("Initializing asset system...\n");
    ir_asset_init();

    // Add search path with alias
    printf("Adding search path: assets/sprites -> @sprites\n");
    ir_asset_add_search_path("assets/sprites", "sprites");

    // Test virtual path resolution
    char resolved_path[512];
    if (ir_asset_resolve_path("@sprites/player.png", resolved_path, sizeof(resolved_path))) {
        printf("✓ Virtual path resolved: @sprites/player.png -> %s\n", resolved_path);
    } else {
        printf("✗ Failed to resolve virtual path\n");
    }

    // Load sprites from memory (simulating file loading)
    printf("\nLoading test sprites...\n");

    IRSpriteAtlasID atlas = ir_sprite_atlas_create(1024, 1024);
    if (atlas == IR_INVALID_SPRITE_ATLAS) {
        fprintf(stderr, "Failed to create sprite atlas\n");
        return 1;
    }

    // Create and load test sprites with different colors
    IRSpriteID sprites[4];
    uint8_t colors[4][3] = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {255, 255, 0}   // Yellow
    };

    for (int i = 0; i < 4; i++) {
        uint8_t* pixels = create_colored_square(128, colors[i][0], colors[i][1], colors[i][2]);
        char name[32];
        snprintf(name, sizeof(name), "sprite_%d", i);
        sprites[i] = ir_sprite_atlas_add_image_from_memory(atlas, name, pixels, 128, 128);
        free(pixels);

        if (sprites[i] == IR_INVALID_SPRITE) {
            fprintf(stderr, "Failed to add sprite %d\n", i);
            return 1;
        }
        printf("✓ Loaded sprite_%d\n", i);
    }

    // Pack and upload atlas
    printf("\nPacking atlas and uploading to GPU...\n");
    if (!ir_sprite_atlas_pack(atlas)) {
        fprintf(stderr, "Failed to pack sprite atlas\n");
        return 1;
    }
    printf("✓ Atlas packed and uploaded\n");

    // Print asset statistics
    IRAssetStats stats;
    ir_asset_get_stats(&stats);
    printf("\nAsset Statistics:\n");
    printf("  Total assets: %u\n", stats.total_assets);
    printf("  Loaded: %u\n", stats.loaded_assets);
    printf("  Memory usage: %zu bytes\n", stats.total_memory);

    printf("\nControls:\n");
    printf("  H - Enable hot reload\n");
    printf("  R - Manually reload all assets\n");
    printf("  S - Print asset statistics\n");
    printf("  ESC - Quit\n\n");

    // Main loop
    bool running = true;
    bool hot_reload_enabled = false;
    float angle = 0.0f;

    uint64_t last_frame_time = SDL_GetTicks();

    while (running) {
        uint64_t current_time = SDL_GetTicks();
        float dt = (current_time - last_frame_time) / 1000.0f;
        last_frame_time = current_time;

        // Cap dt
        if (dt > 0.1f) dt = 0.1f;

        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_H:
                        hot_reload_enabled = !hot_reload_enabled;
                        printf("Hot reload %s\n", hot_reload_enabled ? "enabled" : "disabled");
                        break;
                    case SDLK_R:
                        printf("Reloading all assets...\n");
                        ir_asset_reload_all();
                        printf("✓ Assets reloaded\n");
                        break;
                    case SDLK_S:
                        ir_asset_get_stats(&stats);
                        printf("\nAsset Statistics:\n");
                        printf("  Total assets: %u\n", stats.total_assets);
                        printf("  Loaded: %u\n", stats.loaded_assets);
                        printf("  Unloaded: %u\n", stats.unloaded_assets);
                        printf("  Errors: %u\n", stats.error_assets);
                        printf("  Memory usage: %zu bytes\n", stats.total_memory);
                        printf("  Hot reloads: %u\n", stats.hot_reload_count);
                        break;
                }
            }
        }

        // Update asset system (checks for hot reload)
        if (hot_reload_enabled) {
            ir_asset_update();
        }

        // Update rotation
        angle += 30.0f * dt;  // 30 degrees per second
        if (angle > 360.0f) angle -= 360.0f;

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Begin sprite frame
        sdl3_sprite_backend_begin_frame();

        // Draw sprites in a grid pattern
        float center_x = WINDOW_WIDTH / 2.0f;
        float center_y = WINDOW_HEIGHT / 2.0f;
        float radius = 200.0f;

        for (int i = 0; i < 4; i++) {
            float sprite_angle = angle + (i * 90.0f);
            float rad = sprite_angle * (3.14159f / 180.0f);
            float x = center_x + cos(rad) * radius - 64;
            float y = center_y + sin(rad) * radius - 64;

            ir_sprite_draw_ex(
                sprites[i],
                x, y,
                sprite_angle,
                1.0f, 1.0f,
                0xFFFFFFFF  // White (no tint)
            );
        }

        // End sprite frame
        sdl3_sprite_backend_end_frame();

        // Present
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    printf("\nShutting down...\n");
    ir_sprite_atlas_destroy(atlas);
    ir_asset_shutdown();
    sdl3_sprite_backend_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Test completed successfully\n");
    return 0;
}
