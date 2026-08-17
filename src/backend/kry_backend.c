#include "kry_backend.h"

#include <stddef.h>

static void
null_clear(unsigned color)
{
    (void)color;
}

static void
null_rect(int x, int y, int w, int h, unsigned color)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)color;
}

static void
null_text(const char *s, int x, int y, int size, unsigned color)
{
    (void)s;
    (void)x;
    (void)y;
    (void)size;
    (void)color;
}

static int
null_measure_text(const char *s, int size)
{
    size_t n = 0;

    (void)size;
    if(s != NULL) {
        while(s[n] != '\0')
            n++;
    }
    return (int)n * (size > 0 ? size / 2 : 4);
}

static void
null_clip_push(int x, int y, int w, int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

static void
null_clip_pop(void)
{
}

static void
null_mouse(int *x, int *y)
{
    if(x != NULL)
        *x = 0;
    if(y != NULL)
        *y = 0;
}

static int
null_mouse_down(int button)
{
    (void)button;
    return 0;
}

static int
null_mouse_pressed(int button)
{
    (void)button;
    return 0;
}

static int
null_width(void)
{
    return 800;
}

static int
null_height(void)
{
    return 600;
}

static float
null_time(void)
{
    return 0.0f;
}

static int
null_scale_px(int px)
{
    return px;
}

static unsigned
null_theme_color(int slot)
{
    (void)slot;
    return 0x808080ffu;
}

static void
null_texture(const char *asset_path, int x, int y, int w, int h,
             unsigned tint, int fit)
{
    (void)asset_path;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)tint;
    (void)fit;
}

static int
null_wheel(void)
{
    return 0;
}

static unsigned
null_text_key(void)
{
    return 0;
}

static void
null_circle(int cx, int cy, int r, unsigned color)
{
    (void)cx;
    (void)cy;
    (void)r;
    (void)color;
}

static void
null_ring(int cx, int cy, int inner, int outer, unsigned color)
{
    (void)cx;
    (void)cy;
    (void)inner;
    (void)outer;
    (void)color;
}

const KryBackend KryBackendNull = {
    null_clear,
    null_rect,
    null_text,
    null_measure_text,
    null_clip_push,
    null_clip_pop,
    null_mouse,
    null_mouse_down,
    null_mouse_pressed,
    null_width,
    null_height,
    null_time,
    null_scale_px,
    null_theme_color,
    null_texture,
    null_circle,
    null_ring,
    NULL, /* texture_rgba: not expressible on a stub backend */
    null_wheel,
    null_text_key,
};

static const KryBackend *g_backend = &KryBackendNull;

const KryBackend *
KryBackendCurrent(void)
{
    return g_backend != NULL ? g_backend : &KryBackendNull;
}

void
KryBackendSelect(const KryBackend *backend)
{
    g_backend = backend != NULL ? backend : &KryBackendNull;
}
