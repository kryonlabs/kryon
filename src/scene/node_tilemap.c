/*
 * TileMap: renders a grid of tile IDs from a single tileset texture. On draw
 * it iterates the tile grid and blits each non-empty tile via DrawTexturePro
 * (raylib batches these internally by texture). The tileset is shared through
 * the picture texture cache. TileLayer is a thin alias (same kind, used for
 * layered rendering under different Camera2D depths).
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "ui_picture.h"
#include "../ui/ui_picture_internal.h"
#include <stdlib.h>

static void
kry_tilemap_draw(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    TileMapProps *props;
    Texture2D texture;
    int x, y;
    Color tint;

    if(n == NULL)
        return;
    props = (TileMapProps *)n->props;
    if(props == NULL || props->asset_path == NULL ||
       props->asset_path[0] == '\0' || props->tiles == NULL ||
       props->map_w <= 0 || props->map_h <= 0 || props->tiles_per_row <= 0)
        return;
    texture = LoadPictureTexture(props->asset_path);
    if(texture.id == 0)
        return;
    tint = props->tint.a == 0 ? WHITE : props->tint;
    for(y = 0; y < props->map_h; y++) {
        for(x = 0; x < props->map_w; x++) {
            int id = props->tiles[y * props->map_w + x];
            Rectangle source;
            Rectangle dst;
            int col, row;
            if(id <= 0)
                continue;
            /* tile IDs are 1-based; 0 = empty */
            col = (id - 1) % props->tiles_per_row;
            row = (id - 1) / props->tiles_per_row;
            source.x = (float)(col * props->tile_w);
            source.y = (float)(row * props->tile_h);
            source.width = (float)props->tile_w;
            source.height = (float)props->tile_h;
            dst.x = n->world.position.x + (float)(x * props->tile_px_w);
            dst.y = n->world.position.y + (float)(y * props->tile_px_h);
            dst.width = (float)props->tile_px_w;
            dst.height = (float)props->tile_px_h;
            DrawTexturePro(texture, source, dst, (Vector2){0, 0}, 0.0f, tint);
        }
    }
}

static void
kry_tilemap_destroy(Scene *scene, Node *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const NodeOps kry_tilemap_ops = {
    NULL, NULL, NULL, kry_tilemap_draw
};

void
kry_register_tilemap(void)
{
    NodeRegisterOps(NODE_TILEMAP, &kry_tilemap_ops);
    NodeRegisterDestroy(NODE_TILEMAP, kry_tilemap_destroy);
    /* TileLayer is a usage convention over TileMap (layered rendering at
     * different z-depths), not a separate runtime kind. Callers create
     * NODE_TILEMAP nodes for both. */
}

TileMapProps *
KryTileMapPropsAlloc(const char *asset_path, int tile_w, int tile_h,
                     int tiles_per_row, int map_w, int map_h)
{
    TileMapProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->asset_path = asset_path;
        p->tile_w = tile_w;
        p->tile_h = tile_h;
        p->tiles_per_row = tiles_per_row;
        p->map_w = map_w;
        p->map_h = map_h;
        p->tile_px_w = tile_w;
        p->tile_px_h = tile_h;
    }
    return p;
}
