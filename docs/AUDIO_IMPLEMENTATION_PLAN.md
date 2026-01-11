# Kryon Audio System - Full Implementation Plan

**Status:** Planning Phase
**Date:** January 11, 2026

---

## Current State Analysis

### ✅ What Exists

| Component | Status | Notes |
|-----------|--------|-------|
| **Core API** (`ir_audio.h/c`) | ✅ Complete | 560-line header, comprehensive API |
| **Plugin Architecture** | ✅ Complete | Backend interface fully defined |
| **SDL3 Backend** | 🟡 Partial | ~450 lines, basic WAV playback |
| **Per-Instance State** | 🟡 Partial | Framework exists, stub implementations |
| **Spatial Audio** | 🟡 Partial | Calculations exist, no backend support |

### ❌ What's Missing

| Feature | Priority | Impact |
|---------|----------|--------|
| **Volume Control** | 🔴 High | Backend doesn't apply volume to PCM |
| **Stereo Panning** | 🔴 High | TODO in SDL3 backend |
| **OGG/MP3 Support** | 🟡 Medium | Only WAV supported natively |
| **Music Streaming** | 🟡 Medium | Loads full file into memory |
| **Per-Instance Functions** | 🟡 Medium | 5 TODO stubs in ir_audio.c |
| **miniaudio Backend** | 🟢 Low | Alternative backend option |
| **OpenAL Backend** | 🟢 Low | For advanced 3D spatial audio |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                        │
│  ir_audio_load_sound(), ir_audio_play_sound(), etc.            │
└─────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Core Audio System (ir_audio.c)             │
│  • Sound/Music Registry  • Channel Management  • Volume Control │
│  • Spatial Audio Calc     • Per-Instance State   • Statistics   │
└─────────────────────────────────────────────────────────────────┘
                                  │
                    ┌─────────────┼─────────────┐
                    ▼             ▼             ▼
            ┌───────────┐  ┌──────────┐  ┌──────────────┐
            │  SDL3     │  │ miniaudio│  │   OpenAL     │
            │  Backend  │  │ Backend  │  │   Backend    │
            └───────────┘  └──────────┘  └──────────────┘
                    │             │             │
                    ▼             ▼             ▼
            ┌───────────────────────────────────────────────────┐
            │            Platform Audio APIs                    │
            │  SDL3_audio  │  miniaudio.h  │  ALC/ALFW        │
            └───────────────────────────────────────────────────┘
```

---

## Implementation Phases

### Phase 1: Complete SDL3 Backend (Foundation)

**Goal:** Make SDL3 backend production-ready with all core features.

#### 1.1 Volume Control (HIGH PRIORITY)

**Problem:** SDL3 doesn't provide per-stream volume control natively.

**Solution:** Implement software mixing by applying volume to PCM data.

```c
// New file: ir/audio_mixer.h
typedef struct {
    int16_t* buffer;
    uint32_t frames;
    uint8_t channels;
} IRAudioBuffer;

void ir_audio_apply_volume(IRAudioBuffer* buffer, float volume);
void ir_audio_apply_pan(IRAudioBuffer* buffer, float pan);
```

**Files to create:**
- `ir/audio_mixer.h` - Software mixing utilities
- `ir/audio_mixer.c` - PCM manipulation functions

**Implementation:**
```c
// Apply volume to 16-bit PCM samples
void ir_audio_apply_volume(int16_t* samples, uint32_t count, float volume) {
    float gain = fmaxf(0.0f, fminf(1.0f, volume));
    for (uint32_t i = 0; i < count; i++) {
        int32_t sample = (int32_t)(samples[i] * gain);
        samples[i] = (int16_t)CLAMP(sample, INT16_MIN, INT16_MAX);
    }
}
```

**Files to modify:**
- `renderers/sdl3/sdl3_audio.c` - Apply volume in `backend_set_channel_volume()`

---

#### 1.2 Stereo Panning

**Problem:** Panning is ignored in current implementation.

**Solution:** Implement stereo panning using constant power panning.

```c
// Constant power panning (equal loudness)
void ir_audio_apply_pan_stereo(int16_t* samples, uint32_t frame_count, float pan) {
    // pan: -1.0 (left) to 1.0 (right)
    float angle = (pan + 1.0f) * M_PI_4;  // 0 to π/2
    float left_gain = cosf(angle);
    float right_gain = sinf(angle);

    for (uint32_t i = 0; i < frame_count; i++) {
        int32_t left = samples[i * 2];
        int32_t right = samples[i * 2 + 1];
        samples[i * 2] = CLAMP(left * left_gain, INT16_MIN, INT16_MAX);
        samples[i * 2 + 1] = CLAMP(right * right_gain, INT16_MIN, INT16_MAX);
    }
}
```

**Files to modify:**
- `ir/audio_mixer.c` - Add panning function
- `renderers/sdl3/sdl3_audio.c` - Apply pan before queuing

---

#### 1.3 In-Memory Sound Loading Fix

**Problem:** `backend_load_sound_from_memory()` not implemented in SDL3 backend.

**Solution:** Use `SDL_LoadWAV_RW()` with memory buffer.

```c
static void* backend_load_sound_from_memory(
    const char* name, const void* data, size_t size,
    uint32_t* duration_ms, uint32_t* sample_rate, uint8_t* channels
) {
    SDL3Sound* sound = malloc(sizeof(SDL3Sound));
    if (!sound) return NULL;

    SDL_RWops* rw = SDL_RWFromConstMem(data, (int)size);
    if (!rw) {
        free(sound);
        return NULL;
    }

    sound->buffer = SDL_LoadWAV_RW(rw, true, &sound->spec, &sound->length);
    sound->path = strdup(name);

    // Calculate duration...
    return sound;
}
```

**Files to modify:**
- `renderers/sdl3/sdl3_audio.c` - Add `backend_load_sound_from_memory()`
- `ir/ir_audio.h` - Already declared in plugin interface

---

#### 1.4 Channel Pool Management

**Problem:** Channel tracking is basic; no proper recycling.

**Solution:** Implement free list for channel allocation.

```c
// New: Channel pool manager
typedef struct {
    IRChannelID free_list[IR_MAX_CHANNELS];
    uint32_t free_count;
} IRChannelPool;

IRChannelID ir_audio_channel_alloc(IRAudioState* state);
void ir_audio_channel_free(IRAudioState* state, IRChannelID id);
```

---

### Phase 2: Audio Format Support

**Goal:** Add OGG and MP3 support through external libraries.

#### 2.1 OGG Vorbis Support

**Library:** libogg + libvorbis (or stb_vorbis for header-only)

**Files to create:**
- `ir/audio_decoders.h` - Format decoder interface
- `ir/audio_decoders.c` - Format decoder implementations
- `ir/decoders/vorbis_decoder.c` - OGG decoder using stb_vorbis

```c
// Audio decoder interface
typedef struct {
    const char* extension;
    void* (*decode)(const void* data, size_t size,
                   uint32_t* out_samples, uint32_t* out_channels,
                   uint32_t* out_sample_rate);
} IRAudioDecoder;
```

**Integration:**
- Modify `backend_load_sound()` to detect format and use appropriate decoder
- Fall back to SDL3's WAV loader for .wav files

---

#### 2.2 MP3 Support

**Library:** minimp3 (header-only, single file)

**Files to create:**
- `ir/decoders/mp3_decoder.c` - MP3 decoder using minimp3

**Integration:**
- Register MP3 decoder with decoder registry
- Handle ID3 tags (optional)

---

#### 2.3 Decoder Registry

```c
// New file: ir/audio_decoder_registry.c
typedef struct {
    IRAudioDecoder decoders[16];
    uint32_t count;
} IRAudioDecoderRegistry;

void ir_audio_register_decoder(IRAudioDecoder* decoder);
IRAudioDecoder* ir_audio_find_decoder(const char* extension);
```

---

### Phase 3: Music Streaming

**Goal:** Stream large music files instead of loading entirely into memory.

#### 3.1 Streaming Architecture

```c
// New: ir/audio_stream.h
typedef struct {
    FILE* file;
    uint32_t file_size;
    uint32_t read_position;
    uint32_t buffer_size;
    uint8_t* decode_buffer;
    uint8_t* pcm_buffer;
    bool eof;
} IRMusicStream;

IRMusicStream* ir_audio_stream_create(const char* path);
uint32_t ir_audio_stream_decode(IRMusicStream* stream, uint32_t frames_needed);
void ir_audio_stream_destroy(IRMusicStream* stream);
```

#### 3.2 Streaming Backend Integration

**Files to modify:**
- `renderers/sdl3/sdl3_audio.c` - Add streaming support
  - Use ring buffer for decoded PCM
  - Background thread for decoding
  - Callback-based buffer refill

```c
// Streaming music playback
static void* music_stream_thread(void* userdata) {
    IRMusicStream* stream = (IRMusicStream*)userdata;

    while (stream->playing) {
        // Decode next chunk
        uint32_t decoded = ir_audio_stream_decode(stream, BUFFER_SIZE);
        if (decoded > 0) {
            SDL_PutAudioStreamData(g_sdl3_audio.music_stream,
                                   stream->pcm_buffer, decoded);
        }
        usleep(10000);  // 10ms
    }

    return NULL;
}
```

---

### Phase 4: Complete Per-Instance Audio

**Goal:** Finish the 5 TODO stub functions in `ir_audio.c`.

#### 4.1 Implement State Functions

**Files to modify:** `ir/ir_audio.c`

```c
// Replace TODO implementations

IRSoundID ir_audio_load_sound_state(IRAudioState* state, const char* path) {
    if (!state) state = get_global_audio_state();

    // Check capacity
    if (state->sound_count >= IR_MAX_SOUNDS) return IR_INVALID_SOUND;

    // Check for existing sound
    for (uint32_t i = 0; i < state->sound_count; i++) {
        if (strcmp(state->sounds[i].path, path) == 0) {
            return state->sounds[i].id;
        }
    }

    // Load via global backend
    IRSoundID global_id = ir_audio_load_sound(path);
    if (global_id == IR_INVALID_SOUND) return IR_INVALID_SOUND;

    // Copy to state
    IRSound* global_sound = find_sound(global_id);
    IRSound* state_sound = &state->sounds[state->sound_count];
    memcpy(state_sound, global_sound, sizeof(IRSound));
    state_sound->id = state->sound_count + 1;
    state->sound_count++;

    return state_sound->id;
}

IRChannelID ir_audio_play_sound_state(IRAudioState* state, IRSoundID sound_id,
                                       float volume, float pan, bool loop) {
    if (!state) state = get_global_audio_state();

    // Find sound in state
    IRSound* sound = find_sound_in_state(state, sound_id);
    if (!sound) return IR_INVALID_CHANNEL;

    // Find free channel in state
    IRChannel* channel = NULL;
    for (uint32_t i = 0; i < IR_MAX_CHANNELS; i++) {
        if (state->channels[i].state == IR_AUDIO_STOPPED) {
            channel = &state->channels[i];
            break;
        }
    }
    if (!channel) return IR_INVALID_CHANNEL;

    // Use global backend for actual playback
    IRChannelID backend_id = g_audio_system.backend.play_sound(
        sound->backend_data,
        volume * state->master_volume,
        pan,
        loop
    );

    if (backend_id == IR_INVALID_CHANNEL) return IR_INVALID_CHANNEL;

    // Track in state
    channel->id = backend_id;
    channel->sound_id = sound_id;
    channel->state = IR_AUDIO_PLAYING;
    channel->volume = volume;
    channel->pan = pan;
    channel->loop = loop;

    return backend_id;
}

// Similar implementations for:
// - ir_audio_stop_channel_state()
// - ir_audio_set_channel_volume_state()
// - ir_audio_update_state()
```

---

### Phase 5: miniaudio Backend

**Goal:** Create alternative backend using miniaudio (header-only).

#### 5.1 Backend Structure

**Files to create:**
- `renderers/miniaudio/miniaudio_audio.h`
- `renderers/miniaudio/miniaudio_audio.c`

```c
// miniaudio backend implementation
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

static ma_device g_device;
static ma_sound g_sounds[IR_MAX_SOUNDS];
static ma_engine g_engine;

static bool backend_init(IRAudioConfig* config) {
    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.sampleRate = config->sample_rate;
    engine_config.channels = config->channels;

    if (ma_engine_init(&engine_config, &g_engine) != MA_SUCCESS) {
        return false;
    }

    return true;
}

static void* backend_load_sound(const char* path, ...) {
    ma_sound* sound = &g_sounds[next_sound_index++];
    if (ma_sound_init_from_file(&g_engine, path, 0, NULL, sound) != MA_SUCCESS) {
        return NULL;
    }
    return sound;
}

static IRChannelID backend_play_sound(void* sound_data, float volume, float pan, bool loop) {
    ma_sound* sound = (ma_sound*)sound_data;
    ma_sound_set_volume(sound, volume);
    ma_sound_set_pan(sound, pan);
    if (loop) ma_sound_set_looping(sound, true);
    ma_sound_start(sound);
    return (IRChannelID)sound;  // Use pointer as ID
}
```

**Advantages of miniaudio:**
- Built-in volume and panning
- Supports more formats
- Simpler API than SDL3 for audio
- Cross-platform

---

### Phase 6: OpenAL Backend (3D Spatial Audio)

**Goal:** Advanced 3D spatial audio with environmental effects.

#### 6.1 Backend Structure

**Files to create:**
- `renderers/openal/openal_audio.h`
- `renderers/openal/openal_audio.c`

```c
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

static ALCdevice* g_device;
static ALCcontext* g_context;
static ALuint g_sources[IR_MAX_CHANNELS];
static ALuint g_buffers[IR_MAX_SOUNDS];

// 3D positioning
static void backend_set_listener_position(float x, float y, float z) {
    alListener3f(AL_POSITION, x, y, z);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    float orientation[6] = {0, 0, -1, 0, 1, 0};  // forward, up
    alListenerfv(AL_ORIENTATION, orientation);
}

static void backend_set_source_position(IRChannelID channel_id,
                                        float x, float y, float z) {
    ALuint source = g_sources[channel_id - 1];
    alSource3f(source, AL_POSITION, x, y, z);
}
```

---

### Phase 7: Audio Components for KIR

**Goal:** Add audio components to KIR for declarative sound definitions.

#### 7.1 KIR Component Definitions

```json
{
  "type": "Sound",
  "props": {
    "src": "@sounds/button_click.wav",
    "autoPlay": false,
    "volume": 0.8,
    "loop": false
  },
  "events": {
    "onPlay": "setPlaying(true)"
  }
}
```

#### 7.2 Parser Implementation

**Files to modify:**
- `ir/parsers/tsx/tsx_parser.c` - Add Sound component parsing
- `ir/parsers/html/html_parser.c` - Add <audio> tag support

---

## File Organization

```
kryon/
├── ir/
│   ├── ir_audio.h              # Core API (✅ existing)
│   ├── ir_audio.c              # Core implementation (🟡 partial)
│   ├── audio_mixer.h           # NEW: Software mixing utilities
│   ├── audio_mixer.c           # NEW: PCM manipulation
│   ├── audio_stream.h          # NEW: Streaming interface
│   ├── audio_stream.c          # NEW: Streaming implementation
│   ├── audio_decoders.h        # NEW: Decoder interface
│   ├── audio_decoders.c        # NEW: Decoder registry
│   └── decoders/
│       ├── vorbis_decoder.c    # NEW: OGG decoder
│       └── mp3_decoder.c       # NEW: MP3 decoder
│
├── renderers/
│   ├── sdl3/
│   │   ├── sdl3_audio.h        # 🟡 partial - needs completion
│   │   └── sdl3_audio.c        # 🟡 partial - needs completion
│   ├── miniaudio/              # NEW: miniaudio backend
│   │   ├── miniaudio_audio.h
│   │   ├── miniaudio_audio.c
│   │   └── miniaudio.h         # (third_party)
│   └── openal/                 # NEW: OpenAL backend
│       ├── openal_audio.h
│       └── openal_audio.c
│
└── examples/
    └── audio_demo.kry          # NEW: Audio example
```

---

## Implementation Order (Recommended)

1. **Phase 1.1-1.2** (Volume & Pan) - Foundation for everything else
2. **Phase 4** (Per-Instance) - Complete the stub functions
3. **Phase 1.3** (Memory Loading) - Enable embedded sounds
4. **Phase 2.1** (OGG Support) - Most common format after WAV
5. **Phase 3** (Streaming) - Better memory usage for music
6. **Phase 5** (miniaudio) - Alternative backend, easier to maintain
7. **Phase 6** (OpenAL) - For games requiring 3D audio
8. **Phase 7** (KIR Integration) - Declarative audio in UI

---

## Testing Strategy

### Unit Tests

```c
// tests/audio/test_mixer.c
void test_volume_clamping(void);
void test_volume_application(void);
void test_stereo_panning(void);
void test_panning_constants(void);

// tests/audio/test_decoders.c
void test_wav_decoder(void);
void test_ogg_decoder(void);
void test_mp3_decoder(void);
```

### Integration Tests

```c
// tests/audio/test_playback.c
void test_sound_playback(void);
void test_music_playback(void);
void test_multiple_sounds(void);
void test_volume_ramping(void);
void test_spatial_audio(void);
```

### Example Application

```lua
-- examples/audio_demo.kry
Window "Audio Demo" {
    Button "Play Sound" {
        onClick = "playSound('click.wav')"
    }

    Slider "Music Volume" {
        value = 0.8,
        onChange = "setMusicVolume(value)"
    }

    Toggle "Mute" {
        checked = false,
        onChange = "setMasterVolume(value ? 1.0 : 0.0)"
    }
}
```

---

## Dependencies

| Library | Purpose | License | Size |
|---------|---------|---------|------|
| **SDL3** | Base audio (already used) | Zlib | ~ |
| **stb_vorbis** | OGG decoding (header-only) | Public Domain | ~60KB |
| **minimp3** | MP3 decoding (header-only) | CC0/MIT | ~20KB |
| **miniaudio** | Alternative backend | Public Domain | ~200KB |
| **OpenAL** | 3D spatial audio | LGPL | ~ |

---

## Success Criteria

- [ ] All 5 TODO functions in `ir_audio.c` implemented
- [ ] Volume control working in SDL3 backend
- [ ] Stereo panning working in SDL3 backend
- [ ] OGG files playable
- [ ] MP3 files playable
- [ ] Music streaming for large files
- [ ] miniaudio backend functional
- [ ] Per-instance audio fully working
- [ ] All tests pass
- [ ] Example demo works

---

## Estimated Effort

| Phase | Estimated Time | Complexity |
|-------|---------------|------------|
| 1.1 Volume Control | 4-6 hours | Medium |
| 1.2 Stereo Panning | 2-3 hours | Low |
| 1.3 Memory Loading | 2-3 hours | Low |
| 1.4 Channel Pool | 2-3 hours | Low |
| 2.1 OGG Support | 4-6 hours | Medium |
| 2.2 MP3 Support | 3-4 hours | Medium |
| 2.3 Decoder Registry | 2-3 hours | Low |
| 3 Streaming | 8-12 hours | High |
| 4 Per-Instance | 4-6 hours | Medium |
| 5 miniaudio Backend | 6-8 hours | Medium |
| 6 OpenAL Backend | 8-12 hours | High |
| 7 KIR Integration | 4-6 hours | Medium |

**Total:** ~53-82 hours (1-2 weeks focused work)

---

## Next Steps

1. Start with **Phase 1.1** (Volume Control) - highest priority
2. Create `ir/audio_mixer.h` and `ir/audio_mixer.c`
3. Modify `renderers/sdl3/sdl3_audio.c` to use mixer
4. Test with example application
5. Continue through phases in order

---

**Document Version:** 1.0
**Last Updated:** January 11, 2026
