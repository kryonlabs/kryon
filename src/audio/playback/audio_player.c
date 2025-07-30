/**
 * @file audio_player.c
 * @brief Kryon Audio Player Implementation
 */

#include "internal/audio.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

// =============================================================================
// AUDIO FORMATS AND BUFFERS
// =============================================================================

typedef struct KryonAudioBuffer {
    void* data;
    size_t size;
    size_t capacity;
    KryonAudioFormat format;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    double duration;
    bool is_loaded;
} KryonAudioBuffer;

typedef struct KryonAudioSource {
    uint32_t id;
    KryonAudioBuffer* buffer;
    
    // Playback state
    bool is_playing;
    bool is_paused;
    bool is_looping;
    size_t playback_position;
    
    // Audio properties
    float volume;
    float pitch;
    float pan; // -1.0 (left) to 1.0 (right)
    
    // Fade effects
    float fade_volume;
    double fade_start_time;
    double fade_duration;
    bool is_fading;
    
    // 3D audio properties
    float position[3];
    float velocity[3];
    float attenuation;
    float min_distance;
    float max_distance;
    
    struct KryonAudioSource* next;
} KryonAudioSource;

typedef struct {
    // Audio sources
    KryonAudioSource* sources;
    uint32_t next_source_id;
    size_t active_sources;
    
    // Global audio state
    float master_volume;
    bool muted;
    
    // Audio device
    KryonAudioDevice* device;
    
    // Audio thread
    pthread_t audio_thread;
    bool audio_thread_running;
    
    // Listener properties (for 3D audio)
    float listener_position[3];
    float listener_velocity[3];
    float listener_orientation[6]; // forward + up vectors
    
} KryonAudioPlayer;

static KryonAudioPlayer g_audio_player = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static float calculate_3d_attenuation(KryonAudioSource* source) {
    if (!source) return 1.0f;
    
    // Calculate distance
    float dx = source->position[0] - g_audio_player.listener_position[0];
    float dy = source->position[1] - g_audio_player.listener_position[1];
    float dz = source->position[2] - g_audio_player.listener_position[2];
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    
    if (distance <= source->min_distance) {
        return 1.0f;
    }
    
    if (distance >= source->max_distance) {
        return 0.0f;
    }
    
    // Linear attenuation
    float attenuation = 1.0f - (distance - source->min_distance) / 
                              (source->max_distance - source->min_distance);
    
    return attenuation * source->attenuation;
}

static float apply_doppler_effect(KryonAudioSource* source) {
    if (!source) return 1.0f;
    
    // Calculate relative velocity
    float rel_velocity[3];
    for (int i = 0; i < 3; i++) {
        rel_velocity[i] = source->velocity[i] - g_audio_player.listener_velocity[i];
    }
    
    // Calculate direction vector
    float direction[3];
    for (int i = 0; i < 3; i++) {
        direction[i] = source->position[i] - g_audio_player.listener_position[i];
    }
    
    float distance = sqrtf(direction[0] * direction[0] + 
                          direction[1] * direction[1] + 
                          direction[2] * direction[2]);
    
    if (distance < 0.001f) return 1.0f;
    
    // Normalize direction
    for (int i = 0; i < 3; i++) {
        direction[i] /= distance;
    }
    
    // Calculate radial velocity (component along line of sight)
    float radial_velocity = 0.0f;
    for (int i = 0; i < 3; i++) {
        radial_velocity += rel_velocity[i] * direction[i];
    }
    
    // Doppler shift (simplified)
    const float speed_of_sound = 343.0f; // m/s
    float doppler_factor = speed_of_sound / (speed_of_sound + radial_velocity);
    
    return fmaxf(0.1f, fminf(2.0f, doppler_factor));
}

static void mix_audio_samples(void* dest, const void* src, size_t samples, 
                             float volume, KryonAudioFormat format) {
    switch (format) {
        case KRYON_AUDIO_FORMAT_S16: {
            int16_t* dst = (int16_t*)dest;
            const int16_t* s = (const int16_t*)src;
            for (size_t i = 0; i < samples; i++) {
                int32_t mixed = dst[i] + (int32_t)(s[i] * volume);
                dst[i] = (int16_t)fmaxf(-32768, fminf(32767, mixed));
            }
            break;
        }
        case KRYON_AUDIO_FORMAT_F32: {
            float* dst = (float*)dest;
            const float* s = (const float*)src;
            for (size_t i = 0; i < samples; i++) {
                dst[i] += s[i] * volume;
                dst[i] = fmaxf(-1.0f, fminf(1.0f, dst[i]));
            }
            break;
        }
        default:
            break;
    }
}

static void* audio_thread_func(void* arg) {
    (void)arg;
    
    while (g_audio_player.audio_thread_running) {
        // Process audio sources and mix them
        // This would interface with the actual audio device
        
        // For now, just sleep to simulate audio processing
        usleep(1000); // 1ms
    }
    
    return NULL;
}

// =============================================================================
// AUDIO BUFFER MANAGEMENT
// =============================================================================

static KryonAudioBuffer* create_audio_buffer(void) {
    KryonAudioBuffer* buffer = kryon_alloc(sizeof(KryonAudioBuffer));
    if (!buffer) return NULL;
    
    memset(buffer, 0, sizeof(KryonAudioBuffer));
    return buffer;
}

static void destroy_audio_buffer(KryonAudioBuffer* buffer) {
    if (!buffer) return;
    
    kryon_free(buffer->data);
    kryon_free(buffer);
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_audio_player_init(const KryonAudioConfig* config) {
    memset(&g_audio_player, 0, sizeof(g_audio_player));
    
    g_audio_player.master_volume = 1.0f;
    g_audio_player.next_source_id = 1;
    
    // Set default listener orientation (forward = -Z, up = +Y)
    g_audio_player.listener_orientation[0] = 0.0f;  // forward X
    g_audio_player.listener_orientation[1] = 0.0f;  // forward Y
    g_audio_player.listener_orientation[2] = -1.0f; // forward Z
    g_audio_player.listener_orientation[3] = 0.0f;  // up X
    g_audio_player.listener_orientation[4] = 1.0f;  // up Y
    g_audio_player.listener_orientation[5] = 0.0f;  // up Z
    
    // Initialize audio device (would be platform-specific)
    if (config && config->device_name) {
        // Open specific device
        printf("Audio: Initializing device '%s'\n", config->device_name);
    } else {
        // Open default device
        printf("Audio: Initializing default device\n");
    }
    
    // Start audio thread
    g_audio_player.audio_thread_running = true;
    if (pthread_create(&g_audio_player.audio_thread, NULL, audio_thread_func, NULL) != 0) {
        return false;
    }
    
    return true;
}

void kryon_audio_player_shutdown(void) {
    // Stop audio thread
    g_audio_player.audio_thread_running = false;
    pthread_join(g_audio_player.audio_thread, NULL);
    
    // Clean up all sources
    KryonAudioSource* current = g_audio_player.sources;
    while (current) {
        KryonAudioSource* next = current->next;
        destroy_audio_buffer(current->buffer);
        kryon_free(current);
        current = next;
    }
    
    // Close audio device
    if (g_audio_player.device) {
        // Platform-specific device cleanup
        g_audio_player.device = NULL;
    }
    
    memset(&g_audio_player, 0, sizeof(g_audio_player));
}

KryonAudioSourceId kryon_audio_load_from_file(const char* file_path) {
    if (!file_path) return 0;
    
    // Create audio buffer
    KryonAudioBuffer* buffer = create_audio_buffer();
    if (!buffer) return 0;
    
    // Load audio file (simplified - would use actual audio library)
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    // Allocate buffer
    buffer->data = kryon_alloc(file_size);
    if (!buffer->data) {
        fclose(file);
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    // Read file data
    size_t bytes_read = fread(buffer->data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    buffer->size = file_size;
    buffer->capacity = file_size;
    
    // Set default audio properties (would be parsed from file header)
    buffer->format = KRYON_AUDIO_FORMAT_S16;
    buffer->sample_rate = 44100;
    buffer->channels = 2;
    buffer->bits_per_sample = 16;
    buffer->duration = (double)file_size / (buffer->sample_rate * buffer->channels * 2);
    buffer->is_loaded = true;
    
    // Create audio source
    KryonAudioSource* source = kryon_alloc(sizeof(KryonAudioSource));
    if (!source) {
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    memset(source, 0, sizeof(KryonAudioSource));
    source->id = g_audio_player.next_source_id++;
    source->buffer = buffer;
    source->volume = 1.0f;
    source->pitch = 1.0f;
    source->pan = 0.0f;
    source->fade_volume = 1.0f;
    source->attenuation = 1.0f;
    source->min_distance = 1.0f;
    source->max_distance = 100.0f;
    
    // Add to sources list
    source->next = g_audio_player.sources;
    g_audio_player.sources = source;
    g_audio_player.active_sources++;
    
    return source->id;
}

KryonAudioSourceId kryon_audio_load_from_memory(const void* data, size_t size, 
                                                KryonAudioFormat format, uint32_t sample_rate, 
                                                uint16_t channels) {
    if (!data || size == 0) return 0;
    
    // Create audio buffer
    KryonAudioBuffer* buffer = create_audio_buffer();
    if (!buffer) return 0;
    
    // Copy data
    buffer->data = kryon_alloc(size);
    if (!buffer->data) {
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    memcpy(buffer->data, data, size);
    buffer->size = size;
    buffer->capacity = size;
    buffer->format = format;
    buffer->sample_rate = sample_rate;
    buffer->channels = channels;
    buffer->bits_per_sample = (format == KRYON_AUDIO_FORMAT_F32) ? 32 : 16;
    
    // Calculate duration
    size_t sample_size = (buffer->bits_per_sample / 8) * channels;
    size_t total_samples = size / sample_size;
    buffer->duration = (double)total_samples / sample_rate;
    buffer->is_loaded = true;
    
    // Create audio source
    KryonAudioSource* source = kryon_alloc(sizeof(KryonAudioSource));
    if (!source) {
        destroy_audio_buffer(buffer);
        return 0;
    }
    
    memset(source, 0, sizeof(KryonAudioSource));
    source->id = g_audio_player.next_source_id++;
    source->buffer = buffer;
    source->volume = 1.0f;
    source->pitch = 1.0f;
    source->pan = 0.0f;
    source->fade_volume = 1.0f;
    source->attenuation = 1.0f;
    source->min_distance = 1.0f;
    source->max_distance = 100.0f;
    
    // Add to sources list
    source->next = g_audio_player.sources;
    g_audio_player.sources = source;
    g_audio_player.active_sources++;
    
    return source->id;
}

static KryonAudioSource* find_source(KryonAudioSourceId source_id) {
    KryonAudioSource* current = g_audio_player.sources;
    while (current) {
        if (current->id == source_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

bool kryon_audio_play(KryonAudioSourceId source_id) {
    KryonAudioSource* source = find_source(source_id);
    if (!source || !source->buffer || !source->buffer->is_loaded) {
        return false;
    }
    
    source->is_playing = true;
    source->is_paused = false;
    source->playback_position = 0;
    
    return true;
}

bool kryon_audio_pause(KryonAudioSourceId source_id) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->is_paused = true;
    return true;
}

bool kryon_audio_resume(KryonAudioSourceId source_id) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->is_paused = false;
    return true;
}

bool kryon_audio_stop(KryonAudioSourceId source_id) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->is_playing = false;
    source->is_paused = false;
    source->playback_position = 0;
    
    return true;
}

bool kryon_audio_set_volume(KryonAudioSourceId source_id, float volume) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->volume = fmaxf(0.0f, fminf(1.0f, volume));
    return true;
}

bool kryon_audio_set_pitch(KryonAudioSourceId source_id, float pitch) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->pitch = fmaxf(0.1f, fminf(4.0f, pitch));
    return true;
}

bool kryon_audio_set_pan(KryonAudioSourceId source_id, float pan) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->pan = fmaxf(-1.0f, fminf(1.0f, pan));
    return true;
}

bool kryon_audio_set_looping(KryonAudioSourceId source_id, bool looping) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->is_looping = looping;
    return true;
}

bool kryon_audio_fade_in(KryonAudioSourceId source_id, float duration) {
    KryonAudioSource* source = find_source(source_id);
    if (!source || duration <= 0.0f) return false;
    
    source->fade_volume = 0.0f;
    source->fade_start_time = kryon_platform_get_time();
    source->fade_duration = duration;
    source->is_fading = true;
    
    kryon_audio_play(source_id);
    return true;
}

bool kryon_audio_fade_out(KryonAudioSourceId source_id, float duration) {
    KryonAudioSource* source = find_source(source_id);
    if (!source || duration <= 0.0f) return false;
    
    source->fade_volume = source->volume;
    source->fade_start_time = kryon_platform_get_time();
    source->fade_duration = duration;
    source->is_fading = true;
    
    return true;
}

bool kryon_audio_set_position_3d(KryonAudioSourceId source_id, float x, float y, float z) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->position[0] = x;
    source->position[1] = y;
    source->position[2] = z;
    
    return true;
}

bool kryon_audio_set_velocity_3d(KryonAudioSourceId source_id, float x, float y, float z) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->velocity[0] = x;
    source->velocity[1] = y;
    source->velocity[2] = z;
    
    return true;
}

bool kryon_audio_set_attenuation(KryonAudioSourceId source_id, float attenuation, 
                                 float min_distance, float max_distance) {
    KryonAudioSource* source = find_source(source_id);
    if (!source) return false;
    
    source->attenuation = fmaxf(0.0f, fminf(1.0f, attenuation));
    source->min_distance = fmaxf(0.1f, min_distance);
    source->max_distance = fmaxf(source->min_distance, max_distance);
    
    return true;
}

void kryon_audio_unload(KryonAudioSourceId source_id) {
    KryonAudioSource* current = g_audio_player.sources;
    KryonAudioSource* prev = NULL;
    
    while (current) {
        if (current->id == source_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_audio_player.sources = current->next;
            }
            
            destroy_audio_buffer(current->buffer);
            kryon_free(current);
            g_audio_player.active_sources--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

void kryon_audio_set_master_volume(float volume) {
    g_audio_player.master_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

float kryon_audio_get_master_volume(void) {
    return g_audio_player.master_volume;
}

void kryon_audio_set_muted(bool muted) {
    g_audio_player.muted = muted;
}

bool kryon_audio_is_muted(void) {
    return g_audio_player.muted;
}

void kryon_audio_set_listener_position(float x, float y, float z) {
    g_audio_player.listener_position[0] = x;
    g_audio_player.listener_position[1] = y;
    g_audio_player.listener_position[2] = z;
}

void kryon_audio_set_listener_velocity(float x, float y, float z) {
    g_audio_player.listener_velocity[0] = x;
    g_audio_player.listener_velocity[1] = y;
    g_audio_player.listener_velocity[2] = z;
}

void kryon_audio_set_listener_orientation(float forward_x, float forward_y, float forward_z,
                                         float up_x, float up_y, float up_z) {
    g_audio_player.listener_orientation[0] = forward_x;
    g_audio_player.listener_orientation[1] = forward_y;
    g_audio_player.listener_orientation[2] = forward_z;
    g_audio_player.listener_orientation[3] = up_x;
    g_audio_player.listener_orientation[4] = up_y;
    g_audio_player.listener_orientation[5] = up_z;
}