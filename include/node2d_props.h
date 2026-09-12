#ifndef NODE2D_PROPS_H
#define NODE2D_PROPS_H

/*
 * Kind-specific props for built-in scene nodes. These are attached to a
 * Node via its `props` pointer and read by the nodes lifecycle hooks.
 * Each concrete kind owns its props struct; the scene tree treats props as
 * opaque (void *) and only the kinds registered ops/destroy touch it.
 */

#include "kryon_compat.generated.h"
#include "ui_image.h" /* ImageProps / ImageFit, shared with the UI widget */
#include "kry_animation.h" /* Animation, held by AnimationPlayerProps */

typedef struct Camera2DProps {
    float zoom;        /* 1.0 = default; >1 zooms in */
    float rotation;    /* extra camera rotation in radians, added to the node transform */
    int active;        /* nonzero = this camera drives BeginMode2D during SceneDraw */
} Camera2DProps;

typedef struct Sprite2DProps {
    const char *asset_path; /* texture path or embedded asset name */
    Rectangle source;       /* sub-rectangle of the texture; {0,0,0,0} = full texture */
    Vector2 size;           /* world-space size of the sprite (width/height) */
    Color tint;             /* .a == 0 -> WHITE */
    ImageFit fit;         /* STRETCH/CONTAIN/COVER within `size` */
} Sprite2DProps;

/*
 * Props allocation helpers for scene builders. Allocate zero-initialized
 * kind-specific props to attach to a node via NodeGet()->props. Returns
 * NULL on allocation failure. The caller transfers the pointer to the node;
 * the kinds destroy hook frees it.
 */
Camera2DProps *Camera2DPropsAlloc(float zoom, int active);
Sprite2DProps *Sprite2DPropsAlloc(const char *asset_path, float w, float h);

/* Light2D: a soft, additive point light rendered in world space. */
typedef struct Light2DProps {
    float radius;       /* world-space falloff radius */
    Color color;        /* emitted color; alpha participates in energy */
    float energy;       /* 0 disables visible output; 1 is full strength */
    int enabled;
} Light2DProps;

Light2DProps *Light2DPropsAlloc(float radius, Color color, float energy);

typedef enum Body2DType {
    Body2DStatic,
    Body2DKinematic,
    Body2DDynamic
} Body2DType;

typedef struct Body2DProps {
    Body2DType body_type;
    int fixed_rotation; /* nonzero prevents the body from rotating */
    float gravity_scale; /* 1.0 = normal; 0.0 = weightless */
    /* b2BodyId is {int32 index1, int16 world0, int16 generation}. Stored as
     * raw ints so this header does not need box2d.h. 0 = uncreated body. */
    int body_id_index;
    short body_id_world;
    short body_id_gen;
} Body2DProps;

typedef enum Shape2DKind {
    Shape2DBox,
    Shape2DCircle
} Shape2DKind;

typedef struct CollisionShape2DProps {
    Shape2DKind shape_kind;
    Vector2 size;    /* full width/height (box) or diameter (circle) */
    int is_sensor;   /* nonzero = trigger volume (Area2D), no solid collision */
} CollisionShape2DProps;

typedef struct Area2DProps {
    int monitoring; /* nonzero collects body_enter/body_exit signals */
    /* last body that entered/left, for signal dispatch */
    int last_enter_body;
    int last_exit_body;
} Area2DProps;

Body2DProps *Body2DPropsAlloc(Body2DType type);
CollisionShape2DProps *CollisionShape2DPropsAlloc(Shape2DKind kind, float w, float h);
Area2DProps *Area2DPropsAlloc(void);

/* AnimationPlayer: holds up to N animations and the current play state. */
#define ANIMATION_PLAYER_ANIMS_MAX 4

typedef struct AnimationPlayerProps {
    Animation anims[ANIMATION_PLAYER_ANIMS_MAX];
    int anim_count;
    int current;       /* index of the playing animation, -1 if stopped */
    float time;        /* seconds into the current animation */
    int playing;       /* nonzero = advancing each tick */
} AnimationPlayerProps;

AnimationPlayerProps *AnimationPlayerPropsAlloc(void);

/* AnimatedSprite2D: cycles through frames of a grid sprite sheet. */
typedef struct AnimatedSprite2DProps {
    const char *asset_path;  /* sprite sheet texture */
    int frame_count;         /* total frames in the sheet */
    int frames_per_row;      /* frames horizontally; rows derived from frame_count */
    int frame_w;             /* source-frame pixel size */
    int frame_h;
    float fps;               /* frames per second */
    Vector2 size;            /* world-space draw size */
    Color tint;              /* .a == 0 -> WHITE */
    float time;              /* accumulated playhead, in seconds */
} AnimatedSprite2DProps;

AnimatedSprite2DProps *AnimatedSprite2DPropsAlloc(const char *asset_path,
                                                     int frame_count,
                                                     int frames_per_row,
                                                     int frame_w, int frame_h,
                                                     float fps);

/* TileMap: a grid of tile IDs rendered from a single tileset texture. */
#define TILEMAP_W_MAX 256
#define TILEMAP_H_MAX 256

typedef struct TileMapProps {
    const char *asset_path;    /* tileset texture */
    int tile_w;                /* source tile pixel size */
    int tile_h;
    int tiles_per_row;         /* tileset layout */
    int map_w;                 /* grid dimensions in tiles */
    int map_h;
    int tile_px_w;             /* world-space tile draw size */
    int tile_px_h;
    const int *tiles;          /* map_w*map_h tile IDs; 0 = empty; owned by caller */
    Color tint;                /* .a == 0 -> WHITE */
} TileMapProps;

TileMapProps *TileMapPropsAlloc(const char *asset_path, int tile_w, int tile_h,
                                   int tiles_per_row, int map_w, int map_h);

/* AudioSource: plays a sound or music stream. */
typedef enum AudioKind {
    AudioSound,  /* one-shot SFX via LoadSound/PlaySound */
    AudioMusic   /* streaming via LoadMusicStream/PlayMusicStream */
} AudioKind;

typedef struct AudioSourceProps {
    const char *asset_path;
    AudioKind kind;
    float volume;       /* 0..1 */
    float pitch;        /* 1.0 = normal */
    int loop;           /* music only */
    int playing;
    /* raylib Sound/Music loaded lazily on first play. Stored as a void* alloc
     * sized by the node impl (which knows the real raylib types); this header
     * stays free of raylib audio struct dependencies. */
    int loaded;
    void *handle;       /* points to a Sound or Music depending on `kind` */
} AudioSourceProps;

AudioSourceProps *AudioSourcePropsAlloc(const char *asset_path, AudioKind kind);
void AudioSourcePlay(Scene *scene, NodeId node);
void AudioSourceStop(Scene *scene, NodeId node);

#endif
