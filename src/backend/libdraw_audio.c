#ifdef KRYON_NATIVE_PLAN9

#include "kryon_plan9.h"

typedef struct KryLibdrawAudioBuffer KryLibdrawAudioBuffer;

struct KryLibdrawAudioBuffer {
    unsigned char *data;
    unsigned int bytes;
    int owns;
};

static int g_audio_ready;
static float g_master_volume = 1.0f;

static unsigned int
rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int
rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int
tag_eq(const unsigned char *p, const char *s)
{
    return p[0] == (unsigned char)s[0] && p[1] == (unsigned char)s[1] &&
           p[2] == (unsigned char)s[2] && p[3] == (unsigned char)s[3];
}

static Wave
zero_wave(void)
{
    Wave wave;

    memset(&wave, 0, sizeof(wave));
    return wave;
}

static AudioStream
zero_stream(void)
{
    AudioStream stream;

    memset(&stream, 0, sizeof(stream));
    return stream;
}

static Sound
zero_sound(void)
{
    Sound sound;

    memset(&sound, 0, sizeof(sound));
    return sound;
}

static KryLibdrawAudioBuffer *
audio_buffer(AudioStream stream)
{
    return (KryLibdrawAudioBuffer *)stream.buffer;
}

static Music
zero_music(void)
{
    Music music;

    memset(&music, 0, sizeof(music));
    return music;
}

static unsigned char *
read_audio_file(const char *path, int *len)
{
    int fd;
    vlong size;
    unsigned char *data;

    if(len != nil)
        *len = 0;
    if(path == nil)
        return nil;
    fd = open((char *)path, OREAD);
    if(fd < 0)
        return nil;
    size = seek(fd, 0, 2);
    if(size <= 0) {
        close(fd);
        return nil;
    }
    seek(fd, 0, 0);
    data = malloc((size_t)size);
    if(data == nil) {
        close(fd);
        return nil;
    }
    if(readn(fd, data, (long)size) != size) {
        free(data);
        close(fd);
        return nil;
    }
    close(fd);
    if(len != nil)
        *len = (int)size;
    return data;
}

void
InitAudioDevice(void)
{
    int fd;

    fd = open("/dev/audio", OWRITE);
    if(fd >= 0) {
        close(fd);
        g_audio_ready = 1;
    } else {
        g_audio_ready = 0;
    }
}

void CloseAudioDevice(void) { g_audio_ready = 0; }
bool IsAudioDeviceReady(void) { return g_audio_ready != 0; }
void SetMasterVolume(float volume) { g_master_volume = volume; }
float GetMasterVolume(void) { return g_master_volume; }

Wave
LoadWaveFromMemory(const char *fileType, const unsigned char *fileData,
                   int dataSize)
{
    Wave wave;
    int pos;
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    unsigned int sampleSize = 0;
    const unsigned char *pcm = nil;
    unsigned int pcm_bytes = 0;

    (void)fileType;
    wave = zero_wave();
    if(fileData == nil || dataSize < 44 || !tag_eq(fileData, "RIFF") ||
       !tag_eq(fileData + 8, "WAVE"))
        return wave;

    pos = 12;
    while(pos + 8 <= dataSize) {
        unsigned int n = rd32(fileData + pos + 4);
        const unsigned char *chunk = fileData + pos + 8;

        if(pos + 8 + (int)n > dataSize)
            break;
        if(tag_eq(fileData + pos, "fmt ") && n >= 16) {
            unsigned int format = rd16(chunk);

            if(format != 1)
                return zero_wave();
            channels = rd16(chunk + 2);
            sampleRate = rd32(chunk + 4);
            sampleSize = rd16(chunk + 14);
        } else if(tag_eq(fileData + pos, "data")) {
            pcm = chunk;
            pcm_bytes = n;
        }
        pos += 8 + (int)n + (n & 1);
    }

    if(pcm == nil || pcm_bytes == 0 || channels == 0 || sampleSize == 0)
        return wave;
    wave.data = malloc(pcm_bytes);
    if(wave.data == nil)
        return zero_wave();
    memcpy(wave.data, (void *)pcm, pcm_bytes);
    wave.channels = channels;
    wave.sampleRate = sampleRate;
    wave.sampleSize = sampleSize;
    wave.frameCount = pcm_bytes / channels / (sampleSize / 8);
    return wave;
}

Wave
LoadWave(const char *fileName)
{
    int len;
    unsigned char *data;
    Wave wave;

    data = read_audio_file(fileName, &len);
    wave = LoadWaveFromMemory(".wav", data, len);
    free(data);
    return wave;
}

bool IsWaveValid(Wave wave)
{
    return wave.data != nil && wave.frameCount > 0 && wave.channels > 0;
}

Sound
LoadSoundFromWave(Wave wave)
{
    Sound sound;
    KryLibdrawAudioBuffer *buffer;
    unsigned int bytes;

    sound = zero_sound();
    if(!IsWaveValid(wave))
        return sound;
    bytes = wave.frameCount * wave.channels * (wave.sampleSize / 8);
    buffer = malloc(sizeof(*buffer));
    if(buffer == nil)
        return sound;
    buffer->data = malloc(bytes);
    if(buffer->data == nil) {
        free(buffer);
        return sound;
    }
    memcpy(buffer->data, wave.data, bytes);
    buffer->bytes = bytes;
    buffer->owns = 1;
    sound.stream.buffer = (rAudioBuffer *)buffer;
    sound.stream.sampleRate = wave.sampleRate;
    sound.stream.sampleSize = wave.sampleSize;
    sound.stream.channels = wave.channels;
    sound.frameCount = wave.frameCount;
    return sound;
}

Sound LoadSound(const char *fileName)
{
    Wave wave;
    Sound sound;

    wave = LoadWave(fileName);
    sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

Sound LoadSoundAlias(Sound source) { return source; }
bool IsSoundValid(Sound sound) { return sound.stream.buffer != nil; }
void UpdateSound(Sound sound, const void *data, int frameCount)
{
    (void)sound;
    (void)data;
    (void)frameCount;
}
void UnloadWave(Wave wave) { free(wave.data); }
void UnloadSound(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;

    buffer = audio_buffer(sound.stream);
    if(buffer != nil && buffer->owns) {
        free(buffer->data);
        free(buffer);
    }
}
void UnloadSoundAlias(Sound alias) { (void)alias; }
bool ExportWave(Wave wave, const char *fileName)
{
    (void)wave;
    (void)fileName;
    return false;
}
bool ExportWaveAsCode(Wave wave, const char *fileName)
{
    (void)wave;
    (void)fileName;
    return false;
}
void PlaySound(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;
    int fd;

    buffer = audio_buffer(sound.stream);
    if(buffer == nil || buffer->data == nil)
        return;
    fd = open("/dev/audio", OWRITE);
    if(fd < 0)
        return;
    write(fd, buffer->data, buffer->bytes);
    close(fd);
}
void StopSound(Sound sound) { (void)sound; }
void PauseSound(Sound sound) { (void)sound; }
void ResumeSound(Sound sound) { (void)sound; }
bool IsSoundPlaying(Sound sound)
{
    (void)sound;
    return false;
}
void SetSoundVolume(Sound sound, float volume) { (void)sound; (void)volume; }
void SetSoundPitch(Sound sound, float pitch) { (void)sound; (void)pitch; }
void SetSoundPan(Sound sound, float pan) { (void)sound; (void)pan; }
Wave WaveCopy(Wave wave)
{
    Wave copy;
    unsigned int bytes;

    copy = zero_wave();
    if(!IsWaveValid(wave))
        return copy;
    bytes = wave.frameCount * wave.channels * (wave.sampleSize / 8);
    copy = wave;
    copy.data = malloc(bytes);
    if(copy.data == nil)
        return zero_wave();
    memcpy(copy.data, wave.data, bytes);
    return copy;
}
void WaveCrop(Wave *wave, int initFrame, int finalFrame)
{
    (void)wave;
    (void)initFrame;
    (void)finalFrame;
}
void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels)
{
    (void)wave;
    (void)sampleRate;
    (void)sampleSize;
    (void)channels;
}
float *LoadWaveSamples(Wave wave) { (void)wave; return nil; }
void UnloadWaveSamples(float *samples) { free(samples); }

Music LoadMusicStream(const char *fileName) { (void)fileName; return zero_music(); }
Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data,
                                int dataSize)
{
    (void)fileType;
    (void)data;
    (void)dataSize;
    return zero_music();
}
bool IsMusicValid(Music music) { (void)music; return false; }
void UnloadMusicStream(Music music) { (void)music; }
void PlayMusicStream(Music music) { (void)music; }
bool IsMusicStreamPlaying(Music music) { (void)music; return false; }
void UpdateMusicStream(Music music) { (void)music; }
void StopMusicStream(Music music) { (void)music; }
void PauseMusicStream(Music music) { (void)music; }
void ResumeMusicStream(Music music) { (void)music; }
void SeekMusicStream(Music music, float position) { (void)music; (void)position; }
void SetMusicVolume(Music music, float volume) { (void)music; (void)volume; }
void SetMusicPitch(Music music, float pitch) { (void)music; (void)pitch; }
void SetMusicPan(Music music, float pan) { (void)music; (void)pan; }
float GetMusicTimeLength(Music music) { (void)music; return 0.0f; }
float GetMusicTimePlayed(Music music) { (void)music; return 0.0f; }

AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize,
                            unsigned int channels)
{
    AudioStream stream;

    stream = zero_stream();
    stream.sampleRate = sampleRate;
    stream.sampleSize = sampleSize;
    stream.channels = channels;
    return stream;
}
bool IsAudioStreamValid(AudioStream stream) { return stream.channels > 0; }
void UnloadAudioStream(AudioStream stream) { (void)stream; }
void UpdateAudioStream(AudioStream stream, const void *data, int frameCount)
{
    (void)stream;
    (void)data;
    (void)frameCount;
}
bool IsAudioStreamProcessed(AudioStream stream) { (void)stream; return true; }
void PlayAudioStream(AudioStream stream) { (void)stream; }
void PauseAudioStream(AudioStream stream) { (void)stream; }
void ResumeAudioStream(AudioStream stream) { (void)stream; }
bool IsAudioStreamPlaying(AudioStream stream) { (void)stream; return false; }
void StopAudioStream(AudioStream stream) { (void)stream; }
void SetAudioStreamVolume(AudioStream stream, float volume) { (void)stream; (void)volume; }
void SetAudioStreamPitch(AudioStream stream, float pitch) { (void)stream; (void)pitch; }
void SetAudioStreamPan(AudioStream stream, float pan) { (void)stream; (void)pan; }
void SetAudioStreamBufferSizeDefault(int size) { (void)size; }
void SetAudioStreamCallback(AudioStream stream, AudioCallback callback)
{
    (void)stream;
    (void)callback;
}
void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    (void)stream;
    (void)processor;
}
void DetachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    (void)stream;
    (void)processor;
}
void AttachAudioMixedProcessor(AudioCallback processor) { (void)processor; }
void DetachAudioMixedProcessor(AudioCallback processor) { (void)processor; }

#else

#ifndef SUPPORT_MODULE_RAUDIO
#define SUPPORT_MODULE_RAUDIO 1
#endif

#ifndef SUPPORT_FILEFORMAT_WAV
#define SUPPORT_FILEFORMAT_WAV 1
#endif

#ifndef SUPPORT_FILEFORMAT_OGG
#define SUPPORT_FILEFORMAT_OGG 1
#endif

#ifndef SUPPORT_FILEFORMAT_MP3
#define SUPPORT_FILEFORMAT_MP3 1
#endif

#ifndef SUPPORT_FILEFORMAT_QOA
#define SUPPORT_FILEFORMAT_QOA 1
#endif

#ifndef SUPPORT_FILEFORMAT_FLAC
#define SUPPORT_FILEFORMAT_FLAC 0
#endif

#ifndef SUPPORT_FILEFORMAT_XM
#define SUPPORT_FILEFORMAT_XM 0
#endif

#ifndef SUPPORT_FILEFORMAT_MOD
#define SUPPORT_FILEFORMAT_MOD 0
#endif

#include "../../vendor/raylib/src/raudio.c"

#endif
