#ifdef KRYON_NATIVE_PLAN9

#include "kryon_plan9.h"

typedef struct KryLibdrawAudioBuffer KryLibdrawAudioBuffer;
typedef struct KryLibdrawVoice KryLibdrawVoice;
typedef struct KryLibdrawMusic KryLibdrawMusic;

struct KryLibdrawAudioBuffer {
    unsigned char *data;
    unsigned int bytes;
    unsigned int frameCount;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
    int owns;
    float volume;
    float pitch;
    float pan;
};

struct KryLibdrawVoice {
    int active;
    int paused;
    KryLibdrawAudioBuffer *buffer;
    double cursor;
    float volume;
    float pitch;
    float pan;
};

struct KryLibdrawMusic {
    KryLibdrawAudioBuffer *buffer;
    int playing;
    int paused;
    int looping;
    double cursor;
    float volume;
    float pitch;
    float pan;
};

enum {
    KRY_AUDIO_RATE = 44100,
    KRY_AUDIO_CHANNELS = 2,
    KRY_AUDIO_CHUNK_FRAMES = 512,
    KRY_AUDIO_MAX_VOICES = 32,
    KRY_AUDIO_MAX_MUSIC = 8,
    KRY_AUDIO_MAX_PROCESSORS = 8
};

static int g_audio_ready;
static int g_audio_running;
static int g_audio_child_started;
static Lock g_audio_lock;
static float g_master_volume = 1.0f;
static KryLibdrawVoice g_voices[KRY_AUDIO_MAX_VOICES];
static KryLibdrawMusic *g_music[KRY_AUDIO_MAX_MUSIC];
static AudioCallback g_mixed_processors[KRY_AUDIO_MAX_PROCESSORS];

static int open_audio_owrite(void);

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

static float
clampf(float v, float lo, float hi)
{
    if(v < lo)
        return lo;
    if(v > hi)
        return hi;
    return v;
}

static void
wr16(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void
wr32(unsigned char *p, long v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
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

static float
pcm_sample_at(const unsigned char *data, unsigned int frame,
              unsigned int channel, unsigned int sampleSize,
              unsigned int channels)
{
    unsigned int index;
    const unsigned char *p;
    int s;
    long l;

    if(data == nil || channels == 0)
        return 0.0f;
    if(channel >= channels)
        channel = channels - 1;
    index = (frame * channels + channel) * (sampleSize / 8);
    p = data + index;
    if(sampleSize == 8)
        return ((float)p[0] - 128.0f) / 128.0f;
    if(sampleSize == 16) {
        s = (int)((ushort)p[0] | ((ushort)p[1] << 8));
        if(s & 0x8000)
            s -= 0x10000;
        return (float)s / 32768.0f;
    }
    if(sampleSize == 32) {
        l = (long)p[0] | ((long)p[1] << 8) |
            ((long)p[2] << 16) | ((long)p[3] << 24);
        return (float)((double)l / 2147483648.0);
    }
    return 0.0f;
}

static float
buffer_sample(KryLibdrawAudioBuffer *buffer, double cursor, unsigned int channel)
{
    unsigned int i0;
    unsigned int i1;
    double frac;
    float a;
    float b;

    if(buffer == nil || buffer->data == nil || buffer->frameCount == 0)
        return 0.0f;
    if(cursor < 0.0)
        cursor = 0.0;
    if(cursor >= (double)buffer->frameCount)
        return 0.0f;
    i0 = (unsigned int)cursor;
    i1 = i0 + 1;
    if(i1 >= buffer->frameCount)
        i1 = i0;
    frac = cursor - (double)i0;
    if(buffer->channels == 1)
        channel = 0;
    else if(channel >= buffer->channels)
        channel = buffer->channels - 1;
    a = pcm_sample_at(buffer->data, i0, channel,
                      buffer->sampleSize, buffer->channels);
    b = pcm_sample_at(buffer->data, i1, channel,
                      buffer->sampleSize, buffer->channels);
    return a + (b - a) * (float)frac;
}

static void
write_sample(unsigned char *data, unsigned int index, unsigned int sampleSize,
             float value)
{
    int s16;
    long s32;

    value = clampf(value, -1.0f, 1.0f);
    if(sampleSize == 8) {
        data[index] = (unsigned char)(value * 127.0f + 128.0f);
    } else if(sampleSize == 16) {
        s16 = (int)(value * 32767.0f);
        wr16(data + index, s16);
    } else if(sampleSize == 32) {
        s32 = (long)((double)value * 2147483647.0);
        wr32(data + index, s32);
    }
}

static void
free_audio_buffer(KryLibdrawAudioBuffer *buffer)
{
    if(buffer == nil)
        return;
    if(buffer->owns)
        free(buffer->data);
    free(buffer);
}

static void
stop_buffer_locked(KryLibdrawAudioBuffer *buffer)
{
    int i;

    if(buffer == nil)
        return;
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            memset(&g_voices[i], 0, sizeof(g_voices[i]));
    }
}

static int
any_audio_active_locked(void)
{
    int i;

    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && !g_voices[i].paused)
            return 1;
    }
    for(i = 0; i < KRY_AUDIO_MAX_MUSIC; i++) {
        if(g_music[i] != nil && g_music[i]->playing && !g_music[i]->paused)
            return 1;
    }
    return 0;
}

static void
mix_buffer(float *mix, unsigned int frames, KryLibdrawAudioBuffer *buffer,
           double *cursor, float volume, float pitch, float pan, int looping,
           int *still_active)
{
    unsigned int i;
    double pos;
    double step;
    float left;
    float right;
    float lgain;
    float rgain;

    if(still_active != nil)
        *still_active = 1;
    if(buffer == nil || buffer->data == nil || buffer->frameCount == 0 ||
       cursor == nil) {
        if(still_active != nil)
            *still_active = 0;
        return;
    }

    pos = *cursor;
    step = ((double)buffer->sampleRate / (double)KRY_AUDIO_RATE) *
           (double)clampf(pitch, 0.01f, 16.0f);
    volume = clampf(volume, 0.0f, 8.0f);
    pan = clampf(pan, 0.0f, 1.0f);
    lgain = pan <= 0.5f ? 1.0f : (1.0f - pan) * 2.0f;
    rgain = pan >= 0.5f ? 1.0f : pan * 2.0f;

    for(i = 0; i < frames; i++) {
        if(pos >= (double)buffer->frameCount) {
            if(looping) {
                while(pos >= (double)buffer->frameCount)
                    pos -= (double)buffer->frameCount;
            } else {
                if(still_active != nil)
                    *still_active = 0;
                break;
            }
        }
        left = buffer_sample(buffer, pos, 0);
        right = buffer->channels > 1 ? buffer_sample(buffer, pos, 1) : left;
        mix[i * 2 + 0] += left * volume * lgain;
        mix[i * 2 + 1] += right * volume * rgain;
        pos += step;
    }
    *cursor = pos;
}

static void
mix_audio(float *mix, unsigned int frames)
{
    int i;
    int active;

    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && !g_voices[i].paused) {
            active = 1;
            mix_buffer(mix, frames, g_voices[i].buffer, &g_voices[i].cursor,
                       g_voices[i].volume, g_voices[i].pitch, g_voices[i].pan,
                       0, &active);
            if(!active)
                memset(&g_voices[i], 0, sizeof(g_voices[i]));
        }
    }
    for(i = 0; i < KRY_AUDIO_MAX_MUSIC; i++) {
        if(g_music[i] != nil && g_music[i]->playing && !g_music[i]->paused) {
            active = 1;
            mix_buffer(mix, frames, g_music[i]->buffer, &g_music[i]->cursor,
                       g_music[i]->volume, g_music[i]->pitch,
                       g_music[i]->pan, g_music[i]->looping, &active);
            if(!active)
                g_music[i]->playing = 0;
        }
    }
    for(i = 0; i < KRY_AUDIO_MAX_PROCESSORS; i++) {
        if(g_mixed_processors[i] != nil)
            g_mixed_processors[i](mix, frames);
    }
    unlock(&g_audio_lock);
}

static void
write_mix(int fd, float *mix, unsigned char *out, unsigned int frames)
{
    unsigned int i;
    int l;
    int r;
    float master;

    master = clampf(g_master_volume, 0.0f, 8.0f);
    for(i = 0; i < frames; i++) {
        l = (int)(clampf(mix[i * 2 + 0] * master, -1.0f, 1.0f) * 32767.0f);
        r = (int)(clampf(mix[i * 2 + 1] * master, -1.0f, 1.0f) * 32767.0f);
        wr16(out + i * 4 + 0, l);
        wr16(out + i * 4 + 2, r);
    }
    write(fd, out, frames * 4);
}

static void
audio_child(void)
{
    int fd;
    float mix[KRY_AUDIO_CHUNK_FRAMES * 2];
    unsigned char out[KRY_AUDIO_CHUNK_FRAMES * 4];
    int active;

    fd = open_audio_owrite();
    if(fd < 0)
        exits("audio");
    while(g_audio_running) {
        memset(mix, 0, sizeof(mix));
        lock(&g_audio_lock);
        active = any_audio_active_locked();
        unlock(&g_audio_lock);
        if(active) {
            mix_audio(mix, KRY_AUDIO_CHUNK_FRAMES);
            write_mix(fd, mix, out, KRY_AUDIO_CHUNK_FRAMES);
        } else {
            sleep(10);
        }
    }
    close(fd);
    exits(nil);
}

static void
start_audio_child(void)
{
    int pid;

    if(g_audio_child_started)
        return;
    g_audio_running = 1;
    pid = rfork(RFPROC|RFMEM|RFNOWAIT);
    if(pid == 0)
        audio_child();
    if(pid > 0)
        g_audio_child_started = 1;
    else
        g_audio_running = 0;
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

static int
open_audio_owrite(void)
{
    int fd;

    fd = open("/dev/audio", OWRITE);
    if(fd >= 0)
        return fd;
    return open("#A/audio", OWRITE);
}

void
InitAudioDevice(void)
{
    int fd;

    fd = open_audio_owrite();
    if(fd >= 0) {
        close(fd);
        g_audio_ready = 1;
        start_audio_child();
    } else {
        g_audio_ready = 0;
    }
}

void
CloseAudioDevice(void)
{
    g_audio_ready = 0;
    g_audio_running = 0;
    sleep(20);
    g_audio_child_started = 0;
}
bool IsAudioDeviceReady(void) { return g_audio_ready != 0; }
void SetMasterVolume(float volume) { g_master_volume = clampf(volume, 0.0f, 8.0f); }
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
    buffer->frameCount = wave.frameCount;
    buffer->sampleRate = wave.sampleRate;
    buffer->sampleSize = wave.sampleSize;
    buffer->channels = wave.channels;
    buffer->owns = 1;
    buffer->volume = 1.0f;
    buffer->pitch = 1.0f;
    buffer->pan = 0.5f;
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
    if(buffer != nil) {
        lock(&g_audio_lock);
        stop_buffer_locked(buffer);
        unlock(&g_audio_lock);
        free_audio_buffer(buffer);
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
    int i;
    int slot;

    buffer = audio_buffer(sound.stream);
    if(!g_audio_ready || buffer == nil || buffer->data == nil)
        return;
    start_audio_child();
    lock(&g_audio_lock);
    slot = -1;
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(!g_voices[i].active) {
            slot = i;
            break;
        }
    }
    if(slot < 0)
        slot = 0;
    memset(&g_voices[slot], 0, sizeof(g_voices[slot]));
    g_voices[slot].active = 1;
    g_voices[slot].buffer = buffer;
    g_voices[slot].volume = buffer->volume;
    g_voices[slot].pitch = buffer->pitch;
    g_voices[slot].pan = buffer->pan;
    unlock(&g_audio_lock);
}
void StopSound(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;

    buffer = audio_buffer(sound.stream);
    lock(&g_audio_lock);
    stop_buffer_locked(buffer);
    unlock(&g_audio_lock);
}
void PauseSound(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;
    int i;

    buffer = audio_buffer(sound.stream);
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            g_voices[i].paused = 1;
    }
    unlock(&g_audio_lock);
}
void ResumeSound(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;
    int i;

    buffer = audio_buffer(sound.stream);
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            g_voices[i].paused = 0;
    }
    unlock(&g_audio_lock);
}
bool IsSoundPlaying(Sound sound)
{
    KryLibdrawAudioBuffer *buffer;
    int i;
    int playing;

    buffer = audio_buffer(sound.stream);
    playing = 0;
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && !g_voices[i].paused &&
           g_voices[i].buffer == buffer) {
            playing = 1;
            break;
        }
    }
    unlock(&g_audio_lock);
    return playing != 0;
}
void SetSoundVolume(Sound sound, float volume)
{
    KryLibdrawAudioBuffer *buffer;
    int i;

    buffer = audio_buffer(sound.stream);
    if(buffer == nil)
        return;
    volume = clampf(volume, 0.0f, 8.0f);
    lock(&g_audio_lock);
    buffer->volume = volume;
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            g_voices[i].volume = volume;
    }
    unlock(&g_audio_lock);
}
void SetSoundPitch(Sound sound, float pitch)
{
    KryLibdrawAudioBuffer *buffer;
    int i;

    buffer = audio_buffer(sound.stream);
    if(buffer == nil)
        return;
    pitch = clampf(pitch, 0.01f, 16.0f);
    lock(&g_audio_lock);
    buffer->pitch = pitch;
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            g_voices[i].pitch = pitch;
    }
    unlock(&g_audio_lock);
}
void SetSoundPan(Sound sound, float pan)
{
    KryLibdrawAudioBuffer *buffer;
    int i;

    buffer = audio_buffer(sound.stream);
    if(buffer == nil)
        return;
    pan = clampf(pan, 0.0f, 1.0f);
    lock(&g_audio_lock);
    buffer->pan = pan;
    for(i = 0; i < KRY_AUDIO_MAX_VOICES; i++) {
        if(g_voices[i].active && g_voices[i].buffer == buffer)
            g_voices[i].pan = pan;
    }
    unlock(&g_audio_lock);
}
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
    unsigned int out_frames;
    unsigned int out_bytes;
    unsigned char *out;
    unsigned int i;
    unsigned int ch;
    unsigned int bps;
    double src_pos;
    unsigned int src_frame;
    float v;

    if(wave == nil || !IsWaveValid(*wave) || sampleRate <= 0 ||
       channels <= 0 || (sampleSize != 8 && sampleSize != 16 &&
                         sampleSize != 32))
        return;
    out_frames = (unsigned int)(((double)wave->frameCount *
                                 (double)sampleRate) /
                                (double)wave->sampleRate);
    if(out_frames == 0)
        out_frames = 1;
    bps = (unsigned int)sampleSize / 8;
    out_bytes = out_frames * (unsigned int)channels * bps;
    out = malloc(out_bytes);
    if(out == nil)
        return;
    for(i = 0; i < out_frames; i++) {
        src_pos = ((double)i * (double)wave->sampleRate) /
                  (double)sampleRate;
        src_frame = (unsigned int)src_pos;
        if(src_frame >= wave->frameCount)
            src_frame = wave->frameCount - 1;
        for(ch = 0; ch < (unsigned int)channels; ch++) {
            if(wave->channels == 1)
                v = pcm_sample_at(wave->data, src_frame, 0,
                                  wave->sampleSize, wave->channels);
            else
                v = pcm_sample_at(wave->data, src_frame,
                                  ch < wave->channels ? ch : wave->channels - 1,
                                  wave->sampleSize, wave->channels);
            write_sample(out, (i * (unsigned int)channels + ch) * bps,
                         (unsigned int)sampleSize, v);
        }
    }
    free(wave->data);
    wave->data = out;
    wave->frameCount = out_frames;
    wave->sampleRate = (unsigned int)sampleRate;
    wave->sampleSize = (unsigned int)sampleSize;
    wave->channels = (unsigned int)channels;
}
float *LoadWaveSamples(Wave wave)
{
    float *samples;
    unsigned int total;
    unsigned int i;
    unsigned int ch;

    if(!IsWaveValid(wave))
        return nil;
    total = wave.frameCount * wave.channels;
    samples = malloc(total * sizeof(float));
    if(samples == nil)
        return nil;
    for(i = 0; i < wave.frameCount; i++) {
        for(ch = 0; ch < wave.channels; ch++)
            samples[i * wave.channels + ch] =
                pcm_sample_at(wave.data, i, ch, wave.sampleSize,
                              wave.channels);
    }
    return samples;
}
void UnloadWaveSamples(float *samples) { free(samples); }

static Music
music_from_wave(Wave wave)
{
    Music music;
    KryLibdrawMusic *state;
    Sound sound;
    int i;

    music = zero_music();
    sound = LoadSoundFromWave(wave);
    if(!IsSoundValid(sound))
        return music;
    state = malloc(sizeof(*state));
    if(state == nil) {
        UnloadSound(sound);
        return music;
    }
    memset(state, 0, sizeof(*state));
    state->buffer = audio_buffer(sound.stream);
    state->volume = 1.0f;
    state->pitch = 1.0f;
    state->pan = 0.5f;
    music.stream = sound.stream;
    music.frameCount = sound.frameCount;
    music.looping = false;
    music.ctxType = 1;
    music.ctxData = state;
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_MUSIC; i++) {
        if(g_music[i] == nil) {
            g_music[i] = state;
            break;
        }
    }
    unlock(&g_audio_lock);
    if(i == KRY_AUDIO_MAX_MUSIC) {
        free_audio_buffer(state->buffer);
        free(state);
        return zero_music();
    }
    return music;
}

Music LoadMusicStream(const char *fileName)
{
    Wave wave;
    Music music;

    wave = LoadWave(fileName);
    music = music_from_wave(wave);
    UnloadWave(wave);
    return music;
}
Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data,
                                int dataSize)
{
    Wave wave;
    Music music;

    wave = LoadWaveFromMemory(fileType, data, dataSize);
    music = music_from_wave(wave);
    UnloadWave(wave);
    return music;
}
bool IsMusicValid(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    return state != nil && state->buffer != nil && state->buffer->data != nil;
}
void UnloadMusicStream(Music music)
{
    KryLibdrawMusic *state;
    int i;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_MUSIC; i++) {
        if(g_music[i] == state)
            g_music[i] = nil;
    }
    unlock(&g_audio_lock);
    free_audio_buffer(state->buffer);
    free(state);
}
void PlayMusicStream(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(!g_audio_ready || state == nil)
        return;
    start_audio_child();
    lock(&g_audio_lock);
    state->playing = 1;
    state->paused = 0;
    state->looping = music.looping ? 1 : 0;
    if(state->cursor >= (double)state->buffer->frameCount)
        state->cursor = 0.0;
    unlock(&g_audio_lock);
}
bool IsMusicStreamPlaying(Music music)
{
    KryLibdrawMusic *state;
    int playing;

    state = (KryLibdrawMusic *)music.ctxData;
    playing = 0;
    lock(&g_audio_lock);
    if(state != nil && state->playing && !state->paused)
        playing = 1;
    unlock(&g_audio_lock);
    return playing != 0;
}
void UpdateMusicStream(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->looping = music.looping ? 1 : 0;
    unlock(&g_audio_lock);
}
void StopMusicStream(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->playing = 0;
    state->paused = 0;
    state->cursor = 0.0;
    unlock(&g_audio_lock);
}
void PauseMusicStream(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->paused = 1;
    unlock(&g_audio_lock);
}
void ResumeMusicStream(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->paused = 0;
    unlock(&g_audio_lock);
}
void SeekMusicStream(Music music, float position)
{
    KryLibdrawMusic *state;
    double frame;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil || state->buffer == nil)
        return;
    frame = (double)position * (double)state->buffer->sampleRate;
    if(frame < 0.0)
        frame = 0.0;
    if(frame > (double)state->buffer->frameCount)
        frame = (double)state->buffer->frameCount;
    lock(&g_audio_lock);
    state->cursor = frame;
    unlock(&g_audio_lock);
}
void SetMusicVolume(Music music, float volume)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->volume = clampf(volume, 0.0f, 8.0f);
    unlock(&g_audio_lock);
}
void SetMusicPitch(Music music, float pitch)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->pitch = clampf(pitch, 0.01f, 16.0f);
    unlock(&g_audio_lock);
}
void SetMusicPan(Music music, float pan)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil)
        return;
    lock(&g_audio_lock);
    state->pan = clampf(pan, 0.0f, 1.0f);
    unlock(&g_audio_lock);
}
float GetMusicTimeLength(Music music)
{
    KryLibdrawMusic *state;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil || state->buffer == nil || state->buffer->sampleRate == 0)
        return 0.0f;
    return (float)((double)state->buffer->frameCount /
                   (double)state->buffer->sampleRate);
}
float GetMusicTimePlayed(Music music)
{
    KryLibdrawMusic *state;
    float t;

    state = (KryLibdrawMusic *)music.ctxData;
    if(state == nil || state->buffer == nil || state->buffer->sampleRate == 0)
        return 0.0f;
    lock(&g_audio_lock);
    t = (float)(state->cursor / (double)state->buffer->sampleRate);
    unlock(&g_audio_lock);
    return t;
}

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
void AttachAudioMixedProcessor(AudioCallback processor)
{
    int i;

    if(processor == nil)
        return;
    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_PROCESSORS; i++) {
        if(g_mixed_processors[i] == processor) {
            unlock(&g_audio_lock);
            return;
        }
    }
    for(i = 0; i < KRY_AUDIO_MAX_PROCESSORS; i++) {
        if(g_mixed_processors[i] == nil) {
            g_mixed_processors[i] = processor;
            break;
        }
    }
    unlock(&g_audio_lock);
}
void DetachAudioMixedProcessor(AudioCallback processor)
{
    int i;

    lock(&g_audio_lock);
    for(i = 0; i < KRY_AUDIO_MAX_PROCESSORS; i++) {
        if(g_mixed_processors[i] == processor)
            g_mixed_processors[i] = nil;
    }
    unlock(&g_audio_lock);
}

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
