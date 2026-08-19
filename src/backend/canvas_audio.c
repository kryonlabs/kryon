/*
 * canvas_audio.c — null-grade audio stubs (UI-only backend).
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * Every audio entry point of the kryon surface is stubbed so UI-only
 * apps link and run silently; sound is future work.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

void SetMasterVolume(float volume)
{
    (void)volume;
    return;
}

float GetMasterVolume(void)
{
    
    return 0.0f;
}

bool IsWaveValid(Wave wave)
{
    (void)wave;
    return false;
}

Sound LoadSoundAlias(Sound source)
{
    (void)source;
    return (Sound){0};
}

bool IsSoundValid(Sound sound)
{
    (void)sound;
    return false;
}

void UpdateSound(Sound sound, const void *data, int frameCount)
{
    (void)sound; (void)data; (void)frameCount;
    return;
}

void UnloadSoundAlias(Sound alias)
{
    (void)alias;
    return;
}

bool ExportWave(Wave wave, const char *fileName)
{
    (void)wave; (void)fileName;
    return false;
}

bool ExportWaveAsCode(Wave wave, const char *fileName)
{
    (void)wave; (void)fileName;
    return false;
}

void ResumeSound(Sound sound)
{
    (void)sound;
    return;
}

void SetSoundPan(Sound sound, float pan)
{
    (void)sound; (void)pan;
    return;
}

Wave WaveCopy(Wave wave)
{
    (void)wave;
    return (Wave){0};
}

void WaveCrop(Wave *wave, int initFrame, int finalFrame)
{
    (void)wave; (void)initFrame; (void)finalFrame;
    return;
}

void UnloadWaveSamples(float *samples)
{
    (void)samples;
    return;
}

Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data, int dataSize)
{
    (void)fileType; (void)data; (void)dataSize;
    return (Music){0};
}

void PauseMusicStream(Music music)
{
    (void)music;
    return;
}

void ResumeMusicStream(Music music)
{
    (void)music;
    return;
}

void SeekMusicStream(Music music, float position)
{
    (void)music; (void)position;
    return;
}

void SetMusicPitch(Music music, float pitch)
{
    (void)music; (void)pitch;
    return;
}

void SetMusicPan(Music music, float pan)
{
    (void)music; (void)pan;
    return;
}

float GetMusicTimeLength(Music music)
{
    (void)music;
    return 0.0f;
}

float GetMusicTimePlayed(Music music)
{
    (void)music;
    return 0.0f;
}

AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels)
{
    (void)sampleRate; (void)sampleSize; (void)channels;
    return (AudioStream){0};
}

bool IsAudioStreamValid(AudioStream stream)
{
    (void)stream;
    return false;
}

void UnloadAudioStream(AudioStream stream)
{
    (void)stream;
    return;
}

void UpdateAudioStream(AudioStream stream, const void *data, int frameCount)
{
    (void)stream; (void)data; (void)frameCount;
    return;
}

bool IsAudioStreamProcessed(AudioStream stream)
{
    (void)stream;
    return false;
}

void PlayAudioStream(AudioStream stream)
{
    (void)stream;
    return;
}

void PauseAudioStream(AudioStream stream)
{
    (void)stream;
    return;
}

void ResumeAudioStream(AudioStream stream)
{
    (void)stream;
    return;
}

bool IsAudioStreamPlaying(AudioStream stream)
{
    (void)stream;
    return false;
}

void StopAudioStream(AudioStream stream)
{
    (void)stream;
    return;
}

void SetAudioStreamVolume(AudioStream stream, float volume)
{
    (void)stream; (void)volume;
    return;
}

void SetAudioStreamPitch(AudioStream stream, float pitch)
{
    (void)stream; (void)pitch;
    return;
}

void SetAudioStreamPan(AudioStream stream, float pan)
{
    (void)stream; (void)pan;
    return;
}

void SetAudioStreamBufferSizeDefault(int size)
{
    (void)size;
    return;
}

void SetAudioStreamCallback(AudioStream stream, AudioCallback callback)
{
    (void)stream; (void)callback;
    return;
}

void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    (void)stream; (void)processor;
    return;
}

void DetachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    (void)stream; (void)processor;
    return;
}

Sound LoadSound(const char *fileName)
{
    (void)fileName;
    return (Sound){0};
}
Sound LoadSoundFromWave(Wave wave)
{
    (void)wave;
    return (Sound){0};
}
void UnloadSound(Sound sound)
{
    (void)sound;
}
void PlaySound(Sound sound)
{
    (void)sound;
}
void StopSound(Sound sound)
{
    (void)sound;
}
void PauseSound(Sound sound)
{
    (void)sound;
}
void SetSoundVolume(Sound sound, float volume)
{
    (void)sound;
    (void)volume;
}
void SetSoundPitch(Sound sound, float pitch)
{
    (void)sound;
    (void)pitch;
}
Music LoadMusicStream(const char *fileName)
{
    (void)fileName;
    return (Music){0};
}
void UnloadMusicStream(Music music)
{
    (void)music;
}
void PlayMusicStream(Music music)
{
    (void)music;
}
void StopMusicStream(Music music)
{
    (void)music;
}
void UpdateMusicStream(Music music)
{
    (void)music;
}
void SetMusicVolume(Music music, float volume)
{
    (void)music;
    (void)volume;
}

/* Audio device/wave surface (null-grade): apps drive these directly. */
void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}
bool IsAudioDeviceReady(void)
{
    return false;
}
Wave LoadWave(const char *fileName)
{
    (void)fileName;
    return (Wave){0};
}
Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize)
{
    (void)fileType;
    (void)fileData;
    (void)dataSize;
    return (Wave){0};
}
void UnloadWave(Wave wave)
{
    (void)wave;
}
void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels)
{
    (void)wave;
    (void)sampleRate;
    (void)sampleSize;
    (void)channels;
}
bool IsSoundPlaying(Sound sound)
{
    (void)sound;
    return false;
}
bool IsMusicValid(Music music)
{
    (void)music;
    return false;
}
bool IsMusicStreamPlaying(Music music)
{
    (void)music;
    return false;
}
void AttachAudioMixedProcessor(void (*processor)(void *buffer,
                                                 unsigned int frames))
{
    (void)processor;
}
void DetachAudioMixedProcessor(void (*processor)(void *buffer,
                                                 unsigned int frames))
{
    (void)processor;
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */
