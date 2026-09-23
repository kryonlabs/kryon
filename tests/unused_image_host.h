#ifndef KRYON_TEST_UNUSED_IMAGE_HOST_H
#define KRYON_TEST_UNUSED_IMAGE_HOST_H

#include "kryon_portable_host.h"

#include <assert.h>

static void unused_image_draw(void *context, const char *path,
                              size_t path_length, uint32_t texture_id,
                              const float source[4],
                              const float destination[4],
                              const float clip[4], const float origin[2],
                              float rotation, float radius,
                              const uint8_t tint[4])
{
    (void)context; (void)path; (void)path_length; (void)texture_id;
    (void)source; (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint;
    assert(0 && "this widget should not draw images");
}

static ImageRasterizer unused_image_renderer = {
    NULL, unused_image_draw, NULL};

#endif
