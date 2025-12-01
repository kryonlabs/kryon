/*
 * Sprite System Stress Test
 *
 * Tests sprite batching performance with 1000+ sprites
 * Goal: 1000 sprites at 60 FPS with < 5 draw calls
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <time.h>

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
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// Sprite count configurations
#define SPRITE_COUNT_LOW 100
#define SPRITE_COUNT_MEDIUM 500
#define SPRITE_COUNT_HIGH 1000
#define SPRITE_COUNT_EXTREME 2000

// Current sprite count
static int g_sprite_count = SPRITE_COUNT_HIGH;

// ============================================================================
// Sprite Instance
// ============================================================================

typedef struct {
    float x, y;
    float vx, vy;
    int sprite_idx;
    float rotation;
    float rotation_speed;
    float scale;
} SpriteInstance;

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
// Performance Metrics
// ============================================================================

typedef struct {
    float min_fps;
    float max_fps;
    float avg_fps;
    uint32_t frame_count;
    float total_time;
} PerfMetrics;

static void update_metrics(PerfMetrics* m, float fps, float dt) {
    if (m->frame_count == 0) {
        m->min_fps = fps;
        m->max_fps = fps;
        m->avg_fps = fps;
    } else {
        if (fps < m->min_fps) m->min_fps = fps;
        if (fps > m->max_fps) m->max_fps = fps;
        m->avg_fps = (m->avg_fps * m->frame_count + fps) / (m->frame_count + 1);
    }
    m->frame_count++;
    m->total_time += dt;
}

// ============================================================================
// Sprite Initialization
// ============================================================================

static void init_sprites(SpriteInstance* inst, int count) {
    for (int i = 0; i < count; i++) {
        inst[i].x = (float)(rand() % WINDOW_WIDTH);
        inst[i].y = (float)(rand() % WINDOW_HEIGHT);
        inst[i].vx = ((rand() % 200) - 100) / 10.0f;
        inst[i].vy = ((rand() % 200) - 100) / 10.0f;
        inst[i].sprite_idx = rand() % 8;
        inst[i].rotation = (float)(rand() % 360);
        inst[i].rotation_speed = ((rand() % 100) - 50) / 10.0f;
        inst[i].scale = 0.5f + (rand() % 100) / 100.0f;
    }
}

// ============================================================================
// Main Program
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc > 1) {
        int count = atoi(argv[1]);
        if (count > 0 && count <= 5000) {
            g_sprite_count = count;
        }
    }

    printf("=== Kryon Sprite System Stress Test ===\n\n");
    printf("Sprite Count: %d\n", g_sprite_count);
    printf("Target: 60 FPS, < 5 draw calls\n\n");

    // Seed random
    srand(time(NULL));

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Sprite Stress Test",
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );

    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer with VSync
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

    // Create sprite atlas (2048x2048 for more sprites)
    printf("Creating sprite atlas (2048x2048)...\n");
    IRSpriteAtlasID atlas = ir_sprite_atlas_create(2048, 2048);
    if (atlas == IR_INVALID_SPRITE_ATLAS) {
        fprintf(stderr, "Failed to create sprite atlas\n");
        return 1;
    }

    // Create test sprites with different colors (8 variants for variety)
    printf("Adding sprites to atlas...\n");
    IRSpriteID sprites[8];

    uint8_t colors[8][3] = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {255, 255, 0},  // Yellow
        {255, 0, 255},  // Magenta
        {0, 255, 255},  // Cyan
        {255, 128, 0},  // Orange
        {128, 0, 255}   // Purple
    };

    for (int i = 0; i < 8; i++) {
        uint8_t* pixels = create_colored_square(64, colors[i][0], colors[i][1], colors[i][2]);
        char name[32];
        snprintf(name, sizeof(name), "sprite_%d", i);
        sprites[i] = ir_sprite_atlas_add_image_from_memory(atlas, name, pixels, 64, 64);
        free(pixels);

        if (sprites[i] == IR_INVALID_SPRITE) {
            fprintf(stderr, "Failed to add sprite %d\n", i);
            return 1;
        }
    }

    // Pack and upload atlas
    printf("Packing atlas and uploading to GPU...\n");
    if (!ir_sprite_atlas_pack(atlas)) {
        fprintf(stderr, "Failed to pack sprite atlas\n");
        return 1;
    }

    printf("\n");
    printf("✓ Sprite system initialized\n");
    printf("✓ Created 8 colored sprites (64x64)\n");
    printf("✓ Atlas packed and uploaded to GPU\n");
    printf("\n");
    printf("Controls:\n");
    printf("  1-4   - Change sprite count (100/500/1000/2000)\n");
    printf("  SPACE - Randomize positions\n");
    printf("  R     - Reset positions\n");
    printf("  ESC   - Quit\n");
    printf("\n");
    printf("Starting stress test with %d sprites...\n\n", g_sprite_count);

    // Allocate sprite instances
    SpriteInstance* instances = malloc(sizeof(SpriteInstance) * SPRITE_COUNT_EXTREME);
    init_sprites(instances, g_sprite_count);

    // Performance tracking
    PerfMetrics metrics = {0};
    uint64_t start_time = SDL_GetTicks();
    uint64_t last_frame_time = start_time;
    uint32_t frame_count = 0;

    // Main loop
    bool running = true;
    bool needs_reinit = false;

    while (running) {
        uint64_t current_time = SDL_GetTicks();
        float dt = (current_time - last_frame_time) / 1000.0f;
        last_frame_time = current_time;

        // Cap dt
        if (dt > 0.1f) dt = 0.1f;

        // Calculate FPS
        float fps = 1.0f / dt;
        update_metrics(&metrics, fps, dt);

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
                    case SDLK_1:
                        g_sprite_count = SPRITE_COUNT_LOW;
                        needs_reinit = true;
                        break;
                    case SDLK_2:
                        g_sprite_count = SPRITE_COUNT_MEDIUM;
                        needs_reinit = true;
                        break;
                    case SDLK_3:
                        g_sprite_count = SPRITE_COUNT_HIGH;
                        needs_reinit = true;
                        break;
                    case SDLK_4:
                        g_sprite_count = SPRITE_COUNT_EXTREME;
                        needs_reinit = true;
                        break;
                    case SDLK_SPACE:
                        init_sprites(instances, g_sprite_count);
                        printf("Randomized sprite positions\n");
                        break;
                    case SDLK_R:
                        init_sprites(instances, g_sprite_count);
                        metrics = (PerfMetrics){0};
                        printf("Reset sprites and metrics\n");
                        break;
                }
            }
        }

        if (needs_reinit) {
            init_sprites(instances, g_sprite_count);
            metrics = (PerfMetrics){0};
            printf("\nChanged to %d sprites\n", g_sprite_count);
            needs_reinit = false;
        }

        // Update sprite positions (bouncing)
        for (int i = 0; i < g_sprite_count; i++) {
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
        for (int i = 0; i < g_sprite_count; i++) {
            ir_sprite_draw_ex(
                sprites[instances[i].sprite_idx],
                instances[i].x,
                instances[i].y,
                instances[i].rotation,
                instances[i].scale,
                instances[i].scale,
                0xFFFFFFFF  // White (no tint)
            );
        }

        // End sprite frame (flushes batches)
        sdl3_sprite_backend_end_frame();

        // Present
        SDL_RenderPresent(renderer);

        // Print statistics every 60 frames
        frame_count++;
        if (frame_count % 60 == 0) {
            uint32_t sprite_count, batch_count;
            sdl3_sprite_backend_get_stats(&sprite_count, &batch_count);

            printf("Frame %4u | FPS: %5.1f (min: %5.1f, max: %5.1f, avg: %5.1f) | %u sprites | %u batches\n",
                   frame_count, fps, metrics.min_fps, metrics.max_fps, metrics.avg_fps,
                   sprite_count, batch_count);
        }
    }

    // Final report
    printf("\n");
    printf("=== Performance Summary ===\n");
    printf("Sprite Count: %d\n", g_sprite_count);
    printf("Total Frames: %u\n", metrics.frame_count);
    printf("Total Time: %.2f seconds\n", metrics.total_time);
    printf("FPS - Min: %.1f, Max: %.1f, Avg: %.1f\n",
           metrics.min_fps, metrics.max_fps, metrics.avg_fps);

    if (metrics.avg_fps >= 60.0f) {
        printf("✓ PASSED: Average FPS >= 60\n");
    } else {
        printf("✗ FAILED: Average FPS < 60 (%.1f)\n", metrics.avg_fps);
    }

    // Cleanup
    free(instances);
    ir_sprite_atlas_destroy(atlas);
    sdl3_sprite_backend_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return (metrics.avg_fps >= 60.0f) ? 0 : 1;
}
