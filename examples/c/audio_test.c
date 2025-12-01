/*
 * Audio System Test
 *
 * Tests audio system initialization and API
 * Note: To test actual playback, place WAV files in /tmp/ directory
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

// Kryon audio system
#include "../../ir/ir_audio.h"
#include "../../backends/sdl3_audio.h"

// Window dimensions
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// ============================================================================
// Main Program
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Kryon Audio System Test ===\n\n");

    // Initialize SDL3 (for window and audio)
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "Failed to initialize SDL3: %s\n", SDL_GetError());
        return 1;
    }

    // Create window (for event handling)
    SDL_Window* window = SDL_CreateWindow(
        "Kryon Audio Test",
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

    // Initialize audio system
    printf("Initializing audio system...\n");
    if (!ir_audio_init()) {
        fprintf(stderr, "Failed to initialize audio system\n");
        return 1;
    }

    // Register SDL3 backend
    printf("Registering SDL3 audio backend...\n");
    if (!ir_audio_register_backend(IR_AUDIO_BACKEND_SDL3, sdl3_audio_get_backend())) {
        fprintf(stderr, "Failed to register SDL3 backend\n");
        return 1;
    }

    printf("✓ Audio system initialized\n\n");

    printf("NOTE: Place WAV files in /tmp/ directory to test playback:\n");
    printf("  /tmp/beep_low.wav\n");
    printf("  /tmp/beep_mid.wav\n");
    printf("  /tmp/beep_high.wav\n");
    printf("  /tmp/music.wav\n\n");

    // Try to load sounds (may fail if files don't exist)
    printf("Attempting to load sounds...\n");
    IRSoundID sound_low = ir_audio_load_sound("/tmp/beep_low.wav");
    IRSoundID sound_mid = ir_audio_load_sound("/tmp/beep_mid.wav");
    IRSoundID sound_high = ir_audio_load_sound("/tmp/beep_high.wav");

    bool has_sounds = (sound_low != IR_INVALID_SOUND &&
                       sound_mid != IR_INVALID_SOUND &&
                       sound_high != IR_INVALID_SOUND);

    if (has_sounds) {
        printf("✓ Loaded 3 sounds\n");
    } else {
        printf("⚠ Sounds not found (playback disabled)\n");
    }

    // Try to load music
    IRMusicID music = ir_audio_load_music("/tmp/music.wav");
    if (music != IR_INVALID_MUSIC) {
        printf("✓ Loaded music\n");
    } else {
        printf("⚠ Music not found (playback disabled)\n");
    }

    printf("\n");

    // Print statistics
    ir_audio_print_stats();

    printf("Controls:\n");
    printf("  1 - Play low beep (left pan)\n");
    printf("  2 - Play mid beep (center)\n");
    printf("  3 - Play high beep (right pan)\n");
    printf("  M - Play music (looping)\n");
    printf("  S - Stop music\n");
    printf("  P - Pause/Resume music\n");
    printf("  + - Increase master volume\n");
    printf("  - - Decrease master volume\n");
    printf("  I - Print audio info\n");
    printf("  ESC - Quit\n\n");

    // Main loop
    bool running = true;
    bool music_paused = false;
    float master_volume = 1.0f;

    uint64_t last_frame_time = SDL_GetTicks();
    uint32_t frame_count = 0;

    while (running) {
        uint64_t current_time = SDL_GetTicks();
        float dt = (current_time - last_frame_time) / 1000.0f;
        last_frame_time = current_time;

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
                        if (has_sounds) {
                            printf("Playing low beep (left)\n");
                            ir_audio_play_sound(sound_low, 0.5f, -1.0f);  // Pan left
                        } else {
                            printf("No sound loaded\n");
                        }
                        break;

                    case SDLK_2:
                        if (has_sounds) {
                            printf("Playing mid beep (center)\n");
                            ir_audio_play_sound(sound_mid, 0.5f, 0.0f);  // Pan center
                        } else {
                            printf("No sound loaded\n");
                        }
                        break;

                    case SDLK_3:
                        if (has_sounds) {
                            printf("Playing high beep (right)\n");
                            ir_audio_play_sound(sound_high, 0.5f, 1.0f);  // Pan right
                        } else {
                            printf("No sound loaded\n");
                        }
                        break;

                    case SDLK_M:
                        if (music != IR_INVALID_MUSIC) {
                            printf("Playing music (looping)\n");
                            ir_audio_play_music(music, true);
                            music_paused = false;
                        } else {
                            printf("No music loaded\n");
                        }
                        break;

                    case SDLK_S:
                        printf("Stopping music\n");
                        ir_audio_stop_music();
                        music_paused = false;
                        break;

                    case SDLK_P:
                        if (music_paused) {
                            printf("Resuming music\n");
                            ir_audio_resume_music();
                        } else {
                            printf("Pausing music\n");
                            ir_audio_pause_music();
                        }
                        music_paused = !music_paused;
                        break;

                    case SDLK_EQUALS:  // +
                    case SDLK_PLUS:
                        master_volume += 0.1f;
                        if (master_volume > 1.0f) master_volume = 1.0f;
                        ir_audio_set_master_volume(master_volume);
                        printf("Master volume: %.1f\n", master_volume);
                        break;

                    case SDLK_MINUS:
                        master_volume -= 0.1f;
                        if (master_volume < 0.0f) master_volume = 0.0f;
                        ir_audio_set_master_volume(master_volume);
                        printf("Master volume: %.1f\n", master_volume);
                        break;

                    case SDLK_I:
                        ir_audio_print_stats();
                        break;
                }
            }
        }

        // Update audio system
        ir_audio_update(dt);

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_RenderClear(renderer);

        // Draw simple visualization
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);

        // Draw volume bar
        int bar_width = (int)(master_volume * 400);
        SDL_FRect bar = {200, 250, bar_width, 100};
        SDL_RenderFillRect(renderer, &bar);

        // Present
        SDL_RenderPresent(renderer);

        frame_count++;

        // Small delay to prevent busy-waiting
        SDL_Delay(16);  // ~60 FPS
    }

    // Cleanup
    printf("\nShutting down...\n");

    if (sound_low != IR_INVALID_SOUND) ir_audio_unload_sound(sound_low);
    if (sound_mid != IR_INVALID_SOUND) ir_audio_unload_sound(sound_mid);
    if (sound_high != IR_INVALID_SOUND) ir_audio_unload_sound(sound_high);
    if (music != IR_INVALID_MUSIC) ir_audio_unload_music(music);

    ir_audio_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("✓ Test completed successfully\n");
    printf("Total frames: %u\n", frame_count);

    return 0;
}
