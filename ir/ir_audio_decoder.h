/*
 * Kryon Audio Decoder Module
 *
 * Provides audio format decoding support for OGG Vorbis and MP3 formats.
 * Integrates stb_vorbis and dr_mp3 for decoding.
 *
 * Supported formats:
 * - WAV (via SDL3 - existing)
 * - OGG Vorbis (via stb_vorbis)
 * - MP3 (via dr_mp3)
 */

#ifndef IR_AUDIO_DECODER_H
#define IR_AUDIO_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio format enumeration
 */
typedef enum {
    IR_AUDIO_FORMAT_UNKNOWN = 0,
    IR_AUDIO_FORMAT_WAV,
    IR_AUDIO_FORMAT_OGG,
    IR_AUDIO_FORMAT_MP3,
    IR_AUDIO_FORMAT_FLAC,     // Future
    IR_AUDIO_FORMAT_OPUS,     // Future
} IRAudioFormat;

/**
 * Decoded audio data structure
 * Contains raw PCM data that can be passed to audio backends
 */
typedef struct {
    void* data;               // Raw PCM samples (int16_t)
    size_t data_size;         // Size in bytes
    uint32_t sample_rate;     // Sample rate (Hz)
    uint8_t channels;         // Number of channels (1=mono, 2=stereo)
    uint16_t bits_per_sample; // Bits per sample (usually 16)
    uint32_t duration_ms;     // Estimated duration in milliseconds
    bool owned;               // If true, data should be freed
} IRDecodedAudio;

/**
 * Detect audio format from file extension
 *
 * @param path File path or extension
 * @return Audio format enum value
 */
IRAudioFormat ir_audio_detect_format(const char* path);

/**
 * Detect audio format from memory buffer (magic bytes)
 *
 * @param data Pointer to audio data
 * @param size Size of data in bytes
 * @return Audio format enum value
 */
IRAudioFormat ir_audio_detect_format_from_memory(const void* data, size_t size);

/**
 * Decode audio file from path
 * Allocates and returns decoded PCM data.
 *
 * @param path Path to audio file
 * @param out_decoded Output structure for decoded audio
 * @return true on success, false on failure
 */
bool ir_audio_decode_file(const char* path, IRDecodedAudio* out_decoded);

/**
 * Decode audio from memory buffer
 * Allocates and returns decoded PCM data.
 *
 * @param data Pointer to encoded audio data
 * @param size Size of data in bytes
 * @param format Format of the audio data
 * @param out_decoded Output structure for decoded audio
 * @return true on success, false on failure
 */
bool ir_audio_decode_from_memory(const void* data, size_t size,
                                  IRAudioFormat format,
                                  IRDecodedAudio* out_decoded);

/**
 * Free decoded audio data
 *
 * @param audio Decoded audio structure to free
 */
void ir_audio_free_decoded(IRDecodedAudio* audio);

/**
 * Get format name as string
 *
 * @param format Audio format enum
 * @return String name of format
 */
const char* ir_audio_format_name(IRAudioFormat format);

/**
 * Check if a format is supported for decoding
 *
 * @param format Audio format to check
 * @return true if supported, false otherwise
 */
bool ir_audio_format_supported(IRAudioFormat format);

#ifdef __cplusplus
}
#endif

#endif // IR_AUDIO_DECODER_H
