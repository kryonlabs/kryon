#include "kryon.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define CHECK(expr) do { \
    if(!(expr)) { \
        fprintf(stderr, "canvas audio check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while(0)

static int stream_callback_calls;
static int stream_processor_calls;
static int mixed_processor_calls;

static void StreamCallback(void *bufferData, unsigned int frames)
{
    float *samples = (float *)bufferData;

    stream_callback_calls++;
    for(unsigned int i = 0; i < frames * 2u; i++)
        samples[i] = (float)(i & 7u) / 8.0f;
}

static void StreamProcessor(void *bufferData, unsigned int frames)
{
    float *samples = (float *)bufferData;

    stream_processor_calls++;
    if(frames > 0) samples[0] = 0.125f;
}

static void MixedProcessor(void *bufferData, unsigned int frames)
{
    float *samples = (float *)bufferData;

    mixed_processor_calls++;
    if(frames > 0) samples[1] = -0.125f;
}

static void put_u16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void put_u32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static void make_wav(unsigned char *out, int frames)
{
    int data_bytes = frames * 2;

    memcpy(out + 0, "RIFF", 4);
    put_u32(out + 4, 36u + (unsigned int)data_bytes);
    memcpy(out + 8, "WAVEfmt ", 8);
    put_u32(out + 16, 16);
    put_u16(out + 20, 1);
    put_u16(out + 22, 1);
    put_u32(out + 24, 22050);
    put_u32(out + 28, 22050 * 2);
    put_u16(out + 32, 2);
    put_u16(out + 34, 16);
    memcpy(out + 36, "data", 4);
    put_u32(out + 40, (unsigned int)data_bytes);

    for(int i = 0; i < frames; i++) {
        int sample = (i & 1) ? 12000 : -12000;
        put_u16(out + 44 + i * 2, (unsigned int)(unsigned short)sample);
    }
}

int main(void)
{
    enum { frames = 16, wav_size = 44 + frames * 2 };
    unsigned char wav[wav_size];
    unsigned char encoded[4] = {1, 2, 3, 4};
    float stream_data[64];

    InitAudioDevice();
    CHECK(IsAudioDeviceReady());
    SetMasterVolume(0.35f);
    CHECK(fabsf(GetMasterVolume() - 0.35f) < 0.01f);

    make_wav(wav, frames);
    Wave wave = LoadWaveFromMemory(".wav", wav, (int)sizeof(wav));
    CHECK(IsWaveValid(wave));
    CHECK(wave.frameCount == frames);
    CHECK(wave.sampleRate == 22050);
    CHECK(wave.sampleSize == 16);
    CHECK(wave.channels == 1);

    float *samples = LoadWaveSamples(wave);
    CHECK(samples != NULL);
    CHECK(samples[0] < -0.1f);
    UnloadWaveSamples(samples);

    Wave copy = WaveCopy(wave);
    CHECK(IsWaveValid(copy));
    WaveCrop(&copy, 2, 10);
    CHECK(copy.frameCount == 8);
    WaveFormat(&copy, 44100, 32, 2);
    CHECK(copy.sampleRate == 44100);
    CHECK(copy.sampleSize == 32);
    CHECK(copy.channels == 2);
    CHECK(copy.frameCount > 8);
    CHECK(ExportWave(copy, "export.wav"));
    CHECK(FileExists("export.wav"));
    CHECK(ExportWaveAsCode(copy, "export_wave.h"));
    CHECK(FileExists("export_wave.h"));

    Sound sound = LoadSoundFromWave(copy);
    CHECK(IsSoundValid(sound));
    SetSoundVolume(sound, 0.5f);
    SetSoundPitch(sound, 1.25f);
    SetSoundPan(sound, -0.25f);
    PlaySound(sound);
    CHECK(IsSoundPlaying(sound));
    PauseSound(sound);
    ResumeSound(sound);
    StopSound(sound);
    CHECK(!IsSoundPlaying(sound));
    UpdateSound(sound, copy.data, (int)copy.frameCount);

    Music music = LoadMusicStreamFromMemory(".ogg", encoded, (int)sizeof(encoded));
    CHECK(IsMusicValid(music));
    SetMusicVolume(music, 0.75f);
    SetMusicPitch(music, 0.9f);
    SetMusicPan(music, 0.5f);
    PlayMusicStream(music);
    CHECK(IsMusicStreamPlaying(music));
    SeekMusicStream(music, 0.001f);
    CHECK(GetMusicTimeLength(music) > 0.0f);
    PauseMusicStream(music);
    ResumeMusicStream(music);
    StopMusicStream(music);
    CHECK(!IsMusicStreamPlaying(music));

    AudioStream stream = LoadAudioStream(44100, 32, 2);
    CHECK(IsAudioStreamValid(stream));
    CHECK(IsAudioStreamProcessed(stream));
    for(int i = 0; i < 64; i++) stream_data[i] = (float)i / 64.0f;
    AttachAudioStreamProcessor(stream, StreamProcessor);
    AttachAudioMixedProcessor(MixedProcessor);
    PlayAudioStream(stream);
    CHECK(IsAudioStreamPlaying(stream));
    UpdateAudioStream(stream, stream_data, 32);
    CHECK(stream_processor_calls == 1);
    CHECK(mixed_processor_calls == 1);
    SetAudioStreamVolume(stream, 0.4f);
    SetAudioStreamPitch(stream, 1.1f);
    SetAudioStreamPan(stream, 0.2f);
    SetAudioStreamBufferSizeDefault(64);
    SetAudioStreamCallback(stream, StreamCallback);
#ifdef __EMSCRIPTEN__
    emscripten_sleep(30);
#endif
    CHECK(stream_callback_calls > 0);
    SetAudioStreamCallback(stream, NULL);
    DetachAudioStreamProcessor(stream, StreamProcessor);
    DetachAudioMixedProcessor(MixedProcessor);
    PauseAudioStream(stream);
    CHECK(!IsAudioStreamPlaying(stream));
    ResumeAudioStream(stream);
    StopAudioStream(stream);
    CHECK(!IsAudioStreamPlaying(stream));

    UnloadAudioStream(stream);
    UnloadMusicStream(music);
    UnloadSound(sound);
    UnloadWave(copy);
    UnloadWave(wave);
    CloseAudioDevice();

    return 0;
}
