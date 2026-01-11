#ifndef IR_AUDIO_MIXER_H
#define IR_AUDIO_MIXER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio Mixer Utilities
 *
 * Software audio mixing for volume control, panning, and mixing.
 * Designed for 16-bit PCM audio data.
 *
 * Usage:
 *   // Apply volume to buffer
 *   ir_audio_apply_volume(buffer, frame_count, 2, 0.8f);
 *
 *   // Apply stereo pan
 *   ir_audio_apply_pan_stereo(buffer, frame_count, -0.5f);
 *
 *   // Mix multiple buffers
 *   ir_audio_mix_buffers(dest, src1, src2, frame_count, 2);
 */

// ============================================================================
// Constants
// ============================================================================

#define IR_AUDIO_MAX_VOLUME 1.0f
#define IR_AUDIO_MIN_VOLUME 0.0f

#define IR_AUDIO_PAN_LEFT -1.0f
#define IR_AUDIO_PAN_CENTER 0.0f
#define IR_AUDIO_PAN_RIGHT 1.0f

// ============================================================================
// Volume Control
// ============================================================================

/**
 * Apply volume to 16-bit PCM audio buffer
 * @param samples Audio samples (interleaved)
 * @param sample_count Number of samples (not frames)
 * @param volume Volume multiplier (0.0 to 1.0)
 */
void ir_audio_apply_volume_int16(int16_t* samples, size_t sample_count, float volume);

/**
 * Apply volume to float PCM audio buffer
 * @param samples Audio samples (interleaved)
 * @param sample_count Number of samples (not frames)
 * @param volume Volume multiplier (0.0 to 1.0, can exceed 1.0 for gain)
 */
void ir_audio_apply_volume_float(float* samples, size_t sample_count, float volume);

/**
 * Apply volume to 8-bit PCM audio buffer
 * @param samples Audio samples (interleaved)
 * @param sample_count Number of samples (not frames)
 * @param volume Volume multiplier (0.0 to 1.0)
 */
void ir_audio_apply_volume_uint8(uint8_t* samples, size_t sample_count, float volume);

/**
 * Generic volume application (detects format)
 * @param samples Audio samples
 * @param sample_count Number of samples
 * @param volume Volume multiplier
 * @param bits_per_sample 8 or 16
 */
void ir_audio_apply_volume(void* samples, size_t sample_count, float volume, int bits_per_sample);

// ============================================================================
// Stereo Panning
// ============================================================================

/**
 * Apply stereo panning to interleaved 16-bit PCM
 * Uses constant power panning for equal loudness
 * @param samples Stereo samples (L, R, L, R, ...)
 * @param frame_count Number of frames (pairs of samples)
 * @param pan Pan value (-1.0 = left, 0.0 = center, 1.0 = right)
 */
void ir_audio_apply_pan_stereo_int16(int16_t* samples, size_t frame_count, float pan);

/**
 * Apply stereo panning to interleaved float PCM
 * @param samples Stereo samples (L, R, L, R, ...)
 * @param frame_count Number of frames (pairs of samples)
 * @param pan Pan value (-1.0 = left, 0.0 = center, 1.0 = right)
 */
void ir_audio_apply_pan_stereo_float(float* samples, size_t frame_count, float pan);

/**
 * Calculate left/right gain values for constant power panning
 * @param pan Pan value (-1.0 to 1.0)
 * @param out_left Output left channel gain
 * @param out_right Output right channel gain
 */
void ir_audio_calculate_pan_gains(float pan, float* out_left, float* out_right);

/**
 * Simple linear panning (faster but not equal loudness)
 * @param pan Pan value (-1.0 to 1.0)
 * @param out_left Output left channel gain
 * @param out_right Output right channel gain
 */
void ir_audio_calculate_pan_gains_linear(float pan, float* out_left, float* out_right);

// ============================================================================
// Audio Mixing
// ============================================================================

/**
 * Mix two audio buffers (additive mixing)
 * Results are clamped to prevent overflow
 * @param dest Destination buffer (accumulates result)
 * @param src Source buffer to add
 * @param frame_count Number of frames
 * @param channels Number of channels (1 or 2)
 */
void ir_audio_mix_buffers_int16(int16_t* dest, const int16_t* src,
                                 size_t frame_count, int channels);

/**
 * Mix two audio buffers with per-channel volume
 * @param dest Destination buffer
 * @param src Source buffer
 * @param frame_count Number of frames
 * @param channels Number of channels
 * @param volume Volume for source buffer
 */
void ir_audio_mix_buffers_volume_int16(int16_t* dest, const int16_t* src,
                                       size_t frame_count, int channels, float volume);

/**
 * Mix two float audio buffers
 * @param dest Destination buffer
 * @param src Source buffer
 * @param frame_count Number of frames
 * @param channels Number of channels
 */
void ir_audio_mix_buffers_float(float* dest, const float* src,
                                size_t frame_count, int channels);

// ============================================================================
// Conversions
// ============================================================================

/**
 * Convert uint8 audio (0-255) to int16 (-32768 to 32767)
 * @param src Source uint8 samples
 * @param dest Destination int16 samples
 * @param sample_count Number of samples to convert
 */
void ir_audio_convert_uint8_to_int16(const uint8_t* src, int16_t* dest, size_t sample_count);

/**
 * Convert int16 audio to uint8
 * @param src Source int16 samples
 * @param dest Destination uint8 samples
 * @param sample_count Number of samples to convert
 */
void ir_audio_convert_int16_to_uint8(const int16_t* src, uint8_t* dest, size_t sample_count);

/**
 * Convert float audio (-1.0 to 1.0) to int16
 * @param src Source float samples
 * @param dest Destination int16 samples
 * @param sample_count Number of samples to convert
 */
void ir_audio_convert_float_to_int16(const float* src, int16_t* dest, size_t sample_count);

/**
 * Convert int16 audio to float
 * @param src Source int16 samples
 * @param dest Destination float samples
 * @param sample_count Number of samples to convert
 */
void ir_audio_convert_int16_to_float(const int16_t* src, float* dest, size_t sample_count);

// ============================================================================
// Utilities
// ============================================================================

/**
 * Clamp sample to int16 range
 */
static inline int16_t ir_audio_clamp_int16(int32_t value) {
    if (value < INT16_MIN) return INT16_MIN;
    if (value > INT16_MAX) return INT16_MAX;
    return (int16_t)value;
}

/**
 * Clamp float sample to [-1.0, 1.0]
 */
static inline float ir_audio_clamp_float(float value) {
    if (value < -1.0f) return -1.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

/**
 * Fade in audio (apply ramp from 0 to target volume)
 * @param samples Audio buffer to modify
 * @param frame_count Number of frames
 * @param channels Number of channels
 * @param target_volume Target volume at end of buffer
 */
void ir_audio_fade_in_int16(int16_t* samples, size_t frame_count, int channels, float target_volume);

/**
 * Fade out audio (apply ramp from current volume to 0)
 * @param samples Audio buffer to modify
 * @param frame_count Number of frames
 * @param channels Number of channels
 * @param start_volume Starting volume
 */
void ir_audio_fade_out_int16(int16_t* samples, size_t frame_count, int channels, float start_volume);

/**
 * Cross-fade between two audio buffers
 * @param src1 First buffer (fades out)
 * @param src2 Second buffer (fades in)
 * @param dest Destination buffer
 * @param frame_count Number of frames
 * @param channels Number of channels
 */
void ir_audio_crossfade_int16(const int16_t* src1, const int16_t* src2,
                               int16_t* dest, size_t frame_count, int channels);

// ============================================================================
// Silence
// ============================================================================

/**
 * Fill buffer with silence
 * @param buffer Audio buffer
 * @param frame_count Number of frames
 * @param channels Number of channels
 * @param bits_per_sample 8 or 16
 */
void ir_audio_silence(void* buffer, size_t frame_count, int channels, int bits_per_sample);

/**
 * Check if buffer is silent (all zeros)
 * @param buffer Audio buffer
 * @param frame_count Number of frames
 * @param channels Number of channels
 * @param bits_per_sample 8 or 16
 * @return true if buffer is silent
 */
bool ir_audio_is_silent(const void* buffer, size_t frame_count, int channels, int bits_per_sample);

#ifdef __cplusplus
}
#endif

#endif // IR_AUDIO_MIXER_H
