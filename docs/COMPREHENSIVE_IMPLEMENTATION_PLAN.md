# Kryon Framework - Comprehensive Implementation Plan

**Date:** January 11, 2026
**Status:** Planning
**Scope:** All missing features across the Kryon framework

---

## Executive Summary

This plan covers implementation of **all identified missing features** across the Kryon framework, organized by module and priority.

| Module | TODOs | Est. Complexity |
|--------|-------|-----------------|
| Audio System | 3 | Medium |
| Sprite System | 3 | Low-Medium |
| Physics System | 3 | Medium-High |
| Validation | 3 | Low |
| Stylesheet | 3 | Low |
| Hot Reload | 2 | Medium |
| HTML/KRY Elements | 20+ | Medium |
| Other (particles, executor, assets) | 3 | Low |

**Total: ~40 implementation tasks**

---

## Phase A: Complete Audio System (HIGH PRIORITY)

### A.1 Music Streaming Implementation

**Goal:** Stream large music files instead of loading entirely into memory.

**Files to Create:**
- `ir/ir_audio_stream.h`
- `ir/ir_audio_stream.c`

**Files to Modify:**
- `ir/ir_audio.h` - Add streaming API
- `renderers/sdl3/sdl3_audio.c` - Streaming backend
- `ir/ir_audio.c` - Stream management

**API Design:**
```c
// Streaming state
typedef struct {
    FILE* file;
    uint32_t file_size;
    uint32_t read_position;
    uint32_t buffer_size;
    uint8_t* decode_buffer;
    uint8_t* pcm_buffer;
    uint32_t pcm_capacity;
    uint32_t pcm_available;
    bool eof;
    bool playing;
    bool loop;
    IRAudioFormat format;
} IRMusicStream;

// Stream API
IRMusicStream* ir_audio_stream_create(const char* path);
uint32_t ir_audio_stream_decode(IRMusicStream* stream, uint32_t frames_needed);
void ir_audio_stream_destroy(IRMusicStream* stream);
bool ir_audio_stream_play(IRMusicStream* stream, bool loop);
void ir_audio_stream_stop(IRMusicStream* stream);
void ir_audio_stream_set_volume(IRMusicStream* stream, float volume);
```

**Implementation Steps:**
1. Create stream data structure with ring buffer
2. Implement chunked decoding (decode 4KB chunks)
3. Add background thread for continuous decoding
4. Integrate with SDL3 audio callbacks
5. Handle EOF and looping

**Dependencies:** None (builds on existing decoder)

**Est. Time:** 4-6 hours

---

### A.2 OpenAL Backend for 3D Spatial Audio

**Goal:** Hardware-accelerated 3D positional audio.

**Files to Create:**
- `backends/openal/openal_audio.h`
- `backends/openal/openal_audio.c`
- `backends/openal/Makefile`

**Features:**
- 3D positional audio
- Distance attenuation
- Doppler effect
- Reverb effects
- HRTF support (headphones)

**API Design:**
```c
typedef struct {
    float position[3];
    float velocity[3];
    float direction[3];
    float max_distance;
    float rolloff_factor;
    float reference_distance;
} IRAudioSource3D;

IRAudioSource3D* ir_audio_create_source_3d(void);
void ir_audio_set_source_position_3d(IRAudioSource3D* src, float x, float y, float z);
void ir_audio_set_listener_3d(float x, float y, float z,
                                float fx, float fy, float fz,
                                float ux, float uy, float uz);
```

**Dependencies:** OpenAL Soft library

**Est. Time:** 6-8 hours

---

### A.3 KIR Audio Components

**Goal:** Declarative audio in KRY/TSX markup.

**Syntax Examples:**
```kry
// Sound component
<Sound src="click.ogg" volume={0.5} autoplay={false} />

// Music component
<Music src="background.mp3" loop={true} volume={0.3} />

// Audio with events
<Sound src="hover.ogg" onPlay={handleHoverSound} />
```

**Files to Modify:**
- `parsers/kry/kry_parser.c` - Add Sound/Music node parsing
- `parsers/kry/kry_to_ir.c` - Convert to IR audio nodes
- `ir/ir_component_types.h` - Add IR_COMPONENT_SOUND, IR_COMPONENT_MUSIC
- `components/ir_component_audio.c` - Audio component renderer

**Est. Time:** 3-4 hours

---

## Phase B: Sprite System Completion

### B.1 Region Drawing

**Goal:** Draw subsections of sprite sheets without creating separate sprites.

**Files to Modify:**
- `ir/ir_sprite.h` - Add region API
- `ir/ir_sprite.c` - Implement region drawing

**API Design:**
```c
typedef struct {
    float x;      // Normalized X (0-1)
    float y;      // Normalized Y (0-1)
    float width;  // Normalized width (0-1)
    float height; // Normalized height (0-1)
} IRTextureRegion;

IRSprite* ir_sprite_create_region(IRSprite* sprite, IRTextureRegion* region);
void ir_sprite_draw_region(IRSprite* sprite, IRTextureRegion* region,
                            float x, float y, float width, float height);
```

**Est. Time:** 2-3 hours

---

### B.2 Command Buffer Integration

**Goal:** Push sprite commands to IR command buffer for efficient rendering.

**Files to Modify:**
- `ir/ir_sprite.h` - Command buffer API
- `ir/ir_sprite.c` - Implementation

**API Design:**
```c
typedef enum {
    IR_SPRITE_CMD_DRAW,
    IR_SPRITE_CMD_DRAW_REGION,
    IR_SPRITE_CMD_BATCH
} IRSpriteCommandType;

typedef struct {
    IRSpriteCommandType type;
    IRSprite* sprite;
    float x, y, width, height;
    IRTextureRegion region;
} IRSpriteCommand;

void ir_sprite_push_command(IRSpriteCommand* cmd);
void ir_sprite_execute_commands(void);
```

**Est. Time:** 3-4 hours

---

### B.3 Metadata-Based Sprite Sheet Loading

**Goal:** Load sprite sheets from metadata files (JSON, XML, custom).

**Supported Formats:**
- TexturePacker JSON
- Aseprite JSON
- Custom KRY format

**Files to Create:**
- `ir/ir_sprite_metadata.h`
- `ir/ir_sprite_metadata.c`

**API Design:**
```c
typedef enum {
    IR_SPRITE_JSON,
    IR_SPRITE_ASEPRITE,
    IR_SPRITE_KRY
} IRSpriteMetadataFormat;

IRSpriteSheet* ir_sprite_load_from_metadata(const char* metadata_file,
                                             IRSpriteMetadataFormat format);
IRSprite* ir_sprite_get_named(IRSpriteSheet* sheet, const char* name);
```

**Est. Time:** 4-5 hours

---

## Phase C: Physics System Enhancement

### C.1 Torque Calculation

**Goal:** Implement proper torque for rotational physics.

**Files to Modify:**
- `ir/ir_physics.h`
- `ir/ir_physics.c`

**Implementation:**
```c
typedef struct {
    float moment_of_inertia;
    float angular_velocity;
    float torque;
    float angular_damping;
} IRBodyAngular;

// In IRBody
IRBodyAngular angular;

// API
void ir_body_apply_torque(IRBody* body, float torque);
void ir_body_apply_force_at_offset(IRBody* body, float fx, float fy,
                                     float ox, float oy);
```

**Est. Time:** 2-3 hours

---

### C.2 Per-Body Callbacks

**Goal:** Collision/contact callbacks per body.

**Files to Modify:**
- `ir/ir_physics.h`
- `ir/ir_physics.c`

**API Design:**
```c
typedef void (*IRCollisionCallback)(IRBody* a, IRBody* b, void* userdata);

typedef struct {
    IRCollisionCallback on_collide;
    IRCollisionCallback on_separate;
    void* userdata;
} IRBodyCallbacks;

void ir_body_set_callbacks(IRBody* body, IRBodyCallbacks* callbacks);
```

**Est. Time:** 2-3 hours

---

### C.3 Raycasting

**Goal:** Cast rays for line-of-sight, shooting, etc.

**Files to Create:**
- `ir/ir_physics_ray.h`
- `ir/ir_physics_ray.c`

**API Design:**
```c
typedef struct {
    float origin[3];
    float direction[3];
    float max_distance;
    uint32_t collision_mask;
} IRRay;

typedef struct {
    bool hit;
    float point[3];
    float normal[3];
    IRBody* body;
    float distance;
} IRRaycastResult;

IRRaycastResult ir_physics_raycast(IRRay* ray);
```

**Est. Time:** 4-5 hours

---

## Phase D: Validation System Enhancement

### D.1 Partial Recovery

**Goal:** Recover from validation errors without failing completely.

**Files to Modify:**
- `ir/ir_validation.h`
- `ir/ir_validation.c`

**Implementation:**
```c
typedef enum {
    IR_RECOVERY_NONE,
    IR_RECOVERY_SKIP,
    IR_RECOVERY_DEFAULT,
    IR_RECOVERY_SANITIZE
} IRRecoveryStrategy;

void ir_validation_set_recovery_strategy(IRRecoveryStrategy strategy);
bool ir_validation_recover_field(IRValidationContext* ctx, const char* field_path);
```

**Est. Time:** 2-3 hours

---

### D.2 Error Recovery Strategies

**Goal:** Multiple recovery strategies for different error types.

**Strategies:**
- Type coercion (string to number)
- Range clamping
- Default value substitution
- Property removal

**Est. Time:** 3-4 hours

---

### D.3 Section Skipping

**Goal:** Skip invalid sections while continuing validation.

**Implementation:**
```c
typedef struct {
    const char* section_path;
    bool optional;
    IRRecoveryStrategy recovery;
} IRValidationSection;

void ir_validation_mark_section_optional(IRValidationContext* ctx,
                                          const char* path);
```

**Est. Time:** 2 hours

---

## Phase E: Stylesheet System Enhancement

### E.1 Component ID Support

**Goal:** Add ID field to IRComponent.

**Files to Modify:**
- `ir/ir_core.h` - IRComponent struct
- `ir/ir_builder.c` - ID assignment

**Implementation:**
```c
// In IRComponent
char* id;

// API
const char* ir_component_get_id(IRComponent* comp);
void ir_component_set_id(IRComponent* comp, const char* id);
IRComponent* ir_component_find_by_id(IRComponent* root, const char* id);
```

**Est. Time:** 2-3 hours

---

### E.2 Parent Pointer

**Goal:** Add parent pointer for tree traversal.

**Files to Modify:**
- `ir/ir_core.h` - IRComponent struct
- `ir/ir_builder.c` - Parent tracking

**Implementation:**
```c
// In IRComponent
struct IRComponent* parent;

// API
IRComponent* ir_component_get_parent(IRComponent* comp);
IRComponent* ir_component_get_root(IRComponent* comp);
uint32_t ir_component_get_depth(IRComponent* comp);
```

**Est. Time:** 2 hours

---

### E.3 Sibling Traversal

**Goal:** Navigate between siblings in component tree.

**Files to Create:**
- `ir/ir_component_traversal.h`
- `ir/ir_component_traversal.c`

**API Design:**
```c
IRComponent* ir_component_next_sibling(IRComponent* comp);
IRComponent* ir_component_prev_sibling(IRComponent* comp);
IRComponent* ir_component_first_child(IRComponent* comp);
IRComponent* ir_component_last_child(IRComponent* comp);
```

**Est. Time:** 2-3 hours

---

## Phase F: Hot Reload State Preservation

### F.1 State Serialization

**Goal:** Serialize component state before hot reload.

**Files to Modify:**
- `ir/ir_hot_reload.h`
- `ir/ir_hot_reload.c`

**API Design:**
```c
typedef struct {
    const char* component_id;
    const char* key;
    IRValue value;
} IRComponentState;

IRComponentState* ir_hot_reload_serialize_state(IRComponent* root);
void ir_hot_reload_free_state(IRComponentState* state);
```

**Est. Time:** 4-5 hours

---

### F.2 State Restoration

**Goal:** Restore serialized state after hot reload.

**Implementation:**
```c
bool ir_hot_reload_restore_state(IRComponent* new_root,
                                  IRComponentState* old_state);
```

**Est. Time:** 3-4 hours

---

## Phase G: HTML/KRY Element Implementation

### G.1 Text Formatting Elements

**Priority:** Medium

| Element | Description | Complexity |
|---------|-------------|------------|
| `<sub>` | Subscript | Low |
| `<sup>` | Superscript | Low |
| `<ins>` | Inserted/underlined | Low |
| `<abbr>` | Abbreviation with tooltip | Low |
| `<dfn>` | Definition term | Low |
| `<kbd>` | Keyboard input style | Low |
| `<samp>` | Sample output style | Low |
| `<var>` | Variable style | Low |
| `<time>` | Date/time | Medium |
| `<data>` | Machine-readable data | Low |

**Files to Modify:**
- `parsers/html/html_parser.c` - Add element parsing
- `parsers/html/html_ast.h` - Add node types
- `parsers/html/html_to_ir.c` - Convert to IR
- `components/ir_component_text.c` - Add text formatting support

**Est. Time:** 6-8 hours

---

### G.2 Description Lists

**Elements:** `<dl>`, `<dt>`, `<dd>`

**Example:**
```html
<dl>
  <dt>Coffee</dt>
  <dd>Black hot drink</dd>
  <dt>Milk</dt>
  <dd>White cold drink</dd>
</dl>
```

**Implementation:**
- New component type: `IR_COMPONENT_DESCRIPTION_LIST`
- Style vars: `description_indent`, `term_spacing`, `description_spacing`

**Est. Time:** 3-4 hours

---

### G.3 Table Enhancements

**Elements:** `<caption>`, `<col>`, `<colgroup>`

**Implementation:**
- Extend `IR_COMPONENT_TABLE`
- Add caption support
- Add column styling

**Est. Time:** 4-5 hours

---

### G.4 Advanced Input Types

**Types:** `radio`, `file`, `range`, `date`, `color`

**Implementation:**

| Type | Component | Notes |
|------|-----------|-------|
| `radio` | `IR_COMPONENT_RADIO` | Group selection |
| `file` | `IR_COMPONENT_FILE_UPLOAD` | File picker |
| `range` | `IR_COMPONENT_SLIDER` | Slider widget |
| `date` | `IR_COMPONENT_DATE_PICKER` | Calendar popup |
| `color` | `IR_COMPONENT_COLOR_PICKER` | Color selection |

**Est. Time:** 10-12 hours

---

### G.5 Form Enhancements

**Elements:** `<optgroup>`, `<legend>`, `<datalist>`

**Est. Time:** 5-6 hours

---

## Phase H: Other Enhancements

### H.1 Particle Emitter ID Tracking

**Files to Modify:**
- `ir/ir_particle.h`
- `ir/ir_particle.c`

**Implementation:**
```c
// In IRParticleEmitter
uint32_t emitter_id;

void ir_particle_emitter_set_id(IRParticleEmitter* emitter, uint32_t id);
uint32_t ir_particle_emitter_get_id(IRParticleEmitter* emitter);
IRParticleEmitter* ir_particle_find_emitter(uint32_t id);
```

**Est. Time:** 1-2 hours

---

### H.2 Per-Instance Asset Management

**Files to Modify:**
- `ir/ir_asset.h`
- `ir/ir_asset.c`

**Implementation:**
- Move asset registry to `IRAudioState` pattern
- Create per-instance asset registries
- Asset sharing between instances

**Est. Time:** 4-5 hours

---

### H.3 Instance Ownership Tracking

**Files to Modify:**
- `ir/ir_executor.h`
- `ir/ir_executor.c`

**Implementation:**
```c
typedef struct {
    uint32_t instance_id;
    IRComponent* root_component;
} IRInstanceOwnership;

void ir_executor_track_ownership(uint32_t instance_id, IRComponent* root);
IRComponent* ir_executor_get_owned(uint32_t instance_id);
```

**Est. Time:** 3-4 hours

---

## Implementation Order

### Sprint 1: Audio & Physics (Foundation)
1. Music Streaming (A.1)
2. Torque Calculation (C.1)
3. Raycasting (C.3)
4. Per-Body Callbacks (C.2)

### Sprint 2: Rendering & Visuals
5. Sprite Region Drawing (B.1)
6. Command Buffer Integration (B.2)
7. Metadata Sprite Sheets (B.3)
8. Text Formatting Elements (G.1)

### Sprint 3: System Architecture
9. Component ID Support (E.1)
10. Parent Pointer (E.2)
11. Sibling Traversal (E.3)
12. Instance Ownership (H.3)

### Sprint 4: Data & Validation
13. Partial Recovery (D.1)
14. Error Recovery Strategies (D.2)
15. Section Skipping (D.3)
16. Hot Reload State (F.1, F.2)

### Sprint 5: Form & UI Components
17. Advanced Input Types (G.4)
18. Form Enhancements (G.5)
19. Description Lists (G.2)
20. Table Enhancements (G.3)

### Sprint 6: Advanced Features
21. OpenAL Backend (A.2)
22. KIR Audio Components (A.3)
23. Per-Instance Assets (H.2)
24. Particle Emitter Tracking (H.1)

---

## Summary

| Phase | Tasks | Est. Total Time |
|-------|-------|-----------------|
| A - Audio | 3 | 13-18 hours |
| B - Sprite | 3 | 9-12 hours |
| C - Physics | 3 | 8-11 hours |
| D - Validation | 3 | 7-9 hours |
| E - Stylesheet | 3 | 6-8 hours |
| F - Hot Reload | 2 | 7-9 hours |
| G - HTML Elements | 5 | 28-36 hours |
| H - Other | 3 | 8-11 hours |

**Grand Total: 72-104 hours of implementation work**

---

## Next Steps

1. **Review and prioritize** - Which phase should we start with?
2. **Set up testing** - Create test cases for each feature
3. **Documentation** - Update docs as features are implemented
4. **Code review** - Ensure consistency with existing architecture

**Recommendation:** Start with **Sprint 1 (Audio & Physics)** as these provide core functionality that other features depend on.
