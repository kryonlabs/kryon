# Kryon Plugin Architecture - CORRECTED

**Date:** January 11, 2026
**Status:** Planning - CORRECTED

---

## Correct Architecture Understanding

### Core IR (`kryon/ir/`) - Minimal UI Framework

**Purpose:** Universal UI representation, NOT game/graphics engine

**Contains:**
- IR structures (components, properties, styles, layouts)
- Serialization (binary, JSON)
- Parsers (KRY, HTML, Markdown, TSX, C, Lua)
- Logic/Executor (universal logic)
- Plugin interface
- Validation, hot reload
- Text shaping (needed for all UIs)
- Component definitions (types only, not behavior)

**NOT Contains:**
- No physics
- No sprites
- No particles
- No audio (maybe minimal types)
- No game loop
- No entities

---

### Kryon Plugins

| Plugin | Purpose | Contains |
|--------|---------|----------|
| **canvas** | 2D graphics/rendering | Sprites, Particles, Physics (collision), Animation, Tilemaps |
| **audio** | Sound/music | Streaming, mixing, decoder, Sound/Music components |
| storage | Data persistence | JSON storage, key-value |
| syntax | Code syntax highlighting | Lexers, tokenizers, themes |
| flowchart | Diagrams | Flowchart rendering |

---

## Canvas Plugin Structure

**`kryon-plugins/canvas/`** - Unified 2D graphics plugin

```
kryon-plugins/canvas/
├── include/
│   ├── kryon_canvas.h              # Main canvas API
│   ├── canvas_sprite.h             # Sprite rendering
│   ├── canvas_particle.h           # Particle systems
│   ├── canvas_physics.h            # 2D physics for games
│   ├── canvas_tilemap.h            # Tilemap rendering
│   ├── canvas_animation.h          # Animation/frames
│   └── canvas_plugin.h             # Plugin registration
├── src/
│   ├── canvas.c                    # Core canvas
│   ├── canvas_renderer.c           # Rendering backend interface
│   ├── sprite.c                    # Sprite/atlas system
│   ├── particle.c                  # Particle emitters
│   ├── physics.c                   # 2D physics (AABB, circle, raycast)
│   ├── tilemap.c                   # Tilemap
│   ├── animation.c                 # Animation
│   └── plugin_init.c
├── tests/
│   ├── test_sprite.c
│   ├── test_physics.c
│   └── test_particle.c
├── plugin.toml
├── Makefile
└── README.md
```

### Canvas Plugin API

```c
// kryon_canvas.h - Main entry point
typedef struct {
    uint32_t width;
    uint32_t height;
    void* backend_context;  // SDL_Renderer*, etc.
} KCanvas;

KCanvas* k_canvas_create(uint32_t width, uint32_t height);
void k_canvas_clear(KCanvas* canvas, uint32_t color);
void k_canvas_present(KCanvas* canvas);

// canvas_sprite.h - Part of canvas plugin
typedef uint32_t KSpriteID;
KSpriteID k_canvas_sprite_load(KCanvas* canvas, const char* path);
void k_canvas_sprite_draw(KCanvas* canvas, KSpriteID sprite, float x, float y);

// canvas_particle.h - Part of canvas plugin
typedef uint32_t KParticleEmitterID;
KParticleEmitterID k_canvas_particle_emitter_create(KCanvas* canvas);
void k_canvas_particle_emitter_emit(KCanvas* canvas, KParticleEmitterID emitter);

// canvas_physics.h - Physics for games (part of canvas)
typedef uint32_t KPhysicsBodyID;
KPhysicsBodyID k_canvas_physics_create_box(KCanvas* canvas, float x, float y, float w, float h);
void k_canvas_physics_step(KCanvas* canvas, float dt);
bool k_canvas_physics_raycast(KCanvas* canvas, float x1, float y1, float x2, float y2);
```

---

## Audio Plugin Structure

**`kryon-plugins/audio/`** - Sound and music

```
kryon-plugins/audio/
├── include/
│   ├── kryon_audio.h
│   ├── audio_mixer.h
│   ├── audio_stream.h
│   ├── audio_decoder.h
│   └── audio_plugin.h
├── src/
│   ├── audio.c
│   ├── mixer.c
│   ├── stream.c
│   ├── decoder_ogg.c
│   ├── decoder_mp3.c
│   └── plugin_init.c
└── tests/
```

### Canvas-Audio Relationship

The canvas plugin can OPTIONALLY depend on audio plugin:

```c
// In canvas plugin, if audio is available:
#ifdef KRYON_HAS_AUDIO
    // Play sound on particle collision
    k_audio_play_sound(collision_sound, 0.5f);
#endif
```

---

## What to Remove from Core IR

### Delete from `kryon/ir/`:

```
ir_physics.h/c      → Move to kryon-plugins/canvas/src/physics.c
ir_sprite.h/c       → Move to kryon-plugins/canvas/src/sprite.c
ir_particle.h/c     → Move to kryon-plugins/canvas/src/particle.c
ir_entity.h/c       → Move to kryon-plugins/canvas/src/entity.c (or delete?)
ir_game_loop.h/c    → Delete (backend manages its own loop)
ir_input.h/c        → Keep minimal types in core, move state to backend
```

### Keep in Core IR (Minimal):

```
ir_native_canvas.h/c  → KEEP - Just a bridge to canvas plugin
```

The `ir_native_canvas.h` stays because it's the BRIDGE between IR components and the canvas plugin:

```c
// ir_native_canvas.h - In core IR
// This just stores the callback, actual rendering is in canvas plugin

typedef struct {
    void (*render_callback)(uint32_t component_id);
    void* user_data;
} IRNativeCanvasData;

// The canvas plugin registers a callback bridge to handle this component type
```

---

## Implementation Plan

### Phase 1: Create Canvas Plugin Structure

1. Extend existing `kryon-plugins/canvas/` with new modules
2. Add `src/sprite.c` (from `ir_sprite.c`)
3. Add `src/particle.c` (from `ir_particle.c`)
4. Add `src/physics.c` (from `ir_physics.c`)
5. Update `plugin.toml` with new capabilities
6. Update Makefile

### Phase 2: Create Audio Plugin

1. Create `kryon-plugins/audio/`
2. Move `ir_audio*.h/c` to plugin
3. Register Sound/Music component types
4. Test

### Phase 3: Clean Core IR

1. Delete migrated files from `kryon/ir/`
2. Update `ir/Makefile`
3. Update any internal dependencies

### Phase 4: Update Canvas Component

The `IR_COMPONENT_CANVAS` stays in core, but the canvas plugin handles rendering:

```c
// In core IR - component definition
#define IR_COMPONENT_CANVAS 45

// In canvas plugin - registration
bool kryon_canvas_plugin_init(void* ctx) {
    ir_plugin_register_component_renderer(IR_COMPONENT_CANVAS, canvas_render);
    return true;
}
```

---

## File Mapping

| From Core IR | To Canvas Plugin |
|--------------|------------------|
| `ir_physics.h/c` | `canvas/include/canvas_physics.h`, `canvas/src/physics.c` |
| `ir_sprite.h/c` | `canvas/include/canvas_sprite.h`, `canvas/src/sprite.c` |
| `ir_particle.h/c` | `canvas/include/canvas_particle.h`, `canvas/src/particle.c` |
| `ir_entity.h/c` | `canvas/include/canvas_entity.h`, `canvas/src/entity.c` |

| From Core IR | To Audio Plugin |
|--------------|----------------|
| `ir_audio.h/c` | `audio/include/kryon_audio.h`, `audio/src/audio.c` |
| `ir_audio_mixer.h/c` | `audio/include/audio_mixer.h`, `audio/src/mixer.c` |
| `ir_audio_stream.h/c` | `audio/include/audio_stream.h`, `audio/src/stream.c` |
| `ir_audio_decoder.h/c` | `audio/include/audio_decoder.h`, `audio/src/decoder.c` |

---

## Questions

1. Should entities stay in core or move to canvas plugin?
2. Should audio be a standalone plugin or integrated into canvas?
3. What about input - keep minimal in core or move to canvas/backend?
