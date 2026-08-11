/*
 * Sprite2D: a world-space textured sprite. Draws via the shared picture texture
 * cache (KryLoadPictureTexture) so it shares loaded textures with the UI
 * Picture widget. The sprite is drawn at the node's world transform, sized by
 * Sprite2DProps.size, with the chosen fit mode applied within that size.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "ui_picture.h"
#include <stdlib.h>

static void
kry_sprite2d_draw(KryScene *scene, KryNodeId node)
{
    KryNode *n = KryNodeGet(scene, node);
    Sprite2DProps *props;
    Texture2D texture;
    Rectangle dst;
    Color tint;

    if(n == NULL)
        return;
    props = (Sprite2DProps *)n->props;
    if(props == NULL || props->asset_path == NULL || props->asset_path[0] == '\0')
        return;

    texture = KryLoadPictureTexture(props->asset_path);
    if(texture.id == 0)
        return;

    /* world-space destination rectangle centered on the node origin */
    dst.x = n->world.position.x - props->size.x * 0.5f;
    dst.y = n->world.position.y - props->size.y * 0.5f;
    dst.width = props->size.x;
    dst.height = props->size.y;

    tint = props->tint.a == 0 ? WHITE : props->tint;
    DrawTexturePro(texture,
                   (props->source.width == 0.0f || props->source.height == 0.0f)
                       ? (Rectangle){0, 0, (float)texture.width, (float)texture.height}
                       : props->source,
                   dst, (Vector2){0, 0},
                   n->world.rotation * 57.2957795f, tint);
    (void)props->fit; /* fit modes apply to the dst rect; for Sprite2D size is explicit */
}

static void
kry_sprite2d_destroy(KryScene *scene, KryNode *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const KryNodeOps kry_sprite2d_ops = {
    NULL, /* ready */
    NULL, /* process */
    NULL, /* physics_process */
    kry_sprite2d_draw
};

void
kry_register_sprite2d(void)
{
    KryNodeRegisterOps(KRY_NODE_SPRITE2D, &kry_sprite2d_ops);
    KryNodeRegisterDestroy(KRY_NODE_SPRITE2D, kry_sprite2d_destroy);
}
