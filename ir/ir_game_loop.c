/*
 * Kryon Game Loop Implementation
 *
 * Fixed-timestep game loop with frame interpolation
 * Based on "Fix Your Timestep" by Glenn Fiedler
 * https://gafferongames.com/post/fix_your_timestep/
 */

#define _POSIX_C_SOURCE 199309L
#include "ir_game_loop.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Platform-specific high-resolution timer
#ifdef _WIN32
#include <windows.h>
static uint64_t get_time_microseconds(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000) / freq.QuadPart);
}
#else
#include <sys/time.h>
static uint64_t get_time_microseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}
#endif

// FPS smoothing buffer
#define FPS_SAMPLE_COUNT 60

typedef struct {
    float samples[FPS_SAMPLE_COUNT];
    uint32_t index;
    uint32_t count;
} FPSTracker;

static void fps_tracker_init(FPSTracker* tracker) {
    memset(tracker, 0, sizeof(FPSTracker));
}

static void fps_tracker_add_sample(FPSTracker* tracker, float frame_time) {
    tracker->samples[tracker->index] = frame_time;
    tracker->index = (tracker->index + 1) % FPS_SAMPLE_COUNT;
    if (tracker->count < FPS_SAMPLE_COUNT) {
        tracker->count++;
    }
}

static float fps_tracker_get_average(const FPSTracker* tracker) {
    if (tracker->count == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < tracker->count; i++) {
        sum += tracker->samples[i];
    }
    float avg_frame_time = sum / tracker->count;
    return avg_frame_time > 0.0f ? 1.0f / avg_frame_time : 0.0f;
}

// Default configuration
IRGameLoopConfig ir_game_loop_default_config(void) {
    IRGameLoopConfig config = {
        .fixed_timestep = 1.0f / 60.0f,  // 60 FPS
        .max_frame_time = 0.25f,         // 250ms max (prevents spiral of death)
        .vsync = true,
        .limit_fps = false,
        .target_fps = 60
    };
    return config;
}

// Initialize game loop
void ir_game_loop_init(IRGameLoop* loop, IRGameCallbacks callbacks,
                       void* user_data, const IRGameLoopConfig* config) {
    if (!loop) return;

    memset(loop, 0, sizeof(IRGameLoop));
    loop->callbacks = callbacks;
    loop->user_data = user_data;

    if (config) {
        loop->config = *config;
    } else {
        loop->config = ir_game_loop_default_config();
    }

    loop->running = false;
    loop->paused = false;
    loop->frame_count = 0;
    loop->fps = 0.0f;
    loop->accumulator = 0.0f;
}

// Run the game loop
void ir_game_loop_run(IRGameLoop* loop) {
    if (!loop) return;

    // Call init callback
    if (loop->callbacks.init) {
        loop->callbacks.init(loop->user_data);
    }

    // Initialize timing
    loop->start_time = get_time_microseconds();
    loop->previous_time = loop->start_time;
    loop->current_time = loop->start_time;
    loop->running = true;

    // FPS tracking
    FPSTracker fps_tracker;
    fps_tracker_init(&fps_tracker);

    // Main game loop
    while (loop->running) {
        // Get current time
        loop->current_time = get_time_microseconds();

        // Calculate frame time (in seconds)
        float frame_time = (loop->current_time - loop->previous_time) / 1000000.0f;
        loop->previous_time = loop->current_time;

        // Clamp frame time to prevent spiral of death
        if (frame_time > loop->config.max_frame_time) {
            frame_time = loop->config.max_frame_time;
        }

        // Update FPS tracker
        fps_tracker_add_sample(&fps_tracker, frame_time);
        loop->fps = fps_tracker_get_average(&fps_tracker);
        loop->average_frame_time = frame_time;

        // Variable timestep update (every frame)
        if (!loop->paused && loop->callbacks.update) {
            loop->callbacks.update(frame_time, loop->user_data);
        }

        // Fixed timestep updates (physics, game logic)
        if (!loop->paused && loop->callbacks.fixed_update) {
            loop->accumulator += frame_time;

            // Safety limit: don't allow more than 10 fixed updates per frame
            int update_count = 0;
            const int max_updates = 10;

            while (loop->accumulator >= loop->config.fixed_timestep && update_count < max_updates) {
                loop->callbacks.fixed_update(loop->config.fixed_timestep, loop->user_data);
                loop->accumulator -= loop->config.fixed_timestep;
                update_count++;
            }

            // If we hit the limit, discard remaining time to prevent catch-up spiral
            if (update_count >= max_updates) {
                loop->accumulator = 0.0f;
            }
        }

        // Render with interpolation
        if (loop->callbacks.render) {
            // Calculate interpolation alpha [0.0, 1.0]
            float alpha = loop->accumulator / loop->config.fixed_timestep;
            loop->callbacks.render(alpha, loop->user_data);
        }

        loop->frame_count++;

        // FPS limiting (if enabled and not using vsync)
        if (loop->config.limit_fps && !loop->config.vsync) {
            float target_frame_time = 1.0f / loop->config.target_fps;
            uint64_t frame_end_time = get_time_microseconds();
            float elapsed = (frame_end_time - loop->current_time) / 1000000.0f;

            if (elapsed < target_frame_time) {
                float sleep_time = target_frame_time - elapsed;
                uint64_t sleep_us = (uint64_t)(sleep_time * 1000000.0f);

                #ifdef _WIN32
                Sleep((DWORD)(sleep_us / 1000));
                #else
                struct timespec ts;
                ts.tv_sec = sleep_us / 1000000;
                ts.tv_nsec = (sleep_us % 1000000) * 1000;
                nanosleep(&ts, NULL);
                #endif
            }
        }
    }

    // Call cleanup callback
    if (loop->callbacks.cleanup) {
        loop->callbacks.cleanup(loop->user_data);
    }
}

// Stop the game loop
void ir_game_loop_stop(IRGameLoop* loop) {
    if (loop) {
        loop->running = false;
    }
}

// Pause/unpause
void ir_game_loop_set_paused(IRGameLoop* loop, bool paused) {
    if (loop) {
        loop->paused = paused;
    }
}

bool ir_game_loop_is_paused(const IRGameLoop* loop) {
    return loop ? loop->paused : false;
}

// Get FPS
float ir_game_loop_get_fps(const IRGameLoop* loop) {
    return loop ? loop->fps : 0.0f;
}

// Get average frame time in milliseconds
float ir_game_loop_get_frame_time_ms(const IRGameLoop* loop) {
    return loop ? (loop->average_frame_time * 1000.0f) : 0.0f;
}

// Get frame count
uint32_t ir_game_loop_get_frame_count(const IRGameLoop* loop) {
    return loop ? loop->frame_count : 0;
}

// Get elapsed time
float ir_game_loop_get_elapsed_time(const IRGameLoop* loop) {
    if (!loop) return 0.0f;
    uint64_t current = get_time_microseconds();
    return (current - loop->start_time) / 1000000.0f;
}

// Set timestep
void ir_game_loop_set_timestep(IRGameLoop* loop, float timestep) {
    if (loop && timestep > 0.0f) {
        loop->config.fixed_timestep = timestep;
    }
}

// Get timestep
float ir_game_loop_get_timestep(const IRGameLoop* loop) {
    return loop ? loop->config.fixed_timestep : 0.0f;
}
