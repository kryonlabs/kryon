#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include "kryon_compat.generated.h"

typedef struct SpriteSheet {
    const char *asset_path;
    int frame_width;
    int frame_height;
    int columns;
    int rows;
    int frame_count;
} SpriteSheet;

SpriteSheet SpriteSheetGrid(const char *asset_path, int frame_width,
                            int frame_height, int columns, int rows);
int SpriteSheetFrameCount(SpriteSheet sheet);
int SpriteSheetNormalizeFrame(SpriteSheet sheet, int frame);
Rectangle SpriteSheetFrameSource(SpriteSheet sheet, int frame);

void DrawSpriteSheetFrame(Texture2D texture, SpriteSheet sheet, int frame,
                          Rectangle bounds, Vector2 origin, float rotation,
                          Color tint);
void DrawSpriteSheetFrameEx(Texture2D texture, SpriteSheet sheet, int frame,
                            Rectangle bounds, Vector2 origin, float rotation,
                            Color tint, int flip_x, int flip_y);
void DrawSpriteSheetFrameFromPath(SpriteSheet sheet, int frame,
                                  Rectangle bounds, Vector2 origin,
                                  float rotation, Color tint);
void DrawSpriteSheetFrameFromPathEx(SpriteSheet sheet, int frame,
                                    Rectangle bounds, Vector2 origin,
                                    float rotation, Color tint,
                                    int flip_x, int flip_y);

#endif
