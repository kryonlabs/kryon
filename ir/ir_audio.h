#ifndef IR_AUDIO_H
#define IR_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Kryon Audio System
 *
 * Provides sound effects and music playback with mixing and volume control.
 * Features:
 * - Plugin architecture for multiple backends (SDL3_mixer, OpenAL, miniaudio, etc.)
 * - Sound effects (short clips, multiple instances)
 * - Music streaming (long files, single instance)
 * - Volume control (per-sound, per-channel, global)
 * - Mixing (multiple sounds playing simultaneously)
 * - 3D spatial audio (optional, backend-dependent)
 * - Format support: WAV, OGG, MP3 (backend-dependent)
 *
 * Usage:
 *   ir_audio_init();
 *
 *   IRSoundID sound = ir_audio_load_sound("@sounds/jump.wav");
 *   ir_audio_play_sound(sound, 1.0f, 0.0f);  // volume=1.0, pan=0.0
 *
 *   IRMusicID music = ir_audio_load_music("@music/theme.ogg");
 *   ir_audio_play_music(music, true);  // loop=true
 *
 *   ir_audio_shutdown();
 */

// ============================================================================
// Type Definitions
// ============================================================================

typedef uint32_t IRSoundID;
typedef uint32_t IRMusicID;
typedef uint32_t IRChannelID;

#define IR_INVALID_SOUND 0
#define IR_INVALID_MUSIC 0
#define IR_INVALID_CHANNEL 0

#define IR_MAX_SOUNDS 256
#define IR_MAX_MUSIC 16
#define IR_MAX_CHANNELS 32

// Audio state
typedef enum {
    IR_AUDIO_STOPPED,
    IR_AUDIO_PLAYING,
    IR_AUDIO_PAUSED
} IRAudioState;

// Audio backend type
typedef enum {
    IR_AUDIO_BACKEND_NONE,
    IR_AUDIO_BACKEND_SDL3,      // SDL3_mixer
    IR_AUDIO_BACKEND_OPENAL,    // OpenAL
    IR_AUDIO_BACKEND_MINIAUDIO, // miniaudio
    IR_AUDIO_BACKEND_CUSTOM     // User-defined
} IRAudioBackend;

// Sound metadata
typedef struct {
    IRSoundID id;
    char path[512];
    void* backend_data;  // Backend-specific data
    uint32_t duration_ms;
    uint32_t sample_rate;
    uint8_t channels;    // 1=mono, 2=stereo
    bool loaded;
} IRSound;

// Music metadata
typedef struct {
    IRMusicID id;
    char path[512];
    void* backend_data;  // Backend-specific data
    uint32_t duration_ms;
    bool loaded;
    bool looping;
} IRMusic;

// Channel (playing sound instance)
typedef struct {
    IRChannelID id;
    IRSoundID sound_id;
    IRAudioState state;
    float volume;        // 0.0 to 1.0
    float pan;           // -1.0 (left) to 1.0 (right)
    bool loop;
    uint32_t start_time_ms;
    void* backend_data;  // Backend-specific data
} IRChannel;

// Audio configuration
typedef struct {
    IRAudioBackend backend;
    uint32_t sample_rate;    // Default: 44100 Hz
    uint32_t buffer_size;    // Default: 4096 samples
    uint8_t channels;        // Default: 2 (stereo)
    bool enable_spatial;     // Enable 3D spatial audio
} IRAudioConfig;

// ============================================================================
// Backend Plugin Interface
// ============================================================================

typedef struct {
    // Initialization
    bool (*init)(IRAudioConfig* config);
    void (*shutdown)(void);

    // Sound loading
    void* (*load_sound)(const char* path, uint32_t* duration_ms, uint32_t* sample_rate, uint8_t* channels);
    void* (*load_sound_from_memory)(const char* name, const void* data, size_t size,
                                     uint32_t* duration_ms, uint32_t* sample_rate, uint8_t* channels);
    void (*unload_sound)(void* backend_data);

    // Music loading
    void* (*load_music)(const char* path, uint32_t* duration_ms);
    void (*unload_music)(void* backend_data);

    // Sound playback
    IRChannelID (*play_sound)(void* sound_data, float volume, float pan, bool loop);
    void (*stop_channel)(IRChannelID channel_id);
    void (*pause_channel)(IRChannelID channel_id);
    void (*resume_channel)(IRChannelID channel_id);

    // Music playback
    void (*play_music)(void* music_data, bool loop);
    void (*stop_music)(void);
    void (*pause_music)(void);
    void (*resume_music)(void);

    // Volume control
    void (*set_channel_volume)(IRChannelID channel_id, float volume);
    void (*set_music_volume)(float volume);
    void (*set_master_volume)(float volume);

    // State queries
    IRAudioState (*get_channel_state)(IRChannelID channel_id);
    IRAudioState (*get_music_state)(void);

    // Update (called every frame)
    void (*update)(float dt);
} IRAudioBackendPlugin;

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize audio system with default configuration
 * Uses SDL3 backend by default
 */
bool ir_audio_init(void);

/**
 * Initialize audio system with custom configuration
 */
bool ir_audio_init_ex(IRAudioConfig* config);

/**
 * Shutdown audio system
 * Stops all sounds and frees resources
 */
void ir_audio_shutdown(void);

/**
 * Update audio system (call every frame)
 * Handles streaming, mixing, and state management
 */
void ir_audio_update(float dt);

/**
 * Register custom audio backend
 */
bool ir_audio_register_backend(IRAudioBackend backend, IRAudioBackendPlugin* plugin);

// ============================================================================
// Sound Loading
// ============================================================================

/**
 * Load sound effect from file
 * Returns sound ID or IR_INVALID_SOUND on failure
 */
IRSoundID ir_audio_load_sound(const char* path);

/**
 * Load sound effect from memory
 */
IRSoundID ir_audio_load_sound_from_memory(const char* name, const void* data, size_t size);

/**
 * Unload sound effect
 * Stops all channels playing this sound
 */
void ir_audio_unload_sound(IRSoundID sound_id);

/**
 * Get sound metadata
 */
IRSound* ir_audio_get_sound(IRSoundID sound_id);

// ============================================================================
// Music Loading
// ============================================================================

/**
 * Load music from file
 * Returns music ID or IR_INVALID_MUSIC on failure
 */
IRMusicID ir_audio_load_music(const char* path);

/**
 * Unload music
 * Stops music if playing
 */
void ir_audio_unload_music(IRMusicID music_id);

/**
 * Get music metadata
 */
IRMusic* ir_audio_get_music(IRMusicID music_id);

// ============================================================================
// Sound Playback
// ============================================================================

/**
 * Play sound effect
 * Returns channel ID or IR_INVALID_CHANNEL on failure
 *
 * @param sound_id Sound to play
 * @param volume Volume (0.0 to 1.0)
 * @param pan Stereo pan (-1.0 left, 0.0 center, 1.0 right)
 * @return Channel ID for controlling playback
 */
IRChannelID ir_audio_play_sound(IRSoundID sound_id, float volume, float pan);

/**
 * Play sound effect with looping
 */
IRChannelID ir_audio_play_sound_ex(IRSoundID sound_id, float volume, float pan, bool loop);

/**
 * Stop channel
 */
void ir_audio_stop_channel(IRChannelID channel_id);

/**
 * Pause channel
 */
void ir_audio_pause_channel(IRChannelID channel_id);

/**
 * Resume channel
 */
void ir_audio_resume_channel(IRChannelID channel_id);

/**
 * Stop all sound effects
 */
void ir_audio_stop_all_sounds(void);

// ============================================================================
// Music Playback
// ============================================================================

/**
 * Play music
 * @param music_id Music to play
 * @param loop Loop music when finished
 */
void ir_audio_play_music(IRMusicID music_id, bool loop);

/**
 * Stop music
 */
void ir_audio_stop_music(void);

/**
 * Pause music
 */
void ir_audio_pause_music(void);

/**
 * Resume music
 */
void ir_audio_resume_music(void);

/**
 * Check if music is playing
 */
bool ir_audio_is_music_playing(void);

// ============================================================================
// Volume Control
// ============================================================================

/**
 * Set channel volume
 * @param channel_id Channel to modify
 * @param volume Volume (0.0 to 1.0)
 */
void ir_audio_set_channel_volume(IRChannelID channel_id, float volume);

/**
 * Set music volume
 * @param volume Volume (0.0 to 1.0)
 */
void ir_audio_set_music_volume(float volume);

/**
 * Set master volume (affects all audio)
 * @param volume Volume (0.0 to 1.0)
 */
void ir_audio_set_master_volume(float volume);

/**
 * Get channel volume
 */
float ir_audio_get_channel_volume(IRChannelID channel_id);

/**
 * Get music volume
 */
float ir_audio_get_music_volume(void);

/**
 * Get master volume
 */
float ir_audio_get_master_volume(void);

// ============================================================================
// Channel Management
// ============================================================================

/**
 * Get channel state
 */
IRAudioState ir_audio_get_channel_state(IRChannelID channel_id);

/**
 * Get music state
 */
IRAudioState ir_audio_get_music_state(void);

/**
 * Get number of active channels
 */
uint32_t ir_audio_get_active_channels(void);

/**
 * Find channel by sound ID
 * Returns first channel playing this sound
 */
IRChannelID ir_audio_find_channel(IRSoundID sound_id);

// ============================================================================
// Spatial Audio (3D)
// ============================================================================

/**
 * Set listener position (camera/player position)
 */
void ir_audio_set_listener_position(float x, float y, float z);

/**
 * Set listener orientation
 */
void ir_audio_set_listener_orientation(float forward_x, float forward_y, float forward_z,
                                        float up_x, float up_y, float up_z);

/**
 * Set channel 3D position
 */
void ir_audio_set_channel_position(IRChannelID channel_id, float x, float y, float z);

/**
 * Set channel 3D velocity (for doppler effect)
 */
void ir_audio_set_channel_velocity(IRChannelID channel_id, float vx, float vy, float vz);

/**
 * Set channel attenuation parameters
 * @param min_distance Distance at which sound is at full volume
 * @param max_distance Distance at which sound is silent
 */
void ir_audio_set_channel_attenuation(IRChannelID channel_id, float min_distance, float max_distance);

// ============================================================================
// Audio Statistics
// ============================================================================

typedef struct {
    uint32_t loaded_sounds;
    uint32_t loaded_music;
    uint32_t active_channels;
    uint32_t total_memory_bytes;
    IRAudioBackend backend;
    uint32_t sample_rate;
} IRAudioStats;

/**
 * Get audio system statistics
 */
void ir_audio_get_stats(IRAudioStats* stats);

/**
 * Print audio statistics for debugging
 */
void ir_audio_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif // IR_AUDIO_H
