/*
 * Animation sampling and application. Linear interpolation between keyframes;
 * STEP interpolation snaps to the most recent key. Tracks hold out of the box
 * below the first keyframe and hold the last value past the final keyframe.
 */

#include "kry_animation.h"
#include <string.h>

int
KryAnimTrackSample(const KryAnimTrack *track, float t, float *out_value)
{
    int i;
    if(track == NULL || out_value == NULL || track->keyframe_count <= 0)
        return 0;

    /* hold-before: before the first keyframe, clamp to it */
    if(t <= track->keyframes[0].time) {
        *out_value = track->keyframes[0].value;
        return 1;
    }
    /* hold-after: at or past the last keyframe, clamp to it */
    if(t >= track->keyframes[track->keyframe_count - 1].time) {
        *out_value = track->keyframes[track->keyframe_count - 1].value;
        return 1;
    }
    /* find the bracketing pair */
    for(i = 0; i < track->keyframe_count - 1; i++) {
        const KryKeyframe *a = &track->keyframes[i];
        const KryKeyframe *b = &track->keyframes[i + 1];
        if(t >= a->time && t <= b->time) {
            if(track->interp == KRY_ANIM_INTERP_STEP || b->time == a->time) {
                *out_value = a->value;
            } else {
                float alpha = (t - a->time) / (b->time - a->time);
                *out_value = a->value + (b->value - a->value) * alpha;
            }
            return 1;
        }
    }
    *out_value = track->keyframes[track->keyframe_count - 1].value;
    return 1;
}

void
KryAnimationApply(Scene *scene, const KryAnimation *anim, float t)
{
    int i;
    if(scene == NULL || anim == NULL)
        return;
    for(i = 0; i < anim->track_count; i++) {
        const KryAnimTrack *track = &anim->tracks[i];
        Node *n;
        float v;
        if(track->keyframe_count <= 0)
            continue;
        if(!KryAnimTrackSample(track, t, &v))
            continue;
        n = NodeGet(scene, track->target);
        if(n == NULL)
            continue;
        if(strcmp(track->property, "position") == 0) {
            if(track->component == 0)
                n->local.position.x = v;
            else if(track->component == 1)
                n->local.position.y = v;
            n->flags |= NODE_FLAG_DIRTY;
        } else if(strcmp(track->property, "rotation") == 0) {
            n->local.rotation = v;
            n->flags |= NODE_FLAG_DIRTY;
        } else if(strcmp(track->property, "scale") == 0) {
            if(track->component == 0)
                n->local.scale.x = v;
            else if(track->component == 1)
                n->local.scale.y = v;
            n->flags |= NODE_FLAG_DIRTY;
        }
    }
}
