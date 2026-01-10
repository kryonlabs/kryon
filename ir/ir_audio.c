/*
 * Kryon Audio System Implementation
 *
 * Core audio management with plugin architecture
 */

#define _POSIX_C_SOURCE 199309L

#include "ir_audio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Global State
// ============================================================================

static struct {
    bool initialized;

    // Backend
    IRAudioBackend backend_type;
    IRAudioBackendPlugin backend;

    // Sound registry
    IRSound sounds[IR_MAX_SOUNDS];
    uint32_t sound_count;

    // Music registry
    IRMusic music[IR_MAX_MUSIC];
    uint32_t music_count;
    IRMusicID current_music;

    // Channels
    IRChannel channels[IR_MAX_CHANNELS];
    uint32_t next_channel_id;

    // Volume control
    float master_volume;
    float music_volume;

    // Statistics
    uint32_t total_memory_bytes;
    uint32_t sounds_played;
    uint32_t music_played;

    // Configuration
    IRAudioConfig config;
} g_audio_system = {0};

// ============================================================================
// Helper Functions
// ============================================================================

// Get current time in milliseconds
static uint64_t get_time_ms(void) {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#else
    return (uint64_t)GetTickCount64();
#endif
}

// Find sound by ID
static IRSound* find_sound(IRSoundID sound_id) {
    if (sound_id == IR_INVALID_SOUND) return NULL;

    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (g_audio_system.sounds[i].id == sound_id) {
            return &g_audio_system.sounds[i];
        }
    }
    return NULL;
}

// Find music by ID
static IRMusic* find_music(IRMusicID music_id) {
    if (music_id == IR_INVALID_MUSIC) return NULL;

    for (uint32_t i = 0; i < g_audio_system.music_count; i++) {
        if (g_audio_system.music[i].id == music_id) {
            return &g_audio_system.music[i];
        }
    }
    return NULL;
}

// Find free channel
static IRChannel* find_free_channel(void) {
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].state == IR_AUDIO_STOPPED) {
            return &g_audio_system.channels[i];
        }
    }
    return NULL;
}

// Find channel by ID
static IRChannel* find_channel(IRChannelID channel_id) {
    if (channel_id == IR_INVALID_CHANNEL) return NULL;

    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].id == channel_id) {
            return &g_audio_system.channels[i];
        }
    }
    return NULL;
}

// ============================================================================
// Initialization
// ============================================================================

bool ir_audio_init(void) {
    IRAudioConfig default_config = {
        .backend = IR_AUDIO_BACKEND_SDL3,
        .sample_rate = 44100,
        .buffer_size = 4096,
        .channels = 2,
        .enable_spatial = false
    };

    return ir_audio_init_ex(&default_config);
}

bool ir_audio_init_ex(IRAudioConfig* config) {
    if (g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Already initialized\n");
        return false;
    }

    memset(&g_audio_system, 0, sizeof(g_audio_system));

    // Copy configuration
    g_audio_system.config = *config;
    g_audio_system.backend_type = config->backend;

    // Set default volumes
    g_audio_system.master_volume = 1.0f;
    g_audio_system.music_volume = 1.0f;

    // Initialize channels
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        g_audio_system.channels[i].id = i + 1;
        g_audio_system.channels[i].state = IR_AUDIO_STOPPED;
        g_audio_system.channels[i].volume = 1.0f;
        g_audio_system.channels[i].pan = 0.0f;
    }
    g_audio_system.next_channel_id = IR_MAX_CHANNELS + 1;

    // Backend will be initialized by register_backend or SDL3 backend
    g_audio_system.initialized = true;

    printf("[Audio] System initialized (backend: %d, rate: %u Hz)\n",
           config->backend, config->sample_rate);

    return true;
}

void ir_audio_shutdown(void) {
    if (!g_audio_system.initialized) return;

    // Stop all sounds and music
    ir_audio_stop_all_sounds();
    ir_audio_stop_music();

    // Unload all sounds
    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (g_audio_system.sounds[i].loaded && g_audio_system.backend.unload_sound) {
            g_audio_system.backend.unload_sound(g_audio_system.sounds[i].backend_data);
        }
    }

    // Unload all music
    for (uint32_t i = 0; i < g_audio_system.music_count; i++) {
        if (g_audio_system.music[i].loaded && g_audio_system.backend.unload_music) {
            g_audio_system.backend.unload_music(g_audio_system.music[i].backend_data);
        }
    }

    // Shutdown backend
    if (g_audio_system.backend.shutdown) {
        g_audio_system.backend.shutdown();
    }

    memset(&g_audio_system, 0, sizeof(g_audio_system));

    printf("[Audio] System shutdown\n");
}

void ir_audio_update(float dt) {
    if (!g_audio_system.initialized) return;

    // Update backend
    if (g_audio_system.backend.update) {
        g_audio_system.backend.update(dt);
    }

    // Update channel states (poll backend for finished sounds)
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        IRChannel* channel = &g_audio_system.channels[i];
        if (channel->state == IR_AUDIO_PLAYING) {
            if (g_audio_system.backend.get_channel_state) {
                channel->state = g_audio_system.backend.get_channel_state(channel->id);
            }
        }
    }
}

bool ir_audio_register_backend(IRAudioBackend backend, IRAudioBackendPlugin* plugin) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return false;
    }

    if (!plugin) {
        fprintf(stderr, "[Audio] Invalid backend plugin\n");
        return false;
    }

    g_audio_system.backend_type = backend;
    g_audio_system.backend = *plugin;

    // Initialize backend
    if (g_audio_system.backend.init) {
        if (!g_audio_system.backend.init(&g_audio_system.config)) {
            fprintf(stderr, "[Audio] Backend initialization failed\n");
            return false;
        }
    }

    printf("[Audio] Registered backend: %d\n", backend);
    return true;
}

// ============================================================================
// Sound Loading
// ============================================================================

IRSoundID ir_audio_load_sound(const char* path) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return IR_INVALID_SOUND;
    }

    if (g_audio_system.sound_count >= IR_MAX_SOUNDS) {
        fprintf(stderr, "[Audio] Max sounds reached\n");
        return IR_INVALID_SOUND;
    }

    // Check if already loaded
    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (strcmp(g_audio_system.sounds[i].path, path) == 0) {
            printf("[Audio] Sound already loaded: %s\n", path);
            return g_audio_system.sounds[i].id;
        }
    }

    // Allocate new sound
    IRSound* sound = &g_audio_system.sounds[g_audio_system.sound_count];
    memset(sound, 0, sizeof(IRSound));

    sound->id = g_audio_system.sound_count + 1;
    strncpy(sound->path, path, sizeof(sound->path) - 1);

    // Load via backend
    if (g_audio_system.backend.load_sound) {
        sound->backend_data = g_audio_system.backend.load_sound(
            path,
            &sound->duration_ms,
            &sound->sample_rate,
            &sound->channels
        );

        if (!sound->backend_data) {
            fprintf(stderr, "[Audio] Failed to load sound: %s\n", path);
            return IR_INVALID_SOUND;
        }
    } else {
        fprintf(stderr, "[Audio] No backend registered\n");
        return IR_INVALID_SOUND;
    }

    sound->loaded = true;
    g_audio_system.sound_count++;

    printf("[Audio] Loaded sound: %s (id=%u, %u ms, %u Hz, %u ch)\n",
           path, sound->id, sound->duration_ms, sound->sample_rate, sound->channels);

    return sound->id;
}

IRSoundID ir_audio_load_sound_from_memory(const char* name, const void* data, size_t size) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return IR_INVALID_SOUND;
    }

    if (!name || !data || size == 0) {
        fprintf(stderr, "[Audio] Invalid parameters for memory loading\n");
        return IR_INVALID_SOUND;
    }

    if (g_audio_system.sound_count >= IR_MAX_SOUNDS) {
        fprintf(stderr, "[Audio] Max sounds reached\n");
        return IR_INVALID_SOUND;
    }

    // Check if already loaded by name
    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (strcmp(g_audio_system.sounds[i].path, name) == 0) {
            printf("[Audio] Sound already loaded: %s\n", name);
            return g_audio_system.sounds[i].id;
        }
    }

    // Allocate new sound
    IRSound* sound = &g_audio_system.sounds[g_audio_system.sound_count];
    memset(sound, 0, sizeof(IRSound));

    sound->id = g_audio_system.sound_count + 1;
    strncpy(sound->path, name, sizeof(sound->path) - 1);
    // Mark as memory-loaded with special prefix
    char prefixed_name[512];
    snprintf(prefixed_name, sizeof(prefixed_name), "@memory:%s", name);

    // Load via backend
    if (g_audio_system.backend.load_sound_from_memory) {
        sound->backend_data = g_audio_system.backend.load_sound_from_memory(
            prefixed_name,
            data,
            size,
            &sound->duration_ms,
            &sound->sample_rate,
            &sound->channels
        );

        if (!sound->backend_data) {
            fprintf(stderr, "[Audio] Failed to load sound from memory: %s (%zu bytes)\n", name, size);
            return IR_INVALID_SOUND;
        }
    } else {
        // Fallback: try to use the file loader if backend doesn't support memory loading
        fprintf(stderr, "[Audio] Backend doesn't support memory loading\n");
        return IR_INVALID_SOUND;
    }

    sound->loaded = true;
    g_audio_system.sound_count++;

    // Estimate memory usage
    g_audio_system.total_memory_bytes += (uint32_t)size;

    printf("[Audio] Loaded sound from memory: %s (id=%u, %zu bytes, %u ms, %u Hz, %u ch)\n",
           name, sound->id, size, sound->duration_ms, sound->sample_rate, sound->channels);

    return sound->id;
}

void ir_audio_unload_sound(IRSoundID sound_id) {
    IRSound* sound = find_sound(sound_id);
    if (!sound || !sound->loaded) return;

    // Stop all channels playing this sound
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].sound_id == sound_id) {
            ir_audio_stop_channel(g_audio_system.channels[i].id);
        }
    }

    // Unload via backend
    if (g_audio_system.backend.unload_sound) {
        g_audio_system.backend.unload_sound(sound->backend_data);
    }

    sound->loaded = false;
    printf("[Audio] Unloaded sound: %s\n", sound->path);
}

IRSound* ir_audio_get_sound(IRSoundID sound_id) {
    return find_sound(sound_id);
}

// ============================================================================
// Music Loading
// ============================================================================

IRMusicID ir_audio_load_music(const char* path) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return IR_INVALID_MUSIC;
    }

    if (g_audio_system.music_count >= IR_MAX_MUSIC) {
        fprintf(stderr, "[Audio] Max music reached\n");
        return IR_INVALID_MUSIC;
    }

    // Check if already loaded
    for (uint32_t i = 0; i < g_audio_system.music_count; i++) {
        if (strcmp(g_audio_system.music[i].path, path) == 0) {
            printf("[Audio] Music already loaded: %s\n", path);
            return g_audio_system.music[i].id;
        }
    }

    // Allocate new music
    IRMusic* music = &g_audio_system.music[g_audio_system.music_count];
    memset(music, 0, sizeof(IRMusic));

    music->id = g_audio_system.music_count + 1;
    strncpy(music->path, path, sizeof(music->path) - 1);

    // Load via backend
    if (g_audio_system.backend.load_music) {
        music->backend_data = g_audio_system.backend.load_music(path, &music->duration_ms);

        if (!music->backend_data) {
            fprintf(stderr, "[Audio] Failed to load music: %s\n", path);
            return IR_INVALID_MUSIC;
        }
    } else {
        fprintf(stderr, "[Audio] No backend registered\n");
        return IR_INVALID_MUSIC;
    }

    music->loaded = true;
    g_audio_system.music_count++;

    printf("[Audio] Loaded music: %s (id=%u, %u ms)\n", path, music->id, music->duration_ms);

    return music->id;
}

void ir_audio_unload_music(IRMusicID music_id) {
    IRMusic* music = find_music(music_id);
    if (!music || !music->loaded) return;

    // Stop music if playing
    if (g_audio_system.current_music == music_id) {
        ir_audio_stop_music();
    }

    // Unload via backend
    if (g_audio_system.backend.unload_music) {
        g_audio_system.backend.unload_music(music->backend_data);
    }

    music->loaded = false;
    printf("[Audio] Unloaded music: %s\n", music->path);
}

IRMusic* ir_audio_get_music(IRMusicID music_id) {
    return find_music(music_id);
}

// ============================================================================
// Sound Playback
// ============================================================================

IRChannelID ir_audio_play_sound(IRSoundID sound_id, float volume, float pan) {
    return ir_audio_play_sound_ex(sound_id, volume, pan, false);
}

IRChannelID ir_audio_play_sound_ex(IRSoundID sound_id, float volume, float pan, bool loop) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return IR_INVALID_CHANNEL;
    }

    IRSound* sound = find_sound(sound_id);
    if (!sound || !sound->loaded) {
        fprintf(stderr, "[Audio] Invalid sound ID: %u\n", sound_id);
        return IR_INVALID_CHANNEL;
    }

    // Find free channel
    IRChannel* channel = find_free_channel();
    if (!channel) {
        fprintf(stderr, "[Audio] No free channels\n");
        return IR_INVALID_CHANNEL;
    }

    // Setup channel
    channel->sound_id = sound_id;
    channel->state = IR_AUDIO_PLAYING;
    channel->volume = volume;
    channel->pan = pan;
    channel->loop = loop;
    channel->start_time_ms = get_time_ms();

    // Play via backend
    if (g_audio_system.backend.play_sound) {
        IRChannelID backend_id = g_audio_system.backend.play_sound(
            sound->backend_data,
            volume * g_audio_system.master_volume,
            pan,
            loop
        );

        if (backend_id == IR_INVALID_CHANNEL) {
            channel->state = IR_AUDIO_STOPPED;
            return IR_INVALID_CHANNEL;
        }
    }

    g_audio_system.sounds_played++;

    return channel->id;
}

void ir_audio_stop_channel(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel) return;

    if (g_audio_system.backend.stop_channel) {
        g_audio_system.backend.stop_channel(channel_id);
    }

    channel->state = IR_AUDIO_STOPPED;
    channel->sound_id = IR_INVALID_SOUND;
}

void ir_audio_pause_channel(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel || channel->state != IR_AUDIO_PLAYING) return;

    if (g_audio_system.backend.pause_channel) {
        g_audio_system.backend.pause_channel(channel_id);
    }

    channel->state = IR_AUDIO_PAUSED;
}

void ir_audio_resume_channel(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel || channel->state != IR_AUDIO_PAUSED) return;

    if (g_audio_system.backend.resume_channel) {
        g_audio_system.backend.resume_channel(channel_id);
    }

    channel->state = IR_AUDIO_PLAYING;
}

void ir_audio_stop_all_sounds(void) {
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].state != IR_AUDIO_STOPPED) {
            ir_audio_stop_channel(g_audio_system.channels[i].id);
        }
    }
}

// ============================================================================
// Music Playback
// ============================================================================

void ir_audio_play_music(IRMusicID music_id, bool loop) {
    if (!g_audio_system.initialized) {
        fprintf(stderr, "[Audio] Not initialized\n");
        return;
    }

    IRMusic* music = find_music(music_id);
    if (!music || !music->loaded) {
        fprintf(stderr, "[Audio] Invalid music ID: %u\n", music_id);
        return;
    }

    // Stop current music
    if (g_audio_system.current_music != IR_INVALID_MUSIC) {
        ir_audio_stop_music();
    }

    // Play via backend
    if (g_audio_system.backend.play_music) {
        g_audio_system.backend.play_music(music->backend_data, loop);
    }

    music->looping = loop;
    g_audio_system.current_music = music_id;
    g_audio_system.music_played++;

    printf("[Audio] Playing music: %s (loop=%d)\n", music->path, loop);
}

void ir_audio_stop_music(void) {
    if (g_audio_system.current_music == IR_INVALID_MUSIC) return;

    if (g_audio_system.backend.stop_music) {
        g_audio_system.backend.stop_music();
    }

    g_audio_system.current_music = IR_INVALID_MUSIC;
}

void ir_audio_pause_music(void) {
    if (g_audio_system.backend.pause_music) {
        g_audio_system.backend.pause_music();
    }
}

void ir_audio_resume_music(void) {
    if (g_audio_system.backend.resume_music) {
        g_audio_system.backend.resume_music();
    }
}

bool ir_audio_is_music_playing(void) {
    if (g_audio_system.current_music == IR_INVALID_MUSIC) return false;

    if (g_audio_system.backend.get_music_state) {
        return g_audio_system.backend.get_music_state() == IR_AUDIO_PLAYING;
    }

    return false;
}

// ============================================================================
// Volume Control
// ============================================================================

void ir_audio_set_channel_volume(IRChannelID channel_id, float volume) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel) return;

    channel->volume = volume;

    if (g_audio_system.backend.set_channel_volume) {
        g_audio_system.backend.set_channel_volume(channel_id, volume * g_audio_system.master_volume);
    }
}

void ir_audio_set_music_volume(float volume) {
    g_audio_system.music_volume = volume;

    if (g_audio_system.backend.set_music_volume) {
        g_audio_system.backend.set_music_volume(volume * g_audio_system.master_volume);
    }
}

void ir_audio_set_master_volume(float volume) {
    g_audio_system.master_volume = volume;

    if (g_audio_system.backend.set_master_volume) {
        g_audio_system.backend.set_master_volume(volume);
    }
}

float ir_audio_get_channel_volume(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    return channel ? channel->volume : 0.0f;
}

float ir_audio_get_music_volume(void) {
    return g_audio_system.music_volume;
}

float ir_audio_get_master_volume(void) {
    return g_audio_system.master_volume;
}

// ============================================================================
// Channel Management
// ============================================================================

IRAudioState ir_audio_get_channel_state(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    return channel ? channel->state : IR_AUDIO_STOPPED;
}

IRAudioState ir_audio_get_music_state(void) {
    if (g_audio_system.backend.get_music_state) {
        return g_audio_system.backend.get_music_state();
    }
    return IR_AUDIO_STOPPED;
}

uint32_t ir_audio_get_active_channels(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].state != IR_AUDIO_STOPPED) {
            count++;
        }
    }
    return count;
}

IRChannelID ir_audio_find_channel(IRSoundID sound_id) {
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (g_audio_system.channels[i].sound_id == sound_id &&
            g_audio_system.channels[i].state == IR_AUDIO_PLAYING) {
            return g_audio_system.channels[i].id;
        }
    }
    return IR_INVALID_CHANNEL;
}

// ============================================================================
// Spatial Audio (Stubs - backend-dependent)
// ============================================================================

void ir_audio_set_listener_position(float x, float y, float z) {
    (void)x; (void)y; (void)z;
    // TODO: Implement spatial audio
}

void ir_audio_set_listener_orientation(float forward_x, float forward_y, float forward_z,
                                        float up_x, float up_y, float up_z) {
    (void)forward_x; (void)forward_y; (void)forward_z;
    (void)up_x; (void)up_y; (void)up_z;
    // TODO: Implement spatial audio
}

void ir_audio_set_channel_position(IRChannelID channel_id, float x, float y, float z) {
    (void)channel_id; (void)x; (void)y; (void)z;
    // TODO: Implement spatial audio
}

void ir_audio_set_channel_velocity(IRChannelID channel_id, float vx, float vy, float vz) {
    (void)channel_id; (void)vx; (void)vy; (void)vz;
    // TODO: Implement spatial audio
}

void ir_audio_set_channel_attenuation(IRChannelID channel_id, float min_distance, float max_distance) {
    (void)channel_id; (void)min_distance; (void)max_distance;
    // TODO: Implement spatial audio
}

// ============================================================================
// Statistics
// ============================================================================

void ir_audio_get_stats(IRAudioStats* stats) {
    if (!stats) return;

    stats->loaded_sounds = g_audio_system.sound_count;
    stats->loaded_music = g_audio_system.music_count;
    stats->active_channels = ir_audio_get_active_channels();
    stats->total_memory_bytes = g_audio_system.total_memory_bytes;
    stats->backend = g_audio_system.backend_type;
    stats->sample_rate = g_audio_system.config.sample_rate;
}

void ir_audio_print_stats(void) {
    IRAudioStats stats;
    ir_audio_get_stats(&stats);

    printf("\n=== Audio System Statistics ===\n");
    printf("Backend: %d\n", stats.backend);
    printf("Sample Rate: %u Hz\n", stats.sample_rate);
    printf("Loaded Sounds: %u / %u\n", stats.loaded_sounds, IR_MAX_SOUNDS);
    printf("Loaded Music: %u / %u\n", stats.loaded_music, IR_MAX_MUSIC);
    printf("Active Channels: %u / %u\n", stats.active_channels, IR_MAX_CHANNELS);
    printf("Total Sounds Played: %u\n", g_audio_system.sounds_played);
    printf("Total Music Played: %u\n", g_audio_system.music_played);
    printf("Memory Usage: %u bytes\n", stats.total_memory_bytes);
    printf("Master Volume: %.2f\n", g_audio_system.master_volume);
    printf("Music Volume: %.2f\n", g_audio_system.music_volume);
    printf("===============================\n\n");
}
