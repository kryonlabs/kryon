/*
 * Particle System Test
 *
 * Demonstrates:
 * - Multiple particle emitters
 * - Different emitter types and presets
 * - Physics simulation (gravity, drag)
 * - Color and size interpolation
 * - Performance with thousands of particles
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

// Kryon systems
#include "../../ir/ir_particle.h"

// Window dimensions
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// ============================================================================
// Particle Render Callback
// ============================================================================

static void render_particle_callback(IRParticle* p, void* user_data) {
    SDL_Renderer* renderer = (SDL_Renderer*)user_data;
    if (!p || !renderer) return;

    // Extract RGBA from particle color
    uint8_t r = (p->color >> 24) & 0xFF;
    uint8_t g = (p->color >> 16) & 0xFF;
    uint8_t b = (p->color >> 8) & 0xFF;
    uint8_t a = p->color & 0xFF;

    // Set draw color with alpha blending
    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    // Draw particle as a filled rectangle
    SDL_FRect rect = {
        .x = p->x - p->size / 2,
        .y = p->y - p->size / 2,
        .w = p->size,
        .h = p->size
    };

    SDL_RenderFillRect(renderer, &rect);
}

// ============================================================================
// Main Program
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("=== Kryon Particle System Test ===\n\n");

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Particle Test",
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

    // Initialize particle system
    printf("Initializing particle system...\n");
    ir_particle_init();

    // Set render callback
    ir_particle_set_render_callback(render_particle_callback, renderer);

    // Create various emitters
    printf("Creating particle emitters...\n");

    // Fire emitter (left side)
    IRParticleEmitterID fire = ir_particle_create_fire(200, 500);
    ir_particle_emitter_start(fire);

    // Smoke emitter (center)
    IRParticleEmitterID smoke = ir_particle_create_smoke(400, 500);
    ir_particle_emitter_start(smoke);

    // Sparkles emitter (right side)
    IRParticleEmitterID sparkles = ir_particle_create_sparkles(600, 500);
    ir_particle_emitter_start(sparkles);

    printf("\n");
    printf("✓ Particle system initialized\n");
    printf("✓ Created 3 emitters (fire, smoke, sparkles)\n");
    printf("\n");
    printf("Controls:\n");
    printf("  SPACE - Create explosion at mouse position\n");
    printf("  R     - Create rain effect\n");
    printf("  C     - Clear all particles\n");
    printf("  ESC   - Quit\n");
    printf("\n");

    // Emitter for rain (created on demand)
    IRParticleEmitterID rain = IR_INVALID_EMITTER;

    // Main loop
    bool running = true;
    uint32_t frame_count = 0;
    uint64_t start_time = SDL_GetTicks();
    uint64_t last_frame_time = start_time;

    while (running) {
        uint64_t current_time = SDL_GetTicks();
        float dt = (current_time - last_frame_time) / 1000.0f;
        last_frame_time = current_time;

        // Cap dt to prevent huge jumps
        if (dt > 0.1f) dt = 0.1f;

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
                else if (event.key.key == SDLK_SPACE) {
                    // Create explosion at mouse position
                    float mx, my;
                    SDL_GetMouseState(&mx, &my);
                    ir_particle_create_explosion(mx, my);
                    printf("Explosion at (%.0f, %.0f)\n", mx, my);
                }
                else if (event.key.key == SDLK_R) {
                    // Toggle rain
                    if (rain == IR_INVALID_EMITTER) {
                        rain = ir_particle_create_rain(400, -10, 800);
                        ir_particle_emitter_start(rain);
                        printf("Rain started\n");
                    } else {
                        ir_particle_emitter_stop(rain);
                        ir_particle_destroy_emitter(rain);
                        rain = IR_INVALID_EMITTER;
                        printf("Rain stopped\n");
                    }
                }
                else if (event.key.key == SDLK_C) {
                    // Clear all particles
                    ir_particle_clear_all();
                    printf("Cleared all particles\n");
                }
            }
        }

        // Update particle system
        ir_particle_update(dt);

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Enable alpha blending for particles
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Render all particles (calls our callback for each particle)
        ir_particle_render();

        // Present
        SDL_RenderPresent(renderer);

        // Print statistics every 60 frames
        frame_count++;
        if (frame_count % 60 == 0) {
            uint64_t elapsed_time = current_time - start_time;
            float elapsed = elapsed_time / 1000.0f;
            float fps = frame_count / elapsed;

            uint32_t active_particles, active_emitters, total_spawned, total_died;
            ir_particle_get_stats(&active_particles, &active_emitters,
                                 &total_spawned, &total_died);

            printf("Frame %u: %.1f FPS | %u particles | %u emitters | Spawned: %u | Died: %u\n",
                   frame_count, fps, active_particles, active_emitters,
                   total_spawned, total_died);
        }

        // Cap at ~60 FPS
        SDL_Delay(16);
    }

    // Cleanup
    printf("\nShutting down...\n");
    ir_particle_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Test completed successfully\n");
    return 0;
}
