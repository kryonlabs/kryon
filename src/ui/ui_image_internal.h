#ifndef IMAGE_INTERNAL_H
#define IMAGE_INTERNAL_H

#include "ui_image_props.generated.h"

/* Capacity of the shared image texture cache in ui_image_cache.c. */
#define KRY_IMAGE_CACHE_MAX 128

Texture2D LoadImageTexture(const char *path);
Rectangle ImageFitRect(ImageProps image, Texture2D texture);
void ImageTexture(Texture2D texture, ImageProps image);
void ImageTextureTinted(Texture2D texture, ImageProps image, Color tint);
void ImageTextureTintedRaw(Texture2D texture, Rectangle bounds, Color tint);
void ui_image_draw_texture_strip(Texture2D texture, Rectangle source,
                                 Rectangle strip, Color tint);
void ui_image_draw_clipped_texture(Texture2D texture, Rectangle source,
                                   Rectangle dst, Rectangle bounds,
                                   Vector2 origin, float rotation,
                                   Color tint);
void ui_image_draw_rounded_texture(Texture2D texture, Rectangle source,
                                   Rectangle dst, Rectangle bounds,
                                   float radius, Color tint);

#endif
