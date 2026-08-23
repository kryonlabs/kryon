#ifndef KRYON_LIBDRAW_INTERNAL_H
#define KRYON_LIBDRAW_INTERNAL_H

#include "kry_sw.h"
#include "kryon.h"

#include <stddef.h>

#define Point P9Point
#define Rectangle P9Rectangle
#define Image P9Image
#define Font P9Font
#define Screen P9Screen
#define Display P9Display
#define Mouse P9Mouse
#define Event P9Event
#define Cursor P9Cursor
#define Cursor2 P9Cursor2
#ifdef Rect
#undef Rect
#endif
#define Rect P9Rect
#define Menu P9Menu
#ifdef PI
#undef PI
#endif
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#undef Menu
#undef Rect
#undef Cursor2
#undef Cursor
#undef Event
#undef Mouse
#undef Display
#undef Screen
#undef Font
#undef Image
#undef Rectangle
#undef Point
#ifdef PI
#undef PI
#endif

typedef struct KryLibdrawTexture KryLibdrawTexture;
typedef struct KryLibdrawFont KryLibdrawFont;

struct KryLibdrawTexture {
    unsigned id;
    P9Image *image;
    unsigned char *rgba;
    int width;
    int height;
    int owned_rgba;
    int render_target;
    int mask;
    KrySw sw;
    int sw_ready;
};

struct KryLibdrawFont {
    unsigned id;
    P9Font *font;
    int base_size;
};

extern int kry_libdraw_width;
extern int kry_libdraw_height;
extern int kry_libdraw_ready;
extern int kry_libdraw_should_close;
extern double kry_libdraw_last_time;
extern float kry_libdraw_frame_time;

extern int kry_libdraw_mouse_x;
extern int kry_libdraw_mouse_y;
extern int kry_libdraw_mouse_dx;
extern int kry_libdraw_mouse_dy;
extern int kry_libdraw_mouse_down[3];
extern int kry_libdraw_mouse_pressed[3];
extern int kry_libdraw_mouse_released[3];
extern int kry_libdraw_wheel;

extern int kry_libdraw_key_down[512];
extern int kry_libdraw_key_pressed[512];
extern int kry_libdraw_key_released[512];
extern int kry_libdraw_key_queue[64];
extern int kry_libdraw_key_qr;
extern int kry_libdraw_key_qw;
extern int kry_libdraw_char_queue[128];
extern int kry_libdraw_char_qr;
extern int kry_libdraw_char_qw;

extern P9Image *kry_libdraw_target;

P9Rectangle kry_p9_rect(int x, int y, int w, int h);
P9Point kry_p9_point(int x, int y);
P9Image *kry_libdraw_color(Color color);
P9Image *kry_libdraw_color_u32(unsigned rgba);
void kry_libdraw_flush(void);
void kry_libdraw_poll(void);
void kry_libdraw_reset_edges(void);
void kry_libdraw_push_key(int key);
void kry_libdraw_push_char(int ch);
int kry_libdraw_map_key(int ch);
int kry_backend_capture_screen(Image *image);

unsigned kry_libdraw_texture_register(P9Image *image, unsigned char *rgba,
                                      int width, int height, int owned_rgba,
                                      int render_target);
KryLibdrawTexture *kry_libdraw_texture(unsigned id);
void kry_libdraw_texture_unregister(unsigned id);
unsigned char *kry_libdraw_png_rgba(const unsigned char *data, int len,
                                    int *width, int *height);
int kry_libdraw_write_png(const char *path, const unsigned char *rgba,
                          int width, int height);

unsigned kry_libdraw_font_register(P9Font *font, int base_size);
KryLibdrawFont *kry_libdraw_font(unsigned id);
void kry_libdraw_font_unregister(unsigned id);

#endif
