/*
 * AnimatedSprite2D: cycles through frames of a grid sprite sheet each process
 * tick, drawing the current frame at the node's world transform. The sheet is
 * laid out as frames_per_row frames horizontally (rows derived from
 * frame_count). Shares the picture texture cache with Sprite2D / Picture.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "ui_picture.h"
#include <stdlib.h>

static void
kry_animated_sprite2d_process(KryScene *scene, KryNodeId node, float dt)
{
    KryNode *n = KryNodeGet(scene, node);
    AnimatedSprite2DProps *props;
    if(n == NULL)
        return;
    props = (AnimatedSprite2DProps *)n->props;
    if(props == NULL || props->fps <= 0.0f)
        return;
    props->time += dt;
}

static void
kry_animated_sprite2d_draw(KryScene *scene, KryNodeId node)
{
    KryNode *n = KryNodeGet(scene, node);
    AnimatedSprite2DProps *props;
    Texture2D texture;
    int frame;
    int col, row;
    Rectangle source;
    Rectangle dst;
    Color tint;

    if(n == NULL)
        return;
    props = (AnimatedSprite2DProps *)n->props;
    if(props == NULL || props->asset_path == NULL ||
       props->asset_path[0] == '\0' || props->frame_count <= 0 ||
       props->frames_per_row <= 0)
        return;
    texture = KryLoadPictureTexture(props->asset_path);
    if(texture.id == 0)
        return;
    frame = (int)(props->time * props->fps) % props->frame_count;
    if(frame < 0)
        frame += props->frame_count;
    col = frame % props->frames_per_row;
    row = frame / props->frames_per_row;
    source.x = (float)(col * props->frame_w);
    source.y = (float)(row * props->frame_h);
    source.width = (float)props->frame_w;
    source.height = (float)props->frame_h;
    dst.x = n->world.position.x - props->size.x * 0.5f;
    dst.y = n->world.position.y - props->size.y * 0.5f;
    dst.width = props->size.x;
    dst.height = props->size.y;
    tint = props->tint.a == 0 ? WHITE : props->tint;
    DrawTexturePro(texture, source, dst, (Vector2){0, 0},
                   n->world.rotation * 57.2957795f, tint);
}

static void
kry_animated_sprite2d_destroy(KryScene *scene, KryNode *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const KryNodeOps kry_animated_sprite2d_ops = {
    NULL,
    kry_animated_sprite2d_process,
    NULL,
    kry_animated_sprite2d_draw
};

void
kry_register_animated_sprite2d(void)
{
    KryNodeRegisterOps(KRY_NODE_ANIMATED_SPRITE2D, &kry_animated_sprite2d_ops);
    KryNodeRegisterDestroy(KRY_NODE_ANIMATED_SPRITE2D,
                           kry_animated_sprite2d_destroy);
}

AnimatedSprite2DProps *
KryAnimatedSprite2DPropsAlloc(const char *asset_path, int frame_count,
                              int frames_per_row, int frame_w, int frame_h,
                              float fps)
{
    AnimatedSprite2DProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->asset_path = asset_path;
        p->frame_count = frame_count;
        p->frames_per_row = frames_per_row;
        p->frame_w = frame_w;
        p->frame_h = frame_h;
        p->fps = fps;
        p->size = (Vector2){(float)frame_w, (float)frame_h};
    }
    return p;
}
