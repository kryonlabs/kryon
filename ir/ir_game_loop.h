#ifndef IR_GAME_LOOP_H
#define IR_GAME_LOOP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Game Loop & Timing System for Kryon
 *
 * Provides a fixed-timestep game loop with variable rendering.
 * - Fixed update at 60 FPS for physics/game logic
 * - Variable rendering rate (VSync or unlocked)
 * - Frame interpolation support (alpha parameter)
 * - Accumulator-based timing (prevents spiral of death)
 *
 * Usage:
 *   IRGameLoop loop;
 *   IRGameCallbacks callbacks = {
 *       .init = my_init,
 *       .fixed_update = my_update,
 *       .render = my_render,
 *       .cleanup = my_cleanup
 *   };
 *   ir_game_loop_init(&loop, callbacks, user_data);
 *   ir_game_loop_run(&loop);  // Blocks until stopped
 */

// Forward declarations
typedef struct IRGameLoop IRGameLoop;

/**
 * Game loop callback functions
 * All callbacks receive user_data pointer for game state
 */
typedef struct {
    /**
     * Called once at initialization before the main loop starts
     * Use this to load assets, initialize game state, etc.
     */
    void (*init)(void* user_data);

    /**
     * Called at a fixed timestep (default 60 FPS / 16.667ms)
     * This is where physics, collision detection, and game logic should run
     * dt is always constant (0.01666... for 60 FPS)
     */
    void (*fixed_update)(float dt, void* user_data);

    /**
     * Called every frame (variable timestep)
     * Use this for input handling, camera updates, and non-physics logic
     * dt varies based on actual frame time
     */
    void (*update)(float dt, void* user_data);

    /**
     * Called every frame for rendering
     * alpha is interpolation factor between [0.0, 1.0] for smooth rendering
     * Use: render_position = previous_pos + (current_pos - previous_pos) * alpha
     */
    void (*render)(float alpha, void* user_data);

    /**
     * Called once when the loop exits
     * Use this to clean up resources, save state, etc.
     */
    void (*cleanup)(void* user_data);
} IRGameCallbacks;

/**
 * Game loop configuration
 */
typedef struct {
    float fixed_timestep;      // Fixed timestep in seconds (default: 1/60)
    float max_frame_time;      // Maximum frame time to prevent spiral of death (default: 0.25s)
    bool vsync;                // Enable VSync (default: true)
    bool limit_fps;            // Limit FPS to fixed_timestep rate (default: false)
    uint32_t target_fps;       // Target FPS when limit_fps is true (default: 60)
} IRGameLoopConfig;

/**
 * Game loop state
 */
struct IRGameLoop {
    IRGameCallbacks callbacks;
    void* user_data;
    IRGameLoopConfig config;

    // Timing state
    uint64_t start_time;       // Loop start time in microseconds
    uint64_t current_time;     // Current frame time
    uint64_t previous_time;    // Previous frame time
    float accumulator;         // Time accumulator for fixed updates

    // Frame stats
    uint32_t frame_count;      // Total frames rendered
    float fps;                 // Current FPS (smoothed)
    float average_frame_time;  // Average frame time in seconds

    // Control
    bool running;              // Loop is running
    bool paused;               // Loop is paused (updates don't run)
};

/**
 * Initialize game loop with callbacks and user data
 * config can be NULL to use defaults
 */
void ir_game_loop_init(IRGameLoop* loop, IRGameCallbacks callbacks,
                       void* user_data, const IRGameLoopConfig* config);

/**
 * Create default game loop configuration
 */
IRGameLoopConfig ir_game_loop_default_config(void);

/**
 * Run the game loop (blocking call)
 * This will call init(), then run the main loop until stopped
 * When loop exits, cleanup() is called
 */
void ir_game_loop_run(IRGameLoop* loop);

/**
 * Stop the game loop (can be called from any callback)
 */
void ir_game_loop_stop(IRGameLoop* loop);

/**
 * Pause/unpause the game loop
 * When paused, fixed_update and update are not called, but render still runs
 */
void ir_game_loop_set_paused(IRGameLoop* loop, bool paused);

/**
 * Check if loop is paused
 */
bool ir_game_loop_is_paused(const IRGameLoop* loop);

/**
 * Get current FPS (smoothed over last 60 frames)
 */
float ir_game_loop_get_fps(const IRGameLoop* loop);

/**
 * Get average frame time in milliseconds
 */
float ir_game_loop_get_frame_time_ms(const IRGameLoop* loop);

/**
 * Get total frames rendered
 */
uint32_t ir_game_loop_get_frame_count(const IRGameLoop* loop);

/**
 * Get elapsed time since loop started (in seconds)
 */
float ir_game_loop_get_elapsed_time(const IRGameLoop* loop);

/**
 * Set fixed timestep (in seconds)
 * Common values: 1/60 (60 FPS), 1/30 (30 FPS), 1/120 (120 FPS)
 */
void ir_game_loop_set_timestep(IRGameLoop* loop, float timestep);

/**
 * Get current fixed timestep
 */
float ir_game_loop_get_timestep(const IRGameLoop* loop);

#ifdef __cplusplus
}
#endif

#endif // IR_GAME_LOOP_H
