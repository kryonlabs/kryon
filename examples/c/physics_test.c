/*
 * Physics System Test
 *
 * Tests 2D physics simulation with AABB collision detection
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

// Kryon physics system
#include "../../ir/ir_physics.h"

// Window dimensions
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// ============================================================================
// Collision Callback
// ============================================================================

static void on_collision(IRPhysicsBodyID body_a, IRPhysicsBodyID body_b,
                        const IRContact* contact, void* user_data) {
    (void)user_data;
    printf("Collision: Body %u <-> Body %u at (%.1f, %.1f)\n",
           body_a, body_b, contact->point.x, contact->point.y);
}

// ============================================================================
// Helper: Draw Physics Body
// ============================================================================

static void draw_body(SDL_Renderer* renderer, const IRPhysicsBody* body) {
    if (!body->active) return;

    // Set color based on body type
    if (body->type == IR_BODY_STATIC) {
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);  // Gray
    } else if (body->type == IR_BODY_DYNAMIC) {
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);  // Green
    } else {
        SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);  // Yellow
    }

    if (body->shape == IR_SHAPE_AABB) {
        const IRAABB* aabb = &body->shape_data.aabb;
        SDL_FRect rect = {
            aabb->x - aabb->width / 2.0f,
            aabb->y - aabb->height / 2.0f,
            aabb->width,
            aabb->height
        };
        SDL_RenderFillRect(renderer, &rect);

        // Draw outline
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &rect);
    } else if (body->shape == IR_SHAPE_CIRCLE) {
        const IRCircle* circle = &body->shape_data.circle;
        int segments = 32;
        SDL_FPoint points[33];

        for (int i = 0; i <= segments; i++) {
            float angle = (i / (float)segments) * 2.0f * 3.14159f;
            points[i].x = circle->x + cosf(angle) * circle->radius;
            points[i].y = circle->y + sinf(angle) * circle->radius;
        }

        SDL_RenderLines(renderer, points, segments + 1);
    }
}

// ============================================================================
// Main Program
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Kryon Physics System Test ===\n\n");

    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Physics Test",
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

    // Initialize physics system
    printf("Initializing physics system...\n");
    if (!ir_physics_init()) {
        fprintf(stderr, "Failed to initialize physics system\n");
        return 1;
    }

    printf("✓ Physics system initialized\n\n");

    // Set gravity
    ir_physics_set_gravity(0.0f, 500.0f);  // Downward gravity

    // Create ground (static)
    IRPhysicsBodyID ground = ir_physics_create_box(IR_BODY_STATIC, WINDOW_WIDTH/2, WINDOW_HEIGHT - 50, WINDOW_WIDTH, 100);
    printf("Created ground: Body %u\n", ground);

    // Create left wall
    IRPhysicsBodyID wall_left = ir_physics_create_box(IR_BODY_STATIC, 50, WINDOW_HEIGHT/2, 100, WINDOW_HEIGHT);
    printf("Created left wall: Body %u\n", wall_left);

    // Create right wall
    IRPhysicsBodyID wall_right = ir_physics_create_box(IR_BODY_STATIC, WINDOW_WIDTH - 50, WINDOW_HEIGHT/2, 100, WINDOW_HEIGHT);
    printf("Created right wall: Body %u\n", wall_right);

    // Create some dynamic boxes
    IRPhysicsBodyID box1 = ir_physics_create_box(IR_BODY_DYNAMIC, 400, 100, 64, 64);
    ir_physics_set_velocity(box1, 100.0f, 0.0f);
    ir_physics_set_restitution(box1, 0.5f);  // 50% bounce
    printf("Created box 1: Body %u\n", box1);

    IRPhysicsBodyID box2 = ir_physics_create_box(IR_BODY_DYNAMIC, 600, 150, 48, 48);
    ir_physics_set_velocity(box2, -80.0f, 50.0f);
    ir_physics_set_restitution(box2, 0.7f);  // 70% bounce
    printf("Created box 2: Body %u\n", box2);

    IRPhysicsBodyID box3 = ir_physics_create_box(IR_BODY_DYNAMIC, 800, 200, 80, 80);
    ir_physics_set_velocity(box3, -120.0f, 0.0f);
    ir_physics_set_restitution(box3, 0.3f);  // 30% bounce
    printf("Created box 3: Body %u\n", box3);

    // Set collision callback
    ir_physics_set_global_callback(on_collision, NULL);

    printf("\n");
    ir_physics_print_stats();

    printf("Controls:\n");
    printf("  SPACE - Spawn new box at mouse position\n");
    printf("  R     - Reset simulation\n");
    printf("  G     - Toggle gravity\n");
    printf("  I     - Print statistics\n");
    printf("  ESC   - Quit\n\n");

    // Main loop
    bool running = true;
    bool gravity_enabled = true;
    uint64_t last_frame_time = SDL_GetTicks();
    uint32_t frame_count = 0;

    while (running) {
        uint64_t current_time = SDL_GetTicks();
        float dt = (current_time - last_frame_time) / 1000.0f;
        last_frame_time = current_time;

        // Cap dt to avoid large steps
        if (dt > 0.05f) dt = 0.05f;

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

                    case SDLK_SPACE: {
                        float mouse_x, mouse_y;
                        SDL_GetMouseState(&mouse_x, &mouse_y);
                        IRPhysicsBodyID box = ir_physics_create_box(IR_BODY_DYNAMIC, mouse_x, mouse_y, 50, 50);
                        ir_physics_set_restitution(box, 0.6f);
                        printf("Spawned box at (%.0f, %.0f)\n", mouse_x, mouse_y);
                        break;
                    }

                    case SDLK_R:
                        printf("Resetting simulation...\n");
                        ir_physics_shutdown();
                        ir_physics_init();
                        ir_physics_set_gravity(0.0f, gravity_enabled ? 500.0f : 0.0f);
                        ir_physics_set_global_callback(on_collision, NULL);

                        ground = ir_physics_create_box(IR_BODY_STATIC, WINDOW_WIDTH/2, WINDOW_HEIGHT - 50, WINDOW_WIDTH, 100);
                        wall_left = ir_physics_create_box(IR_BODY_STATIC, 50, WINDOW_HEIGHT/2, 100, WINDOW_HEIGHT);
                        wall_right = ir_physics_create_box(IR_BODY_STATIC, WINDOW_WIDTH - 50, WINDOW_HEIGHT/2, 100, WINDOW_HEIGHT);

                        box1 = ir_physics_create_box(IR_BODY_DYNAMIC, 400, 100, 64, 64);
                        ir_physics_set_velocity(box1, 100.0f, 0.0f);
                        ir_physics_set_restitution(box1, 0.5f);

                        box2 = ir_physics_create_box(IR_BODY_DYNAMIC, 600, 150, 48, 48);
                        ir_physics_set_velocity(box2, -80.0f, 50.0f);
                        ir_physics_set_restitution(box2, 0.7f);

                        box3 = ir_physics_create_box(IR_BODY_DYNAMIC, 800, 200, 80, 80);
                        ir_physics_set_velocity(box3, -120.0f, 0.0f);
                        ir_physics_set_restitution(box3, 0.3f);
                        break;

                    case SDLK_G:
                        gravity_enabled = !gravity_enabled;
                        ir_physics_set_gravity(0.0f, gravity_enabled ? 500.0f : 0.0f);
                        printf("Gravity %s\n", gravity_enabled ? "enabled" : "disabled");
                        break;

                    case SDLK_I:
                        ir_physics_print_stats();
                        break;
                }
            }
        }

        // Step physics
        ir_physics_step(dt);

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Draw all physics bodies
        IRPhysicsStats stats;
        ir_physics_get_stats(&stats);

        for (uint32_t i = 0; i < stats.body_count; i++) {
            IRPhysicsBodyID body_id = i + 1;
            IRPhysicsBody* body = ir_physics_get_body(body_id);
            if (body) {
                draw_body(renderer, body);
            }
        }

        // Draw stats on screen
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        // Present
        SDL_RenderPresent(renderer);

        frame_count++;
    }

    // Final statistics
    printf("\n");
    ir_physics_print_stats();
    printf("Total frames: %u\n", frame_count);

    // Cleanup
    printf("\nShutting down...\n");
    ir_physics_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Test completed successfully\n");
    return 0;
}
