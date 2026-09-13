#ifndef NODE2D_PROPS_H
#define NODE2D_PROPS_H

/*
 * Kind-specific props for built-in scene nodes. These are attached to a
 * Node via its `props` pointer and read by the nodes lifecycle hooks.
 * Each concrete kind owns its props struct; the scene tree treats props as
 * opaque (void *) and only the kinds registered ops/destroy touch it.
 */

#include "ui_node2d_props.generated.h"

/*
 * Props allocation helpers for scene builders. Allocate zero-initialized
 * kind-specific props to attach to a node via NodeGet()->props. Returns
 * NULL on allocation failure. The caller transfers the pointer to the node;
 * the kinds destroy hook frees it.
 */
Camera2DProps *Camera2DPropsAlloc(float zoom, int active);
Sprite2DProps *Sprite2DPropsAlloc(const char *asset_path, float w, float h);

Light2DProps *Light2DPropsAlloc(float radius, Color color, float energy);

Body2DProps *Body2DPropsAlloc(Body2DType type);
CollisionShape2DProps *CollisionShape2DPropsAlloc(Shape2DKind kind, float w, float h);
Area2DProps *Area2DPropsAlloc(void);

/* AnimationPlayer: holds up to N animations and the current play state. */
#define ANIMATION_PLAYER_ANIMS_MAX 4

AnimationPlayerProps *AnimationPlayerPropsAlloc(void);

AnimatedSprite2DProps *AnimatedSprite2DPropsAlloc(const char *asset_path,
                                                     int frame_count,
                                                     int frames_per_row,
                                                     int frame_w, int frame_h,
                                                     float fps);

/* TileMap: a grid of tile IDs rendered from a single tileset texture. */
#define TILEMAP_W_MAX 256
#define TILEMAP_H_MAX 256

TileMapProps *TileMapPropsAlloc(const char *asset_path, int tile_w, int tile_h,
                                   int tiles_per_row, int map_w, int map_h);

AudioSourceProps *AudioSourcePropsAlloc(const char *asset_path, AudioKind kind);
void AudioSourcePlay(Scene *scene, NodeId node);
void AudioSourceStop(Scene *scene, NodeId node);

#endif
