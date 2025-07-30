/**
 * @file synthesizer.c
 * @brief Kryon Audio Synthesizer Implementation
 */

#include "internal/audio.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =============================================================================
// WAVEFORM GENERATORS
// =============================================================================

typedef struct {
    double phase;
    double frequency;
    double amplitude;
    KryonWaveformType waveform;
    
    // Envelope parameters
    double attack_time;
    double decay_time;
    double sustain_level;
    double release_time;
    
    // Envelope state
    KryonEnvelopePhase envelope_phase;
    double envelope_start_time;
    double envelope_value;
    double note_on_time;
    double note_off_time;
    
    // Modulation
    double vibrato_frequency;
    double vibrato_depth;
    double tremolo_frequency;
    double tremolo_depth;
    
    // Filter parameters
    double cutoff_frequency;
    double resonance;
    double filter_state[2]; // For biquad filter
    
} KryonOscillator;

typedef struct {
    KryonOscillator oscillators[KRYON_MAX_OSCILLATORS];
    size_t oscillator_count;
    
    // Global synthesizer parameters
    float master_volume;
    uint32_t sample_rate;
    uint16_t channels;
    
    // Voice management
    struct {
        uint8_t note;
        uint8_t velocity;
        bool is_active;
        double start_time;
        KryonOscillator* oscillator;
    } voices[KRYON_MAX_VOICES];
    size_t active_voices;
    
    // LFO (Low Frequency Oscillator)
    struct {
        double phase;
        double frequency;
        double amplitude;
        KryonWaveformType waveform;
    } lfo;
    
    // Effects
    bool reverb_enabled;
    float reverb_amount;
    float reverb_delay_buffer[KRYON_REVERB_BUFFER_SIZE];
    size_t reverb_delay_index;
    
    bool chorus_enabled;
    float chorus_depth;
    float chorus_rate;
    float chorus_delay_buffer[KRYON_CHORUS_BUFFER_SIZE];
    size_t chorus_delay_index;
    
} KryonSynthesizer;

static KryonSynthesizer g_synthesizer = {0};

// =============================================================================
// WAVEFORM GENERATION FUNCTIONS
// =============================================================================

static double generate_sine_wave(double phase) {
    return sin(2.0 * M_PI * phase);
}

static double generate_square_wave(double phase) {
    return (phase < 0.5) ? 1.0 : -1.0;
}

static double generate_triangle_wave(double phase) {
    if (phase < 0.25) {
        return 4.0 * phase;
    } else if (phase < 0.75) {
        return 2.0 - 4.0 * phase;
    } else {
        return 4.0 * phase - 4.0;
    }
}

static double generate_sawtooth_wave(double phase) {
    return 2.0 * phase - 1.0;
}

static double generate_noise(void) {
    return ((double)rand() / RAND_MAX) * 2.0 - 1.0;
}

static double generate_waveform(KryonWaveformType type, double phase) {
    switch (type) {
        case KRYON_WAVEFORM_SINE:
            return generate_sine_wave(phase);
        case KRYON_WAVEFORM_SQUARE:
            return generate_square_wave(phase);
        case KRYON_WAVEFORM_TRIANGLE:
            return generate_triangle_wave(phase);
        case KRYON_WAVEFORM_SAWTOOTH:
            return generate_sawtooth_wave(phase);
        case KRYON_WAVEFORM_NOISE:
            return generate_noise();
        default:
            return 0.0;
    }
}

// =============================================================================
// ENVELOPE GENERATOR
// =============================================================================

static double calculate_envelope(KryonOscillator* osc, double current_time) {
    if (!osc) return 0.0;
    
    double envelope_time = current_time - osc->envelope_start_time;
    
    switch (osc->envelope_phase) {
        case KRYON_ENVELOPE_ATTACK:
            if (envelope_time < osc->attack_time) {
                osc->envelope_value = envelope_time / osc->attack_time;
            } else {
                osc->envelope_phase = KRYON_ENVELOPE_DECAY;
                osc->envelope_start_time = current_time;
                osc->envelope_value = 1.0;
            }
            break;
            
        case KRYON_ENVELOPE_DECAY:
            if (envelope_time < osc->decay_time) {
                double decay_progress = envelope_time / osc->decay_time;
                osc->envelope_value = 1.0 - decay_progress * (1.0 - osc->sustain_level);
            } else {
                osc->envelope_phase = KRYON_ENVELOPE_SUSTAIN;
                osc->envelope_value = osc->sustain_level;
            }
            break;
            
        case KRYON_ENVELOPE_SUSTAIN:
            osc->envelope_value = osc->sustain_level;
            break;
            
        case KRYON_ENVELOPE_RELEASE:
            if (envelope_time < osc->release_time) {
                double release_progress = envelope_time / osc->release_time;
                osc->envelope_value = osc->sustain_level * (1.0 - release_progress);
            } else {
                osc->envelope_value = 0.0;
            }
            break;
            
        default:
            osc->envelope_value = 0.0;
            break;
    }
    
    return fmax(0.0, fmin(1.0, osc->envelope_value));
}

// =============================================================================
// FILTER IMPLEMENTATION
// =============================================================================

static double apply_lowpass_filter(KryonOscillator* osc, double input) {
    // Simple biquad lowpass filter
    double cutoff = osc->cutoff_frequency;
    double q = osc->resonance;
    
    if (cutoff <= 0.0 || cutoff >= g_synthesizer.sample_rate * 0.5) {
        return input; // No filtering
    }
    
    // Calculate filter coefficients
    double omega = 2.0 * M_PI * cutoff / g_synthesizer.sample_rate;
    double sin_omega = sin(omega);
    double cos_omega = cos(omega);
    double alpha = sin_omega / (2.0 * q);
    
    double b0 = (1.0 - cos_omega) / 2.0;
    double b1 = 1.0 - cos_omega;
    double b2 = (1.0 - cos_omega) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cos_omega;
    double a2 = 1.0 - alpha;
    
    // Apply filter
    double output = (b0 * input + b1 * osc->filter_state[0] + b2 * osc->filter_state[1] -
                    a1 * osc->filter_state[0] - a2 * osc->filter_state[1]) / a0;
    
    // Update filter state
    osc->filter_state[1] = osc->filter_state[0];
    osc->filter_state[0] = output;
    
    return output;
}

// =============================================================================
// EFFECTS PROCESSING
// =============================================================================

static double apply_reverb(double input) {
    if (!g_synthesizer.reverb_enabled) return input;
    
    // Simple delay-based reverb
    double delayed = g_synthesizer.reverb_delay_buffer[g_synthesizer.reverb_delay_index];
    g_synthesizer.reverb_delay_buffer[g_synthesizer.reverb_delay_index] = 
        input + delayed * 0.3; // Feedback
    
    g_synthesizer.reverb_delay_index = 
        (g_synthesizer.reverb_delay_index + 1) % KRYON_REVERB_BUFFER_SIZE;
    
    return input + delayed * g_synthesizer.reverb_amount;
}

static double apply_chorus(double input) {
    if (!g_synthesizer.chorus_enabled) return input;
    
    // Simple chorus effect
    double delayed = g_synthesizer.chorus_delay_buffer[g_synthesizer.chorus_delay_index];
    g_synthesizer.chorus_delay_buffer[g_synthesizer.chorus_delay_index] = input;
    
    g_synthesizer.chorus_delay_index = 
        (g_synthesizer.chorus_delay_index + 1) % KRYON_CHORUS_BUFFER_SIZE;
    
    // Modulate delay time with LFO
    double lfo_value = sin(2.0 * M_PI * g_synthesizer.lfo.phase);
    double modulated_delay = delayed * (1.0 + lfo_value * g_synthesizer.chorus_depth);
    
    return (input + modulated_delay) * 0.5;
}

// =============================================================================
// VOICE MANAGEMENT
// =============================================================================

static KryonOscillator* find_free_oscillator(void) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        bool is_used = false;
        for (size_t j = 0; j < KRYON_MAX_VOICES; j++) {
            if (g_synthesizer.voices[j].is_active && 
                g_synthesizer.voices[j].oscillator == &g_synthesizer.oscillators[i]) {
                is_used = true;
                break;
            }
        }
        if (!is_used) {
            return &g_synthesizer.oscillators[i];
        }
    }
    return NULL;
}

static size_t find_free_voice(void) {
    for (size_t i = 0; i < KRYON_MAX_VOICES; i++) {
        if (!g_synthesizer.voices[i].is_active) {
            return i;
        }
    }
    return KRYON_MAX_VOICES; // No free voice
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_synthesizer_init(uint32_t sample_rate, uint16_t channels) {
    memset(&g_synthesizer, 0, sizeof(g_synthesizer));
    
    g_synthesizer.sample_rate = sample_rate;
    g_synthesizer.channels = channels;
    g_synthesizer.master_volume = 1.0f;
    
    // Initialize default oscillator
    g_synthesizer.oscillator_count = 1;
    KryonOscillator* osc = &g_synthesizer.oscillators[0];
    osc->waveform = KRYON_WAVEFORM_SINE;
    osc->amplitude = 0.5;
    osc->attack_time = 0.01;  // 10ms
    osc->decay_time = 0.1;    // 100ms
    osc->sustain_level = 0.7;
    osc->release_time = 0.5;  // 500ms
    osc->cutoff_frequency = sample_rate * 0.25; // Quarter sample rate
    osc->resonance = 1.0;
    
    // Initialize LFO
    g_synthesizer.lfo.frequency = 5.0; // 5 Hz
    g_synthesizer.lfo.amplitude = 1.0;
    g_synthesizer.lfo.waveform = KRYON_WAVEFORM_SINE;
    
    // Initialize effects
    g_synthesizer.reverb_amount = 0.2f;
    g_synthesizer.chorus_depth = 0.1f;
    g_synthesizer.chorus_rate = 1.0f;
    
    return true;
}

void kryon_synthesizer_shutdown(void) {
    // Stop all voices
    for (size_t i = 0; i < KRYON_MAX_VOICES; i++) {
        g_synthesizer.voices[i].is_active = false;
    }
    
    memset(&g_synthesizer, 0, sizeof(g_synthesizer));
}

bool kryon_synthesizer_note_on(uint8_t note, uint8_t velocity) {
    if (note > 127 || velocity > 127) return false;
    
    size_t voice_index = find_free_voice();
    if (voice_index >= KRYON_MAX_VOICES) return false;
    
    KryonOscillator* osc = find_free_oscillator();
    if (!osc) return false;
    
    // Calculate frequency from MIDI note
    double frequency = 440.0 * pow(2.0, (note - 69) / 12.0);
    
    // Initialize oscillator
    osc->frequency = frequency;
    osc->phase = 0.0;
    osc->envelope_phase = KRYON_ENVELOPE_ATTACK;
    osc->envelope_start_time = kryon_platform_get_time();
    osc->envelope_value = 0.0;
    
    // Initialize voice
    g_synthesizer.voices[voice_index].note = note;
    g_synthesizer.voices[voice_index].velocity = velocity;
    g_synthesizer.voices[voice_index].is_active = true;
    g_synthesizer.voices[voice_index].start_time = kryon_platform_get_time();
    g_synthesizer.voices[voice_index].oscillator = osc;
    
    g_synthesizer.active_voices++;
    return true;
}

bool kryon_synthesizer_note_off(uint8_t note) {
    if (note > 127) return false;
    
    for (size_t i = 0; i < KRYON_MAX_VOICES; i++) {
        if (g_synthesizer.voices[i].is_active && 
            g_synthesizer.voices[i].note == note) {
            
            // Start release phase
            KryonOscillator* osc = g_synthesizer.voices[i].oscillator;
            if (osc) {
                osc->envelope_phase = KRYON_ENVELOPE_RELEASE;
                osc->envelope_start_time = kryon_platform_get_time();
            }
            
            return true;
        }
    }
    
    return false;
}

void kryon_synthesizer_all_notes_off(void) {
    for (size_t i = 0; i < KRYON_MAX_VOICES; i++) {
        if (g_synthesizer.voices[i].is_active) {
            KryonOscillator* osc = g_synthesizer.voices[i].oscillator;
            if (osc) {
                osc->envelope_phase = KRYON_ENVELOPE_RELEASE;
                osc->envelope_start_time = kryon_platform_get_time();
            }
        }
    }
}

void kryon_synthesizer_set_waveform(KryonWaveformType waveform) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        g_synthesizer.oscillators[i].waveform = waveform;
    }
}

void kryon_synthesizer_set_envelope(double attack, double decay, double sustain, double release) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        KryonOscillator* osc = &g_synthesizer.oscillators[i];
        osc->attack_time = fmax(0.001, attack);  // Minimum 1ms
        osc->decay_time = fmax(0.001, decay);
        osc->sustain_level = fmax(0.0, fmin(1.0, sustain));
        osc->release_time = fmax(0.001, release);
    }
}

void kryon_synthesizer_set_filter(double cutoff, double resonance) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        KryonOscillator* osc = &g_synthesizer.oscillators[i];
        osc->cutoff_frequency = fmax(20.0, fmin(g_synthesizer.sample_rate * 0.45, cutoff));
        osc->resonance = fmax(0.1, fmin(10.0, resonance));
    }
}

void kryon_synthesizer_set_vibrato(double frequency, double depth) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        KryonOscillator* osc = &g_synthesizer.oscillators[i];
        osc->vibrato_frequency = fmax(0.0, fmin(20.0, frequency));
        osc->vibrato_depth = fmax(0.0, fmin(1.0, depth));
    }
}

void kryon_synthesizer_set_tremolo(double frequency, double depth) {
    for (size_t i = 0; i < g_synthesizer.oscillator_count; i++) {
        KryonOscillator* osc = &g_synthesizer.oscillators[i];
        osc->tremolo_frequency = fmax(0.0, fmin(20.0, frequency));
        osc->tremolo_depth = fmax(0.0, fmin(1.0, depth));
    }
}

void kryon_synthesizer_set_reverb(bool enabled, float amount) {
    g_synthesizer.reverb_enabled = enabled;
    g_synthesizer.reverb_amount = fmaxf(0.0f, fminf(1.0f, amount));
}

void kryon_synthesizer_set_chorus(bool enabled, float depth, float rate) {
    g_synthesizer.chorus_enabled = enabled;
    g_synthesizer.chorus_depth = fmaxf(0.0f, fminf(1.0f, depth));
    g_synthesizer.chorus_rate = fmaxf(0.1f, fminf(10.0f, rate));
}

void kryon_synthesizer_set_master_volume(float volume) {
    g_synthesizer.master_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

size_t kryon_synthesizer_generate_samples(void* buffer, size_t sample_count) {
    if (!buffer) return 0;
    
    float* output = (float*)buffer;
    double current_time = kryon_platform_get_time();
    double time_per_sample = 1.0 / g_synthesizer.sample_rate;
    
    for (size_t sample = 0; sample < sample_count; sample++) {
        double mixed_sample = 0.0;
        
        // Update LFO
        g_synthesizer.lfo.phase += g_synthesizer.lfo.frequency * time_per_sample;
        if (g_synthesizer.lfo.phase >= 1.0) {
            g_synthesizer.lfo.phase -= 1.0;
        }
        
        double lfo_value = generate_waveform(g_synthesizer.lfo.waveform, g_synthesizer.lfo.phase);
        
        // Process all active voices
        for (size_t voice = 0; voice < KRYON_MAX_VOICES; voice++) {
            if (!g_synthesizer.voices[voice].is_active) continue;
            
            KryonOscillator* osc = g_synthesizer.voices[voice].oscillator;
            if (!osc) continue;
            
            // Calculate envelope
            double envelope = calculate_envelope(osc, current_time);
            
            // Check if voice should be stopped
            if (osc->envelope_phase == KRYON_ENVELOPE_RELEASE && envelope <= 0.001) {
                g_synthesizer.voices[voice].is_active = false;
                g_synthesizer.active_voices--;
                continue;
            }
            
            // Apply vibrato
            double vibrato_mod = 1.0 + osc->vibrato_depth * lfo_value * 0.1;
            double current_frequency = osc->frequency * vibrato_mod;
            
            // Generate waveform
            double waveform_sample = generate_waveform(osc->waveform, osc->phase);
            
            // Apply tremolo
            double tremolo_mod = 1.0 + osc->tremolo_depth * lfo_value * 0.5;
            waveform_sample *= tremolo_mod;
            
            // Apply filter
            waveform_sample = apply_lowpass_filter(osc, waveform_sample);
            
            // Apply envelope and amplitude
            waveform_sample *= envelope * osc->amplitude;
            
            // Apply velocity scaling
            waveform_sample *= (g_synthesizer.voices[voice].velocity / 127.0);
            
            mixed_sample += waveform_sample;
            
            // Update phase
            osc->phase += current_frequency * time_per_sample;
            if (osc->phase >= 1.0) {
                osc->phase -= 1.0;
            }
        }
        
        // Apply effects
        mixed_sample = apply_reverb(mixed_sample);
        mixed_sample = apply_chorus(mixed_sample);
        
        // Apply master volume and clipping
        mixed_sample *= g_synthesizer.master_volume;
        mixed_sample = fmax(-1.0, fmin(1.0, mixed_sample));
        
        // Output to all channels
        for (uint16_t ch = 0; ch < g_synthesizer.channels; ch++) {
            output[sample * g_synthesizer.channels + ch] = (float)mixed_sample;
        }
        
        current_time += time_per_sample;
    }
    
    return sample_count;
}

size_t kryon_synthesizer_get_active_voices(void) {
    return g_synthesizer.active_voices;
}