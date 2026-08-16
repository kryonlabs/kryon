/*
 * Animation sampler unit tests. Verifies keyframe interpolation (linear + step),
 * hold-before/hold-after clamping, and KryAnimationApply writing position/rotation
 * values onto a target node.
 */

#include "kryon.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

static int failures;

static void
check_float(const char *name, float got, float want, float epsilon)
{
    if(fabsf(got - want) <= epsilon)
        return;
    fprintf(stderr, "FAIL: %s got %f want %f\n", name, got, want);
    failures++;
}

int
main(void)
{
    Scene scene;
    NodeId target;
    Node *n;
    KryAnimTrack track = {0};
    float v;
    KryAnimation anim = {0};

    SceneRegisterBuiltins();
    SceneInit(&scene);
    target = NodeCreate(&scene, scene.root, NODE_NODE2D, "t");

    /* linear track: 0->100 over t=0..1 */
    track.target = target;
    strcpy(track.property, "position");
    track.component = 0;
    track.interp = KRY_ANIM_INTERP_LINEAR;
    track.keyframe_count = 2;
    track.keyframes[0].time = 0.0f;
    track.keyframes[0].value = 0.0f;
    track.keyframes[1].time = 1.0f;
    track.keyframes[1].value = 100.0f;

    KryAnimTrackSample(&track, -0.5f, &v);
    check_float("hold-before clamps to first", v, 0.0f, 0.001f);
    KryAnimTrackSample(&track, 0.0f, &v);
    check_float("at first keyframe", v, 0.0f, 0.001f);
    KryAnimTrackSample(&track, 0.25f, &v);
    check_float("linear midpoint 0.25", v, 25.0f, 0.001f);
    KryAnimTrackSample(&track, 0.5f, &v);
    check_float("linear midpoint 0.5", v, 50.0f, 0.001f);
    KryAnimTrackSample(&track, 0.75f, &v);
    check_float("linear midpoint 0.75", v, 75.0f, 0.001f);
    KryAnimTrackSample(&track, 1.0f, &v);
    check_float("at last keyframe", v, 100.0f, 0.001f);
    KryAnimTrackSample(&track, 1.5f, &v);
    check_float("hold-after clamps to last", v, 100.0f, 0.001f);

    /* step track snaps to the lower key */
    track.interp = KRY_ANIM_INTERP_STEP;
    KryAnimTrackSample(&track, 0.9f, &v);
    check_float("step snaps to lower key at 0.9", v, 0.0f, 0.001f);

    /* KryAnimationApply writes the sampled value into the target node */
    track.interp = KRY_ANIM_INTERP_LINEAR;
    anim.duration = 1.0f;
    anim.track_count = 1;
    anim.tracks[0] = track;
    KryAnimationApply(&scene, &anim, 0.5f);
    n = NodeGet(&scene, target);
    check_float("apply writes position.x at 0.5", n->local.position.x, 50.0f, 0.001f);

    /* rotation track */
    strcpy(anim.tracks[0].property, "rotation");
    anim.tracks[0].component = -1;
    anim.tracks[0].keyframes[0].value = 0.0f;
    anim.tracks[0].keyframes[1].value = 3.14f;
    KryAnimationApply(&scene, &anim, 0.5f);
    n = NodeGet(&scene, target);
    check_float("apply writes rotation at 0.5", n->local.rotation, 1.57f, 0.01f);

    SceneDestroy(&scene);
    return failures == 0 ? 0 : 1;
}
