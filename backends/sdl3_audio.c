/*
 * SDL3 Audio Backend Implementation
 *
 * Simple backend using SDL3's audio API
 * Supports WAV files natively
 */

#define _POSIX_C_SOURCE 200809L

#include "sdl3_audio.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Backend Data Structures
// ============================================================================

typedef struct {
    SDL_AudioSpec spec;
    uint8_t* buffer;
    uint32_t length;
    char* path;
} SDL3Sound;

typedef struct {
    SDL3Sound* sound;
    SDL_AudioStream* stream;
    bool playing;
    bool loop;
    float volume;
    uint32_t position;
} SDL3Channel;

static struct {
    bool initialized;
    SDL_AudioDeviceID device;
    SDL_AudioSpec device_spec;

    SDL3Channel channels[IR_MAX_CHANNELS];
    SDL3Sound* current_music;
    SDL_AudioStream* music_stream;
    bool music_playing;
    bool music_loop;
    float music_volume;
    float master_volume;
} g_sdl3_audio = {0};

// ============================================================================
// Helper Functions
// ============================================================================

static void audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    (void)userdata;
    (void)additional_amount;
    (void)total_amount;
    (void)stream;

    // SDL3 uses push-based streaming, callback not needed for basic playback
}

// ============================================================================
// Backend Implementation
// ============================================================================

static bool backend_init(IRAudioConfig* config) {
    if (g_sdl3_audio.initialized) {
        return true;
    }

    // Initialize SDL audio subsystem
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "[SDL3 Audio] Failed to initialize: %s\n", SDL_GetError());
        return false;
    }

    // Setup audio spec
    SDL_AudioSpec desired_spec = {0};
    desired_spec.freq = config->sample_rate;
    desired_spec.channels = config->channels;
    desired_spec.format = SDL_AUDIO_S16;  // 16-bit signed

    // Open audio device
    g_sdl3_audio.device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec);
    if (g_sdl3_audio.device == 0) {
        fprintf(stderr, "[SDL3 Audio] Failed to open audio device: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    g_sdl3_audio.device_spec = desired_spec;
    g_sdl3_audio.master_volume = 1.0f;
    g_sdl3_audio.music_volume = 1.0f;

    // Initialize channels
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        g_sdl3_audio.channels[i].playing = false;
        g_sdl3_audio.channels[i].volume = 1.0f;
    }

    // Resume audio device (start playback)
    SDL_ResumeAudioDevice(g_sdl3_audio.device);

    g_sdl3_audio.initialized = true;

    printf("[SDL3 Audio] Backend initialized (%u Hz, %u channels)\n",
           desired_spec.freq, desired_spec.channels);

    return true;
}

static void backend_shutdown(void) {
    if (!g_sdl3_audio.initialized) return;

    // Close all streams
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_sdl3_audio.channels[i].stream) {
            SDL_DestroyAudioStream(g_sdl3_audio.channels[i].stream);
            g_sdl3_audio.channels[i].stream = NULL;
        }
    }

    if (g_sdl3_audio.music_stream) {
        SDL_DestroyAudioStream(g_sdl3_audio.music_stream);
        g_sdl3_audio.music_stream = NULL;
    }

    // Close audio device
    if (g_sdl3_audio.device) {
        SDL_CloseAudioDevice(g_sdl3_audio.device);
        g_sdl3_audio.device = 0;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    memset(&g_sdl3_audio, 0, sizeof(g_sdl3_audio));

    printf("[SDL3 Audio] Backend shutdown\n");
}

static void* backend_load_sound(const char* path, uint32_t* duration_ms,
                                 uint32_t* sample_rate, uint8_t* channels) {
    SDL3Sound* sound = (SDL3Sound*)malloc(sizeof(SDL3Sound));
    if (!sound) return NULL;

    memset(sound, 0, sizeof(SDL3Sound));

    // Load WAV file
    if (!SDL_LoadWAV(path, &sound->spec, &sound->buffer, &sound->length)) {
        fprintf(stderr, "[SDL3 Audio] Failed to load WAV: %s - %s\n", path, SDL_GetError());
        free(sound);
        return NULL;
    }

    sound->path = strdup(path);

    // Calculate duration in milliseconds
    uint32_t samples = sound->length / (SDL_AUDIO_BITSIZE(sound->spec.format) / 8) / sound->spec.channels;
    *duration_ms = (samples * 1000) / sound->spec.freq;
    *sample_rate = sound->spec.freq;
    *channels = sound->spec.channels;

    printf("[SDL3 Audio] Loaded WAV: %s (%u bytes, %u ms)\n", path, sound->length, *duration_ms);

    return sound;
}

static void backend_unload_sound(void* backend_data) {
    if (!backend_data) return;

    SDL3Sound* sound = (SDL3Sound*)backend_data;

    if (sound->buffer) {
        SDL_free(sound->buffer);
    }
    if (sound->path) {
        free(sound->path);
    }

    free(sound);
}

static void* backend_load_music(const char* path, uint32_t* duration_ms) {
    // For now, music uses the same loading as sounds
    uint32_t sample_rate;
    uint8_t channels;
    return backend_load_sound(path, duration_ms, &sample_rate, &channels);
}

static void backend_unload_music(void* backend_data) {
    backend_unload_sound(backend_data);
}

static IRChannelID backend_play_sound(void* sound_data, float volume, float pan, bool loop) {
    (void)pan;  // TODO: Implement panning

    if (!sound_data) return IR_INVALID_CHANNEL;

    SDL3Sound* sound = (SDL3Sound*)sound_data;

    // Find free channel
    int channel_idx = -1;
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        if (!g_sdl3_audio.channels[i].playing) {
            channel_idx = i;
            break;
        }
    }

    if (channel_idx == -1) {
        fprintf(stderr, "[SDL3 Audio] No free channels\n");
        return IR_INVALID_CHANNEL;
    }

    SDL3Channel* channel = &g_sdl3_audio.channels[channel_idx];

    // Create audio stream if needed
    if (!channel->stream) {
        channel->stream = SDL_CreateAudioStream(&sound->spec, &g_sdl3_audio.device_spec);
        if (!channel->stream) {
            fprintf(stderr, "[SDL3 Audio] Failed to create stream: %s\n", SDL_GetError());
            return IR_INVALID_CHANNEL;
        }

        // Bind stream to device
        SDL_BindAudioStream(g_sdl3_audio.device, channel->stream);
    }

    // Setup channel
    channel->sound = sound;
    channel->playing = true;
    channel->loop = loop;
    channel->volume = volume;
    channel->position = 0;

    // Put audio data into stream
    if (!SDL_PutAudioStreamData(channel->stream, sound->buffer, sound->length)) {
        fprintf(stderr, "[SDL3 Audio] Failed to queue audio: %s\n", SDL_GetError());
        channel->playing = false;
        return IR_INVALID_CHANNEL;
    }

    // Set volume (SDL3 doesn't have per-stream volume, so we'll skip for now)
    // In production, you'd apply volume by modifying the PCM data

    return channel_idx + 1;
}

static void backend_stop_channel(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];
    if (!channel->playing) return;

    // Clear the stream
    if (channel->stream) {
        SDL_ClearAudioStream(channel->stream);
    }

    channel->playing = false;
    channel->sound = NULL;
}

static void backend_pause_channel(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];
    if (channel->stream) {
        SDL_PauseAudioStreamDevice(channel->stream);
    }
}

static void backend_resume_channel(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];
    if (channel->stream) {
        SDL_ResumeAudioStreamDevice(channel->stream);
    }
}

static void backend_play_music(void* music_data, bool loop) {
    if (!music_data) return;

    SDL3Sound* music = (SDL3Sound*)music_data;

    // Stop current music
    if (g_sdl3_audio.music_stream) {
        SDL_ClearAudioStream(g_sdl3_audio.music_stream);
    } else {
        g_sdl3_audio.music_stream = SDL_CreateAudioStream(&music->spec, &g_sdl3_audio.device_spec);
        if (!g_sdl3_audio.music_stream) {
            fprintf(stderr, "[SDL3 Audio] Failed to create music stream: %s\n", SDL_GetError());
            return;
        }
        SDL_BindAudioStream(g_sdl3_audio.device, g_sdl3_audio.music_stream);
    }

    g_sdl3_audio.current_music = music;
    g_sdl3_audio.music_playing = true;
    g_sdl3_audio.music_loop = loop;

    // Queue music data
    if (!SDL_PutAudioStreamData(g_sdl3_audio.music_stream, music->buffer, music->length)) {
        fprintf(stderr, "[SDL3 Audio] Failed to queue music: %s\n", SDL_GetError());
        g_sdl3_audio.music_playing = false;
    }
}

static void backend_stop_music(void) {
    if (g_sdl3_audio.music_stream) {
        SDL_ClearAudioStream(g_sdl3_audio.music_stream);
    }
    g_sdl3_audio.music_playing = false;
    g_sdl3_audio.current_music = NULL;
}

static void backend_pause_music(void) {
    if (g_sdl3_audio.music_stream) {
        SDL_PauseAudioStreamDevice(g_sdl3_audio.music_stream);
    }
}

static void backend_resume_music(void) {
    if (g_sdl3_audio.music_stream) {
        SDL_ResumeAudioStreamDevice(g_sdl3_audio.music_stream);
    }
}

static void backend_set_channel_volume(IRChannelID channel_id, float volume) {
    (void)channel_id;
    (void)volume;
    // SDL3 doesn't have per-stream volume control easily
    // Would need to modify PCM data or use a mixing library
}

static void backend_set_music_volume(float volume) {
    g_sdl3_audio.music_volume = volume;
    // Would need to modify PCM data or use a mixing library
}

static void backend_set_master_volume(float volume) {
    g_sdl3_audio.master_volume = volume;
    // Would need to modify PCM data or use a mixing library
}

static IRAudioState backend_get_channel_state(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return IR_AUDIO_STOPPED;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return IR_AUDIO_STOPPED;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];

    if (!channel->playing) return IR_AUDIO_STOPPED;

    // Check if stream is empty (finished playing)
    if (channel->stream) {
        int queued = SDL_GetAudioStreamQueued(channel->stream);
        if (queued == 0 && !channel->loop) {
            channel->playing = false;
            return IR_AUDIO_STOPPED;
        }
    }

    return IR_AUDIO_PLAYING;
}

static IRAudioState backend_get_music_state(void) {
    if (!g_sdl3_audio.music_playing) return IR_AUDIO_STOPPED;

    // Check if music stream is empty
    if (g_sdl3_audio.music_stream) {
        int queued = SDL_GetAudioStreamQueued(g_sdl3_audio.music_stream);
        if (queued == 0) {
            if (g_sdl3_audio.music_loop && g_sdl3_audio.current_music) {
                // Re-queue for loop
                SDL_PutAudioStreamData(g_sdl3_audio.music_stream,
                                      g_sdl3_audio.current_music->buffer,
                                      g_sdl3_audio.current_music->length);
            } else {
                g_sdl3_audio.music_playing = false;
                return IR_AUDIO_STOPPED;
            }
        }
    }

    return IR_AUDIO_PLAYING;
}

static void backend_update(float dt) {
    (void)dt;

    // Update channel states
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_sdl3_audio.channels[i].playing) {
            backend_get_channel_state(i + 1);  // Will update state if finished
        }
    }

    // Update music state
    backend_get_music_state();  // Will handle looping if needed
}

// ============================================================================
// Backend Plugin Interface
// ============================================================================

static IRAudioBackendPlugin g_sdl3_backend_plugin = {
    .init = backend_init,
    .shutdown = backend_shutdown,
    .load_sound = backend_load_sound,
    .unload_sound = backend_unload_sound,
    .load_music = backend_load_music,
    .unload_music = backend_unload_music,
    .play_sound = backend_play_sound,
    .stop_channel = backend_stop_channel,
    .pause_channel = backend_pause_channel,
    .resume_channel = backend_resume_channel,
    .play_music = backend_play_music,
    .stop_music = backend_stop_music,
    .pause_music = backend_pause_music,
    .resume_music = backend_resume_music,
    .set_channel_volume = backend_set_channel_volume,
    .set_music_volume = backend_set_music_volume,
    .set_master_volume = backend_set_master_volume,
    .get_channel_state = backend_get_channel_state,
    .get_music_state = backend_get_music_state,
    .update = backend_update
};

IRAudioBackendPlugin* sdl3_audio_get_backend(void) {
    return &g_sdl3_backend_plugin;
}

bool sdl3_audio_init(IRAudioConfig* config) {
    return backend_init(config);
}

void sdl3_audio_shutdown(void) {
    backend_shutdown();
}
