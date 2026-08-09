#ifndef UI_SPRITE_H
#define UI_SPRITE_H

#include "kryon_compat.generated.h"

typedef enum UISpriteFit {
    UI_SPRITE_FIT_STRETCH,
    UI_SPRITE_FIT_CONTAIN,
    UI_SPRITE_FIT_COVER
} UISpriteFit;

typedef struct SpriteProps {
    const char *asset_path;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    UISpriteFit fit;
} SpriteProps;

#endif
