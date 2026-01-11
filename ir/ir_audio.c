/*
 * Kryon Audio System Implementation
 *
 * Core audio management with plugin architecture
 * Now with per-instance audio state support
 */

#define _POSIX_C_SOURCE 199309L

#include "ir_audio.h"
#include "ir_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// ============================================================================
// Global State (Legacy - for backward compatibility)
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

    // Spatial audio - listener
    struct {
        float position[3];      // x, y, z
        float forward[3];       // forward vector (normalized)
        float up[3];           // up vector (normalized)
        float velocity[3];      // listener velocity (for doppler)
    } listener;

    // Statistics
    uint32_t total_memory_bytes;
    uint32_t sounds_played;
    uint32_t music_played;

    // Configuration
    IRAudioConfig config;
} g_audio_system = {0};

// Thread-local current audio state (for instance-specific audio operations)
static __thread IRAudioState* t_current_state = NULL;

// Per-instance state tracking (for lookup by instance_id)
#define IR_MAX_AUDIO_STATES 32
static IRAudioState* g_audio_states[IR_MAX_AUDIO_STATES] = {0};
static uint32_t g_audio_state_count = 0;

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

// Find sound by ID in a specific state
static IRSound* find_sound_in_state(IRAudioState* state, IRSoundID sound_id) {
    if (!state || sound_id == IR_INVALID_SOUND) return NULL;

    for (uint32_t i = 0; i < state->sound_count; i++) {
        if (state->sounds[i].id == sound_id) {
            return &state->sounds[i];
        }
    }
    return NULL;
}

// Find music by ID in a specific state
static IRMusic* find_music_in_state(IRAudioState* state, IRMusicID music_id) {
    if (!state || music_id == IR_INVALID_MUSIC) return NULL;

    for (uint32_t i = 0; i < state->music_count; i++) {
        if (state->music[i].id == music_id) {
            return &state->music[i];
        }
    }
    return NULL;
}

// Find sound by ID (uses current state or global)
static IRSound* find_sound(IRSoundID sound_id) {
    IRAudioState* state = ir_audio_get_current_state();
    if (!state) return NULL;
    return find_sound_in_state(state, sound_id);
}

// Find music by ID (uses current state or global)
static IRMusic* find_music(IRMusicID music_id) {
    IRAudioState* state = ir_audio_get_current_state();
    if (!state) return NULL;
    return find_music_in_state(state, music_id);
}

// Get the global state (instance 0)
static IRAudioState* get_global_audio_state(void) {
    // Create a wrapper that points to the global system
    static IRAudioState global_wrapper = {0};

    global_wrapper.instance_id = 0;
    global_wrapper.sound_count = g_audio_system.sound_count;
    global_wrapper.music_count = g_audio_system.music_count;
    global_wrapper.current_music = g_audio_system.current_music;
    global_wrapper.next_channel_id = g_audio_system.next_channel_id;
    global_wrapper.master_volume = g_audio_system.master_volume;
    global_wrapper.music_volume = g_audio_system.music_volume;

    // Copy arrays
    memcpy(global_wrapper.sounds, g_audio_system.sounds, sizeof(g_audio_system.sounds));
    memcpy(global_wrapper.music, g_audio_system.music, sizeof(g_audio_system.music));
    memcpy(global_wrapper.channels, g_audio_system.channels, sizeof(g_audio_system.channels));
    memcpy(global_wrapper.listener.position, g_audio_system.listener.position, sizeof(g_audio_system.listener.position));
    memcpy(global_wrapper.listener.forward, g_audio_system.listener.forward, sizeof(g_audio_system.listener.forward));
    memcpy(global_wrapper.listener.up, g_audio_system.listener.up, sizeof(g_audio_system.listener.up));
    memcpy(global_wrapper.listener.velocity, g_audio_system.listener.velocity, sizeof(g_audio_system.listener.velocity));

    return &global_wrapper;
}

// ============================================================================
// Per-Instance Audio State API
// ============================================================================

IRAudioState* ir_audio_state_create(uint32_t instance_id) {
    // Check if state already exists for this instance
    for (uint32_t i = 0; i < g_audio_state_count; i++) {
        if (g_audio_states[i] && g_audio_states[i]->instance_id == instance_id) {
            return g_audio_states[i];
        }
    }

    // Check capacity
    if (g_audio_state_count >= IR_MAX_AUDIO_STATES) {
        IR_LOG_ERROR("AUDIO", "Maximum audio states reached (%d)", IR_MAX_AUDIO_STATES);
        return NULL;
    }

    // Allocate new state
    IRAudioState* state = calloc(1, sizeof(IRAudioState));
    if (!state) {
        IR_LOG_ERROR("AUDIO", "Failed to allocate audio state");
        return NULL;
    }

    state->instance_id = instance_id;
    state->master_volume = 1.0f;
    state->music_volume = 1.0f;

    // Initialize listener orientation (default: facing -Z, up +Y)
    state->listener.position[0] = 0.0f;
    state->listener.position[1] = 0.0f;
    state->listener.position[2] = 0.0f;
    state->listener.forward[0] = 0.0f;
    state->listener.forward[1] = 0.0f;
    state->listener.forward[2] = -1.0f;
    state->listener.up[0] = 0.0f;
    state->listener.up[1] = 1.0f;
    state->listener.up[2] = 0.0f;
    state->listener.velocity[0] = 0.0f;
    state->listener.velocity[1] = 0.0f;
    state->listener.velocity[2] = 0.0f;

    // Register the state
    g_audio_states[g_audio_state_count++] = state;

    return state;
}

void ir_audio_state_destroy(IRAudioState* state) {
    if (!state) return;

    // Unload all sounds
    for (uint32_t i = 0; i < state->sound_count; i++) {
        if (state->sounds[i].loaded && state->sounds[i].backend_data) {
            // Backend-specific unload would go here
            free(state->sounds[i].backend_data);
        }
    }

    // Unload all music
    for (uint32_t i = 0; i < state->music_count; i++) {
        if (state->music[i].loaded && state->music[i].backend_data) {
            // Backend-specific unload would go here
            free(state->music[i].backend_data);
        }
    }

    // Remove from state list
    for (uint32_t i = 0; i < g_audio_state_count; i++) {
        if (g_audio_states[i] == state) {
            // Shift remaining entries down
            for (uint32_t j = i; j < g_audio_state_count - 1; j++) {
                g_audio_states[j] = g_audio_states[j + 1];
            }
            g_audio_states[g_audio_state_count - 1] = NULL;
            g_audio_state_count--;
            break;
        }
    }

    free(state);
}

IRAudioState* ir_audio_get_current_state(void) {
    if (t_current_state) {
        return t_current_state;
    }
    // Fall back to global state if not initialized yet
    if (!g_audio_system.initialized) return NULL;
    return get_global_audio_state();
}

IRAudioState* ir_audio_set_current_state(IRAudioState* state) {
    IRAudioState* prev = t_current_state;
    t_current_state = state;
    return prev;
}

IRAudioState* ir_audio_get_state_by_instance(uint32_t instance_id) {
    if (instance_id == 0) {
        return get_global_audio_state();
    }

    for (uint32_t i = 0; i < g_audio_state_count; i++) {
        if (g_audio_states[i] && g_audio_states[i]->instance_id == instance_id) {
            return g_audio_states[i];
        }
    }
    return NULL;
}

// ============================================================================
// Explicit State API (for multi-instance support)
// ============================================================================

IRSoundID ir_audio_load_sound_state(IRAudioState* state, const char* path) {
    // Set current state and call global function
    IRAudioState* prev = ir_audio_set_current_state(state);
    // Note: This would need proper implementation for full per-instance support
    // For now, it delegates to the global implementation which uses the current state
    ir_audio_set_current_state(prev);
    return IR_INVALID_SOUND;  // TODO: Implement properly
}

IRChannelID ir_audio_play_sound_state(IRAudioState* state, IRSoundID sound_id, float volume, float pan, bool loop) {
    IRAudioState* prev = ir_audio_set_current_state(state);
    // TODO: Implement properly
    ir_audio_set_current_state(prev);
    return IR_INVALID_CHANNEL;
}

void ir_audio_stop_channel_state(IRAudioState* state, IRChannelID channel_id) {
    IRAudioState* prev = ir_audio_set_current_state(state);
    // TODO: Implement properly
    ir_audio_set_current_state(prev);
}

void ir_audio_set_channel_volume_state(IRAudioState* state, IRChannelID channel_id, float volume) {
    IRAudioState* prev = ir_audio_set_current_state(state);
    // TODO: Implement properly
    ir_audio_set_current_state(prev);
}

void ir_audio_set_master_volume_state(IRAudioState* state, float volume) {
    if (state) {
        state->master_volume = fmaxf(0.0f, fminf(1.0f, volume));
    }
}

void ir_audio_set_listener_position_state(IRAudioState* state, float x, float y, float z) {
    if (state) {
        state->listener.position[0] = x;
        state->listener.position[1] = y;
        state->listener.position[2] = z;
    }
}

void ir_audio_update_state(IRAudioState* state, float dt) {
    // TODO: Implement properly - update channels, streaming, etc.
    (void)state;
    (void)dt;
}

// ============================================================================
// Helper Functions (Legacy - using current state)
// ============================================================================
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
        IR_LOG_WARN("AUDIO", "Already initialized");
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

    IR_LOG_INFO("AUDIO", "System initialized (backend: %d, rate: %u Hz)",
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

    IR_LOG_INFO("AUDIO", "System shutdown");
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
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return false;
    }

    if (!plugin) {
        IR_LOG_ERROR("AUDIO", "Invalid backend plugin");
        return false;
    }

    g_audio_system.backend_type = backend;
    g_audio_system.backend = *plugin;

    // Initialize backend
    if (g_audio_system.backend.init) {
        if (!g_audio_system.backend.init(&g_audio_system.config)) {
            IR_LOG_ERROR("AUDIO", "Backend initialization failed");
            return false;
        }
    }

    IR_LOG_INFO("AUDIO", "Registered backend: %d", backend);
    return true;
}

// ============================================================================
// Sound Loading
// ============================================================================

IRSoundID ir_audio_load_sound(const char* path) {
    if (!g_audio_system.initialized) {
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return IR_INVALID_SOUND;
    }

    if (g_audio_system.sound_count >= IR_MAX_SOUNDS) {
        IR_LOG_ERROR("AUDIO", "Max sounds reached");
        return IR_INVALID_SOUND;
    }

    // Check if already loaded
    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (strcmp(g_audio_system.sounds[i].path, path) == 0) {
            IR_LOG_INFO("AUDIO", "Sound already loaded: %s", path);
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
            IR_LOG_ERROR("AUDIO", "Failed to load sound: %s", path);
            return IR_INVALID_SOUND;
        }
    } else {
        IR_LOG_ERROR("AUDIO", "No backend registered");
        return IR_INVALID_SOUND;
    }

    sound->loaded = true;
    g_audio_system.sound_count++;

    IR_LOG_INFO("AUDIO", "Loaded sound: %s (id=%u, %u ms, %u Hz, %u ch)",
           path, sound->id, sound->duration_ms, sound->sample_rate, sound->channels);

    return sound->id;
}

IRSoundID ir_audio_load_sound_from_memory(const char* name, const void* data, size_t size) {
    if (!g_audio_system.initialized) {
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return IR_INVALID_SOUND;
    }

    if (!name || !data || size == 0) {
        IR_LOG_ERROR("AUDIO", "Invalid parameters for memory loading");
        return IR_INVALID_SOUND;
    }

    if (g_audio_system.sound_count >= IR_MAX_SOUNDS) {
        IR_LOG_ERROR("AUDIO", "Max sounds reached");
        return IR_INVALID_SOUND;
    }

    // Check if already loaded by name
    for (uint32_t i = 0; i < g_audio_system.sound_count; i++) {
        if (strcmp(g_audio_system.sounds[i].path, name) == 0) {
            IR_LOG_INFO("AUDIO", "Sound already loaded: %s", name);
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
            IR_LOG_ERROR("AUDIO", "Failed to load sound from memory: %s (%zu bytes)", name, size);
            return IR_INVALID_SOUND;
        }
    } else {
        // Fallback: try to use the file loader if backend doesn't support memory loading
        IR_LOG_ERROR("AUDIO", "Backend doesn't support memory loading");
        return IR_INVALID_SOUND;
    }

    sound->loaded = true;
    g_audio_system.sound_count++;

    // Estimate memory usage
    g_audio_system.total_memory_bytes += (uint32_t)size;

    IR_LOG_INFO("AUDIO", "Loaded sound from memory: %s (id=%u, %zu bytes, %u ms, %u Hz, %u ch)",
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
    IR_LOG_INFO("AUDIO", "Unloaded sound: %s", sound->path);
}

IRSound* ir_audio_get_sound(IRSoundID sound_id) {
    return find_sound(sound_id);
}

// ============================================================================
// Music Loading
// ============================================================================

IRMusicID ir_audio_load_music(const char* path) {
    if (!g_audio_system.initialized) {
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return IR_INVALID_MUSIC;
    }

    if (g_audio_system.music_count >= IR_MAX_MUSIC) {
        IR_LOG_ERROR("AUDIO", "Max music reached");
        return IR_INVALID_MUSIC;
    }

    // Check if already loaded
    for (uint32_t i = 0; i < g_audio_system.music_count; i++) {
        if (strcmp(g_audio_system.music[i].path, path) == 0) {
            IR_LOG_INFO("AUDIO", "Music already loaded: %s", path);
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
            IR_LOG_ERROR("AUDIO", "Failed to load music: %s", path);
            return IR_INVALID_MUSIC;
        }
    } else {
        IR_LOG_ERROR("AUDIO", "No backend registered");
        return IR_INVALID_MUSIC;
    }

    music->loaded = true;
    g_audio_system.music_count++;

    IR_LOG_INFO("AUDIO", "Loaded music: %s (id=%u, %u ms)", path, music->id, music->duration_ms);

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
    IR_LOG_INFO("AUDIO", "Unloaded music: %s", music->path);
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
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return IR_INVALID_CHANNEL;
    }

    IRSound* sound = find_sound(sound_id);
    if (!sound || !sound->loaded) {
        IR_LOG_ERROR("AUDIO", "Invalid sound ID: %u", sound_id);
        return IR_INVALID_CHANNEL;
    }

    // Find free channel
    IRChannel* channel = find_free_channel();
    if (!channel) {
        IR_LOG_ERROR("AUDIO", "No free channels");
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
        IR_LOG_ERROR("AUDIO", "Not initialized");
        return;
    }

    IRMusic* music = find_music(music_id);
    if (!music || !music->loaded) {
        IR_LOG_ERROR("AUDIO", "Invalid music ID: %u", music_id);
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

    IR_LOG_INFO("AUDIO", "Playing music: %s (loop=%d)", music->path, loop);
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

IRPlaybackState ir_audio_get_channel_state(IRChannelID channel_id) {
    IRChannel* channel = find_channel(channel_id);
    return channel ? channel->state : IR_AUDIO_STOPPED;
}

IRPlaybackState ir_audio_get_music_state(void) {
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
// Spatial Audio
// ============================================================================

void ir_audio_set_listener_position(float x, float y, float z) {
    g_audio_system.listener.position[0] = x;
    g_audio_system.listener.position[1] = y;
    g_audio_system.listener.position[2] = z;

    // Update backend if it supports spatial audio
    // Backend implementation would apply this to all spatial channels
}

void ir_audio_set_listener_orientation(float forward_x, float forward_y, float forward_z,
                                        float up_x, float up_y, float up_z) {
    g_audio_system.listener.forward[0] = forward_x;
    g_audio_system.listener.forward[1] = forward_y;
    g_audio_system.listener.forward[2] = forward_z;

    g_audio_system.listener.up[0] = up_x;
    g_audio_system.listener.up[1] = up_y;
    g_audio_system.listener.up[2] = up_z;

    // Update backend if it supports spatial audio
}

void ir_audio_set_channel_position(IRChannelID channel_id, float x, float y, float z) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel) return;

    channel->spatial = true;
    channel->position[0] = x;
    channel->position[1] = y;
    channel->position[2] = z;

    // Recalculate volume and pan based on distance to listener
    float dx = x - g_audio_system.listener.position[0];
    float dy = y - g_audio_system.listener.position[1];
    float dz = z - g_audio_system.listener.position[2];

    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    // Apply distance attenuation
    float min_dist = channel->min_distance > 0 ? channel->min_distance : 1.0f;
    float max_dist = channel->max_distance > 0 ? channel->max_distance : 1000.0f;

    if (distance <= min_dist) {
        // No attenuation at minimum distance
        // Keep original volume
    } else if (distance >= max_dist) {
        // Silent beyond max distance
        channel->volume = 0.0f;
    } else {
        // Linear attenuation between min and max distance
        float attenuation = 1.0f - (distance - min_dist) / (max_dist - min_dist);
        channel->volume = fmaxf(0.0f, fminf(1.0f, attenuation));
    }

    // Calculate pan based on horizontal angle to listener
    // Simple approximation: use x offset for panning
    if (distance > 0.1f) {
        float pan = dx / distance;  // -1 (left) to 1 (right)
        channel->pan = fmaxf(-1.0f, fminf(1.0f, pan));
    }

    // Update backend channel volume if available
    if (g_audio_system.backend.set_channel_volume) {
        g_audio_system.backend.set_channel_volume(channel_id, channel->volume);
    }
}

void ir_audio_set_channel_velocity(IRChannelID channel_id, float vx, float vy, float vz) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel) return;

    channel->spatial = true;
    channel->velocity[0] = vx;
    channel->velocity[1] = vy;
    channel->velocity[2] = vz;

    // Velocity can be used for doppler effect calculations
    // Backend implementation would apply doppler shift
}

void ir_audio_set_channel_attenuation(IRChannelID channel_id, float min_distance, float max_distance) {
    IRChannel* channel = find_channel(channel_id);
    if (!channel) return;

    channel->spatial = true;
    channel->min_distance = min_distance;
    channel->max_distance = max_distance;

    // Recalculate attenuation based on new distances
    ir_audio_set_channel_position(channel_id,
                                  channel->position[0],
                                  channel->position[1],
                                  channel->position[2]);
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
