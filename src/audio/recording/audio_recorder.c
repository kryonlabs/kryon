/**
 * @file audio_recorder.c
 * @brief Kryon Audio Recorder Implementation
 */

#include "audio.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

// =============================================================================
// RECORDING BUFFER MANAGEMENT
// =============================================================================

typedef struct KryonRecordingBuffer {
    void* data;
    size_t size;
    size_t capacity;
    size_t write_position;
    KryonAudioFormat format;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    bool is_circular;
} KryonRecordingBuffer;

typedef struct {
    // Recording state
    bool is_recording;
    bool is_paused;
    double start_time;
    double pause_time;
    double total_paused_time;
    
    // Recording configuration
    KryonAudioFormat format;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    
    // Buffer management
    KryonRecordingBuffer* active_buffer;
    KryonRecordingBuffer** saved_recordings;
    size_t recording_count;
    size_t max_recordings;
    
    // Recording thread
    pthread_t recording_thread;
    bool recording_thread_running;
    
    // Input device
    KryonAudioDevice* input_device;
    
    // Recording limits
    size_t max_recording_size;
    double max_recording_duration;
    
    // Monitoring
    float input_level;
    float input_peak;
    bool voice_activation;
    float voice_threshold;
    
    // Callbacks
    KryonAudioRecordingCallback recording_callback;
    void* callback_user_data;
    
} KryonAudioRecorder;

static KryonAudioRecorder g_audio_recorder = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static KryonRecordingBuffer* create_recording_buffer(size_t capacity, KryonAudioFormat format,
                                                    uint32_t sample_rate, uint16_t channels) {
    KryonRecordingBuffer* buffer = kryon_alloc(sizeof(KryonRecordingBuffer));
    if (!buffer) return NULL;
    
    memset(buffer, 0, sizeof(KryonRecordingBuffer));
    
    buffer->data = kryon_alloc(capacity);
    if (!buffer->data) {
        kryon_free(buffer);
        return NULL;
    }
    
    buffer->capacity = capacity;
    buffer->format = format;
    buffer->sample_rate = sample_rate;
    buffer->channels = channels;
    buffer->bits_per_sample = (format == KRYON_AUDIO_FORMAT_F32) ? 32 : 16;
    buffer->is_circular = false;
    
    return buffer;
}

static void destroy_recording_buffer(KryonRecordingBuffer* buffer) {
    if (!buffer) return;
    
    kryon_free(buffer->data);
    kryon_free(buffer);
}

static size_t get_sample_size(KryonAudioFormat format, uint16_t channels) {
    size_t bytes_per_sample = (format == KRYON_AUDIO_FORMAT_F32) ? 4 : 2;
    return bytes_per_sample * channels;
}

static float calculate_audio_level(const void* data, size_t samples, KryonAudioFormat format) {
    float level = 0.0f;
    
    if (format == KRYON_AUDIO_FORMAT_S16) {
        const int16_t* samples_16 = (const int16_t*)data;
        for (size_t i = 0; i < samples; i++) {
            float sample = (float)samples_16[i] / 32768.0f;
            level += sample * sample;
        }
    } else if (format == KRYON_AUDIO_FORMAT_F32) {
        const float* samples_f32 = (const float*)data;
        for (size_t i = 0; i < samples; i++) {
            level += samples_f32[i] * samples_f32[i];
        }
    }
    
    return sqrtf(level / samples);
}

static void* recording_thread_func(void* arg) {
    (void)arg;
    
    char temp_buffer[4096];
    
    while (g_audio_recorder.recording_thread_running) {
        if (!g_audio_recorder.is_recording || g_audio_recorder.is_paused) {
            usleep(1000); // 1ms
            continue;
        }
        
        KryonRecordingBuffer* buffer = g_audio_recorder.active_buffer;
        if (!buffer) {
            usleep(1000);
            continue;
        }
        
        // Simulate reading from audio input device
        // In a real implementation, this would read from the actual audio device
        size_t bytes_to_read = sizeof(temp_buffer);
        size_t sample_size = get_sample_size(buffer->format, buffer->channels);
        size_t samples_to_read = bytes_to_read / sample_size;
        
        // Check if we have space in the buffer
        size_t available_space = buffer->capacity - buffer->write_position;
        if (available_space < bytes_to_read) {
            if (buffer->is_circular) {
                // Wrap around for circular buffer
                buffer->write_position = 0;
            } else {
                // Stop recording if buffer is full
                g_audio_recorder.is_recording = false;
                if (g_audio_recorder.recording_callback) {
                    KryonAudioRecordingEvent event = {
                        .type = KRYON_AUDIO_RECORDING_BUFFER_FULL,
                        .timestamp = kryon_platform_get_time(),
                        .buffer = buffer,
                        .size = buffer->size
                    };
                    g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
                }
                continue;
            }
        }
        
        // Simulate audio data (in real implementation, read from device)
        memset(temp_buffer, 0, bytes_to_read);
        
        // Copy to recording buffer
        memcpy((char*)buffer->data + buffer->write_position, temp_buffer, bytes_to_read);
        buffer->write_position += bytes_to_read;
        
        if (!buffer->is_circular) {
            buffer->size = buffer->write_position;
        }
        
        // Calculate audio levels
        g_audio_recorder.input_level = calculate_audio_level(temp_buffer, samples_to_read, buffer->format);
        if (g_audio_recorder.input_level > g_audio_recorder.input_peak) {
            g_audio_recorder.input_peak = g_audio_recorder.input_level;
        }
        
        // Voice activation detection
        if (g_audio_recorder.voice_activation) {
            if (g_audio_recorder.input_level < g_audio_recorder.voice_threshold) {
                // Pause recording if below threshold
                g_audio_recorder.is_paused = true;
            }
        }
        
        // Check duration limits
        double current_time = kryon_platform_get_time();
        double recording_duration = current_time - g_audio_recorder.start_time - g_audio_recorder.total_paused_time;
        
        if (g_audio_recorder.max_recording_duration > 0 && 
            recording_duration >= g_audio_recorder.max_recording_duration) {
            g_audio_recorder.is_recording = false;
            if (g_audio_recorder.recording_callback) {
                KryonAudioRecordingEvent event = {
                    .type = KRYON_AUDIO_RECORDING_DURATION_LIMIT,
                    .timestamp = current_time,
                    .buffer = buffer,
                    .size = buffer->size
                };
                g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
            }
        }
        
        usleep(1000); // 1ms
    }
    
    return NULL;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_audio_recorder_init(const KryonAudioRecorderConfig* config) {
    memset(&g_audio_recorder, 0, sizeof(g_audio_recorder));
    
    if (config) {
        g_audio_recorder.format = config->format;
        g_audio_recorder.sample_rate = config->sample_rate;
        g_audio_recorder.channels = config->channels;
        g_audio_recorder.max_recording_size = config->max_buffer_size;
        g_audio_recorder.max_recording_duration = config->max_duration;
        g_audio_recorder.voice_threshold = config->voice_threshold;
    } else {
        // Default configuration
        g_audio_recorder.format = KRYON_AUDIO_FORMAT_S16;
        g_audio_recorder.sample_rate = 44100;
        g_audio_recorder.channels = 1; // Mono for recording
        g_audio_recorder.max_recording_size = 16 * 1024 * 1024; // 16MB
        g_audio_recorder.max_recording_duration = 300.0; // 5 minutes
        g_audio_recorder.voice_threshold = 0.01f;
    }
    
    g_audio_recorder.bits_per_sample = (g_audio_recorder.format == KRYON_AUDIO_FORMAT_F32) ? 32 : 16;
    g_audio_recorder.max_recordings = 10;
    
    // Allocate recordings array
    g_audio_recorder.saved_recordings = kryon_alloc(sizeof(KryonRecordingBuffer*) * g_audio_recorder.max_recordings);
    if (!g_audio_recorder.saved_recordings) {
        return false;
    }
    memset(g_audio_recorder.saved_recordings, 0, sizeof(KryonRecordingBuffer*) * g_audio_recorder.max_recordings);
    
    // Initialize input device (platform-specific)
    printf("Audio Recorder: Initializing input device\n");
    
    // Start recording thread
    g_audio_recorder.recording_thread_running = true;
    if (pthread_create(&g_audio_recorder.recording_thread, NULL, recording_thread_func, NULL) != 0) {
        kryon_free(g_audio_recorder.saved_recordings);
        return false;
    }
    
    return true;
}

void kryon_audio_recorder_shutdown(void) {
    // Stop recording
    kryon_audio_recorder_stop();
    
    // Stop recording thread
    g_audio_recorder.recording_thread_running = false;
    pthread_join(g_audio_recorder.recording_thread, NULL);
    
    // Clean up active buffer
    if (g_audio_recorder.active_buffer) {
        destroy_recording_buffer(g_audio_recorder.active_buffer);
    }
    
    // Clean up saved recordings
    for (size_t i = 0; i < g_audio_recorder.recording_count; i++) {
        destroy_recording_buffer(g_audio_recorder.saved_recordings[i]);
    }
    kryon_free(g_audio_recorder.saved_recordings);
    
    // Close input device
    if (g_audio_recorder.input_device) {
        // Platform-specific device cleanup
        g_audio_recorder.input_device = NULL;
    }
    
    memset(&g_audio_recorder, 0, sizeof(g_audio_recorder));
}

bool kryon_audio_recorder_start(void) {
    if (g_audio_recorder.is_recording) {
        return false; // Already recording
    }
    
    // Create new recording buffer
    size_t buffer_capacity = g_audio_recorder.max_recording_size;
    if (buffer_capacity == 0) {
        buffer_capacity = 16 * 1024 * 1024; // 16MB default
    }
    
    g_audio_recorder.active_buffer = create_recording_buffer(
        buffer_capacity,
        g_audio_recorder.format,
        g_audio_recorder.sample_rate,
        g_audio_recorder.channels
    );
    
    if (!g_audio_recorder.active_buffer) {
        return false;
    }
    
    // Reset state
    g_audio_recorder.is_recording = true;
    g_audio_recorder.is_paused = false;
    g_audio_recorder.start_time = kryon_platform_get_time();
    g_audio_recorder.pause_time = 0.0;
    g_audio_recorder.total_paused_time = 0.0;
    g_audio_recorder.input_peak = 0.0f;
    
    // Notify callback
    if (g_audio_recorder.recording_callback) {
        KryonAudioRecordingEvent event = {
            .type = KRYON_AUDIO_RECORDING_STARTED,
            .timestamp = g_audio_recorder.start_time,
            .buffer = g_audio_recorder.active_buffer,
            .size = 0
        };
        g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
    }
    
    return true;
}

bool kryon_audio_recorder_pause(void) {
    if (!g_audio_recorder.is_recording || g_audio_recorder.is_paused) {
        return false;
    }
    
    g_audio_recorder.is_paused = true;
    g_audio_recorder.pause_time = kryon_platform_get_time();
    
    // Notify callback
    if (g_audio_recorder.recording_callback) {
        KryonAudioRecordingEvent event = {
            .type = KRYON_AUDIO_RECORDING_PAUSED,
            .timestamp = g_audio_recorder.pause_time,
            .buffer = g_audio_recorder.active_buffer,
            .size = g_audio_recorder.active_buffer ? g_audio_recorder.active_buffer->size : 0
        };
        g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
    }
    
    return true;
}

bool kryon_audio_recorder_resume(void) {
    if (!g_audio_recorder.is_recording || !g_audio_recorder.is_paused) {
        return false;
    }
    
    double current_time = kryon_platform_get_time();
    g_audio_recorder.total_paused_time += (current_time - g_audio_recorder.pause_time);
    g_audio_recorder.is_paused = false;
    
    // Notify callback
    if (g_audio_recorder.recording_callback) {
        KryonAudioRecordingEvent event = {
            .type = KRYON_AUDIO_RECORDING_RESUMED,
            .timestamp = current_time,
            .buffer = g_audio_recorder.active_buffer,
            .size = g_audio_recorder.active_buffer ? g_audio_recorder.active_buffer->size : 0
        };
        g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
    }
    
    return true;
}

KryonAudioRecordingId kryon_audio_recorder_stop(void) {
    if (!g_audio_recorder.is_recording) {
        return 0;
    }
    
    g_audio_recorder.is_recording = false;
    g_audio_recorder.is_paused = false;
    
    double stop_time = kryon_platform_get_time();
    
    if (!g_audio_recorder.active_buffer) {
        return 0;
    }
    
    // Save the recording
    if (g_audio_recorder.recording_count < g_audio_recorder.max_recordings) {
        g_audio_recorder.saved_recordings[g_audio_recorder.recording_count] = g_audio_recorder.active_buffer;
        g_audio_recorder.recording_count++;
        
        KryonAudioRecordingId recording_id = g_audio_recorder.recording_count;
        g_audio_recorder.active_buffer = NULL; // Transfer ownership
        
        // Notify callback
        if (g_audio_recorder.recording_callback) {
            KryonAudioRecordingEvent event = {
                .type = KRYON_AUDIO_RECORDING_STOPPED,
                .timestamp = stop_time,
                .buffer = g_audio_recorder.saved_recordings[recording_id - 1],
                .size = g_audio_recorder.saved_recordings[recording_id - 1]->size
            };
            g_audio_recorder.recording_callback(&event, g_audio_recorder.callback_user_data);
        }
        
        return recording_id;
    } else {
        // No space for more recordings, discard
        destroy_recording_buffer(g_audio_recorder.active_buffer);
        g_audio_recorder.active_buffer = NULL;
        return 0;
    }
}

bool kryon_audio_recorder_is_recording(void) {
    return g_audio_recorder.is_recording;
}

bool kryon_audio_recorder_is_paused(void) {
    return g_audio_recorder.is_paused;
}

double kryon_audio_recorder_get_duration(void) {
    if (!g_audio_recorder.is_recording) {
        return 0.0;
    }
    
    double current_time = kryon_platform_get_time();
    double total_paused = g_audio_recorder.total_paused_time;
    
    if (g_audio_recorder.is_paused) {
        total_paused += (current_time - g_audio_recorder.pause_time);
    }
    
    return current_time - g_audio_recorder.start_time - total_paused;
}

float kryon_audio_recorder_get_input_level(void) {
    return g_audio_recorder.input_level;
}

float kryon_audio_recorder_get_input_peak(void) {
    return g_audio_recorder.input_peak;
}

void kryon_audio_recorder_reset_peak(void) {
    g_audio_recorder.input_peak = 0.0f;
}

void kryon_audio_recorder_set_voice_activation(bool enabled, float threshold) {
    g_audio_recorder.voice_activation = enabled;
    g_audio_recorder.voice_threshold = fmaxf(0.0f, fminf(1.0f, threshold));
}

bool kryon_audio_recorder_get_voice_activation(float* threshold) {
    if (threshold) {
        *threshold = g_audio_recorder.voice_threshold;
    }
    return g_audio_recorder.voice_activation;
}

void kryon_audio_recorder_set_callback(KryonAudioRecordingCallback callback, void* user_data) {
    g_audio_recorder.recording_callback = callback;
    g_audio_recorder.callback_user_data = user_data;
}

size_t kryon_audio_recorder_get_recording_count(void) {
    return g_audio_recorder.recording_count;
}

KryonAudioRecordingInfo kryon_audio_recorder_get_recording_info(KryonAudioRecordingId recording_id) {
    KryonAudioRecordingInfo info = {0};
    
    if (recording_id == 0 || recording_id > g_audio_recorder.recording_count) {
        return info;
    }
    
    KryonRecordingBuffer* buffer = g_audio_recorder.saved_recordings[recording_id - 1];
    if (!buffer) return info;
    
    info.format = buffer->format;
    info.sample_rate = buffer->sample_rate;
    info.channels = buffer->channels;
    info.bits_per_sample = buffer->bits_per_sample;
    info.size = buffer->size;
    
    // Calculate duration
    size_t sample_size = get_sample_size(buffer->format, buffer->channels);
    size_t total_samples = buffer->size / sample_size;
    info.duration = (double)total_samples / buffer->sample_rate;
    
    return info;
}

bool kryon_audio_recorder_save_to_file(KryonAudioRecordingId recording_id, const char* file_path) {
    if (!file_path || recording_id == 0 || recording_id > g_audio_recorder.recording_count) {
        return false;
    }
    
    KryonRecordingBuffer* buffer = g_audio_recorder.saved_recordings[recording_id - 1];
    if (!buffer || !buffer->data || buffer->size == 0) {
        return false;
    }
    
    FILE* file = fopen(file_path, "wb");
    if (!file) return false;
    
    // Write raw audio data (in real implementation, would write proper format headers)
    size_t bytes_written = fwrite(buffer->data, 1, buffer->size, file);
    fclose(file);
    
    return bytes_written == buffer->size;
}

const void* kryon_audio_recorder_get_recording_data(KryonAudioRecordingId recording_id, size_t* size) {
    if (recording_id == 0 || recording_id > g_audio_recorder.recording_count) {
        if (size) *size = 0;
        return NULL;
    }
    
    KryonRecordingBuffer* buffer = g_audio_recorder.saved_recordings[recording_id - 1];
    if (!buffer) {
        if (size) *size = 0;
        return NULL;
    }
    
    if (size) *size = buffer->size;
    return buffer->data;
}

void kryon_audio_recorder_delete_recording(KryonAudioRecordingId recording_id) {
    if (recording_id == 0 || recording_id > g_audio_recorder.recording_count) {
        return;
    }
    
    size_t index = recording_id - 1;
    destroy_recording_buffer(g_audio_recorder.saved_recordings[index]);
    
    // Shift remaining recordings
    for (size_t i = index + 1; i < g_audio_recorder.recording_count; i++) {
        g_audio_recorder.saved_recordings[i - 1] = g_audio_recorder.saved_recordings[i];
    }
    
    g_audio_recorder.recording_count--;
    g_audio_recorder.saved_recordings[g_audio_recorder.recording_count] = NULL;
}

void kryon_audio_recorder_clear_all_recordings(void) {
    for (size_t i = 0; i < g_audio_recorder.recording_count; i++) {
        destroy_recording_buffer(g_audio_recorder.saved_recordings[i]);
        g_audio_recorder.saved_recordings[i] = NULL;
    }
    g_audio_recorder.recording_count = 0;
}