/*
 * Sprite System Test - SDL3 Backend
 *
 * Demonstrates:
 * - Creating sprite atlases
 * - Adding sprites from memory
 * - GPU sprite batching
 * - Performance with many sprites
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

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

// Kryon sprite system
#include "../../ir/ir_sprite.h"
#include "../../renderers/sdl3/sdl3_sprite_backend.h"

// Window dimensions
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Number of sprites to test
#define SPRITE_COUNT 100

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
    (void)argc; (void)argv;

    printf("=== Kryon Sprite System Test ===\n\n");

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Sprite Test",
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

    // Create sprite atlas (1024x1024)
    printf("Creating sprite atlas...\n");
    IRSpriteAtlasID atlas = ir_sprite_atlas_create(1024, 1024);
    if (atlas == IR_INVALID_SPRITE_ATLAS) {
        fprintf(stderr, "Failed to create sprite atlas\n");
        return 1;
    }

    // Create test sprites with different colors
    printf("Adding sprites to atlas...\n");
    IRSpriteID sprites[4];

    // Red square
    uint8_t* red_pixels = create_colored_square(64, 255, 0, 0);
    sprites[0] = ir_sprite_atlas_add_image_from_memory(atlas, "red_square", red_pixels, 64, 64);
    free(red_pixels);

    // Green square
    uint8_t* green_pixels = create_colored_square(64, 0, 255, 0);
    sprites[1] = ir_sprite_atlas_add_image_from_memory(atlas, "green_square", green_pixels, 64, 64);
    free(green_pixels);

    // Blue square
    uint8_t* blue_pixels = create_colored_square(64, 0, 0, 255);
    sprites[2] = ir_sprite_atlas_add_image_from_memory(atlas, "blue_square", blue_pixels, 64, 64);
    free(blue_pixels);

    // Yellow square
    uint8_t* yellow_pixels = create_colored_square(64, 255, 255, 0);
    sprites[3] = ir_sprite_atlas_add_image_from_memory(atlas, "yellow_square", yellow_pixels, 64, 64);
    free(yellow_pixels);

    // Pack and upload atlas
    printf("Packing atlas and uploading to GPU...\n");
    if (!ir_sprite_atlas_pack(atlas)) {
        fprintf(stderr, "Failed to pack sprite atlas\n");
        return 1;
    }

    printf("\n");
    printf("✓ Sprite system initialized\n");
    printf("✓ Created 4 sprites (red, green, blue, yellow)\n");
    printf("✓ Atlas packed and uploaded to GPU\n");
    printf("\n");
    printf("Rendering %d sprites...\n", SPRITE_COUNT);
    printf("Press ESC to quit\n\n");

    // Sprite positions and velocities (for animation)
    typedef struct {
        float x, y;
        float vx, vy;
        int sprite_idx;
        float rotation;
        float rotation_speed;
    } SpriteInstance;

    SpriteInstance instances[SPRITE_COUNT];

    // Initialize sprite instances with random positions and velocities
    for (int i = 0; i < SPRITE_COUNT; i++) {
        instances[i].x = (float)(rand() % WINDOW_WIDTH);
        instances[i].y = (float)(rand() % WINDOW_HEIGHT);
        instances[i].vx = ((rand() % 200) - 100) / 10.0f;
        instances[i].vy = ((rand() % 200) - 100) / 10.0f;
        instances[i].sprite_idx = rand() % 4;
        instances[i].rotation = (float)(rand() % 360);
        instances[i].rotation_speed = ((rand() % 100) - 50) / 10.0f;
    }

    // Main loop
    bool running = true;
    uint32_t frame_count = 0;
    uint64_t start_time = SDL_GetTicks();

    while (running) {
        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        // Update sprite positions (bouncing)
        for (int i = 0; i < SPRITE_COUNT; i++) {
            instances[i].x += instances[i].vx;
            instances[i].y += instances[i].vy;
            instances[i].rotation += instances[i].rotation_speed;

            // Bounce off edges
            if (instances[i].x < 0 || instances[i].x > WINDOW_WIDTH - 64) {
                instances[i].vx = -instances[i].vx;
            }
            if (instances[i].y < 0 || instances[i].y > WINDOW_HEIGHT - 64) {
                instances[i].vy = -instances[i].vy;
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Begin sprite frame
        sdl3_sprite_backend_begin_frame();

        // Draw all sprites
        for (int i = 0; i < SPRITE_COUNT; i++) {
            ir_sprite_draw_ex(
                sprites[instances[i].sprite_idx],
                instances[i].x,
                instances[i].y,
                instances[i].rotation,
                1.0f,  // scale_x
                1.0f,  // scale_y
                0xFFFFFFFF  // white tint (no tint)
            );
        }

        // End sprite frame (flushes batches)
        sdl3_sprite_backend_end_frame();

        // Present
        SDL_RenderPresent(renderer);

        // Print FPS every 60 frames
        frame_count++;
        if (frame_count % 60 == 0) {
            uint64_t current_time = SDL_GetTicks();
            float elapsed = (current_time - start_time) / 1000.0f;
            float fps = frame_count / elapsed;

            uint32_t sprite_count, batch_count;
            sdl3_sprite_backend_get_stats(&sprite_count, &batch_count);

            printf("Frame %u: %.1f FPS | %u sprites | %u batches\n",
                   frame_count, fps, sprite_count, batch_count);
        }

        // Cap at ~60 FPS
        SDL_Delay(16);
    }

    // Cleanup
    printf("\nShutting down...\n");
    ir_sprite_atlas_destroy(atlas);
    sdl3_sprite_backend_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Test completed successfully\n");
    return 0;
}
