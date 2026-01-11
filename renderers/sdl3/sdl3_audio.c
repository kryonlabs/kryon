/*
 * SDL3 Audio Backend Implementation
 *
 * Backend using SDL3's audio API with software mixing
 * Supports WAV, OGG, and MP3 files via decoder integration
 * Provides volume control and stereo panning
 */

#define _POSIX_C_SOURCE 200809L

#include "sdl3_audio.h"
#include "../../ir/ir_audio_mixer.h"
#include "../../ir/ir_audio_decoder.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Backend Data Structures
// ============================================================================

typedef struct {
    SDL_AudioSpec spec;
    uint8_t* buffer;           // Original audio data (read-only)
    uint32_t length;
    char* path;
    bool is_memory_loaded;     // True if loaded from memory
} SDL3Sound;

typedef struct {
    SDL3Sound* sound;
    SDL_AudioStream* stream;
    bool playing;
    bool loop;
    float volume;              // Per-channel volume (0.0 to 1.0)
    float pan;                 // Stereo pan (-1.0 to 1.0)
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
    float music_pan;
    float master_volume;
} g_sdl3_audio = {0};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Process audio buffer with volume and pan before queuing
 * Creates a temporary buffer with effects applied
 */
static bool queue_audio_processed(SDL_AudioStream* stream, const uint8_t* src,
                                  uint32_t length, const SDL_AudioSpec* spec,
                                  float volume, float pan) {
    if (!stream || !src || length == 0) return false;

    // Calculate total gain (volume * master)
    float total_volume = volume * g_sdl3_audio.master_volume;

    // If no processing needed, queue directly
    if (total_volume >= 0.99f && total_volume <= 1.01f &&
        pan >= -0.01f && pan <= 0.01f) {
        return SDL_PutAudioStreamData(stream, src, length);
    }

    // Create temporary buffer for processing
    uint8_t* temp_buffer = (uint8_t*)malloc(length);
    if (!temp_buffer) {
        fprintf(stderr, "[SDL3 Audio] Failed to allocate processing buffer\n");
        return false;
    }

    memcpy(temp_buffer, src, length);

    // Apply volume (works for 16-bit audio)
    if (spec->format == SDL_AUDIO_S16) {
        int16_t* samples = (int16_t*)temp_buffer;
        size_t sample_count = length / sizeof(int16_t);

        // Apply volume
        if (total_volume < 0.99f || total_volume > 1.01f) {
            ir_audio_apply_volume_int16(samples, sample_count, total_volume);
        }

        // Apply pan for stereo
        if (spec->channels == 2 && (pan < -0.01f || pan > 0.01f)) {
            size_t frame_count = sample_count / 2;
            ir_audio_apply_pan_stereo_int16(samples, frame_count, pan);
        }
    } else if (spec->format == SDL_AUDIO_S8) {
        // Convert to int16, process, convert back
        size_t sample_count = length;
        int16_t* temp_int16 = (int16_t*)malloc(length * 2);
        if (temp_int16) {
            for (size_t i = 0; i < sample_count; i++) {
                temp_int16[i] = ((int8_t*)temp_buffer)[i] * 256;
            }

            if (total_volume < 0.99f || total_volume > 1.01f) {
                ir_audio_apply_volume_int16(temp_int16, sample_count, total_volume);
            }

            if (spec->channels == 2 && (pan < -0.01f || pan > 0.01f)) {
                size_t frame_count = sample_count / 2;
                ir_audio_apply_pan_stereo_int16(temp_int16, frame_count, pan);
            }

            for (size_t i = 0; i < sample_count; i++) {
                ((int8_t*)temp_buffer)[i] = (int8_t)(temp_int16[i] / 256);
            }
            free(temp_int16);
        }
    }

    bool result = SDL_PutAudioStreamData(stream, temp_buffer, length);
    free(temp_buffer);

    return result;
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
    g_sdl3_audio.music_pan = 0.0f;

    // Initialize channels
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        g_sdl3_audio.channels[i].playing = false;
        g_sdl3_audio.channels[i].volume = 1.0f;
        g_sdl3_audio.channels[i].pan = 0.0f;
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

    // Detect format from file extension
    IRAudioFormat format = ir_audio_detect_format(path);

    // For OGG and MP3, use the decoder
    if (format == IR_AUDIO_FORMAT_OGG || format == IR_AUDIO_FORMAT_MP3) {
        IRDecodedAudio decoded = {0};
        if (ir_audio_decode_file(path, &decoded)) {
            // Setup SDL audio spec from decoded data
            sound->spec.freq = decoded.sample_rate;
            sound->spec.channels = decoded.channels;
            sound->spec.format = SDL_AUDIO_S16;

            sound->buffer = decoded.data;  // Transfer ownership
            sound->length = decoded.data_size;
            sound->path = strdup(path);
            sound->is_memory_loaded = false;

            *duration_ms = decoded.duration_ms;
            *sample_rate = decoded.sample_rate;
            *channels = decoded.channels;

            printf("[SDL3 Audio] Loaded %s: %s (%u bytes, %u ms)\n",
                   ir_audio_format_name(format), path, sound->length, *duration_ms);

            return sound;
        } else {
            fprintf(stderr, "[SDL3 Audio] Failed to decode %s: %s\n",
                    ir_audio_format_name(format), path);
            free(sound);
            return NULL;
        }
    }

    // For WAV, use SDL3's native loader
    if (!SDL_LoadWAV(path, &sound->spec, &sound->buffer, &sound->length)) {
        fprintf(stderr, "[SDL3 Audio] Failed to load WAV: %s - %s\n", path, SDL_GetError());
        free(sound);
        return NULL;
    }

    sound->path = strdup(path);
    sound->is_memory_loaded = false;

    // Calculate duration in milliseconds
    uint32_t samples = sound->length / (SDL_AUDIO_BITSIZE(sound->spec.format) / 8) / sound->spec.channels;
    *duration_ms = (samples * 1000) / sound->spec.freq;
    *sample_rate = sound->spec.freq;
    *channels = sound->spec.channels;

    printf("[SDL3 Audio] Loaded WAV: %s (%u bytes, %u ms)\n", path, sound->length, *duration_ms);

    return sound;
}

static void* backend_load_sound_from_memory(const char* name, const void* data, size_t size,
                                           uint32_t* duration_ms, uint32_t* sample_rate, uint8_t* channels) {
    if (!data || size == 0) {
        fprintf(stderr, "[SDL3 Audio] Invalid memory data\n");
        return NULL;
    }

    SDL3Sound* sound = (SDL3Sound*)malloc(sizeof(SDL3Sound));
    if (!sound) return NULL;

    memset(sound, 0, sizeof(SDL3Sound));

    // Detect format from magic bytes
    IRAudioFormat format = ir_audio_detect_format_from_memory(data, size);

    // For OGG and MP3, use the decoder
    if (format == IR_AUDIO_FORMAT_OGG || format == IR_AUDIO_FORMAT_MP3) {
        IRDecodedAudio decoded = {0};
        if (ir_audio_decode_from_memory(data, size, format, &decoded)) {
            // Setup SDL audio spec from decoded data
            sound->spec.freq = decoded.sample_rate;
            sound->spec.channels = decoded.channels;
            sound->spec.format = SDL_AUDIO_S16;

            sound->buffer = decoded.data;  // Transfer ownership
            sound->length = decoded.data_size;
            sound->path = strdup(name);
            sound->is_memory_loaded = true;

            *duration_ms = decoded.duration_ms;
            *sample_rate = decoded.sample_rate;
            *channels = decoded.channels;

            printf("[SDL3 Audio] Loaded %s from memory: %s (%zu input bytes, %u PCM bytes, %u ms)\n",
                   ir_audio_format_name(format), name, size, sound->length, *duration_ms);

            return sound;
        } else {
            fprintf(stderr, "[SDL3 Audio] Failed to decode %s from memory: %s\n",
                    ir_audio_format_name(format), name);
            free(sound);
            return NULL;
        }
    }

    // For WAV, use SDL3's native loader
    SDL_IOStream* io = SDL_IOFromConstMem(data, (int)size);
    if (!io) {
        fprintf(stderr, "[SDL3 Audio] Failed to create memory stream: %s\n", SDL_GetError());
        free(sound);
        return NULL;
    }

    bool loaded = SDL_LoadWAV_IO(io, true, &sound->spec, &sound->buffer, &sound->length);
    if (!loaded) {
        fprintf(stderr, "[SDL3 Audio] Failed to load WAV from memory: %s\n", SDL_GetError());
        free(sound);
        return NULL;
    }

    sound->path = strdup(name);
    sound->is_memory_loaded = true;

    // Calculate duration in milliseconds
    uint32_t samples = sound->length / (SDL_AUDIO_BITSIZE(sound->spec.format) / 8) / sound->spec.channels;
    *duration_ms = (samples * 1000) / sound->spec.freq;
    *sample_rate = sound->spec.freq;
    *channels = sound->spec.channels;

    printf("[SDL3 Audio] Loaded WAV from memory: %s (%zu bytes, %u ms)\n", name, size, *duration_ms);

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

    // Destroy old stream if format doesn't match
    if (channel->stream) {
        // Check if we need to recreate the stream
        // For now, just recreate on every play to handle format changes
        SDL_DestroyAudioStream(channel->stream);
        channel->stream = NULL;
    }

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
    channel->pan = pan;
    channel->position = 0;

    // Queue audio with volume and pan applied
    if (!queue_audio_processed(channel->stream, sound->buffer, sound->length,
                               &sound->spec, volume, pan)) {
        fprintf(stderr, "[SDL3 Audio] Failed to queue audio: %s\n", SDL_GetError());
        channel->playing = false;
        return IR_INVALID_CHANNEL;
    }

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

    // Queue music data with volume and pan applied
    if (!queue_audio_processed(g_sdl3_audio.music_stream, music->buffer, music->length,
                               &music->spec, g_sdl3_audio.music_volume, g_sdl3_audio.music_pan)) {
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
    if (channel_id == IR_INVALID_CHANNEL) return;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];

    // Store new volume
    channel->volume = volume;

    // If channel is playing, we need to re-queue with new volume
    // For simplicity, we just update the stored value - the new volume
    // will apply on next play or loop iteration
}

static void backend_set_channel_pan(IRChannelID channel_id, float pan) {
    if (channel_id == IR_INVALID_CHANNEL) return;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];
    channel->pan = pan;
}

static void backend_set_music_volume(float volume) {
    g_sdl3_audio.music_volume = volume;

    // Re-queue music with new volume if playing
    if (g_sdl3_audio.music_playing && g_sdl3_audio.current_music && g_sdl3_audio.music_stream) {
        SDL_ClearAudioStream(g_sdl3_audio.music_stream);
        queue_audio_processed(g_sdl3_audio.music_stream,
                            g_sdl3_audio.current_music->buffer,
                            g_sdl3_audio.current_music->length,
                            &g_sdl3_audio.current_music->spec,
                            g_sdl3_audio.music_volume,
                            g_sdl3_audio.music_pan);
    }
}

static void backend_set_music_pan(float pan) {
    g_sdl3_audio.music_pan = pan;

    // Re-queue music with new pan if playing
    if (g_sdl3_audio.music_playing && g_sdl3_audio.current_music && g_sdl3_audio.music_stream) {
        SDL_ClearAudioStream(g_sdl3_audio.music_stream);
        queue_audio_processed(g_sdl3_audio.music_stream,
                            g_sdl3_audio.current_music->buffer,
                            g_sdl3_audio.current_music->length,
                            &g_sdl3_audio.current_music->spec,
                            g_sdl3_audio.music_volume,
                            g_sdl3_audio.music_pan);
    }
}

static void backend_set_master_volume(float volume) {
    g_sdl3_audio.master_volume = volume;

    // Re-queue all active channels with new master volume
    for (int i = 0; i < IR_MAX_CHANNELS; i++) {
        SDL3Channel* channel = &g_sdl3_audio.channels[i];
        if (channel->playing && channel->sound && channel->stream) {
            // For simplicity, just update the stored value
            // Full implementation would re-queue with new volume
        }
    }

    // Re-queue music with new master volume
    if (g_sdl3_audio.music_playing && g_sdl3_audio.current_music && g_sdl3_audio.music_stream) {
        SDL_ClearAudioStream(g_sdl3_audio.music_stream);
        queue_audio_processed(g_sdl3_audio.music_stream,
                            g_sdl3_audio.current_music->buffer,
                            g_sdl3_audio.current_music->length,
                            &g_sdl3_audio.current_music->spec,
                            g_sdl3_audio.music_volume,
                            g_sdl3_audio.music_pan);
    }
}

static IRPlaybackState backend_get_channel_state(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return IR_AUDIO_STOPPED;

    int idx = channel_id - 1;
    if (idx < 0 || idx >= IR_MAX_CHANNELS) return IR_AUDIO_STOPPED;

    SDL3Channel* channel = &g_sdl3_audio.channels[idx];

    if (!channel->playing) return IR_AUDIO_STOPPED;

    // Check if stream is empty (finished playing)
    if (channel->stream) {
        int queued = SDL_GetAudioStreamQueued(channel->stream);
        if (queued == 0) {
            if (channel->loop && channel->sound) {
                // Re-queue for loop
                queue_audio_processed(channel->stream,
                                    channel->sound->buffer,
                                    channel->sound->length,
                                    &channel->sound->spec,
                                    channel->volume,
                                    channel->pan);
            } else {
                channel->playing = false;
                return IR_AUDIO_STOPPED;
            }
        }
    }

    return IR_AUDIO_PLAYING;
}

static IRPlaybackState backend_get_music_state(void) {
    if (!g_sdl3_audio.music_playing) return IR_AUDIO_STOPPED;

    // Check if music stream is empty
    if (g_sdl3_audio.music_stream) {
        int queued = SDL_GetAudioStreamQueued(g_sdl3_audio.music_stream);
        if (queued == 0) {
            if (g_sdl3_audio.music_loop && g_sdl3_audio.current_music) {
                // Re-queue for loop
                queue_audio_processed(g_sdl3_audio.music_stream,
                                    g_sdl3_audio.current_music->buffer,
                                    g_sdl3_audio.current_music->length,
                                    &g_sdl3_audio.current_music->spec,
                                    g_sdl3_audio.music_volume,
                                    g_sdl3_audio.music_pan);
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
    .load_sound_from_memory = backend_load_sound_from_memory,
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
