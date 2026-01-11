/*
 * Kryon Audio Decoder Implementation
 *
 * Integrates stb_vorbis (OGG) and dr_mp3 (MP3) for audio decoding.
 * All output is converted to 16-bit signed PCM for compatibility.
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_audio_decoder.h"
#include "ir_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>  // For strcasecmp on POSIX

// ============================================================================
// Decoder Library Includes
// ============================================================================

// STB Vorbis for OGG decoding
#define STB_VORBIS_NO_PUSHDATA_API 1
#define STB_VORBIS_NO_STDIO 1
#include "../third_party/stb/stb_vorbis.c"

// dr_mp3 for MP3 decoding
#define DR_MP3_IMPLEMENTATION 1
#define DR_MP3_NO_STDIO 1
#include "../third_party/dr_mp3.h"

// ============================================================================
// Format Detection
// ============================================================================

IRAudioFormat ir_audio_detect_format(const char* path) {
    if (!path) return IR_AUDIO_FORMAT_UNKNOWN;

    const char* ext = strrchr(path, '.');
    if (!ext) return IR_AUDIO_FORMAT_UNKNOWN;
    ext++; // Skip the dot

    // Case-insensitive comparison
    if (strcasecmp(ext, "wav") == 0) return IR_AUDIO_FORMAT_WAV;
    if (strcasecmp(ext, "wave") == 0) return IR_AUDIO_FORMAT_WAV;
    if (strcasecmp(ext, "ogg") == 0) return IR_AUDIO_FORMAT_OGG;
    if (strcasecmp(ext, "oga") == 0) return IR_AUDIO_FORMAT_OGG;
    if (strcasecmp(ext, "mp3") == 0) return IR_AUDIO_FORMAT_MP3;
    // FLAC, OPUS for future
    if (strcasecmp(ext, "flac") == 0) return IR_AUDIO_FORMAT_FLAC;
    if (strcasecmp(ext, "opus") == 0) return IR_AUDIO_FORMAT_OPUS;

    return IR_AUDIO_FORMAT_UNKNOWN;
}

IRAudioFormat ir_audio_detect_format_from_memory(const void* data, size_t size) {
    if (!data || size < 12) return IR_AUDIO_FORMAT_UNKNOWN;

    const uint8_t* bytes = (const uint8_t*)data;

    // WAV: RIFF header
    if (size >= 12 &&
        bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'A' && bytes[10] == 'V' && bytes[11] == 'E') {
        return IR_AUDIO_FORMAT_WAV;
    }

    // OGG: OggS header
    if (size >= 4 &&
        bytes[0] == 'O' && bytes[1] == 'g' && bytes[2] == 'g' && bytes[3] == 'S') {
        return IR_AUDIO_FORMAT_OGG;
    }

    // MP3: Check for ID3v2 tag or MP3 sync word
    if (size >= 3) {
        // ID3v2 tag
        if (bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') {
            return IR_AUDIO_FORMAT_MP3;
        }
        // MP3 sync word (0xFF followed by 0xE0-0xFF)
        if (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0) {
            return IR_AUDIO_FORMAT_MP3;
        }
    }

    return IR_AUDIO_FORMAT_UNKNOWN;
}

const char* ir_audio_format_name(IRAudioFormat format) {
    switch (format) {
        case IR_AUDIO_FORMAT_WAV:  return "WAV";
        case IR_AUDIO_FORMAT_OGG:  return "OGG Vorbis";
        case IR_AUDIO_FORMAT_MP3:  return "MP3";
        case IR_AUDIO_FORMAT_FLAC: return "FLAC";
        case IR_AUDIO_FORMAT_OPUS: return "Opus";
        default:                  return "Unknown";
    }
}

bool ir_audio_format_supported(IRAudioFormat format) {
    switch (format) {
        case IR_AUDIO_FORMAT_WAV:
        case IR_AUDIO_FORMAT_OGG:
        case IR_AUDIO_FORMAT_MP3:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// OGG Vorbis Decoding
// ============================================================================

static bool decode_ogg_from_memory(const void* data, size_t size,
                                    IRDecodedAudio* out_decoded) {
    int error = 0;
    int channels_out = 0;
    int sample_rate_out = 0;
    int16_t* pcm_out = NULL;

    // Use the convenience decoder that returns everything at once
    int sample_count = stb_vorbis_decode_memory((const unsigned char*)data, (int)size,
                                                 &channels_out, &sample_rate_out, &pcm_out);

    if (sample_count <= 0 || !pcm_out) {
        IR_LOG_ERROR("AUDIO", "Failed to decode OGG: sample_count=%d", sample_count);
        return false;
    }

    uint32_t sample_rate = (uint32_t)sample_rate_out;
    uint8_t channels = (uint8_t)channels_out;
    uint32_t duration_ms = sample_rate > 0 ? ((uint32_t)sample_count * 1000) / (sample_rate * channels) : 0;

    out_decoded->data = pcm_out;
    out_decoded->data_size = (size_t)sample_count * sizeof(int16_t);
    out_decoded->sample_rate = sample_rate;
    out_decoded->channels = channels;
    out_decoded->bits_per_sample = 16;
    out_decoded->duration_ms = duration_ms;
    out_decoded->owned = true;

    IR_LOG_INFO("AUDIO", "Decoded OGG: %d samples, %u Hz, %u ch, %u ms",
                sample_count / channels, sample_rate, channels, duration_ms);

    return true;
}

// ============================================================================
// MP3 Decoding
// ============================================================================

static bool decode_mp3_from_memory(const void* data, size_t size,
                                    IRDecodedAudio* out_decoded) {
    drmp3 mp3;
    memset(&mp3, 0, sizeof(mp3));

    // Initialize MP3 decoder from memory
    if (!drmp3_init_memory(&mp3, data, size, NULL)) {
        IR_LOG_ERROR("AUDIO", "Failed to open MP3");
        return false;
    }

    // Get audio info
    uint32_t sample_rate = mp3.sampleRate;
    uint8_t channels = mp3.channels;

    // Get total sample count
    drmp3_uint64 total_samples = drmp3_get_pcm_frame_count(&mp3);
    uint32_t duration_ms = total_samples > 0 ? (uint32_t)((total_samples * 1000) / sample_rate) : 0;

    // Allocate output buffer
    size_t total_pcm_samples = (size_t)total_samples * channels;
    int16_t* pcm_out = (int16_t*)malloc(total_pcm_samples * sizeof(int16_t));
    if (!pcm_out) {
        drmp3_uninit(&mp3);
        IR_LOG_ERROR("AUDIO", "Failed to allocate MP3 output buffer");
        return false;
    }

    // Decode all frames
    drmp3_uint64 frames_read = drmp3_read_pcm_frames_s16(&mp3, total_samples, pcm_out);
    drmp3_uninit(&mp3);

    if (frames_read == 0) {
        free(pcm_out);
        IR_LOG_ERROR("AUDIO", "Failed to decode MP3: no frames decoded");
        return false;
    }

    size_t actual_samples = (size_t)frames_read * channels;
    // Trim buffer if needed
    if (actual_samples < total_pcm_samples) {
        int16_t* trimmed = (int16_t*)realloc(pcm_out, actual_samples * sizeof(int16_t));
        if (trimmed) {
            pcm_out = trimmed;
        }
    }

    out_decoded->data = pcm_out;
    out_decoded->data_size = actual_samples * sizeof(int16_t);
    out_decoded->sample_rate = sample_rate;
    out_decoded->channels = channels;
    out_decoded->bits_per_sample = 16;
    out_decoded->duration_ms = duration_ms;
    out_decoded->owned = true;

    IR_LOG_INFO("AUDIO", "Decoded MP3: %zu samples, %u Hz, %u ch, %u ms",
                actual_samples / channels, sample_rate, channels, duration_ms);

    return true;
}

// ============================================================================
// File Decoding
// ============================================================================

static bool read_file_contents(const char* path, void** out_data, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        IR_LOG_ERROR("AUDIO", "Failed to open file: %s", path);
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        IR_LOG_ERROR("AUDIO", "Invalid file size: %s", path);
        return false;
    }

    // Allocate and read
    void* data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        IR_LOG_ERROR("AUDIO", "Failed to allocate buffer for file");
        return false;
    }

    size_t read_size = fread(data, 1, (size_t)size, f);
    fclose(f);

    if (read_size != (size_t)size) {
        free(data);
        IR_LOG_ERROR("AUDIO", "Failed to read complete file: %s", path);
        return false;
    }

    *out_data = data;
    *out_size = (size_t)size;
    return true;
}

bool ir_audio_decode_file(const char* path, IRDecodedAudio* out_decoded) {
    if (!path || !out_decoded) {
        IR_LOG_ERROR("AUDIO", "Invalid parameters for decode_file");
        return false;
    }

    memset(out_decoded, 0, sizeof(IRDecodedAudio));

    // Detect format from extension
    IRAudioFormat format = ir_audio_detect_format(path);
    if (format == IR_AUDIO_FORMAT_UNKNOWN) {
        IR_LOG_ERROR("AUDIO", "Unknown format for file: %s", path);
        return false;
    }

    // WAV is handled by SDL3 backend, skip here
    if (format == IR_AUDIO_FORMAT_WAV) {
        IR_LOG_INFO("AUDIO", "WAV format - use SDL3 backend directly");
        return false;
    }

    // Read file contents
    void* data = NULL;
    size_t size = 0;
    if (!read_file_contents(path, &data, &size)) {
        return false;
    }

    // Decode based on format
    bool success = false;
    switch (format) {
        case IR_AUDIO_FORMAT_OGG:
            success = decode_ogg_from_memory(data, size, out_decoded);
            break;
        case IR_AUDIO_FORMAT_MP3:
            success = decode_mp3_from_memory(data, size, out_decoded);
            break;
        default:
            IR_LOG_ERROR("AUDIO", "Unsupported format: %s", ir_audio_format_name(format));
            break;
    }

    // Free file data (decoded data is in out_decoded)
    free(data);

    if (!success) {
        IR_LOG_ERROR("AUDIO", "Failed to decode file: %s", path);
    }

    return success;
}

bool ir_audio_decode_from_memory(const void* data, size_t size,
                                  IRAudioFormat format,
                                  IRDecodedAudio* out_decoded) {
    if (!data || size == 0 || !out_decoded) {
        IR_LOG_ERROR("AUDIO", "Invalid parameters for decode_from_memory");
        return false;
    }

    memset(out_decoded, 0, sizeof(IRDecodedAudio));

    // Detect format from magic bytes if unknown
    if (format == IR_AUDIO_FORMAT_UNKNOWN) {
        format = ir_audio_detect_format_from_memory(data, size);
    }

    if (format == IR_AUDIO_FORMAT_UNKNOWN) {
        IR_LOG_ERROR("AUDIO", "Cannot detect format from memory");
        return false;
    }

    // WAV is handled by SDL3 backend
    if (format == IR_AUDIO_FORMAT_WAV) {
        IR_LOG_INFO("AUDIO", "WAV format - use SDL3 backend directly");
        return false;
    }

    switch (format) {
        case IR_AUDIO_FORMAT_OGG:
            return decode_ogg_from_memory(data, size, out_decoded);
        case IR_AUDIO_FORMAT_MP3:
            return decode_mp3_from_memory(data, size, out_decoded);
        default:
            IR_LOG_ERROR("AUDIO", "Unsupported format: %s", ir_audio_format_name(format));
            return false;
    }
}

// ============================================================================
// Cleanup
// ============================================================================

void ir_audio_free_decoded(IRDecodedAudio* audio) {
    if (!audio) return;

    if (audio->owned && audio->data) {
        free(audio->data);
    }

    memset(audio, 0, sizeof(IRDecodedAudio));
}
