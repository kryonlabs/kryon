#ifndef KRY_BACKEND_H
#define KRY_BACKEND_H

/*
 * Small draw/input table for cartridges and non-raylib hosts.
 * The public app API (DrawRectangle, GetMousePosition, ...) stays as it is.
 * A .krb walker only calls this table so raylib, the null backend, a
 * framebuffer, or /dev/draw can host the same image.
 */

enum {
    KRY_THEME_BACKGROUND = 0,
    KRY_THEME_TEXT = 1,
    KRY_THEME_ICON = 2,
    KRY_THEME_SURFACE = 3,
    KRY_THEME_BUTTON = 4,
    KRY_THEME_COUNT
};

enum {
    KRY_MOUSE_LEFT = 0,
    KRY_MOUSE_RIGHT = 1,
    KRY_MOUSE_MIDDLE = 2
};

typedef struct KryBackend {
    void (*clear)(unsigned color);
    void (*rect)(int x, int y, int w, int h, unsigned color);
    void (*text)(const char *s, int x, int y, int size, unsigned color);
    int (*measure_text)(const char *s, int size);
    void (*clip_push)(int x, int y, int w, int h);
    void (*clip_pop)(void);
    void (*mouse)(int *x, int *y);
    int (*mouse_down)(int button);
    int (*mouse_pressed)(int button);
    int (*width)(void);
    int (*height)(void);
    float (*time)(void);
    int (*scale_px)(int px);
    unsigned (*theme_color)(int slot);
} KryBackend;

extern const KryBackend KryBackendDraw;
extern const KryBackend KryBackendNull;

const KryBackend *KryBackendCurrent(void);
void KryBackendSelect(const KryBackend *backend);

#endif
