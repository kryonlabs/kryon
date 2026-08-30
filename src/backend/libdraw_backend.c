#include "kry_sw.h"
#include "libdraw_internal.h"
#include "kry_sw_png.h"

#ifndef KRYON_NATIVE_PLAN9
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define KRY_LIBDRAW_MAX_TEXTURES 512
#define KRY_LIBDRAW_MAX_FONTS 64
#define KRY_LIBDRAW_KEY_CAP 512
#define KRY_LIBDRAW_MAX_TEXT_DRAWS 2048

typedef struct KryLibdrawQueuedText {
    unsigned font_id;
    char *text;
    int x;
    int y;
    int clip_active;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;
    Color color;
} KryLibdrawQueuedText;

int kry_libdraw_width = 800;
int kry_libdraw_height = 600;
int kry_libdraw_ready = 0;
int kry_libdraw_should_close = 0;
double kry_libdraw_last_time = 0.0;
float kry_libdraw_frame_time = 1.0f / 60.0f;
static int kry_libdraw_target_fps = 0;
static int kry_libdraw_fps = 0;
static int kry_libdraw_frame_counter = 0;
static double kry_libdraw_fps_time = 0.0;

int kry_libdraw_mouse_x = 0;
int kry_libdraw_mouse_y = 0;
int kry_libdraw_mouse_dx = 0;
int kry_libdraw_mouse_dy = 0;
int kry_libdraw_mouse_down[3] = {0};
int kry_libdraw_mouse_pressed[3] = {0};
int kry_libdraw_mouse_released[3] = {0};
int kry_libdraw_wheel = 0;

int kry_libdraw_key_down[512] = {0};
int kry_libdraw_key_pressed[512] = {0};
int kry_libdraw_key_released[512] = {0};
int kry_libdraw_key_queue[64] = {0};
int kry_libdraw_key_qr = 0;
int kry_libdraw_key_qw = 0;
int kry_libdraw_char_queue[128] = {0};
int kry_libdraw_char_qr = 0;
int kry_libdraw_char_qw = 0;

P9Image *kry_libdraw_target = NULL;

static KrySw g_sw;
static KrySw *g_active_sw;
static const KryBackend *g_sw_backend;
static P9Image *g_present;
static P9Image *g_text_surface;
static unsigned char *g_present_pixels;
static size_t g_present_pixels_cap;
static int g_sw_ready;
static int g_target_stack[16];
static int g_target_depth;
static unsigned g_active_texture_id;
static KryLibdrawTexture g_textures[KRY_LIBDRAW_MAX_TEXTURES];
static KryLibdrawFont g_fonts[KRY_LIBDRAW_MAX_FONTS];
static KryLibdrawQueuedText g_text_draws[KRY_LIBDRAW_MAX_TEXT_DRAWS];
static int g_text_draw_count;
static unsigned g_next_texture_id = 1;
static unsigned g_next_font_id = 1;
static unsigned char *g_clipboard;
static TraceLogCallback g_trace_log_callback;
static unsigned g_window_state;
static int g_exit_key = KEY_ESCAPE;

typedef struct KryLibdrawCursor {
    struct {
        int x;
        int y;
    } offset;
    unsigned char clr[2 * 16];
    unsigned char set[2 * 16];
} KryLibdrawCursor;

static KryLibdrawCursor g_default_cursor = {
    {0, 0},
    {0x00, 0x00, 0x40, 0x00, 0x60, 0x00, 0x70, 0x00,
     0x78, 0x00, 0x7C, 0x00, 0x7E, 0x00, 0x7F, 0x00,
     0x7F, 0x80, 0x7C, 0x00, 0x70, 0x00, 0x68, 0x00,
     0x48, 0x00, 0x08, 0x00, 0x04, 0x00, 0x00, 0x00},
    {0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
     0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
     0x80, 0x00, 0x83, 0xC0, 0x8C, 0x00, 0x94, 0x00,
     0xA4, 0x00, 0xC4, 0x00, 0x82, 0x00, 0x06, 0x00}
};

#ifdef KRYON_NATIVE_PLAN9
static void
put_be32(unsigned char *out, int value)
{
    unsigned v = (unsigned)value;

    out[0] = (unsigned char)(v >> 24);
    out[1] = (unsigned char)(v >> 16);
    out[2] = (unsigned char)(v >> 8);
    out[3] = (unsigned char)v;
}
#endif

static void
set_plan9_cursor(KryLibdrawCursor *cursor)
{
#ifdef KRYON_NATIVE_PLAN9
    unsigned char data[2 * 4 + 2 * 2 * 16];
    int fd;

    fd = open("/dev/cursor", OWRITE);
    if(fd < 0)
        return;
    if(cursor == NULL) {
        write(fd, "", 0);
        close(fd);
        return;
    }
    put_be32(data + 0 * 4, cursor->offset.x);
    put_be32(data + 1 * 4, cursor->offset.y);
    memcpy(data + 2 * 4, cursor->clr, 2 * 2 * 16);
    write(fd, data, sizeof(data));
    close(fd);
#else
    (void)cursor;
#endif
}

static void
reset_default_cursor(void)
{
    set_plan9_cursor(&g_default_cursor);
#ifndef KRYON_NATIVE_PLAN9
    if(display != nil)
        _displaycursor(display, nil, nil);
#endif
}

static double
now_seconds(void)
{
#ifdef KRYON_PLATFORM_PLAN9
    /* Native libc keeps a high-resolution clock in nsec(). */
    return (double)nsec() / 1000000000.0;
#else
#ifdef CLOCK_MONOTONIC
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
    return (double)time(NULL);
#endif
}

static unsigned char *
dup_text_bytes(const char *text)
{
    size_t len;
    unsigned char *copy;

    if(text == NULL)
        text = "";
    len = strlen(text);
    copy = malloc(len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static char *
dup_text_n_bytes(const char *text, int byte_len)
{
    char *copy;

    if(text == NULL)
        text = "";
    if(byte_len < 0)
        byte_len = (int)strlen(text);
    copy = malloc((size_t)byte_len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, text, (size_t)byte_len);
    copy[byte_len] = '\0';
    return copy;
}

static void
clear_text_draws(void)
{
    int i;

    for(i = 0; i < g_text_draw_count; i++)
        free(g_text_draws[i].text);
    g_text_draw_count = 0;
}

static void
draw_queued_text(void)
{
    P9Rectangle old_clipr;
    int old_repl;
    int i;

    if(display == nil || screen == nil)
        goto done;
    old_clipr = screen->clipr;
    old_repl = screen->repl;
    for(i = 0; i < g_text_draw_count; i++) {
        KryLibdrawQueuedText *q = &g_text_draws[i];
        KryLibdrawFont *registered = kry_libdraw_font(q->font_id);
        P9Font *p9font = registered != NULL ? registered->font : font;
        P9Image *color = kry_libdraw_color(q->color);

        if(p9font == nil || color == nil || q->text == NULL)
            continue;
        if(q->clip_active) {
            P9Rectangle clip = kry_p9_rect(q->clip_x + screen->r.min.x,
                                           q->clip_y + screen->r.min.y,
                                           q->clip_w, q->clip_h);
            replclipr(screen, 0, clip);
        } else if(!eqrect(screen->clipr, old_clipr) ||
                  screen->repl != old_repl) {
            replclipr(screen, old_repl, old_clipr);
        }
        string(screen, addpt(screen->r.min, kry_p9_point(q->x, q->y)),
               color, ZP, p9font, q->text);
    }
    if(!eqrect(screen->clipr, old_clipr) || screen->repl != old_repl)
        replclipr(screen, old_repl, old_clipr);

done:
    clear_text_draws();
}

static int
active_clip_rect(int *x, int *y, int *w, int *h)
{
    KrySw *sw = g_active_sw;
    int cx;
    int cy;
    int cw;
    int ch;

    if(sw == NULL || sw->clip_n <= 0)
        return 0;
    cx = 0;
    cy = 0;
    cw = sw->w;
    ch = sw->h;
    for(int i = 0; i < sw->clip_n; i++) {
        int nx = cx > sw->clip_x[i] ? cx : sw->clip_x[i];
        int ny = cy > sw->clip_y[i] ? cy : sw->clip_y[i];
        int nx2 = cx + cw < sw->clip_x[i] + sw->clip_w[i] ?
                  cx + cw : sw->clip_x[i] + sw->clip_w[i];
        int ny2 = cy + ch < sw->clip_y[i] + sw->clip_h[i] ?
                  cy + ch : sw->clip_y[i] + sw->clip_h[i];

        if(nx2 <= nx || ny2 <= ny)
            return 0;
        cx = nx;
        cy = ny;
        cw = nx2 - nx;
        ch = ny2 - ny;
    }
    if(x != NULL)
        *x = cx;
    if(y != NULL)
        *y = cy;
    if(w != NULL)
        *w = cw;
    if(h != NULL)
        *h = ch;
    return 1;
}

static P9Image *
ensure_text_surface(KrySw *sw)
{
    if(display == nil || sw == NULL || sw->w <= 0 || sw->h <= 0)
        return nil;
    if(g_text_surface != nil && Dx(g_text_surface->r) == sw->w &&
       Dy(g_text_surface->r) == sw->h && g_text_surface->chan == RGBA32)
        return g_text_surface;
    if(g_text_surface != nil)
        freeimage(g_text_surface);
    g_text_surface = allocimage(display, kry_p9_rect(0, 0, sw->w, sw->h),
                                RGBA32, 0, DTransparent);
    return g_text_surface;
}

static int
draw_native_text_into_active_sw(unsigned font_id, const char *text,
                                int byte_len, int x, int y, Color color)
{
    KryLibdrawFont *registered;
    P9Font *p9font;
    P9Image *surface;
    P9Image *text_color;
    P9Rectangle old_clipr;
    int old_repl;
    int ndata;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;
    int clip_active;

    if(text == NULL || text[0] == '\0' || g_active_sw == NULL ||
       g_active_sw->pixels == NULL)
        return 0;
    registered = kry_libdraw_font(font_id);
    p9font = registered != NULL ? registered->font : font;
    if(display == nil || p9font == nil)
        return 0;
    surface = ensure_text_surface(g_active_sw);
    text_color = kry_libdraw_color(color);
    if(surface == nil || text_color == nil)
        return 0;
    ndata = g_active_sw->stride * g_active_sw->h;
    if(loadimage(surface, surface->r, g_active_sw->pixels, ndata) < 0)
        return 0;

    old_clipr = surface->clipr;
    old_repl = surface->repl;
    clip_active = active_clip_rect(&clip_x, &clip_y, &clip_w, &clip_h);
    if(clip_active) {
        replclipr(surface, 0, kry_p9_rect(clip_x, clip_y, clip_w, clip_h));
    }
    if(byte_len < 0)
        string(surface, kry_p9_point(x, y), text_color, ZP, p9font,
               (char *)text);
    else
        stringn(surface, kry_p9_point(x, y), text_color, ZP, p9font,
                (char *)text, byte_len);
    if(clip_active &&
       (!eqrect(surface->clipr, old_clipr) || surface->repl != old_repl)) {
        replclipr(surface, old_repl, old_clipr);
    }
    if(unloadimage(surface, surface->r, g_active_sw->pixels, ndata) < 0)
        return 0;
    return 1;
}

static int
abs_int(int value)
{
    return value < 0 ? -value : value;
}

P9Rectangle
kry_p9_rect(int x, int y, int w, int h)
{
    P9Rectangle r;

    r.min.x = x;
    r.min.y = y;
    r.max.x = x + w;
    r.max.y = y + h;
    return r;
}

P9Point
kry_p9_point(int x, int y)
{
    P9Point p;

    p.x = x;
    p.y = y;
    return p;
}

static unsigned
pack(Color c)
{
    return ((unsigned)c.r << 24) | ((unsigned)c.g << 16) |
           ((unsigned)c.b << 8) | (unsigned)c.a;
}

P9Image *
kry_libdraw_color_u32(unsigned rgba)
{
    static struct {
        unsigned rgba;
        ulong chan;
        P9Image *image;
    } cache[128];
    static int next;
    ulong chan;
    int i;

    if(display == nil)
        return nil;
    chan = screen != nil ? screen->chan : RGBA32;
    for(i = 0; i < 128; i++)
        if(cache[i].image != nil && cache[i].rgba == rgba &&
           cache[i].chan == chan)
            return cache[i].image;
    i = next++ % 128;
    if(cache[i].image != nil)
        freeimage(cache[i].image);
    cache[i].rgba = rgba;
    cache[i].chan = chan;
    cache[i].image = allocimage(display, kry_p9_rect(0, 0, 1, 1), chan, 1,
                                rgba);
    return cache[i].image;
}

P9Image *
kry_libdraw_color(Color color)
{
    return kry_libdraw_color_u32(pack(color));
}

extern int kry_write_png_file(const char *path, const unsigned char *rgba,
                              int w, int h);
extern unsigned char *kry_decode_image_rgba(const unsigned char *data, int len,
                                            int *width, int *height);

int
kry_libdraw_write_png(const char *path, const unsigned char *rgba, int width,
                      int height)
{
    return kry_write_png_file(path, rgba, width, height);
}

static void
ensure_sw(int width, int height)
{
    if(width <= 0)
        width = 1;
    if(height <= 0)
        height = 1;
    if(g_sw_ready && g_sw.w == width && g_sw.h == height)
        return;
    if(g_sw_ready)
        KrySwFree(&g_sw);
    if(KrySwInit(&g_sw, NULL, width, height) == 0) {
        g_sw_backend = KrySwBackend(&g_sw);
        g_active_sw = &g_sw;
        g_active_texture_id = 0;
        g_sw_ready = 1;
    }
}

static void
ensure_present(void)
{
    size_t need;

    if(display == nil || screen == nil || !g_sw_ready)
        return;
    need = (size_t)g_sw.stride * g_sw.h;
    if(g_present != nil && Dx(g_present->r) == g_sw.w &&
       Dy(g_present->r) == g_sw.h && g_present->chan == screen->chan &&
       g_present_pixels_cap >= need)
        return;
    if(g_present != nil)
        freeimage(g_present);
    g_present = allocimage(display, kry_p9_rect(0, 0, g_sw.w, g_sw.h),
                           screen->chan, 0, DBlack);
    if(g_present_pixels_cap < need) {
        unsigned char *pixels = realloc(g_present_pixels, need);

        if(pixels == NULL) {
            free(g_present_pixels);
            g_present_pixels = NULL;
            g_present_pixels_cap = 0;
            return;
        }
        g_present_pixels = pixels;
        g_present_pixels_cap = need;
    }
}

static int
ensure_present_pixels_cap(size_t need)
{
    if(g_present_pixels_cap < need) {
        unsigned char *pixels = realloc(g_present_pixels, need);

        if(pixels == NULL) {
            free(g_present_pixels);
            g_present_pixels = NULL;
            g_present_pixels_cap = 0;
            return 0;
        }
        g_present_pixels = pixels;
        g_present_pixels_cap = need;
    }
    return 1;
}

static const unsigned char *
present_region_pixels(P9Rectangle rect, int *size)
{
    int x;
    int y;
    int w = Dx(rect);
    int h = Dy(rect);
    size_t need = (size_t)w * h * 4;

    if(size != NULL)
        *size = (int)need;
    if(w <= 0 || h <= 0)
        return NULL;
    if(g_present == nil || g_present->depth != 32)
        return rect.min.x == 0 && w == g_sw.w ?
               g_sw.pixels + (size_t)rect.min.y * g_sw.stride : NULL;
    if(g_present->chan == RGBA32 && rect.min.x == 0 && w == g_sw.w) {
        if(size != NULL)
            *size = g_sw.stride * h;
        return g_sw.pixels + (size_t)rect.min.y * g_sw.stride;
    }
    if(!ensure_present_pixels_cap(need))
        return NULL;
    if(g_present->chan == RGBA32) {
        for(y = 0; y < h; y++) {
            const unsigned char *src = g_sw.pixels +
                (size_t)(rect.min.y + y) * g_sw.stride + rect.min.x * 4;
            unsigned char *dst = g_present_pixels + (size_t)y * w * 4;

            memcpy(dst, src, (size_t)w * 4);
        }
        return g_present_pixels;
    }
    if(g_present->chan == XRGB32) {
        for(y = 0; y < h; y++) {
            const unsigned char *src = g_sw.pixels +
                (size_t)(rect.min.y + y) * g_sw.stride + rect.min.x * 4;
            unsigned char *dst = g_present_pixels + (size_t)y * w * 4;

            for(x = 0; x < w; x++) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = 0xff;
                src += 4;
                dst += 4;
            }
        }
        return g_present_pixels;
    }
    if(g_present->chan == ARGB32) {
        for(y = 0; y < h; y++) {
            const unsigned char *src = g_sw.pixels +
                (size_t)(rect.min.y + y) * g_sw.stride + rect.min.x * 4;
            unsigned char *dst = g_present_pixels + (size_t)y * w * 4;

            for(x = 0; x < w; x++) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
                src += 4;
                dst += 4;
            }
        }
        return g_present_pixels;
    }
    return rect.min.x == 0 && w == g_sw.w ?
           g_sw.pixels + (size_t)rect.min.y * g_sw.stride : NULL;
}

static void
present_sw(void)
{
    const unsigned char *pixels;
    int ndata;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    P9Rectangle src_rect;
    P9Rectangle dst_rect;

    if(display == nil || screen == nil || !g_sw_ready)
        return;
    ensure_present();
    if(g_present == nil)
        return;
    if(g_text_draw_count > 0) {
        int dirty_x;
        int dirty_y;
        int dirty_w;
        int dirty_h;

        KrySwDirty(&g_sw, &dirty_x, &dirty_y, &dirty_w, &dirty_h);
        w = g_sw.w;
        h = g_sw.h;
    } else {
        KrySwDirty(&g_sw, &x, &y, &w, &h);
    }
    if(w <= 0 || h <= 0) {
        draw_queued_text();
        return;
    }
    src_rect = kry_p9_rect(x, y, w, h);
    dst_rect = rectaddpt(src_rect, screen->r.min);
    pixels = present_region_pixels(src_rect, &ndata);
    if(pixels == NULL)
        return;
    loadimage(g_present, src_rect, (unsigned char *)pixels, ndata);
    draw(screen, dst_rect, g_present, nil, src_rect.min);
    draw_queued_text();
    flushimage(display, 1);
}

void
kry_libdraw_flush(void)
{
    present_sw();
}

void
eresized(int new)
{
    (void)new;
    if(display == nil)
        return;
    if(getwindow(display, Refnone) < 0)
        return;
    if(screen != nil) {
        kry_libdraw_width = Dx(screen->r);
        kry_libdraw_height = Dy(screen->r);
        ensure_sw(kry_libdraw_width, kry_libdraw_height);
        if(g_present != nil) {
            freeimage(g_present);
            g_present = nil;
        }
    }
}

void
kry_libdraw_reset_edges(void)
{
    memset(kry_libdraw_mouse_pressed, 0, sizeof(kry_libdraw_mouse_pressed));
    memset(kry_libdraw_mouse_released, 0, sizeof(kry_libdraw_mouse_released));
    memset(kry_libdraw_key_pressed, 0, sizeof(kry_libdraw_key_pressed));
    memset(kry_libdraw_key_released, 0, sizeof(kry_libdraw_key_released));
    kry_libdraw_mouse_dx = 0;
    kry_libdraw_mouse_dy = 0;
    kry_libdraw_wheel = 0;
}

static int
button_index(int buttons)
{
    if(buttons & 1)
        return 0;
    if(buttons & 4)
        return 1;
    if(buttons & 2)
        return 2;
    return -1;
}

void
kry_libdraw_push_key(int key)
{
    if(key <= 0)
        return;
    kry_libdraw_key_queue[kry_libdraw_key_qw++ % 64] = key;
    if(key < KRY_LIBDRAW_KEY_CAP) {
        kry_libdraw_key_pressed[key] = 1;
        kry_libdraw_key_down[key] = 1;
    }
}

void
kry_libdraw_push_char(int ch)
{
    if(ch <= 0)
        return;
    kry_libdraw_char_queue[kry_libdraw_char_qw++ % 128] = ch;
}

int
kry_libdraw_map_key(int ch)
{
    switch(ch) {
    case Kesc:
        return KEY_ESCAPE;
    case '\n':
    case '\r':
        return KEY_ENTER;
    case '\t':
        return KEY_TAB;
    case Kbs:
    case Kdel:
        return KEY_BACKSPACE;
    case Kleft:
        return KEY_LEFT;
    case Kright:
        return KEY_RIGHT;
    case Kup:
        return KEY_UP;
    case Kdown:
        return KEY_DOWN;
    case Khome:
        return KEY_HOME;
    case Kend:
        return KEY_END;
    case Kpgup:
        return KEY_PAGE_UP;
    case Kpgdown:
        return KEY_PAGE_DOWN;
    default:
        if(ch >= 'a' && ch <= 'z')
            return ch - 'a' + 'A';
        if(ch >= 32 && ch <= 126)
            return ch;
        if(ch >= (KF | 1) && ch <= (KF | 12))
            return KEY_F1 + (ch - (KF | 1));
        return 0;
    }
}

void
kry_libdraw_poll(void)
{
    if(!kry_libdraw_ready)
        return;
    for(;;) {
        int handled = 0;

        while(ecanread(Emouse)) {
            P9Mouse m = emouse();
            int oldx = kry_libdraw_mouse_x;
            int oldy = kry_libdraw_mouse_y;
            int old[3];
            int i;

            old[0] = kry_libdraw_mouse_down[0];
            old[1] = kry_libdraw_mouse_down[1];
            old[2] = kry_libdraw_mouse_down[2];
            kry_libdraw_mouse_x = m.xy.x - screen->r.min.x;
            kry_libdraw_mouse_y = m.xy.y - screen->r.min.y;
            kry_libdraw_mouse_dx += kry_libdraw_mouse_x - oldx;
            kry_libdraw_mouse_dy += kry_libdraw_mouse_y - oldy;
            kry_libdraw_mouse_down[0] = (m.buttons & 1) != 0;
            kry_libdraw_mouse_down[1] = (m.buttons & 4) != 0;
            kry_libdraw_mouse_down[2] = (m.buttons & 2) != 0;
            for(i = 0; i < 3; i++) {
                if(!old[i] && kry_libdraw_mouse_down[i])
                    kry_libdraw_mouse_pressed[i] = 1;
                if(old[i] && !kry_libdraw_mouse_down[i])
                    kry_libdraw_mouse_released[i] = 1;
            }
            if(m.buttons & 8)
                kry_libdraw_wheel++;
            if(m.buttons & 16)
                kry_libdraw_wheel--;
            (void)button_index(m.buttons);
            handled = 1;
        }
        while(ecanread(Ekeyboard)) {
            int ch = ekbd();
            int key = kry_libdraw_map_key(ch);

            if(key != 0)
                kry_libdraw_push_key(key);
            if((ch >= 32 && ch != 127) || ch == Kbs)
                kry_libdraw_push_char(ch == Kbs ? 8 : ch);
            if(key == KEY_ESCAPE)
                kry_libdraw_should_close = 1;
            handled = 1;
        }
        if(!handled)
            break;
    }
}

unsigned char *
kry_libdraw_png_rgba(const unsigned char *data, int len, int *width,
                     int *height)
{
    return kry_sw_png_rgba(data, (size_t)len, width, height);
}

static unsigned char *
read_file(const char *path, int *len)
{
#ifdef KRYON_NATIVE_PLAN9
    int fd;
    vlong n;
    unsigned char *data;

    if(len != NULL)
        *len = 0;
    if(path == NULL)
        return NULL;
    fd = open((char *)path, OREAD);
    if(fd < 0)
        return NULL;
    n = seek(fd, 0, 2);
    if(n < 0) {
        close(fd);
        return NULL;
    }
    seek(fd, 0, 0);
    data = malloc((size_t)n + 1);
    if(data == NULL) {
        close(fd);
        return NULL;
    }
    if(readn(fd, data, (long)n) != n) {
        free(data);
        close(fd);
        return NULL;
    }
    close(fd);
    data[n] = 0;
    if(len != NULL)
        *len = (int)n;
    return data;
#else
    FILE *f;
    long n;
    unsigned char *data;

    if(len != NULL)
        *len = 0;
    if(path == NULL)
        return NULL;
    f = fopen(path, "rb");
    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if(n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    data = malloc((size_t)n + 1);
    if(data == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    data[n] = 0;
    if(len != NULL)
        *len = (int)n;
    return data;
#endif
}

static int
texture_pixels_are_mask(const unsigned char *rgba, int width, int height)
{
    int seen = 0;
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    int i;

    if(rgba == NULL || width <= 0 || height <= 0)
        return 0;
    for(i = 0; i < width * height; i++) {
        const unsigned char *p = rgba + (size_t)i * 4;

        if(p[3] == 0)
            continue;
        if(!seen) {
            r = p[0];
            g = p[1];
            b = p[2];
            seen = 1;
            continue;
        }
        if(p[0] != r || p[1] != g || p[2] != b)
            return 0;
    }
    if(!seen)
        return 0;
    return (r == 0 && g == 0 && b == 0) ||
           (r == 255 && g == 255 && b == 255);
}

unsigned
kry_libdraw_texture_register(P9Image *image, unsigned char *rgba, int width,
                             int height, int owned_rgba, int render_target)
{
    int i;
    unsigned id = g_next_texture_id++;

    if(id == 0)
        id = g_next_texture_id++;
    for(i = 1; i < KRY_LIBDRAW_MAX_TEXTURES; i++) {
        if(g_textures[i].id == 0) {
            memset(&g_textures[i], 0, sizeof(g_textures[i]));
            g_textures[i].id = id;
            g_textures[i].image = image;
            g_textures[i].rgba = rgba;
            g_textures[i].width = width;
            g_textures[i].height = height;
            g_textures[i].owned_rgba = owned_rgba;
            g_textures[i].render_target = render_target;
            g_textures[i].mask = !render_target &&
                                 texture_pixels_are_mask(rgba, width, height);
            return id;
        }
    }
    if(owned_rgba)
        free(rgba);
    if(image != nil)
        freeimage(image);
    return 0;
}

KryLibdrawTexture *
kry_libdraw_texture(unsigned id)
{
    int i;

    if(id == 0)
        return NULL;
    for(i = 1; i < KRY_LIBDRAW_MAX_TEXTURES; i++)
        if(g_textures[i].id == id)
            return &g_textures[i];
    return NULL;
}

void
kry_libdraw_texture_preserve_colors(Texture2D texture)
{
    KryLibdrawTexture *t = kry_libdraw_texture(texture.id);

    if(t == NULL)
        return;
    t->preserve_colors = 1;
    t->mask = 0;
}

void
kry_libdraw_texture_unregister(unsigned id)
{
    KryLibdrawTexture *t = kry_libdraw_texture(id);

    if(t == NULL)
        return;
    if(t->sw_ready)
        KrySwFree(&t->sw);
    if(t->image != nil)
        freeimage(t->image);
    if(t->owned_rgba)
        free(t->rgba);
    memset(t, 0, sizeof(*t));
}

unsigned
kry_libdraw_font_register(P9Font *font, int base_size)
{
    int i;
    unsigned id = g_next_font_id++;

    if(id == 0)
        id = g_next_font_id++;
    for(i = 1; i < KRY_LIBDRAW_MAX_FONTS; i++) {
        if(g_fonts[i].id == 0) {
            g_fonts[i] = (KryLibdrawFont){id, font, base_size};
            return id;
        }
    }
    return 0;
}

KryLibdrawFont *
kry_libdraw_font(unsigned id)
{
    int i;

    if(id == 0)
        return NULL;
    for(i = 1; i < KRY_LIBDRAW_MAX_FONTS; i++)
        if(g_fonts[i].id == id)
            return &g_fonts[i];
    return NULL;
}

void
kry_libdraw_font_unregister(unsigned id)
{
    KryLibdrawFont *f = kry_libdraw_font(id);

    if(f == NULL)
        return;
    memset(f, 0, sizeof(*f));
}

int
kry_libdraw_font_height(unsigned id)
{
    KryLibdrawFont *f = kry_libdraw_font(id);

    if(f == NULL || f->font == nil)
        return 0;
    return f->font->height;
}

int
kry_libdraw_font_text_width(unsigned id, const char *text, int byte_len)
{
    KryLibdrawFont *registered = kry_libdraw_font(id);
    P9Font *p9font = registered != NULL ? registered->font : font;

    if(p9font == nil || text == NULL)
        return 0;
    if(byte_len < 0)
        return stringwidth(p9font, (char *)text);
    return stringnwidth(p9font, (char *)text, byte_len);
}

void
kry_libdraw_queue_text(unsigned font_id, const char *text, int byte_len, int x,
                       int y, Color color)
{
    KryLibdrawQueuedText *q;

    if(text == NULL || text[0] == '\0')
        return;
    if(g_active_texture_id != 0 &&
       draw_native_text_into_active_sw(font_id, text, byte_len, x, y, color))
        return;
    if(g_text_draw_count >= KRY_LIBDRAW_MAX_TEXT_DRAWS)
        return;
    q = &g_text_draws[g_text_draw_count];
    memset(q, 0, sizeof(*q));
    q->text = dup_text_n_bytes(text, byte_len);
    if(q->text == NULL)
        return;
    q->font_id = font_id;
    q->x = x;
    q->y = y;
    q->clip_active = active_clip_rect(&q->clip_x, &q->clip_y, &q->clip_w,
                                      &q->clip_h);
    q->color = color;
    g_text_draw_count++;
}

void
KryonRaylibBackend_InitWindow(int width, int height, const char *title)
{
    kry_libdraw_width = width > 0 ? width : 800;
    kry_libdraw_height = height > 0 ? height : 600;
    ensure_sw(kry_libdraw_width, kry_libdraw_height);
    if(initdraw(NULL, NULL, (char *)(title != NULL ? title : "Kryon")) < 0) {
        kry_libdraw_ready = 0;
        return;
    }
    einit(Emouse | Ekeyboard);
    reset_default_cursor();
    if(screen != nil) {
        kry_libdraw_width = Dx(screen->r);
        kry_libdraw_height = Dy(screen->r);
        ensure_sw(kry_libdraw_width, kry_libdraw_height);
    }
    kry_libdraw_target = screen;
    kry_libdraw_ready = 1;
    kry_libdraw_last_time = now_seconds();
}

void
KryonRaylibBackend_CloseWindow(void)
{
    kry_libdraw_ready = 0;
    clear_text_draws();
    if(g_present != nil) {
        freeimage(g_present);
        g_present = nil;
    }
    if(g_text_surface != nil) {
        freeimage(g_text_surface);
        g_text_surface = nil;
    }
    free(g_present_pixels);
    g_present_pixels = NULL;
    g_present_pixels_cap = 0;
    if(g_sw_ready) {
        KrySwFree(&g_sw);
        g_sw_ready = 0;
    }
}

bool
KryonRaylibBackend_WindowShouldClose(void)
{
    kry_libdraw_poll();
    if(g_exit_key > 0 && g_exit_key < KRY_LIBDRAW_KEY_CAP &&
       kry_libdraw_key_pressed[g_exit_key])
        kry_libdraw_should_close = 1;
    return kry_libdraw_should_close != 0;
}

void
KryonRaylibBackend_EndDrawing(void)
{
    double now = now_seconds();

    present_sw();
    now = now_seconds();
    if(kry_libdraw_target_fps > 0 && kry_libdraw_last_time > 0.0) {
        double target = 1.0 / (double)kry_libdraw_target_fps;
        double elapsed = now - kry_libdraw_last_time;

        if(elapsed < target) {
            double remaining = target - elapsed;

            if(remaining > 0.0)
#ifdef KRYON_NATIVE_PLAN9
                sleep((long)(remaining * 1000.0));
#else
                usleep((unsigned int)(remaining * 1000000.0));
#endif
            now = now_seconds();
        }
    }
    kry_libdraw_frame_time = (float)(now - kry_libdraw_last_time);
    if(kry_libdraw_frame_time <= 0.0f)
        kry_libdraw_frame_time = 1.0f / 60.0f;
    kry_libdraw_last_time = now;
    kry_libdraw_frame_counter++;
    if(kry_libdraw_fps_time <= 0.0)
        kry_libdraw_fps_time = now;
    if(now - kry_libdraw_fps_time >= 1.0) {
        kry_libdraw_fps = kry_libdraw_frame_counter;
        kry_libdraw_frame_counter = 0;
        kry_libdraw_fps_time = now;
    }
    kry_libdraw_reset_edges();
}

bool IsWindowReady(void) { return kry_libdraw_ready != 0; }
bool IsWindowFocused(void) { return kry_libdraw_ready != 0; }
bool IsWindowFullscreen(void) { return (g_window_state & FLAG_FULLSCREEN_MODE) != 0; }
bool IsWindowHidden(void) { return (g_window_state & FLAG_WINDOW_HIDDEN) != 0; }
bool IsWindowMinimized(void) { return (g_window_state & FLAG_WINDOW_MINIMIZED) != 0; }
bool IsWindowMaximized(void) { return (g_window_state & FLAG_WINDOW_MAXIMIZED) != 0; }
bool IsWindowResized(void) { return false; }
bool IsWindowState(unsigned int flag) { return (g_window_state & flag) != 0; }
void SetWindowState(unsigned int flags) { g_window_state |= flags; }
void ClearWindowState(unsigned int flags) { g_window_state &= ~flags; }
void ToggleFullscreen(void) { g_window_state ^= FLAG_FULLSCREEN_MODE; }
void ToggleBorderlessWindowed(void) {}
void MaximizeWindow(void) { g_window_state |= FLAG_WINDOW_MAXIMIZED; }
void MinimizeWindow(void) { g_window_state |= FLAG_WINDOW_MINIMIZED; }
void RestoreWindow(void)
{
    g_window_state &= ~(FLAG_WINDOW_MINIMIZED | FLAG_WINDOW_MAXIMIZED |
                        FLAG_WINDOW_HIDDEN);
}
void SetWindowIcon(Image image) { (void)image; }
void SetWindowIcons(Image *images, int count)
{
    (void)images;
    (void)count;
}
void SetWindowTitle(const char *title) { (void)title; }
void SetWindowPosition(int x, int y)
{
    (void)x;
    (void)y;
}
void SetWindowMonitor(int monitor) { (void)monitor; }
void SetWindowMinSize(int width, int height)
{
    (void)width;
    (void)height;
}
void SetWindowMaxSize(int width, int height)
{
    (void)width;
    (void)height;
}
void SetWindowOpacity(float opacity) { (void)opacity; }
void SetWindowFocused(void) {}
void *GetWindowHandle(void) { return NULL; }
void SetConfigFlags(unsigned int flags) { (void)flags; }
void SetTargetFPS(int fps) { kry_libdraw_target_fps = fps > 0 ? fps : 0; }
void SetTraceLogLevel(int logLevel) { (void)logLevel; }
void SetMouseCursor(int cursor)
{
    (void)cursor;
    set_plan9_cursor(&g_default_cursor);
}
void SetExitKey(int key) { g_exit_key = key; }
void SetWindowSize(int width, int height)
{
    kry_libdraw_width = width;
    kry_libdraw_height = height;
    ensure_sw(width, height);
}
int GetScreenWidth(void) { return kry_libdraw_width; }
int GetScreenHeight(void) { return kry_libdraw_height; }
int GetRenderWidth(void) { return kry_libdraw_width; }
int GetRenderHeight(void) { return kry_libdraw_height; }
int GetCurrentMonitor(void) { return 0; }
int GetMonitorCount(void) { return 1; }
Vector2 GetMonitorPosition(int monitor)
{
    (void)monitor;
    return (Vector2){0.0f, 0.0f};
}
int GetMonitorWidth(int monitor)
{
    (void)monitor;
    return kry_libdraw_width;
}
int GetMonitorHeight(int monitor)
{
    (void)monitor;
    return kry_libdraw_height;
}
float GetFrameTime(void) { return kry_libdraw_frame_time; }
int GetFPS(void)
{
    if(kry_libdraw_fps > 0)
        return kry_libdraw_fps;
    if(kry_libdraw_frame_time > 0.0f)
        return (int)(1.0f / kry_libdraw_frame_time + 0.5f);
    return 0;
}
double GetTime(void) { return now_seconds(); }
Vector2 GetWindowScaleDPI(void) { return (Vector2){1.0f, 1.0f}; }
void WaitTime(double seconds)
{
    if(seconds > 0.0)
#ifdef KRYON_NATIVE_PLAN9
        sleep((long)(seconds * 1000.0));
#else
        usleep((unsigned int)(seconds * 1000000.0));
#endif
}

void BeginDrawing(void)
{
    kry_libdraw_poll();
}

static void sw_clear(Color color) { g_sw_backend->clear(pack(color)); }
static void sw_rect(int x, int y, int w, int h, Color color)
{
    g_sw_backend->rect(x, y, w, h, pack(color));
}

static int
imin(int a, int b)
{
    return a < b ? a : b;
}

static int
rounded_radius(Rectangle rec, float roundness)
{
    int w = abs_int((int)rec.width);
    int h = abs_int((int)rec.height);
    int m = imin(w, h);
    int r;

    if(roundness <= 0.0f || m <= 0)
        return 0;
    if(roundness > 1.0f)
        roundness = 1.0f;
    r = (int)((float)m * roundness * 0.5f + 0.5f);
    if(r < 1)
        r = 1;
    if(r > m / 2)
        r = m / 2;
    return r;
}

static int
inside_rounded_rect(int px, int py, int w, int h, int r)
{
    int cx;
    int cy;
    int dx;
    int dy;

    if(px < 0 || py < 0 || px >= w || py >= h)
        return 0;
    if(r <= 0)
        return 1;
    if((px >= r && px < w - r) || (py >= r && py < h - r))
        return 1;
    cx = px < r ? r : w - r - 1;
    cy = py < r ? r : h - r - 1;
    dx = px - cx;
    dy = py - cy;
    return dx * dx + dy * dy <= r * r;
}

static void
draw_rounded_rect_pixels(Rectangle rec, float roundness, Color color)
{
    int x0 = (int)rec.x;
    int y0 = (int)rec.y;
    int w = abs_int((int)rec.width);
    int h = abs_int((int)rec.height);
    int r = rounded_radius(rec, roundness);
    int y;

    if(w <= 0 || h <= 0)
        return;
    if(r <= 0) {
        sw_rect(x0, y0, w, h, color);
        return;
    }
    for(y = 0; y < h; y++) {
        int left = 0;
        int right = w - 1;

        if(y < r || y >= h - r) {
            int cy = y < r ? r : h - r - 1;
            int dy = y - cy;
            int span = (int)sqrtf((float)(r * r - dy * dy));

            left = r - span;
            right = w - r - 1 + span;
        }
        sw_rect(x0 + left, y0 + y, right - left + 1, 1, color);
    }
}

static void
draw_rounded_rect_outline_pixels(Rectangle rec, float roundness, int thick,
                                 Color color)
{
    int x0 = (int)rec.x;
    int y0 = (int)rec.y;
    int w = abs_int((int)rec.width);
    int h = abs_int((int)rec.height);
    int r = rounded_radius(rec, roundness);
    int inner_w;
    int inner_h;
    int inner_r;
    int x;
    int y;

    if(w <= 0 || h <= 0)
        return;
    if(thick < 1)
        thick = 1;
    if(thick * 2 >= w || thick * 2 >= h) {
        draw_rounded_rect_pixels(rec, roundness, color);
        return;
    }
    inner_w = w - thick * 2;
    inner_h = h - thick * 2;
    inner_r = r - thick;
    if(inner_r < 0)
        inner_r = 0;
    for(y = 0; y < h; y++) {
        for(x = 0; x < w; x++) {
            if(!inside_rounded_rect(x, y, w, h, r))
                continue;
            if(inside_rounded_rect(x - thick, y - thick, inner_w, inner_h,
                                   inner_r))
                continue;
            sw_rect(x0 + x, y0 + y, 1, 1, color);
        }
    }
}

static Color
tinted_color_from_rgba(const unsigned char *src, Color tint, int mask)
{
    Color color;

    if(mask) {
        color.r = tint.r;
        color.g = tint.g;
        color.b = tint.b;
    } else {
        color.r = (unsigned char)((src[0] * (int)tint.r) / 255);
        color.g = (unsigned char)((src[1] * (int)tint.g) / 255);
        color.b = (unsigned char)((src[2] * (int)tint.b) / 255);
    }
    color.a = (unsigned char)((src[3] * (int)tint.a) / 255);
    return color;
}

static int
clip_rect_to_sw(KrySw *sw, int *x, int *y, int *w, int *h)
{
    int i;
    int x1;
    int y1;

    if(sw == NULL || x == NULL || y == NULL || w == NULL || h == NULL ||
       *w <= 0 || *h <= 0)
        return 0;
    x1 = *x + *w;
    y1 = *y + *h;
    for(i = 0; i < sw->clip_n; i++) {
        int nx = *x > sw->clip_x[i] ? *x : sw->clip_x[i];
        int ny = *y > sw->clip_y[i] ? *y : sw->clip_y[i];
        int nx2 = x1 < sw->clip_x[i] + sw->clip_w[i] ?
                  x1 : sw->clip_x[i] + sw->clip_w[i];
        int ny2 = y1 < sw->clip_y[i] + sw->clip_h[i] ?
                  y1 : sw->clip_y[i] + sw->clip_h[i];

        if(nx2 <= nx || ny2 <= ny)
            return 0;
        *x = nx;
        *y = ny;
        x1 = nx2;
        y1 = ny2;
    }
    if(*x < 0)
        *x = 0;
    if(*y < 0)
        *y = 0;
    if(x1 > sw->w)
        x1 = sw->w;
    if(y1 > sw->h)
        y1 = sw->h;
    if(x1 <= *x || y1 <= *y)
        return 0;
    *w = x1 - *x;
    *h = y1 - *y;
    return 1;
}

static void
blend_pixel_rgba(unsigned char *dst, Color c)
{
    unsigned inv;

    if(dst == NULL || c.a == 0)
        return;
    if(c.a == 255) {
        dst[0] = c.r;
        dst[1] = c.g;
        dst[2] = c.b;
        dst[3] = c.a;
        return;
    }
    inv = 255u - c.a;
    dst[0] = (unsigned char)(((unsigned)c.r * c.a + dst[0] * inv) / 255u);
    dst[1] = (unsigned char)(((unsigned)c.g * c.a + dst[1] * inv) / 255u);
    dst[2] = (unsigned char)(((unsigned)c.b * c.a + dst[2] * inv) / 255u);
    dst[3] = (unsigned char)(c.a + dst[3] * inv / 255u);
}

static int
draw_texture_unrotated_pixels(KryLibdrawTexture *t, Rectangle source,
                              Rectangle dest, Vector2 origin, Color tint,
                              int mask)
{
    KrySw *sw = g_active_sw;
    int src_w;
    int src_h;
    int dst_x;
    int dst_y;
    int dst_w;
    int dst_h;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;
    int flip_x;
    int flip_y;
    int y;

    if(sw == NULL || sw->pixels == NULL || t == NULL || t->rgba == NULL)
        return 0;
    if(tint.a == 0)
        return 1;
    src_w = (int)ceilf(fabsf(source.width));
    src_h = (int)ceilf(fabsf(source.height));
    dst_w = abs_int((int)dest.width);
    dst_h = abs_int((int)dest.height);
    if(src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0)
        return 1;
    dst_x = (int)(dest.x - origin.x);
    dst_y = (int)(dest.y - origin.y);
    clip_x = dst_x;
    clip_y = dst_y;
    clip_w = dst_w;
    clip_h = dst_h;
    if(!clip_rect_to_sw(sw, &clip_x, &clip_y, &clip_w, &clip_h))
        return 1;
    KrySwMarkDirty(sw, clip_x, clip_y, clip_w, clip_h);
    flip_x = source.width < 0.0f;
    flip_y = source.height < 0.0f;
    for(y = 0; y < clip_h; y++) {
        int py = clip_y + y;
        int rel_y = py - dst_y;
        int region_y = rel_y * src_h / dst_h;
        float ty = ((float)region_y + 0.5f) / (float)src_h;
        float src_yf = flip_y ? source.y - ty * fabsf(source.height)
                              : source.y + ty * fabsf(source.height);
        int src_y = (int)floorf(src_yf);
        unsigned char *dst =
            sw->pixels + (size_t)py * sw->stride + clip_x * 4;
        int x;

        if(src_y < 0 || src_y >= t->height) {
            continue;
        }
        for(x = 0; x < clip_w; x++) {
            int px = clip_x + x;
            int rel_x = px - dst_x;
            int region_x = rel_x * src_w / dst_w;
            float tx = ((float)region_x + 0.5f) / (float)src_w;
            float src_xf = flip_x ? source.x - tx * fabsf(source.width)
                                  : source.x + tx * fabsf(source.width);
            int src_x = (int)floorf(src_xf);

            if(src_x >= 0 && src_x < t->width) {
                const unsigned char *src =
                    t->rgba + ((size_t)src_y * t->width + src_x) * 4;
                blend_pixel_rgba(dst, tinted_color_from_rgba(src, tint, mask));
            }
            dst += 4;
        }
    }
    return 1;
}

static void
set_active_sw(KrySw *sw, unsigned texture_id)
{
    if(sw == NULL)
        return;
    g_sw_backend = KrySwBackend(sw);
    g_active_sw = sw;
    g_active_texture_id = texture_id;
}

void ClearBackground(Color color) { sw_clear(color); }
void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    sw_rect(posX, posY, width, height, color);
}
void DrawRectangleRec(Rectangle rec, Color color)
{
    sw_rect((int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, color);
}
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
    sw_rect(posX, posY, width, 1, color);
    sw_rect(posX, posY + height - 1, width, 1, color);
    sw_rect(posX, posY, 1, height, color);
    sw_rect(posX + width - 1, posY, 1, height, color);
}
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    int t = lineThick > 1.0f ? (int)lineThick : 1;
    sw_rect((int)rec.x, (int)rec.y, (int)rec.width, t, color);
    sw_rect((int)rec.x, (int)(rec.y + rec.height) - t, (int)rec.width, t,
            color);
    sw_rect((int)rec.x, (int)rec.y, t, (int)rec.height, color);
    sw_rect((int)(rec.x + rec.width) - t, (int)rec.y, t, (int)rec.height,
            color);
}
void DrawRectangleGradientV(int x, int y, int w, int h, Color top, Color bottom)
{
    int i;

    for(i = 0; i < h; i++) {
        float a = h > 1 ? (float)i / (float)(h - 1) : 0.0f;
        Color c = {(unsigned char)(top.r + (bottom.r - top.r) * a),
                   (unsigned char)(top.g + (bottom.g - top.g) * a),
                   (unsigned char)(top.b + (bottom.b - top.b) * a),
                   (unsigned char)(top.a + (bottom.a - top.a) * a)};
        sw_rect(x, y + i, w, 1, c);
    }
}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments,
                          Color color)
{
    (void)segments;
    draw_rounded_rect_pixels(rec, roundness, color);
}
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments,
                               Color color)
{
    DrawRectangleRoundedLinesEx(rec, roundness, segments, 1.0f, color);
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                                 float lineThick, Color color)
{
    (void)segments;
    draw_rounded_rect_outline_pixels(rec, roundness,
                                     lineThick > 1.0f ? (int)(lineThick + 0.5f)
                                                      : 1,
                                     color);
}
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    if(g_sw_backend->circle != NULL)
        g_sw_backend->circle(centerX, centerY, (int)radius, pack(color));
}
void DrawCircleV(Vector2 center, float radius, Color color)
{
    DrawCircle((int)center.x, (int)center.y, radius, color);
}
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    float inner = radius - 1.0f;

    if(inner < 0.0f)
        inner = 0.0f;
    DrawRing((Vector2){(float)centerX, (float)centerY}, inner, radius,
             0.0f, 360.0f, 32, color);
}
void DrawLine(int x1, int y1, int x2, int y2, Color color)
{
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    for(;;) {
        sw_rect(x1, y1, 1, 1, color);
        if(x1 == x2 && y1 == y2)
            break;
        int e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if(e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}
void DrawLineEx(Vector2 start, Vector2 end, float thick, Color color)
{
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len2 = dx * dx + dy * dy;
    float radius = thick > 1.0f ? thick * 0.5f : 0.5f;
    int minx = (int)floorf((start.x < end.x ? start.x : end.x) - radius);
    int maxx = (int)ceilf((start.x > end.x ? start.x : end.x) + radius);
    int miny = (int)floorf((start.y < end.y ? start.y : end.y) - radius);
    int maxy = (int)ceilf((start.y > end.y ? start.y : end.y) + radius);
    int x;
    int y;

    if(len2 <= 0.0001f) {
        DrawCircle((int)start.x, (int)start.y, radius, color);
        return;
    }
    for(y = miny; y <= maxy; y++) {
        for(x = minx; x <= maxx; x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float t = ((px - start.x) * dx + (py - start.y) * dy) / len2;
            float cx;
            float cy;
            float ddx;
            float ddy;

            if(t < 0.0f)
                t = 0.0f;
            else if(t > 1.0f)
                t = 1.0f;
            cx = start.x + dx * t;
            cy = start.y + dy * t;
            ddx = px - cx;
            ddy = py - cy;
            if(ddx * ddx + ddy * ddy <= radius * radius)
                sw_rect(x, y, 1, 1, color);
        }
    }
}
void DrawRing(Vector2 center, float innerRadius, float outerRadius,
              float startAngle, float endAngle, int segments, Color color)
{
    (void)startAngle;
    (void)endAngle;
    (void)segments;
    if(g_sw_backend->ring != NULL)
        g_sw_backend->ring((int)center.x, (int)center.y, (int)innerRadius,
                           (int)outerRadius, pack(color));
}
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    float minx = v1.x < v2.x ? (v1.x < v3.x ? v1.x : v3.x)
                             : (v2.x < v3.x ? v2.x : v3.x);
    float maxx = v1.x > v2.x ? (v1.x > v3.x ? v1.x : v3.x)
                             : (v2.x > v3.x ? v2.x : v3.x);
    float miny = v1.y < v2.y ? (v1.y < v3.y ? v1.y : v3.y)
                             : (v2.y < v3.y ? v2.y : v3.y);
    float maxy = v1.y > v2.y ? (v1.y > v3.y ? v1.y : v3.y)
                             : (v2.y > v3.y ? v2.y : v3.y);
    float area = (v2.x - v1.x) * (v3.y - v1.y) -
                 (v2.y - v1.y) * (v3.x - v1.x);
    int x;
    int y;

    if(area == 0.0f)
        return;
    for(y = (int)floorf(miny); y <= (int)ceilf(maxy); y++) {
        for(x = (int)floorf(minx); x <= (int)ceilf(maxx); x++) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float a = ((v2.x - px) * (v3.y - py) -
                       (v2.y - py) * (v3.x - px)) / area;
            float b = ((v3.x - px) * (v1.y - py) -
                       (v3.y - py) * (v1.x - px)) / area;
            float c = 1.0f - a - b;

            if(a >= 0.0f && b >= 0.0f && c >= 0.0f)
                sw_rect(x, y, 1, 1, color);
        }
    }
}
void BeginScissorMode(int x, int y, int width, int height)
{
    g_sw_backend->clip_push(x, y, width, height);
}
void EndScissorMode(void) { g_sw_backend->clip_pop(); }
void BeginMode2D(Camera2D camera) { (void)camera; }
void EndMode2D(void) {}

bool BackendRaw_IsKeyPressed(int key)
{
    return key >= 0 && key < KRY_LIBDRAW_KEY_CAP && kry_libdraw_key_pressed[key];
}
bool BackendRaw_IsKeyPressedRepeat(int key)
{
    return BackendRaw_IsKeyPressed(key);
}
bool BackendRaw_IsKeyDown(int key)
{
    return key >= 0 && key < KRY_LIBDRAW_KEY_CAP && kry_libdraw_key_down[key];
}
bool BackendRaw_IsKeyReleased(int key)
{
    return key >= 0 && key < KRY_LIBDRAW_KEY_CAP && kry_libdraw_key_released[key];
}
int BackendRaw_GetKeyPressed(void)
{
    if(kry_libdraw_key_qr == kry_libdraw_key_qw)
        return 0;
    return kry_libdraw_key_queue[kry_libdraw_key_qr++ % 64];
}
int BackendRaw_GetCharPressed(void)
{
    if(kry_libdraw_char_qr == kry_libdraw_char_qw)
        return 0;
    return kry_libdraw_char_queue[kry_libdraw_char_qr++ % 128];
}
const char *GetKeyName(int key)
{
    static char name[16];

    if(key >= 32 && key <= 126) {
        name[0] = (char)key;
        name[1] = '\0';
        return name;
    }
    switch(key) {
    case KEY_SPACE: return "space";
    case KEY_ESCAPE: return "escape";
    case KEY_ENTER: return "enter";
    case KEY_TAB: return "tab";
    case KEY_BACKSPACE: return "backspace";
    case KEY_INSERT: return "insert";
    case KEY_DELETE: return "delete";
    case KEY_RIGHT: return "right";
    case KEY_LEFT: return "left";
    case KEY_DOWN: return "down";
    case KEY_UP: return "up";
    case KEY_PAGE_UP: return "page up";
    case KEY_PAGE_DOWN: return "page down";
    case KEY_HOME: return "home";
    case KEY_END: return "end";
    default:
        if(key >= KEY_F1 && key <= KEY_F12) {
            snprintf(name, sizeof(name), "f%d", key - KEY_F1 + 1);
            return name;
        }
        return "";
    }
}
bool BackendRaw_IsMouseButtonPressed(int button)
{
    return button >= 0 && button < 3 && kry_libdraw_mouse_pressed[button];
}
bool BackendRaw_IsMouseButtonDown(int button)
{
    return button >= 0 && button < 3 && kry_libdraw_mouse_down[button];
}
bool BackendRaw_IsMouseButtonReleased(int button)
{
    return button >= 0 && button < 3 && kry_libdraw_mouse_released[button];
}
bool BackendRaw_IsMouseButtonUp(int button)
{
    return !BackendRaw_IsMouseButtonDown(button);
}
int BackendRaw_GetMouseX(void)
{
    return kry_libdraw_mouse_x;
}
int BackendRaw_GetMouseY(void)
{
    return kry_libdraw_mouse_y;
}
Vector2 BackendRaw_GetMousePosition(void)
{
    return (Vector2){(float)BackendRaw_GetMouseX(),
                     (float)BackendRaw_GetMouseY()};
}
Vector2 BackendRaw_GetMouseDelta(void)
{
    return (Vector2){(float)kry_libdraw_mouse_dx, (float)kry_libdraw_mouse_dy};
}
float BackendRaw_GetMouseWheelMove(void)
{
    return (float)kry_libdraw_wheel;
}
Vector2 BackendRaw_GetMouseWheelMoveV(void)
{
    return (Vector2){0.0f, BackendRaw_GetMouseWheelMove()};
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    Image img = {0};

    (void)fileType;
    img.data = kry_libdraw_png_rgba(fileData, dataSize, &img.width, &img.height);
#ifndef KRYON_NATIVE_PLAN9
    if(img.data == NULL)
        img.data = kry_decode_image_rgba(fileData, dataSize, &img.width,
                                         &img.height);
#endif
    if(img.data != NULL) {
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    }
    return img;
}
Image LoadImage(const char *fileName)
{
    int len = 0;
    unsigned char *data = read_file(fileName, &len);
    const char *ext = fileName != NULL ? strrchr(fileName, '.') : NULL;
    Image img = LoadImageFromMemory(ext != NULL ? ext : "", data, len);
    free(data);
    return img;
}
Image GenImageColor(int width, int height, Color color)
{
    Image img = {0};
    int i;
    unsigned char *p;

    if(width <= 0 || height <= 0)
        return img;
    p = malloc((size_t)width * height * 4);
    if(p == NULL)
        return img;
    for(i = 0; i < width * height; i++) {
        p[i * 4 + 0] = color.r;
        p[i * 4 + 1] = color.g;
        p[i * 4 + 2] = color.b;
        p[i * 4 + 3] = color.a;
    }
    img.data = p;
    img.width = width;
    img.height = height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}
void UnloadImage(Image image) { free(image.data); }
void ImageFormat(Image *image, int newFormat)
{
    if(image != NULL)
        image->format = newFormat;
}
void ImageFlipVertical(Image *image)
{
    int y;
    unsigned char *tmp;

    if(image == NULL || image->data == NULL || image->height <= 1)
        return;
    tmp = malloc((size_t)image->width * 4);
    if(tmp == NULL)
        return;
    for(y = 0; y < image->height / 2; y++) {
        unsigned char *a = (unsigned char *)image->data + (size_t)y * image->width * 4;
        unsigned char *b = (unsigned char *)image->data +
                           (size_t)(image->height - 1 - y) * image->width * 4;
        memcpy(tmp, a, (size_t)image->width * 4);
        memcpy(a, b, (size_t)image->width * 4);
        memcpy(b, tmp, (size_t)image->width * 4);
    }
    free(tmp);
}
Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex = {0};
    unsigned char *copy;

    if(image.data == NULL || image.width <= 0 || image.height <= 0)
        return tex;
    copy = malloc((size_t)image.width * image.height * 4);
    if(copy == NULL)
        return tex;
    memcpy(copy, image.data, (size_t)image.width * image.height * 4);
    tex.id = kry_libdraw_texture_register(nil, copy, image.width, image.height, 1, 0);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = image.format;
    return tex;
}
Texture2D LoadTexture(const char *fileName)
{
    Image img = LoadImage(fileName);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}
void UnloadTexture(Texture2D texture)
{
    kry_libdraw_texture_unregister(texture.id);
}
void UpdateTexture(Texture2D texture, const void *pixels)
{
    KryLibdrawTexture *t = kry_libdraw_texture(texture.id);

    if(t == NULL || t->rgba == NULL || pixels == NULL ||
       t->width <= 0 || t->height <= 0)
        return;
    memcpy(t->rgba, pixels, (size_t)t->width * t->height * 4);
    t->mask = !t->render_target &&
              texture_pixels_are_mask(t->rgba, t->width, t->height);
}
void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels)
{
    KryLibdrawTexture *t = kry_libdraw_texture(texture.id);
    const unsigned char *src = pixels;
    int x;
    int y;
    int w;
    int h;

    if(t == NULL || t->rgba == NULL || src == NULL ||
       t->width <= 0 || t->height <= 0)
        return;
    x = (int)rec.x;
    y = (int)rec.y;
    w = (int)rec.width;
    h = (int)rec.height;
    if(x < 0 || y < 0 || w <= 0 || h <= 0 ||
       x + w > t->width || y + h > t->height)
        return;
    for(int row = 0; row < h; row++) {
        memcpy(t->rgba + ((size_t)(y + row) * t->width + x) * 4,
               src + (size_t)row * w * 4, (size_t)w * 4);
    }
    t->mask = !t->render_target &&
              texture_pixels_are_mask(t->rgba, t->width, t->height);
}
RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D rt = {0};
    Image img = GenImageColor(width, height, BLANK);
    KryLibdrawTexture *t;

    rt.texture = LoadTextureFromImage(img);
    rt.id = rt.texture.id;
    t = kry_libdraw_texture(rt.texture.id);
    if(t != NULL) {
        t->render_target = 1;
        if(KrySwInit(&t->sw, t->rgba, t->width, t->height) == 0)
            t->sw_ready = 1;
    }
    UnloadImage(img);
    return rt;
}
void UnloadRenderTexture(RenderTexture2D target)
{
    UnloadTexture(target.texture);
}
void BeginTextureMode(RenderTexture2D target)
{
    KryLibdrawTexture *t = kry_libdraw_texture(target.texture.id);

    if(t == NULL || t->rgba == NULL || t->width <= 0 || t->height <= 0)
        return;
    if(!t->sw_ready) {
        if(KrySwInit(&t->sw, t->rgba, t->width, t->height) != 0)
            return;
        t->sw_ready = 1;
    }
    if(g_target_depth < 16)
        g_target_stack[g_target_depth++] = (int)g_active_texture_id;
    set_active_sw(&t->sw, t->id);
}
void EndTextureMode(void)
{
    unsigned restore_id;
    KryLibdrawTexture *t;

    if(g_target_depth <= 0)
        return;
    restore_id = (unsigned)g_target_stack[--g_target_depth];
    if(restore_id == 0) {
        set_active_sw(&g_sw, 0);
        return;
    }
    t = kry_libdraw_texture(restore_id);
    if(t != NULL && t->sw_ready)
        set_active_sw(&t->sw, restore_id);
    else
        set_active_sw(&g_sw, 0);
}
Image LoadImageFromTexture(Texture2D texture)
{
    KryLibdrawTexture *t = kry_libdraw_texture(texture.id);
    Image img = {0};

    if(t == NULL || t->rgba == NULL)
        return img;
    img.data = malloc((size_t)t->width * t->height * 4);
    if(img.data == NULL)
        return img;
    memcpy(img.data, t->rgba, (size_t)t->width * t->height * 4);
    img.width = t->width;
    img.height = t->height;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return img;
}
bool ExportImage(Image image, const char *fileName)
{
    return image.data != NULL && fileName != NULL &&
           kry_libdraw_write_png(fileName, image.data, image.width, image.height) == 0;
}

int
kry_backend_capture_screen(Image *img)
{
    if(img == NULL || !g_sw_ready)
        return -1;
    memset(img, 0, sizeof(*img));
    img->data = malloc((size_t)g_sw.w * g_sw.h * 4);
    if(img->data == NULL)
        return -1;
    memcpy(img->data, g_sw.pixels, (size_t)g_sw.w * g_sw.h * 4);
    img->width = g_sw.w;
    img->height = g_sw.h;
    img->mipmaps = 1;
    img->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return 0;
}
void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture, (Rectangle){0, 0, (float)texture.width,
                                        (float)texture.height},
                   (Rectangle){(float)posX, (float)posY,
                               (float)texture.width, (float)texture.height},
                   (Vector2){0.0f, 0.0f}, 0.0f, tint);
}
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    KryLibdrawTexture *t = kry_libdraw_texture(texture.id);
    unsigned char *region;
    Color draw_tint;
    int mask;
    int sw;
    int sh;
    int flip_x;
    int flip_y;
    int x;
    int y;

    if(t == NULL || t->rgba == NULL || g_sw_backend->texture_rgba == NULL)
        return;

    mask = t->mask && !t->preserve_colors;
    draw_tint = t->preserve_colors ? (Color){255, 255, 255, tint.a} : tint;
    sw = (int)ceilf(fabsf(source.width));
    sh = (int)ceilf(fabsf(source.height));
    flip_x = source.width < 0.0f;
    flip_y = source.height < 0.0f;
    if(sw <= 0 || sh <= 0 || (int)dest.width == 0 || (int)dest.height == 0)
        return;

    if(rotation > -0.0001f && rotation < 0.0001f &&
       draw_texture_unrotated_pixels(t, source, dest, origin, draw_tint, mask))
        return;

    region = malloc((size_t)sw * sh * 4);
    if(region == NULL)
        return;
    for(y = 0; y < sh; y++) {
        float ty = ((float)y + 0.5f) / (float)sh;
        float src_yf = flip_y ? source.y - ty * fabsf(source.height)
                              : source.y + ty * fabsf(source.height);
        int src_y = (int)floorf(src_yf);

        for(x = 0; x < sw; x++) {
            float tx = ((float)x + 0.5f) / (float)sw;
            float src_xf = flip_x ? source.x - tx * fabsf(source.width)
                                  : source.x + tx * fabsf(source.width);
            int src_x = (int)floorf(src_xf);
            unsigned char *dst = region + ((size_t)y * sw + x) * 4;

            if(src_x < 0 || src_y < 0 || src_x >= t->width ||
               src_y >= t->height) {
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = 0;
            } else {
                const unsigned char *src =
                    t->rgba + ((size_t)src_y * t->width + src_x) * 4;

                dst[0] = mask ? 255 : src[0];
                dst[1] = mask ? 255 : src[1];
                dst[2] = mask ? 255 : src[2];
                dst[3] = src[3];
            }
        }
    }
    if(rotation > -0.0001f && rotation < 0.0001f) {
        g_sw_backend->texture_rgba(region, sw, sh,
                                   (int)(dest.x - origin.x),
                                   (int)(dest.y - origin.y),
                                   abs_int((int)dest.width),
                                   abs_int((int)dest.height),
                                   pack(draw_tint));
    } else {
        float dw = (float)abs_int((int)dest.width);
        float dh = (float)abs_int((int)dest.height);
        float rad = rotation * 0.017453292519943295769f;
        float cs = cosf(rad);
        float sn = sinf(rad);
        float corners[4][2] = {
            {-origin.x, -origin.y},
            {dw - origin.x, -origin.y},
            {dw - origin.x, dh - origin.y},
            {-origin.x, dh - origin.y},
        };
        float minx = 1.0e30f;
        float miny = 1.0e30f;
        float maxx = -1.0e30f;
        float maxy = -1.0e30f;
        int i;
        int ix0;
        int iy0;
        int ix1;
        int iy1;

        for(i = 0; i < 4; i++) {
            float wx = dest.x + corners[i][0] * cs - corners[i][1] * sn;
            float wy = dest.y + corners[i][0] * sn + corners[i][1] * cs;

            if(wx < minx)
                minx = wx;
            if(wy < miny)
                miny = wy;
            if(wx > maxx)
                maxx = wx;
            if(wy > maxy)
                maxy = wy;
        }
        ix0 = (int)floorf(minx);
        iy0 = (int)floorf(miny);
        ix1 = (int)ceilf(maxx);
        iy1 = (int)ceilf(maxy);
        for(y = iy0; y <= iy1; y++) {
            for(x = ix0; x <= ix1; x++) {
                float px = (float)x + 0.5f - dest.x;
                float py = (float)y + 0.5f - dest.y;
                float local_x = px * cs + py * sn + origin.x;
                float local_y = -px * sn + py * cs + origin.y;
                int rx;
                int ry;
                const unsigned char *src;
                Color color;

                if(local_x < 0.0f || local_y < 0.0f ||
                   local_x >= dw || local_y >= dh)
                    continue;
                rx = (int)(local_x * (float)sw / dw);
                ry = (int)(local_y * (float)sh / dh);
                if(rx < 0)
                    rx = 0;
                else if(rx >= sw)
                    rx = sw - 1;
                if(ry < 0)
                    ry = 0;
                else if(ry >= sh)
                    ry = sh - 1;
                src = region + ((size_t)ry * sw + rx) * 4;
                color = tinted_color_from_rgba(src, draw_tint, mask);
                sw_rect(x, y, 1, 1, color);
            }
        }
    }
    free(region);
}
void SetTextureFilter(Texture2D texture, int filter)
{
    (void)texture;
    (void)filter;
}

const char *TextFormat(const char *text, ...)
{
    static char buf[4096];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, sizeof(buf), text != NULL ? text : "", ap);
    va_end(ap);
    return buf;
}
void TraceLog(int logLevel, const char *text, ...)
{
    va_list ap;
    va_start(ap, text);
    if(g_trace_log_callback != NULL) {
        g_trace_log_callback(logLevel, text != NULL ? text : "", ap);
    } else {
#ifdef KRYON_NATIVE_PLAN9
        vfprint(2, (char *)(text != NULL ? text : ""), ap);
        fprint(2, "\n");
#else
        vfprintf(stderr, text != NULL ? text : "", ap);
        fputc('\n', stderr);
#endif
    }
    va_end(ap);
}

void SetTraceLogCallback(TraceLogCallback callback)
{
    g_trace_log_callback = callback;
}

void SetShapesTexture(Texture2D texture, Rectangle rec)
{
    (void)texture;
    (void)rec;
}

unsigned char *LoadFileData(const char *fileName, int *dataSize)
{
    return read_file(fileName, dataSize);
}
void UnloadFileData(unsigned char *data) { free(data); }
char *LoadFileText(const char *fileName)
{
    int len = 0;
    return (char *)read_file(fileName, &len);
}
void UnloadFileText(char *text) { free(text); }
bool SaveFileData(const char *fileName, const void *data, int dataSize)
{
#ifdef KRYON_NATIVE_PLAN9
    int fd;
    int ok;

    if(fileName == NULL || data == NULL || dataSize < 0)
        return false;
    fd = create((char *)fileName, OWRITE|OTRUNC, 0666);
    if(fd < 0)
        return false;
    ok = write(fd, (void *)data, dataSize) == dataSize;
    close(fd);
    return ok;
#else
    FILE *f = fopen(fileName, "wb");
    if(f == NULL)
        return false;
    fwrite(data, 1, (size_t)dataSize, f);
    fclose(f);
    return true;
#endif
}
bool SaveFileText(const char *fileName, const char *text)
{
    return SaveFileData(fileName, text, text != NULL ? (int)strlen(text) : 0);
}
bool FileExists(const char *fileName)
{
#ifdef KRYON_PLATFORM_PLAN9
    Dir *d;
    bool regular;

    if(fileName == NULL)
        return false;
    d = dirstat(fileName);
    if(d == NULL)
        return false;
    regular = (d->mode & DMDIR) == 0;
    free(d);
    return regular;
#else
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 && S_ISREG(st.st_mode);
#endif
}
bool DirectoryExists(const char *dirPath)
{
#ifdef KRYON_PLATFORM_PLAN9
    Dir *d;
    bool isdir;

    if(dirPath == NULL)
        return false;
    d = dirstat(dirPath);
    if(d == NULL)
        return false;
    isdir = (d->mode & DMDIR) != 0;
    free(d);
    return isdir;
#else
    struct stat st;
    return dirPath != NULL && stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}
int MakeDirectory(const char *dirPath)
{
    if(dirPath == NULL)
        return -1;
#ifdef KRYON_NATIVE_PLAN9
    {
        int fd = create((char *)dirPath, OREAD, DMDIR|0777);
        if(fd >= 0) {
            close(fd);
            return 0;
        }
    }
    return DirectoryExists(dirPath) ? 0 : -1;
#else
    return (mkdir(dirPath, 0777) == 0 || DirectoryExists(dirPath)) ? 0 : -1;
#endif
}
int ChangeDirectory(const char *dir)
{
    return dir != NULL && chdir(dir) == 0 ? 0 : -1;
}
const char *GetWorkingDirectory(void)
{
    static char buf[1024];
#ifdef KRYON_PLATFORM_PLAN9
    return getwd(buf, sizeof(buf)) != NULL ? buf : "";
#else
    return getcwd(buf, sizeof(buf)) != NULL ? buf : "";
#endif
}
const char *GetApplicationDirectory(void) { return GetWorkingDirectory(); }
const char *GetDirectoryPath(const char *filePath)
{
    static char buf[1024];
    char *slash;
    if(filePath == NULL)
        return "";
    snprintf(buf, sizeof(buf), "%s", filePath);
    slash = strrchr(buf, '/');
    if(slash == NULL)
        return ".";
    *slash = 0;
    return buf;
}
const char *GetFileName(const char *filePath)
{
    const char *slash = filePath != NULL ? strrchr(filePath, '/') : NULL;
    return slash != NULL ? slash + 1 : (filePath != NULL ? filePath : "");
}
const char *GetFileNameWithoutExt(const char *filePath)
{
    static char buf[1024];
    const char *name;
    char *dot;

    name = GetFileName(filePath);
    snprintf(buf, sizeof(buf), "%s", name);
    dot = strrchr(buf, '.');
    if(dot != NULL)
        *dot = '\0';
    return buf;
}
const char *GetFileExtension(const char *fileName)
{
    const char *dot = fileName != NULL ? strrchr(fileName, '.') : NULL;
    return dot != NULL ? dot : "";
}
bool IsFileExtension(const char *fileName, const char *ext)
{
    return ext != NULL && strcmp(GetFileExtension(fileName), ext) == 0;
}
const char *GetClipboardText(void)
{
    return g_clipboard != NULL ? (const char *)g_clipboard : "";
}
void SetClipboardText(const char *text)
{
    free(g_clipboard);
    g_clipboard = dup_text_bytes(text);
}
