/*
 * AnimationPlayer: drives keyframe animations on the scene tree. Each process
 * tick advances the current animation's time and applies sampled values to the
 * target nodes via KryAnimationApply. On finish (non-loop animation reaching
 * duration) it emits an "animation_finished" signal on itself.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "kry_animation.h"
#include "kry_signal.h"
#include "kryon_property.h"
#include <stdlib.h>
#include <string.h>

static void
kry_animation_player_process(KryScene *scene, KryNodeId node, float dt)
{
    KryNode *n = KryNodeGet(scene, node);
    AnimationPlayerProps *props;
    KryAnimation *anim;

    if(n == NULL)
        return;
    props = (AnimationPlayerProps *)n->props;
    if(props == NULL || !props->playing || props->current < 0 ||
       props->current >= props->anim_count)
        return;
    anim = &props->anims[props->current];
    props->time += dt;
    if(props->time >= anim->duration) {
        if(anim->loop) {
            /* wrap, keeping the remainder */
            props->time = anim->duration > 0.0f
                              ? fmodf(props->time, anim->duration)
                              : 0.0f;
        } else {
            /* clamp to the end and emit finished */
            props->time = anim->duration;
            props->playing = 0;
            KryAnimationApply(scene, anim, props->time);
            KrySignalEmit(scene, node, "animation_finished",
                          KryonPropertyInt(props->current));
            return;
        }
    }
    KryAnimationApply(scene, anim, props->time);
}

static void
kry_animation_player_destroy(KryScene *scene, KryNode *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const KryNodeOps kry_animation_player_ops = {
    NULL, /* ready */
    kry_animation_player_process,
    NULL, /* physics_process */
    NULL  /* draw: an AnimationPlayer is invisible */
};

void
kry_register_animation_player(void)
{
    KryNodeRegisterOps(KRY_NODE_TIMER, &kry_animation_player_ops);
    KryNodeRegisterDestroy(KRY_NODE_TIMER, kry_animation_player_destroy);
}

AnimationPlayerProps *
KryAnimationPlayerPropsAlloc(void)
{
    AnimationPlayerProps *p = calloc(1, sizeof(*p));
    if(p != NULL)
        p->current = -1;
    return p;
}
