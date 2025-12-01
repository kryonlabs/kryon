# Kryon Sprite System - Getting Started

## Overview

The Kryon sprite system provides high-performance 2D sprite rendering with GPU batching for game development. It features:

- **Texture Atlas Management**: Automatic packing with shelf bin packing algorithm
- **GPU Sprite Batching**: Render 1000+ sprites in 1-2 draw calls using SDL_RenderGeometry
- **Flexible Rendering**: Rotation, scaling, flipping, color tinting
- **Multiple Backends**: SDL3 (full support), Terminal (2D), Web/WASM (planned)

## Performance

The test demonstrates excellent performance:
- **100 sprites** @ **~61 FPS**
- **1 draw call** (single batch)
- Target: 1000+ sprites at 60 FPS

## Building the Test

```bash
cd examples/c
make sprite_test
```

## Running the Test

```bash
cd examples/c
make run
# Or directly:
../../build/sprite_test
```

## How It Works

### 1. Initialize Sprite Backend

```c
#include "../../ir/ir_sprite.h"
#include "../../renderers/sdl3/sdl3_sprite_backend.h"

// After creating SDL renderer
sdl3_sprite_backend_init(renderer);
```

### 2. Create Sprite Atlas

```c
// Create 1024x1024 atlas
IRSpriteAtlasID atlas = ir_sprite_atlas_create(1024, 1024);
```

### 3. Add Sprites

From memory (RGBA format):
```c
uint8_t* pixels = create_sprite_pixels(64, 64);
IRSpriteID sprite = ir_sprite_atlas_add_image_from_memory(
    atlas, "my_sprite", pixels, 64, 64
);
free(pixels);
```

From file:
```c
IRSpriteID sprite = ir_sprite_atlas_add_image(atlas, "sprite.png");
```

### 4. Pack Atlas

```c
// Packs sprites and uploads texture to GPU
if (!ir_sprite_atlas_pack(atlas)) {
    fprintf(stderr, "Failed to pack atlas\n");
    return 1;
}
```

### 5. Render Sprites

```c
// Begin frame
sdl3_sprite_backend_begin_frame();

// Draw sprites
for (int i = 0; i < sprite_count; i++) {
    ir_sprite_draw_ex(
        sprite_id,
        x, y,              // Position
        rotation,          // Degrees
        scale_x, scale_y,  // Scale factors
        0xFFFFFFFF         // RGBA tint (white = no tint)
    );
}

// End frame (flushes batches)
sdl3_sprite_backend_end_frame();
```

## API Reference

### Atlas Management

```c
// Create atlas
IRSpriteAtlasID ir_sprite_atlas_create(uint16_t width, uint16_t height);

// Add sprite from file
IRSpriteID ir_sprite_atlas_add_image(IRSpriteAtlasID atlas, const char* path);

// Add sprite from memory (RGBA format)
IRSpriteID ir_sprite_atlas_add_image_from_memory(
    IRSpriteAtlasID atlas,
    const char* name,
    uint8_t* pixels,
    uint16_t width, uint16_t height
);

// Pack and upload to GPU
bool ir_sprite_atlas_pack(IRSpriteAtlasID atlas);

// Destroy atlas
void ir_sprite_atlas_destroy(IRSpriteAtlasID atlas);
```

### Sprite Drawing

```c
// Simple draw (no transformation)
void ir_sprite_draw(IRSpriteID sprite, float x, float y);

// Draw with transformation
void ir_sprite_draw_ex(
    IRSpriteID sprite,
    float x, float y,
    float rotation,        // Degrees
    float scale_x,         // 1.0 = normal
    float scale_y,
    uint32_t tint          // RGBA (0xRRGGBBAA)
);

// Advanced draw with flip and pivot
void ir_sprite_draw_advanced(
    IRSpriteID sprite,
    float x, float y,
    float rotation,
    float scale_x, float scale_y,
    float pivot_x,         // 0.0-1.0 (center = 0.5)
    float pivot_y,
    bool flip_x,
    bool flip_y,
    uint32_t tint
);
```

### Backend Control

```c
// Initialize (call after creating SDL renderer)
void sdl3_sprite_backend_init(SDL_Renderer* renderer);

// Begin frame
void sdl3_sprite_backend_begin_frame(void);

// End frame (flushes remaining sprites)
void sdl3_sprite_backend_end_frame(void);

// Get statistics
void sdl3_sprite_backend_get_stats(uint32_t* sprites, uint32_t* batches);

// Shutdown
void sdl3_sprite_backend_shutdown(void);
```

## Image Formats Supported

Via STB Image:
- PNG (recommended for transparency)
- JPEG
- BMP

## Best Practices

1. **Group sprites by texture**: Sprites from the same atlas batch automatically
2. **Pack early**: Call `ir_sprite_atlas_pack()` before rendering
3. **Minimize atlas switches**: Use fewer large atlases instead of many small ones
4. **Monitor batches**: Use `sdl3_sprite_backend_get_stats()` to check batch count
5. **Enable debug**: Set `KRYON_SPRITE_BATCH_DEBUG=1` to see frame statistics

## Example: Bouncing Sprites

See `sprite_test.c` for a complete example that demonstrates:
- Creating sprites from memory
- Rendering many sprites efficiently
- Animated rotation
- Bouncing physics
- FPS and batch statistics

## Performance Tips

- **Target**: < 5 batches per frame for 1000 sprites
- **Atlas size**: 1024x1024 or 2048x2048 recommended
- **Sprite size**: 16x16 to 256x256 typical
- **Batch size**: 1024 sprites max per batch (configurable)

## Debugging

Enable sprite batch debugging:
```bash
KRYON_SPRITE_BATCH_DEBUG=1 ./sprite_test
```

Output:
```
[Sprite Batch] Frame: 100 sprites, 1 batches (avg 100.0 sprites/batch)
```

## Next Steps

- Try loading real PNG sprites instead of procedural ones
- Experiment with sprite animations
- Test with 1000+ sprites
- Integrate with the game loop system (Phase 1)
