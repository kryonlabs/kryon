#ifndef KRY_MATH_H
#define KRY_MATH_H

/*
 * Tiny Kryon-owned 2D math helpers for the retained scene tree. Uses raylib's
 * Vector2 and Camera2D types directly so there is no conversion at the draw
 * boundary. Kept self-contained: no project outside this header is needed.
 */

#include "kryon_compat.generated.h"
#include <math.h>

typedef struct Transform2D {
    Vector2 position;
    float rotation; /* radians */
    Vector2 scale;
} Transform2D;

static inline Vector2
KryVector2Add(Vector2 a, Vector2 b)
{
    return (Vector2){a.x + b.x, a.y + b.y};
}

static inline Vector2
KryVector2Sub(Vector2 a, Vector2 b)
{
    return (Vector2){a.x - b.x, a.y - b.y};
}

static inline Vector2
KryVector2Scale(Vector2 v, float s)
{
    return (Vector2){v.x * s, v.y * s};
}

static inline float
KryVector2Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

static inline float
KryVector2Length(Vector2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

static inline Vector2
KryVector2Normalize(Vector2 v)
{
    float len = KryVector2Length(v);
    if(len == 0.0f)
        return (Vector2){0, 0};
    return (Vector2){v.x / len, v.y / len};
}

static inline Vector2
KryVector2Lerp(Vector2 a, Vector2 b, float t)
{
    return (Vector2){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

static inline Vector2
KryVector2Rotate(Vector2 v, float radians)
{
    float c = cosf(radians);
    float s = sinf(radians);
    return (Vector2){v.x * c - v.y * s, v.x * s + v.y * c};
}

static inline float
KryLerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline float
KryClamp(float v, float lo, float hi)
{
    if(v < lo)
        return lo;
    if(v > hi)
        return hi;
    return v;
}

/* The identity transform: no offset, no rotation, unit scale. */
static inline Transform2D
Transform2DIdentity(void)
{
    Transform2D t;
    t.position = (Vector2){0, 0};
    t.rotation = 0.0f;
    t.scale = (Vector2){1, 1};
    return t;
}

/*
 * Compose a child local transform with a parent world transform. The child is
 * first scaled and rotated about its own origin, then translated by the
 * parent's position. This matches Godot 2D and raylib Camera2D conventions:
 * rotation is clockwise in screen space because +Y points down.
 */
static inline Transform2D
Transform2DCompose(Transform2D parent, Transform2D child)
{
    Transform2D out;
    Vector2 scaled;
    scaled.x = child.position.x * parent.scale.x;
    scaled.y = child.position.y * parent.scale.y;
    out.position = KryVector2Add(parent.position, KryVector2Rotate(scaled, parent.rotation));
    out.rotation = parent.rotation + child.rotation;
    out.scale = (Vector2){parent.scale.x * child.scale.x,
                          parent.scale.y * child.scale.y};
    return out;
}

/* Transform a point from the local space described by `t` into world space. */
static inline Vector2
Transform2DPoint(Transform2D t, Vector2 p)
{
    Vector2 scaled = (Vector2){p.x * t.scale.x, p.y * t.scale.y};
    return KryVector2Add(t.position, KryVector2Rotate(scaled, t.rotation));
}

/*
 * Build a raylib Camera2D that views the world through `view` (the camera
 * node's world transform). The camera's target is its world position and its
 * offset is the screen center; zoom is the camera's uniform scale.
 */
static inline Camera2D
Camera2DFromTransform(Transform2D view, Vector2 screen_size, float zoom)
{
    Camera2D cam;
    cam.target = view.position;
    cam.offset = (Vector2){screen_size.x * 0.5f, screen_size.y * 0.5f};
    cam.rotation = view.rotation * 57.2957795f; /* radians to degrees */
    cam.zoom = zoom > 0.0f ? zoom : 1.0f;
    return cam;
}

#endif
