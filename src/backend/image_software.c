#include "image_canvas.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static int
valid_pixels(const uint8_t *pixels, int width, int height, int stride)
{
    return pixels != NULL && width > 0 && height > 0 &&
           width <= INT_MAX / 4 && stride >= width * 4 &&
           (size_t)stride <= SIZE_MAX / (size_t)height;
}

static const ImageAsset *
find_asset(const ImageCanvas *canvas, const char *path, size_t path_length,
           uint32_t texture_id)
{
    if(canvas == NULL || canvas->assets == NULL)
        return NULL;
    for(size_t i = 0; i < canvas->asset_count; i++) {
        const ImageAsset *asset = &canvas->assets[i];
        if(!valid_pixels(asset->pixels, asset->width, asset->height,
                         asset->stride))
            continue;
        if(texture_id != 0) {
            if(asset->texture_id == texture_id)
                return asset;
        } else if(asset->path != NULL && path != NULL &&
                  strlen(asset->path) == path_length &&
                  memcmp(asset->path, path, path_length) == 0)
            return asset;
    }
    return NULL;
}

static int
asset_size(void *context, const char *path, size_t path_length,
           int *width, int *height)
{
    const ImageAsset *asset = find_asset(context, path, path_length, 0);
    if(width == NULL || height == NULL)
        return 0;
    *width = *height = 0;
    if(asset == NULL)
        return 0;
    *width = asset->width;
    *height = asset->height;
    return 1;
}

static int
inside_round(float px, float py, const float clip[4], float radius)
{
    float left = clip[0], top = clip[1];
    float right = left + clip[2], bottom = top + clip[3];
    float cx, cy;
    if(px < left || px >= right || py < top || py >= bottom)
        return 0;
    if(radius <= 0.0f)
        return 1;
    if(radius > clip[2] * 0.5f)
        radius = clip[2] * 0.5f;
    if(radius > clip[3] * 0.5f)
        radius = clip[3] * 0.5f;
    cx = px < left + radius ? left + radius :
         px > right - radius ? right - radius : px;
    cy = py < top + radius ? top + radius :
         py > bottom - radius ? bottom - radius : py;
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <=
           radius * radius;
}

static void
blend(uint8_t *destination, const uint8_t *source,
      const uint8_t tint[4])
{
    unsigned alpha = (unsigned)source[3] * tint[3] / 255u;
    unsigned background = destination[3] * (255u - alpha);
    unsigned total = alpha * 255u + background;
    if(alpha == 0)
        return;
    for(int i = 0; i < 3; i++) {
        unsigned value = (unsigned)source[i] * tint[i] / 255u;
        destination[i] = (uint8_t)((value * alpha * 255u +
                                   destination[i] * background) / total);
    }
    destination[3] = (uint8_t)(total / 255u);
}

static void
draw_image(void *context, const char *path, size_t path_length,
           uint32_t texture_id, const float source[4],
           const float destination[4], const float clip[4],
           const float origin[2], float rotation, float radius,
           const uint8_t tint[4])
{
    ImageCanvas *canvas = context;
    const ImageAsset *asset = find_asset(canvas, path, path_length, texture_id);
    float angle, cosine, sine, right, bottom;
    int x0, y0, x1, y1;
    if(canvas == NULL || asset == NULL ||
       !valid_pixels(canvas->pixels, canvas->width, canvas->height,
                     canvas->stride) || tint[3] == 0)
        return;
    for(int i = 0; i < 4; i++) {
        if(!isfinite(source[i]) || !isfinite(destination[i]) ||
           !isfinite(clip[i]))
            return;
    }
    if(!isfinite(origin[0]) || !isfinite(origin[1]) ||
       !isfinite(rotation) || !isfinite(radius) ||
       destination[2] <= 0.0f || destination[3] <= 0.0f ||
       clip[2] <= 0.0f || clip[3] <= 0.0f)
        return;
    right = clip[0] + clip[2];
    bottom = clip[1] + clip[3];
    if(!isfinite(right) || !isfinite(bottom))
        return;
    if(right <= 0.0f || bottom <= 0.0f ||
       clip[0] >= canvas->width || clip[1] >= canvas->height)
        return;
    x0 = clip[0] <= 0.0f ? 0 : (int)floorf(clip[0]);
    y0 = clip[1] <= 0.0f ? 0 : (int)floorf(clip[1]);
    x1 = right >= canvas->width ? canvas->width : (int)ceilf(right);
    y1 = bottom >= canvas->height ? canvas->height : (int)ceilf(bottom);
    angle = fmodf(rotation, 360.0f) * 0.017453292519943295f;
    cosine = cosf(angle);
    sine = sinf(angle);
    for(int y = y0; y < y1; y++) {
        for(int x = x0; x < x1; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float dx, dy, local_x, local_y, source_x, source_y;
            int sx, sy;
            const uint8_t *src;
            uint8_t *dst;
            if(!inside_round(px, py, clip, radius))
                continue;
            dx = px - destination[0];
            dy = py - destination[1];
            local_x = dx * cosine + dy * sine + origin[0];
            local_y = -dx * sine + dy * cosine + origin[1];
            if(local_x < 0.0f || local_y < 0.0f ||
               local_x >= destination[2] || local_y >= destination[3])
                continue;
            source_x = source[0] + local_x * source[2] / destination[2];
            source_y = source[1] + local_y * source[3] / destination[3];
            if(!isfinite(source_x) || !isfinite(source_y) ||
               source_x < 0.0f || source_y < 0.0f ||
               source_x >= asset->width || source_y >= asset->height)
                continue;
            sx = (int)floorf(source_x);
            sy = (int)floorf(source_y);
            src = asset->pixels + (size_t)sy * asset->stride + sx * 4;
            dst = canvas->pixels + (size_t)y * canvas->stride + x * 4;
            blend(dst, src, tint);
        }
    }
}

ImageRasterizer
ImageCanvasRasterizer(ImageCanvas *canvas)
{
    ImageRasterizer renderer = {asset_size, draw_image, canvas};
    return renderer;
}
