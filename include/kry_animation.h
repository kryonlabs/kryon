#ifndef KRY_ANIMATION_H
#define KRY_ANIMATION_H

/*
 * Animation system for the retained scene tree. An animation is a collection
 * of tracks; each track targets a named property on a target node and holds
 * ordered keyframes (time + float value). The AnimationPlayer node advances
 * its current animation each process tick and applies interpolated values to
 * the target nodes via the Phase 2 property model.
 *
 * Compile-time authored in .kry (k2c lowers animation tables to KryAnimation
 * structs) and at runtime by app code. No runtime scripting VM.
 */

#include "scene_tree.h"

#define KRY_ANIM_TRACKS_MAX 8
#define KRY_ANIM_KEYS_MAX 64
#define KRY_ANIM_NAME_MAX 48

typedef enum KryAnimInterp {
    KRY_ANIM_INTERP_LINEAR,
    KRY_ANIM_INTERP_STEP
} KryAnimInterp;

typedef struct KryKeyframe {
    float time;   /* seconds from animation start */
    float value;
} KryKeyframe;

typedef struct KryAnimTrack {
    NodeId target;                 /* node whose property is animated */
    char property[32];                /* property id (position/rotation/scale) */
    int component;                    /* 0=x, 1=y for vector2; -1 for scalar */
    KryAnimInterp interp;
    int keyframe_count;
    KryKeyframe keyframes[KRY_ANIM_KEYS_MAX];
} KryAnimTrack;

typedef struct KryAnimation {
    char name[KRY_ANIM_NAME_MAX];
    float duration;                   /* seconds; last keyframe time across tracks */
    int loop;                         /* nonzero wraps around */
    int track_count;
    KryAnimTrack tracks[KRY_ANIM_TRACKS_MAX];
} KryAnimation;

/* Sample a single track at time t into *out_value. Returns 1 if the track has
 * a keyframe at-or-before t, 0 if the track is empty or t is before its first
 * key (hold-before-first semantics returns the first value). */
int KryAnimTrackSample(const KryAnimTrack *track, float t, float *out_value);

/* Apply one animation's sampled values at time t to its target nodes via the
 * scene property model. Walks each track, samples it, and writes the component
 * into the target node's property. */
void KryAnimationApply(Scene *scene, const KryAnimation *anim, float t);

#endif
