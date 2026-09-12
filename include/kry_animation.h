#ifndef KRY_ANIMATION_H
#define KRY_ANIMATION_H

/*
 * Animation system for the retained scene tree. An animation is a collection
 * of tracks; each track targets a named property on a target node and holds
 * ordered keyframes (time + float value). The AnimationPlayer node advances
 * its current animation each process tick and applies interpolated values to
 * the target nodes via the Phase 2 property model.
 *
 * Compile-time authored in .kry (k2c lowers animation tables to Animation
 * structs) and at runtime by app code. No runtime scripting VM.
 */

#include "scene_tree.h"

#define ANIMATION_TRACKS_MAX 8
#define ANIMATION_KEYS_MAX 64
#define ANIMATION_NAME_MAX 48

typedef enum AnimInterp {
    AnimInterpLinear,
    AnimInterpStep
} AnimInterp;

typedef struct Keyframe {
    float time;   /* seconds from animation start */
    float value;
} Keyframe;

typedef struct AnimTrack {
    NodeId target;                 /* node whose property is animated */
    char property[32];                /* property id (position/rotation/scale) */
    int component;                    /* 0=x, 1=y for vector2; -1 for scalar */
    AnimInterp interp;
    int keyframe_count;
    Keyframe keyframes[ANIMATION_KEYS_MAX];
} AnimTrack;

typedef struct Animation {
    char name[ANIMATION_NAME_MAX];
    float duration;                   /* seconds; last keyframe time across tracks */
    int loop;                         /* nonzero wraps around */
    int track_count;
    AnimTrack tracks[ANIMATION_TRACKS_MAX];
} Animation;

/* Sample a single track at time t into *out_value. Returns 1 if the track has
 * a keyframe at-or-before t, 0 if the track is empty or t is before its first
 * key (hold-before-first semantics returns the first value). */
int AnimTrackSample(const AnimTrack *track, float t, float *out_value);

/* Apply one animations sampled values at time t to its target nodes via the
 * scene property model. Walks each track, samples it, and writes the component
 * into the target nodes property. */
void AnimationApply(Scene *scene, const Animation *anim, float t);

#endif
