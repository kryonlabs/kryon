/*
 * Audio Mixer Implementation
 *
 * Software audio mixing utilities for Kryon Audio System
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "ir_audio_mixer.h"
#include "ir_log.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif

// ============================================================================
// Volume Control
// ============================================================================

void ir_audio_apply_volume_int16(int16_t* samples, size_t sample_count, float volume) {
    if (!samples || sample_count == 0) return;

    // Clamp volume to reasonable range
    float gain = (volume < 0.0f) ? 0.0f : (volume > 2.0f) ? 2.0f : volume;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t sample = (int32_t)(samples[i] * gain);
        samples[i] = ir_audio_clamp_int16(sample);
    }
}

void ir_audio_apply_volume_float(float* samples, size_t sample_count, float volume) {
    if (!samples || sample_count == 0) return;

    float gain = (volume < 0.0f) ? 0.0f : volume;

    for (size_t i = 0; i < sample_count; i++) {
        samples[i] = ir_audio_clamp_float(samples[i] * gain);
    }
}

void ir_audio_apply_volume_uint8(uint8_t* samples, size_t sample_count, float volume) {
    if (!samples || sample_count == 0) return;

    float gain = (volume < 0.0f) ? 0.0f : (volume > 2.0f) ? 2.0f : volume;
    float offset = 128.0f;  // uint8 center

    for (size_t i = 0; i < sample_count; i++) {
        float centered = (float)samples[i] - offset;
        int32_t scaled = (int32_t)(centered * gain + offset);
        samples[i] = (uint8_t)((scaled < 0) ? 0 : (scaled > 255) ? 255 : scaled);
    }
}

void ir_audio_apply_volume(void* samples, size_t sample_count, float volume, int bits_per_sample) {
    if (!samples) return;

    switch (bits_per_sample) {
        case 8:
            ir_audio_apply_volume_uint8((uint8_t*)samples, sample_count, volume);
            break;
        case 16:
            ir_audio_apply_volume_int16((int16_t*)samples, sample_count, volume);
            break;
        default:
            IR_LOG_ERROR("AUDIO", "Unsupported bit depth: %d", bits_per_sample);
            break;
    }
}

// ============================================================================
// Stereo Panning
// ============================================================================

void ir_audio_calculate_pan_gains(float pan, float* out_left, float* out_right) {
    // Clamp pan to [-1, 1]
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // Constant power panning (equal loudness curve)
    // Uses sin/cos to maintain constant total power
    float angle = (pan + 1.0f) * (float)M_PI_4;  // Map [-1,1] to [0,π/2]

    *out_left = cosf(angle);
    *out_right = sinf(angle);
}

void ir_audio_calculate_pan_gains_linear(float pan, float* out_left, float* out_right) {
    // Clamp pan to [-1, 1]
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // Linear panning (simpler, but not equal loudness)
    *out_left = 1.0f - pan;
    *out_right = 1.0f + pan;

    // Normalize
    float sum = *out_left + *out_right;
    if (sum > 0.0f) {
        *out_left /= sum;
        *out_right /= sum;
    }
}

void ir_audio_apply_pan_stereo_int16(int16_t* samples, size_t frame_count, float pan) {
    if (!samples || frame_count == 0) return;

    float left_gain, right_gain;
    ir_audio_calculate_pan_gains(pan, &left_gain, &right_gain);

    for (size_t i = 0; i < frame_count; i++) {
        int32_t left = (int32_t)(samples[i * 2] * left_gain);
        int32_t right = (int32_t)(samples[i * 2 + 1] * right_gain);

        samples[i * 2] = ir_audio_clamp_int16(left);
        samples[i * 2 + 1] = ir_audio_clamp_int16(right);
    }
}

void ir_audio_apply_pan_stereo_float(float* samples, size_t frame_count, float pan) {
    if (!samples || frame_count == 0) return;

    float left_gain, right_gain;
    ir_audio_calculate_pan_gains(pan, &left_gain, &right_gain);

    for (size_t i = 0; i < frame_count; i++) {
        samples[i * 2] = ir_audio_clamp_float(samples[i * 2] * left_gain);
        samples[i * 2 + 1] = ir_audio_clamp_float(samples[i * 2 + 1] * right_gain);
    }
}

// ============================================================================
// Audio Mixing
// ============================================================================

void ir_audio_mix_buffers_int16(int16_t* dest, const int16_t* src,
                                size_t frame_count, int channels) {
    if (!dest || !src || frame_count == 0) return;

    size_t sample_count = frame_count * channels;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t mixed = (int32_t)dest[i] + (int32_t)src[i];
        dest[i] = ir_audio_clamp_int16(mixed);
    }
}

void ir_audio_mix_buffers_volume_int16(int16_t* dest, const int16_t* src,
                                       size_t frame_count, int channels, float volume) {
    if (!dest || !src || frame_count == 0) return;

    float gain = (volume < 0.0f) ? 0.0f : (volume > 2.0f) ? 2.0f : volume;
    size_t sample_count = frame_count * channels;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t mixed = (int32_t)dest[i] + (int32_t)(src[i] * gain);
        dest[i] = ir_audio_clamp_int16(mixed);
    }
}

void ir_audio_mix_buffers_float(float* dest, const float* src,
                                size_t frame_count, int channels) {
    if (!dest || !src || frame_count == 0) return;

    size_t sample_count = frame_count * channels;

    for (size_t i = 0; i < sample_count; i++) {
        dest[i] = ir_audio_clamp_float(dest[i] + src[i]);
    }
}

// ============================================================================
// Conversions
// ============================================================================

void ir_audio_convert_uint8_to_int16(const uint8_t* src, int16_t* dest, size_t sample_count) {
    if (!src || !dest) return;

    for (size_t i = 0; i < sample_count; i++) {
        // Convert 0-255 to -32768-32767
        dest[i] = (int16_t)(((int)src[i] - 128) * 256);
    }
}

void ir_audio_convert_int16_to_uint8(const int16_t* src, uint8_t* dest, size_t sample_count) {
    if (!src || !dest) return;

    for (size_t i = 0; i < sample_count; i++) {
        // Convert -32768-32767 to 0-255
        int32_t scaled = ((int32_t)src[i] / 256) + 128;
        dest[i] = (uint8_t)((scaled < 0) ? 0 : (scaled > 255) ? 255 : scaled);
    }
}

void ir_audio_convert_float_to_int16(const float* src, int16_t* dest, size_t sample_count) {
    if (!src || !dest) return;

    for (size_t i = 0; i < sample_count; i++) {
        float clamped = ir_audio_clamp_float(src[i]);
        dest[i] = (int16_t)(clamped * 32767.0f);
    }
}

void ir_audio_convert_int16_to_float(const int16_t* src, float* dest, size_t sample_count) {
    if (!src || !dest) return;

    for (size_t i = 0; i < sample_count; i++) {
        dest[i] = (float)src[i] / 32768.0f;
    }
}

// ============================================================================
// Fading
// ============================================================================

void ir_audio_fade_in_int16(int16_t* samples, size_t frame_count, int channels, float target_volume) {
    if (!samples || frame_count == 0) return;

    for (size_t i = 0; i < frame_count; i++) {
        float progress = (float)(i + 1) / (float)frame_count;
        float volume = progress * target_volume;

        for (int ch = 0; ch < channels; ch++) {
            size_t idx = i * channels + ch;
            int32_t scaled = (int32_t)(samples[idx] * volume);
            samples[idx] = ir_audio_clamp_int16(scaled);
        }
    }
}

void ir_audio_fade_out_int16(int16_t* samples, size_t frame_count, int channels, float start_volume) {
    if (!samples || frame_count == 0) return;

    for (size_t i = 0; i < frame_count; i++) {
        float progress = (float)i / (float)frame_count;  // 1.0 at end
        float volume = (1.0f - progress) * start_volume;

        for (int ch = 0; ch < channels; ch++) {
            size_t idx = i * channels + ch;
            int32_t scaled = (int32_t)(samples[idx] * volume);
            samples[idx] = ir_audio_clamp_int16(scaled);
        }
    }
}

void ir_audio_crossfade_int16(const int16_t* src1, const int16_t* src2,
                              int16_t* dest, size_t frame_count, int channels) {
    if (!src1 || !src2 || !dest || frame_count == 0) return;

    for (size_t i = 0; i < frame_count; i++) {
        float progress = (float)i / (float)frame_count;
        float gain1 = 1.0f - progress;
        float gain2 = progress;

        for (int ch = 0; ch < channels; ch++) {
            size_t idx = i * channels + ch;
            int32_t mixed = (int32_t)(src1[idx] * gain1) + (int32_t)(src2[idx] * gain2);
            dest[idx] = ir_audio_clamp_int16(mixed);
        }
    }
}

// ============================================================================
// Silence
// ============================================================================

void ir_audio_silence(void* buffer, size_t frame_count, int channels, int bits_per_sample) {
    if (!buffer) return;

    size_t byte_count = frame_count * channels * (bits_per_sample / 8);
    memset(buffer, 0, byte_count);
}

bool ir_audio_is_silent(const void* buffer, size_t frame_count, int channels, int bits_per_sample) {
    if (!buffer || frame_count == 0) return true;

    size_t sample_count = frame_count * channels;

    switch (bits_per_sample) {
        case 8: {
            const uint8_t* samples = (const uint8_t*)buffer;
            uint8_t zero_point = 128;  // Center for uint8 audio
            for (size_t i = 0; i < sample_count; i++) {
                if (samples[i] != zero_point) return false;
            }
            break;
        }
        case 16: {
            const int16_t* samples = (const int16_t*)buffer;
            for (size_t i = 0; i < sample_count; i++) {
                if (samples[i] != 0) return false;
            }
            break;
        }
        default:
            return false;
    }

    return true;
}
