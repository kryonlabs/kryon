#include "kry_backend_rec.h"

#include <stddef.h>
#include <stdio.h>

static KryBackendRec *g_rec;

static void
r_clear(unsigned color)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "clear %08x\n", color);
    g_rec->inner->clear(color);
}

static void
r_rect(int x, int y, int w, int h, unsigned color)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "rect %d %d %d %d %08x\n", x, y, w, h, color);
    g_rec->inner->rect(x, y, w, h, color);
}

static void
r_text(const char *s, int x, int y, int size, unsigned color)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "text \"%s\" %d %d %d %08x\n",
                s != NULL ? s : "", x, y, size, color);
    g_rec->inner->text(s, x, y, size, color);
}

static void
r_clip_push(int x, int y, int w, int h)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "clip_push %d %d %d %d\n", x, y, w, h);
    g_rec->inner->clip_push(x, y, w, h);
}

static void
r_clip_pop(void)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "clip_pop\n");
    g_rec->inner->clip_pop();
}

static void
r_texture(const char *asset_path, int x, int y, int w, int h,
          unsigned tint, int fit)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "texture \"%s\" %d %d %d %d %08x %d\n",
                asset_path != NULL ? asset_path : "", x, y, w, h, tint, fit);
    g_rec->inner->texture(asset_path, x, y, w, h, tint, fit);
}

static void
r_circle(int cx, int cy, int r, unsigned color)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "circle %d %d %d %08x\n", cx, cy, r, color);
    if(g_rec->inner->circle != NULL)
        g_rec->inner->circle(cx, cy, r, color);
}

static void
r_ring(int cx, int cy, int inner, int outer, unsigned color)
{
    if(g_rec == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "ring %d %d %d %d %08x\n", cx, cy, inner, outer,
                color);
    if(g_rec->inner->ring != NULL)
        g_rec->inner->ring(cx, cy, inner, outer, color);
}

static void
r_texture_rgba(const unsigned char *rgba, int sw, int sh, int x, int y,
               int dw, int dh, unsigned tint)
{
    if(g_rec == NULL || g_rec->inner->texture_rgba == NULL)
        return;
    g_rec->calls++;
    if(g_rec->log != NULL)
        fprintf(g_rec->log, "texture_rgba %dx%d %d %d %d %d %08x\n", sw, sh,
                x, y, dw, dh, tint);
    g_rec->inner->texture_rgba(rgba, sw, sh, x, y, dw, dh, tint);
}

static void
r_mouse(int *x, int *y)
{
    g_rec->inner->mouse(x, y);
}

static int
r_mouse_down(int button)
{
    return g_rec->inner->mouse_down(button);
}

static int
r_mouse_pressed(int button)
{
    return g_rec->inner->mouse_pressed(button);
}

static int
r_width(void)
{
    return g_rec->inner->width();
}

static int
r_height(void)
{
    return g_rec->inner->height();
}

static float
r_time(void)
{
    return g_rec->inner->time();
}

static int
r_scale_px(int px)
{
    return g_rec->inner->scale_px(px);
}

static unsigned
r_theme_color(int slot)
{
    return g_rec->inner->theme_color(slot);
}

static int
r_measure_text(const char *s, int size)
{
    return g_rec->inner->measure_text(s, size);
}

const KryBackend *
KryBackendRecBackend(KryBackendRec *rec, FILE *log, const KryBackend *inner)
{
    if(rec == NULL)
        return &KryBackendNull;
    rec->log = log;
    rec->inner = inner != NULL ? inner : &KryBackendNull;
    rec->calls = 0;
    rec->backend.clear = r_clear;
    rec->backend.rect = r_rect;
    rec->backend.text = r_text;
    rec->backend.measure_text = r_measure_text;
    rec->backend.clip_push = r_clip_push;
    rec->backend.clip_pop = r_clip_pop;
    rec->backend.mouse = r_mouse;
    rec->backend.mouse_down = r_mouse_down;
    rec->backend.mouse_pressed = r_mouse_pressed;
    rec->backend.width = r_width;
    rec->backend.height = r_height;
    rec->backend.time = r_time;
    rec->backend.scale_px = r_scale_px;
    rec->backend.theme_color = r_theme_color;
    rec->backend.texture = r_texture;
    rec->backend.circle = r_circle;
    rec->backend.ring = r_ring;
    rec->backend.texture_rgba = r_texture_rgba;
    g_rec = rec;
    return &rec->backend;
}

long
KryBackendRecCalls(const KryBackendRec *rec)
{
    return rec != NULL ? rec->calls : 0;
}
