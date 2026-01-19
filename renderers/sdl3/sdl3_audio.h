#ifndef SDL3_AUDIO_BACKEND_H
#define SDL3_AUDIO_BACKEND_H

#include "../../ir/include/ir_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SDL3 Audio Backend
 *
 * Simple audio backend using SDL3's audio API
 * Supports WAV files natively
 * For OGG/MP3, use SDL3_mixer (future enhancement)
 */

/**
 * Get SDL3 audio backend plugin
 * Call this to register the SDL3 backend with the audio system
 */
IRAudioBackendPlugin* sdl3_audio_get_backend(void);

/**
 * Initialize SDL3 audio backend manually
 * (Usually called automatically by ir_audio_init)
 */
bool sdl3_audio_init(IRAudioConfig* config);

/**
 * Shutdown SDL3 audio backend
 */
void sdl3_audio_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // SDL3_AUDIO_BACKEND_H
