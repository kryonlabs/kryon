#include "spritesheet.h"
#include "ui_picture_internal.h"
#include <stddef.h>

SpriteSheet
SpriteSheetGrid(const char *asset_path, int frame_width, int frame_height,
                int columns, int rows)
{
    SpriteSheet sheet;

    sheet.asset_path = asset_path;
    sheet.frame_width = frame_width;
    sheet.frame_height = frame_height;
    sheet.columns = columns;
    sheet.rows = rows;
    sheet.frame_count = columns > 0 && rows > 0 ? columns * rows : 0;
    return sheet;
}

int
SpriteSheetFrameCount(SpriteSheet sheet)
{
    return sheet.frame_count;
}

int
SpriteSheetNormalizeFrame(SpriteSheet sheet, int frame)
{
    int count = SpriteSheetFrameCount(sheet);

    if(count <= 0)
        return 0;
    frame %= count;
    if(frame < 0)
        frame += count;
    return frame;
}

Rectangle
SpriteSheetFrameSource(SpriteSheet sheet, int frame)
{
    int normalized;
    int col;
    int row;

    if(sheet.frame_width <= 0 || sheet.frame_height <= 0 ||
       sheet.columns <= 0 || sheet.rows <= 0 || sheet.frame_count <= 0)
        return (Rectangle){0, 0, 0, 0};
    normalized = SpriteSheetNormalizeFrame(sheet, frame);
    col = normalized % sheet.columns;
    row = normalized / sheet.columns;
    return (Rectangle){(float)(col * sheet.frame_width),
                       (float)(row * sheet.frame_height),
                       (float)sheet.frame_width,
                       (float)sheet.frame_height};
}

void
DrawSpriteSheetFrameEx(Texture2D texture, SpriteSheet sheet, int frame,
                       Rectangle bounds, Vector2 origin, float rotation,
                       Color tint, int flip_x, int flip_y)
{
    Rectangle source = SpriteSheetFrameSource(sheet, frame);

    if(texture.id == 0 || source.width == 0.0f || source.height == 0.0f)
        return;
    if(flip_x)
        source.width = -source.width;
    if(flip_y)
        source.height = -source.height;
    DrawTexturePro(texture, source, bounds, origin, rotation, tint);
}

void
DrawSpriteSheetFrame(Texture2D texture, SpriteSheet sheet, int frame,
                     Rectangle bounds, Vector2 origin, float rotation,
                     Color tint)
{
    DrawSpriteSheetFrameEx(texture, sheet, frame, bounds, origin, rotation,
                           tint, 0, 0);
}

void
DrawSpriteSheetFrameFromPathEx(SpriteSheet sheet, int frame, Rectangle bounds,
                               Vector2 origin, float rotation, Color tint,
                               int flip_x, int flip_y)
{
    Texture2D texture;

    if(sheet.asset_path == NULL || sheet.asset_path[0] == '\0')
        return;
    texture = LoadPictureTexture(sheet.asset_path);
    DrawSpriteSheetFrameEx(texture, sheet, frame, bounds, origin, rotation,
                           tint, flip_x, flip_y);
}

void
DrawSpriteSheetFrameFromPath(SpriteSheet sheet, int frame, Rectangle bounds,
                             Vector2 origin, float rotation, Color tint)
{
    DrawSpriteSheetFrameFromPathEx(sheet, frame, bounds, origin, rotation,
                                   tint, 0, 0);
}
