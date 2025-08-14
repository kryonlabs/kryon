/**
 * @file audio_effects.c
 * @brief Kryon Audio Effects Implementation
 */

#include "audio.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// EFFECT PROCESSING STRUCTURES
// =============================================================================

typedef struct {
    // Delay line
    float* buffer;
    size_t buffer_size;
    size_t write_index;
    
    // Parameters
    float delay_time;    // in seconds
    float feedback;      // 0.0 to 0.99
    float mix;          // 0.0 (dry) to 1.0 (wet)
    
    bool enabled;
} KryonDelayEffect;

typedef struct {
    // Multiple delay lines for reverb
    float* delay_lines[8];
    size_t delay_sizes[8];
    size_t write_indices[8];
    
    // All-pass filters for diffusion
    float* allpass_buffers[4];
    size_t allpass_sizes[4];
    size_t allpass_indices[4];
    
    // Parameters
    float room_size;     // 0.0 to 1.0
    float damping;       // 0.0 to 1.0
    float mix;           // 0.0 (dry) to 1.0 (wet)
    
    // Filter states
    float lowpass_state[8];
    
    bool enabled;
} KryonReverbEffect;

typedef struct {
    // Delay lines for chorus
    float* delay_buffer;
    size_t buffer_size;
    float read_position;
    size_t write_index;
    
    // LFO for modulation
    float lfo_phase;
    
    // Parameters
    float rate;          // LFO frequency in Hz
    float depth;         // Modulation depth
    float feedback;      // Feedback amount
    float mix;           // 0.0 (dry) to 1.0 (wet)
    
    bool enabled;
} KryonChorusEffect;

typedef struct {
    // Biquad filter coefficients
    double b0, b1, b2;   // Numerator coefficients
    double a1, a2;       // Denominator coefficients (a0 = 1)
    
    // Filter state
    double x1, x2;       // Input history
    double y1, y2;       // Output history
    
    // Parameters
    KryonFilterType type;
    float frequency;     // Cutoff frequency
    float resonance;     // Q factor
    float gain;          // For peaking/shelving filters
    
    bool enabled;
} KryonFilterEffect;

typedef struct {
    // Compressor state
    float envelope;
    float gain_reduction;
    
    // Parameters
    float threshold;     // dB
    float ratio;         // 1:1 to inf:1
    float attack_time;   // seconds
    float release_time;  // seconds
    float makeup_gain;   // dB
    
    bool enabled;
} KryonCompressorEffect;

typedef struct {
    // Distortion parameters
    KryonDistortionType type;
    float drive;         // 0.0 to 1.0
    float tone;          // -1.0 (dark) to 1.0 (bright)
    float mix;           // 0.0 (clean) to 1.0 (distorted)
    
    // Filter for tone control
    KryonFilterEffect tone_filter;
    
    bool enabled;
} KryonDistortionEffect;

typedef struct {
    KryonDelayEffect delay;
    KryonReverbEffect reverb;
    KryonChorusEffect chorus;
    KryonFilterEffect filter;
    KryonCompressorEffect compressor;
    KryonDistortionEffect distortion;
    
    // Global settings
    uint32_t sample_rate;
    uint16_t channels;
    
} KryonAudioEffects;

static KryonAudioEffects g_audio_effects = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

static float linear_to_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f));
}

static float interpolate_linear(float a, float b, float t) {
    return a + t * (b - a);
}

// =============================================================================
// DELAY EFFECT
// =============================================================================

static bool delay_init(KryonDelayEffect* delay, uint32_t sample_rate, float max_delay_time) {
    if (!delay) return false;
    
    delay->buffer_size = (size_t)(max_delay_time * sample_rate) + 1;
    delay->buffer = kryon_alloc(sizeof(float) * delay->buffer_size);
    if (!delay->buffer) return false;
    
    memset(delay->buffer, 0, sizeof(float) * delay->buffer_size);
    delay->write_index = 0;
    delay->delay_time = 0.1f; // 100ms default
    delay->feedback = 0.3f;
    delay->mix = 0.3f;
    delay->enabled = false;
    
    return true;
}

static void delay_cleanup(KryonDelayEffect* delay) {
    if (!delay) return;
    
    kryon_free(delay->buffer);
    memset(delay, 0, sizeof(KryonDelayEffect));
}

static float delay_process(KryonDelayEffect* delay, float input, uint32_t sample_rate) {
    if (!delay || !delay->enabled || !delay->buffer) return input;
    
    // Calculate delay in samples
    float delay_samples = delay->delay_time * sample_rate;
    size_t delay_samples_int = (size_t)delay_samples;
    float delay_frac = delay_samples - delay_samples_int;
    
    // Calculate read positions
    size_t read_index1 = (delay->write_index + delay->buffer_size - delay_samples_int) % delay->buffer_size;
    size_t read_index2 = (read_index1 + delay->buffer_size - 1) % delay->buffer_size;
    
    // Interpolated read
    float delayed_sample = interpolate_linear(delay->buffer[read_index1], 
                                            delay->buffer[read_index2], 
                                            delay_frac);
    
    // Write to buffer with feedback
    delay->buffer[delay->write_index] = input + delayed_sample * delay->feedback;
    delay->write_index = (delay->write_index + 1) % delay->buffer_size;
    
    // Mix dry and wet signals
    return input * (1.0f - delay->mix) + delayed_sample * delay->mix;
}

// =============================================================================
// REVERB EFFECT
// =============================================================================

static bool reverb_init(KryonReverbEffect* reverb, uint32_t sample_rate) {
    if (!reverb) return false;
    
    // Initialize delay line sizes (prime numbers for diffusion)
    const size_t base_delays[] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    const size_t allpass_delays[] = {225, 556, 441, 341};
    
    for (int i = 0; i < 8; i++) {
        reverb->delay_sizes[i] = base_delays[i];
        reverb->delay_lines[i] = kryon_alloc(sizeof(float) * reverb->delay_sizes[i]);
        if (!reverb->delay_lines[i]) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                kryon_free(reverb->delay_lines[j]);
            }
            return false;
        }
        memset(reverb->delay_lines[i], 0, sizeof(float) * reverb->delay_sizes[i]);
        reverb->write_indices[i] = 0;
        reverb->lowpass_state[i] = 0.0f;
    }
    
    for (int i = 0; i < 4; i++) {
        reverb->allpass_sizes[i] = allpass_delays[i];
        reverb->allpass_buffers[i] = kryon_alloc(sizeof(float) * reverb->allpass_sizes[i]);
        if (!reverb->allpass_buffers[i]) {
            // Cleanup on failure
            for (int j = 0; j < 8; j++) {
                kryon_free(reverb->delay_lines[j]);
            }
            for (int j = 0; j < i; j++) {
                kryon_free(reverb->allpass_buffers[j]);
            }
            return false;
        }
        memset(reverb->allpass_buffers[i], 0, sizeof(float) * reverb->allpass_sizes[i]);
        reverb->allpass_indices[i] = 0;
    }
    
    reverb->room_size = 0.5f;
    reverb->damping = 0.5f;
    reverb->mix = 0.3f;
    reverb->enabled = false;
    
    return true;
}

static void reverb_cleanup(KryonReverbEffect* reverb) {
    if (!reverb) return;
    
    for (int i = 0; i < 8; i++) {
        kryon_free(reverb->delay_lines[i]);
    }
    for (int i = 0; i < 4; i++) {
        kryon_free(reverb->allpass_buffers[i]);
    }
    
    memset(reverb, 0, sizeof(KryonReverbEffect));
}

static float reverb_process(KryonReverbEffect* reverb, float input) {
    if (!reverb || !reverb->enabled) return input;
    
    float output = 0.0f;
    
    // Process through delay lines
    for (int i = 0; i < 8; i++) {
        float delayed = reverb->delay_lines[i][reverb->write_indices[i]];
        
        // Apply damping (lowpass filter)
        reverb->lowpass_state[i] = delayed * (1.0f - reverb->damping) + 
                                  reverb->lowpass_state[i] * reverb->damping;
        
        // Write input with feedback
        reverb->delay_lines[i][reverb->write_indices[i]] = 
            input + reverb->lowpass_state[i] * reverb->room_size;
        
        reverb->write_indices[i] = (reverb->write_indices[i] + 1) % reverb->delay_sizes[i];
        
        output += delayed;
    }
    
    output *= 0.125f; // Normalize (divide by 8)
    
    // Process through allpass filters for diffusion
    for (int i = 0; i < 4; i++) {
        float delayed = reverb->allpass_buffers[i][reverb->allpass_indices[i]];
        float temp = output + delayed * 0.5f;
        reverb->allpass_buffers[i][reverb->allpass_indices[i]] = temp;
        output = delayed - temp * 0.5f;
        reverb->allpass_indices[i] = (reverb->allpass_indices[i] + 1) % reverb->allpass_sizes[i];
    }
    
    // Mix dry and wet signals
    return input * (1.0f - reverb->mix) + output * reverb->mix;
}

// =============================================================================
// FILTER EFFECT
// =============================================================================

static void filter_calculate_coefficients(KryonFilterEffect* filter, uint32_t sample_rate) {
    if (!filter) return;
    
    double omega = 2.0 * M_PI * filter->frequency / sample_rate;
    double sin_omega = sin(omega);
    double cos_omega = cos(omega);
    double Q = filter->resonance;
    double A = db_to_linear(filter->gain);
    double alpha = sin_omega / (2.0 * Q);
    
    switch (filter->type) {
        case KRYON_FILTER_LOWPASS:
            filter->b0 = (1.0 - cos_omega) / 2.0;
            filter->b1 = 1.0 - cos_omega;
            filter->b2 = (1.0 - cos_omega) / 2.0;
            filter->a1 = -2.0 * cos_omega;
            filter->a2 = 1.0 - alpha;
            break;
            
        case KRYON_FILTER_HIGHPASS:
            filter->b0 = (1.0 + cos_omega) / 2.0;
            filter->b1 = -(1.0 + cos_omega);
            filter->b2 = (1.0 + cos_omega) / 2.0;
            filter->a1 = -2.0 * cos_omega;
            filter->a2 = 1.0 - alpha;
            break;
            
        case KRYON_FILTER_BANDPASS:
            filter->b0 = alpha;
            filter->b1 = 0.0;
            filter->b2 = -alpha;
            filter->a1 = -2.0 * cos_omega;
            filter->a2 = 1.0 - alpha;
            break;
            
        case KRYON_FILTER_NOTCH:
            filter->b0 = 1.0;
            filter->b1 = -2.0 * cos_omega;
            filter->b2 = 1.0;
            filter->a1 = -2.0 * cos_omega;
            filter->a2 = 1.0 - alpha;
            break;
            
        default:
            // Pass-through
            filter->b0 = 1.0;
            filter->b1 = 0.0;
            filter->b2 = 0.0;
            filter->a1 = 0.0;
            filter->a2 = 0.0;
            break;
    }
    
    // Normalize coefficients
    double a0 = 1.0 + alpha;
    filter->b0 /= a0;
    filter->b1 /= a0;
    filter->b2 /= a0;
    filter->a1 /= a0;
    filter->a2 /= a0;
}

static float filter_process(KryonFilterEffect* filter, float input, uint32_t sample_rate) {
    if (!filter || !filter->enabled) return input;
    
    // Apply biquad filter
    double output = filter->b0 * input + filter->b1 * filter->x1 + filter->b2 * filter->x2
                   - filter->a1 * filter->y1 - filter->a2 * filter->y2;
    
    // Update state
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    return (float)output;
}

// =============================================================================
// COMPRESSOR EFFECT
// =============================================================================

static float compressor_process(KryonCompressorEffect* compressor, float input, uint32_t sample_rate) {
    if (!compressor || !compressor->enabled) return input;
    
    float input_level = fabsf(input);
    float input_db = linear_to_db(input_level);
    
    // Calculate gain reduction
    float gain_reduction_db = 0.0f;
    if (input_db > compressor->threshold) {
        float over_threshold = input_db - compressor->threshold;
        gain_reduction_db = over_threshold * (1.0f - 1.0f / compressor->ratio);
    }
    
    // Smooth gain reduction with attack/release
    float target_gain = db_to_linear(-gain_reduction_db);
    float attack_coeff = expf(-1.0f / (compressor->attack_time * sample_rate));
    float release_coeff = expf(-1.0f / (compressor->release_time * sample_rate));
    
    if (target_gain < compressor->envelope) {
        // Attack
        compressor->envelope = target_gain + (compressor->envelope - target_gain) * attack_coeff;
    } else {
        // Release
        compressor->envelope = target_gain + (compressor->envelope - target_gain) * release_coeff;
    }
    
    // Apply compression and makeup gain
    float makeup_linear = db_to_linear(compressor->makeup_gain);
    return input * compressor->envelope * makeup_linear;
}

// =============================================================================
// DISTORTION EFFECT
// =============================================================================

static float distortion_waveshape(float input, KryonDistortionType type, float drive) {
    float driven = input * (1.0f + drive * 10.0f);
    
    switch (type) {
        case KRYON_DISTORTION_SOFT_CLIP:
            if (driven > 1.0f) return 1.0f - expf(-(driven - 1.0f));
            if (driven < -1.0f) return -1.0f + expf(driven + 1.0f);
            return driven;
            
        case KRYON_DISTORTION_HARD_CLIP:
            return fmaxf(-1.0f, fminf(1.0f, driven));
            
        case KRYON_DISTORTION_TUBE:
            return tanhf(driven);
            
        case KRYON_DISTORTION_FUZZ:
            return (driven > 0.0f) ? 1.0f : -1.0f;
            
        default:
            return input;
    }
}

static float distortion_process(KryonDistortionEffect* distortion, float input, uint32_t sample_rate) {
    if (!distortion || !distortion->enabled) return input;
    
    // Apply waveshaping
    float distorted = distortion_waveshape(input, distortion->type, distortion->drive);
    
    // Apply tone control (simple high-frequency rolloff/boost)
    distortion->tone_filter.frequency = 1000.0f + distortion->tone * 3000.0f;
    distortion->tone_filter.type = (distortion->tone > 0) ? KRYON_FILTER_HIGHPASS : KRYON_FILTER_LOWPASS;
    filter_calculate_coefficients(&distortion->tone_filter, sample_rate);
    distorted = filter_process(&distortion->tone_filter, distorted, sample_rate);
    
    // Mix clean and distorted signals
    return input * (1.0f - distortion->mix) + distorted * distortion->mix;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_audio_effects_init(uint32_t sample_rate, uint16_t channels) {
    memset(&g_audio_effects, 0, sizeof(g_audio_effects));
    
    g_audio_effects.sample_rate = sample_rate;
    g_audio_effects.channels = channels;
    
    // Initialize effects
    if (!delay_init(&g_audio_effects.delay, sample_rate, 2.0f)) return false;
    if (!reverb_init(&g_audio_effects.reverb, sample_rate)) return false;
    
    // Initialize chorus
    g_audio_effects.chorus.buffer_size = sample_rate / 4; // 250ms max delay
    g_audio_effects.chorus.delay_buffer = kryon_alloc(sizeof(float) * g_audio_effects.chorus.buffer_size);
    if (!g_audio_effects.chorus.delay_buffer) return false;
    memset(g_audio_effects.chorus.delay_buffer, 0, sizeof(float) * g_audio_effects.chorus.buffer_size);
    g_audio_effects.chorus.rate = 1.0f;
    g_audio_effects.chorus.depth = 0.1f;
    g_audio_effects.chorus.feedback = 0.2f;
    g_audio_effects.chorus.mix = 0.5f;
    
    // Initialize filter
    g_audio_effects.filter.type = KRYON_FILTER_LOWPASS;
    g_audio_effects.filter.frequency = sample_rate * 0.25f;
    g_audio_effects.filter.resonance = 1.0f;
    g_audio_effects.filter.gain = 0.0f;
    filter_calculate_coefficients(&g_audio_effects.filter, sample_rate);
    
    // Initialize compressor
    g_audio_effects.compressor.threshold = -12.0f;
    g_audio_effects.compressor.ratio = 4.0f;
    g_audio_effects.compressor.attack_time = 0.003f;  // 3ms
    g_audio_effects.compressor.release_time = 0.1f;   // 100ms
    g_audio_effects.compressor.makeup_gain = 6.0f;
    g_audio_effects.compressor.envelope = 1.0f;
    
    // Initialize distortion
    g_audio_effects.distortion.type = KRYON_DISTORTION_SOFT_CLIP;
    g_audio_effects.distortion.drive = 0.5f;
    g_audio_effects.distortion.tone = 0.0f;
    g_audio_effects.distortion.mix = 1.0f;
    g_audio_effects.distortion.tone_filter.resonance = 1.0f;
    
    return true;
}

void kryon_audio_effects_shutdown(void) {
    delay_cleanup(&g_audio_effects.delay);
    reverb_cleanup(&g_audio_effects.reverb);
    kryon_free(g_audio_effects.chorus.delay_buffer);
    
    memset(&g_audio_effects, 0, sizeof(g_audio_effects));
}

void kryon_audio_effects_process(void* buffer, size_t sample_count) {
    if (!buffer) return;
    
    float* samples = (float*)buffer;
    
    for (size_t i = 0; i < sample_count * g_audio_effects.channels; i += g_audio_effects.channels) {
        for (uint16_t ch = 0; ch < g_audio_effects.channels; ch++) {
            float sample = samples[i + ch];
            
            // Process through effects chain
            sample = filter_process(&g_audio_effects.filter, sample, g_audio_effects.sample_rate);
            sample = compressor_process(&g_audio_effects.compressor, sample, g_audio_effects.sample_rate);
            sample = distortion_process(&g_audio_effects.distortion, sample, g_audio_effects.sample_rate);
            sample = delay_process(&g_audio_effects.delay, sample, g_audio_effects.sample_rate);
            sample = reverb_process(&g_audio_effects.reverb, sample);
            
            samples[i + ch] = sample;
        }
    }
}

// Effect control functions
void kryon_audio_effects_set_delay(bool enabled, float delay_time, float feedback, float mix) {
    g_audio_effects.delay.enabled = enabled;
    g_audio_effects.delay.delay_time = fmaxf(0.001f, fminf(2.0f, delay_time));
    g_audio_effects.delay.feedback = fmaxf(0.0f, fminf(0.99f, feedback));
    g_audio_effects.delay.mix = fmaxf(0.0f, fminf(1.0f, mix));
}

void kryon_audio_effects_set_reverb(bool enabled, float room_size, float damping, float mix) {
    g_audio_effects.reverb.enabled = enabled;
    g_audio_effects.reverb.room_size = fmaxf(0.0f, fminf(1.0f, room_size));
    g_audio_effects.reverb.damping = fmaxf(0.0f, fminf(1.0f, damping));
    g_audio_effects.reverb.mix = fmaxf(0.0f, fminf(1.0f, mix));
}

void kryon_audio_effects_set_filter(bool enabled, KryonFilterType type, float frequency, float resonance) {
    g_audio_effects.filter.enabled = enabled;
    g_audio_effects.filter.type = type;
    g_audio_effects.filter.frequency = fmaxf(20.0f, fminf(g_audio_effects.sample_rate * 0.45f, frequency));
    g_audio_effects.filter.resonance = fmaxf(0.1f, fminf(20.0f, resonance));
    filter_calculate_coefficients(&g_audio_effects.filter, g_audio_effects.sample_rate);
}

void kryon_audio_effects_set_compressor(bool enabled, float threshold, float ratio, 
                                       float attack, float release, float makeup_gain) {
    g_audio_effects.compressor.enabled = enabled;
    g_audio_effects.compressor.threshold = fmaxf(-60.0f, fminf(0.0f, threshold));
    g_audio_effects.compressor.ratio = fmaxf(1.0f, fminf(20.0f, ratio));
    g_audio_effects.compressor.attack_time = fmaxf(0.001f, fminf(1.0f, attack));
    g_audio_effects.compressor.release_time = fmaxf(0.01f, fminf(5.0f, release));
    g_audio_effects.compressor.makeup_gain = fmaxf(-20.0f, fminf(20.0f, makeup_gain));
}

void kryon_audio_effects_set_distortion(bool enabled, KryonDistortionType type, 
                                       float drive, float tone, float mix) {
    g_audio_effects.distortion.enabled = enabled;
    g_audio_effects.distortion.type = type;
    g_audio_effects.distortion.drive = fmaxf(0.0f, fminf(1.0f, drive));
    g_audio_effects.distortion.tone = fmaxf(-1.0f, fminf(1.0f, tone));
    g_audio_effects.distortion.mix = fmaxf(0.0f, fminf(1.0f, mix));
}