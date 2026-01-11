# Kryon Plugin Migration - FINAL PLAN

**Date:** January 11, 2026
**Status:** Ready for Implementation

---

## Final Architecture Decisions

### Core IR (`kryon/ir/`) - KEEP

**Keep:**
- All IR structures, serialization, parsers
- `ir_input.h/c` - Input is a core UI thing
- `ir_native_canvas.h/c` - Bridge to canvas plugin
- `ir_plugin.h/c` - Plugin interface
- `ir_asset.h/c` - Asset types (minimal)

**Move to Plugins:**
- `ir_physics.h/c` → Canvas plugin
- `ir_sprite.h/c` → Canvas plugin
- `ir_particle.h/c` → Canvas plugin
- `ir_entity.h/c` → Canvas plugin
- `ir_game_loop.h/c` → Canvas plugin
- `ir_audio*.h/c` → Audio plugin

---

## Canvas Plugin Structure

**`kryon-plugins/canvas/`** - 2D graphics + games

```
kryon-plugins/canvas/
├── include/
│   ├── kryon_canvas.h
│   ├── canvas_sprite.h
│   ├── canvas_particle.h
│   ├── canvas_physics.h
│   ├── canvas_entity.h
│   └── canvas_plugin.h
├── src/
│   ├── canvas.c
│   ├── sprite.c
│   ├── particle.c
│   ├── physics.c
│   ├── entity.c
│   ├── game_loop.c
│   └── plugin_init.c
├── plugin.toml
└── Makefile
```

---

## Audio Plugin Structure

**`kryon-plugins/audio/`** - Standalone audio

```
kryon-plugins/audio/
├── include/
│   └── kryon_audio.h
├── src/
│   ├── audio.c
│   ├── mixer.c
│   ├── stream.c
│   ├── decoder.c
│   └── plugin_init.c
├── plugin.toml
└── Makefile
```

**Rationale:** Can use `<audio>` elements without requiring full canvas plugin.

---

## Implementation Steps

1. **Extend canvas plugin** with sprite, particle, physics, entity, game_loop
2. **Create audio plugin** from ir_audio* files
3. **Remove from core IR** the migrated files
4. **Update Makefiles** across all projects

---

## File Mapping

| From Core IR | To Plugin |
|--------------|-----------|
| `ir_physics.h/c` | `canvas/src/physics.c` |
| `ir_sprite.h/c` | `canvas/src/sprite.c` |
| `ir_particle.h/c` | `canvas/src/particle.c` |
| `ir_entity.h/c` | `canvas/src/entity.c` |
| `ir_game_loop.h/c` | `canvas/src/game_loop.c` |
| `ir_audio.h/c` | `audio/src/audio.c` |
| `ir_audio_mixer.h/c` | `audio/src/mixer.c` |
| `ir_audio_stream.h/c` | `audio/src/stream.c` |
| `ir_audio_decoder.h/c` | `audio/src/decoder.c` |
