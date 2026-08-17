#ifndef KRY_SW_H
#define KRY_SW_H

/*
 * kry_sw — portable software rasterizer behind the KryBackend vtable.
 * Renders KRB cartridges into a caller-visible pixel buffer with no GPU,
 * no libc beyond memcpy/malloc. Targets RGBA8 (host byte order R,G,B,A)
 * with an RGB565 conversion for embedded panels.
 *
 * Text uses a built-in public-domain 8x8 bitmap font scaled to the
 * requested size; the pre-baked glyph-atlas pipeline (plan 11, phase 6)
 * replaces this later without touching the interface.
 */

#include "kry_backend.h"

typedef struct KrySwAtlasSize {
    int px;
    int glyphs;
    int w;
    int h;
    unsigned table_off;
    unsigned pixels_off;
} KrySwAtlasSize;

typedef struct KrySw {
    unsigned char *pixels; /* RGBA8, stride * h bytes */
    int w;
    int h;
    int stride; /* bytes per row, >= w * 4 */
    int owns;   /* pixels were malloc'd here */
    int clip_n;
    int clip_x[16];
    int clip_y[16];
    int clip_w[16];
    int clip_h[16];
    int mouse_x;
    int mouse_y;
    int buttons_down;
    int buttons_pressed;
    int wheel; /* pending wheel delta, consumed by the backend poll */
    unsigned keys[16]; /* typed codepoints (0x08 = backspace) */
    int keys_n;
    float now;
    int scale; /* UI scale per mille, 1000 = 1:1 */
    unsigned theme[KRY_THEME_COUNT];
    /* KFA1 atlas */
    const unsigned char *atlas;
    unsigned atlas_len;
    int atlas_sizes;
    KrySwAtlasSize size_tab[8];
    KryBackend backend;
} KrySw;

/* pixels == NULL allocates an internal w*h*4 buffer (free with KrySwFree). */
int KrySwInit(KrySw *sw, unsigned char *pixels, int w, int h);
void KrySwFree(KrySw *sw);

const KryBackend *KrySwBackend(KrySw *sw);

/* Input injection for hosts/tests: move the pointer, latch a press. */
void KrySwMouse(KrySw *sw, int x, int y);
void KrySwButtonDown(KrySw *sw, int button);
void KrySwButtonUp(KrySw *sw, int button);
void KrySwWheel(KrySw *sw, int dy);
void KrySwText(KrySw *sw, unsigned codepoint);
void KrySwAdvance(KrySw *sw, float dt); /* clears pressed edges, ticks time */

void KrySwSetTheme(KrySw *sw, int slot, unsigned rgba);

/* Dirty rectangle. Currently reports the full frame; per-call tracking
 * lands with the framebuffer host (plan 11, phase 2). */
void KrySwDirty(KrySw *sw, int *x, int *y, int *w, int *h);

/* Install a KFA1 glyph atlas (cartridge asset kind 1). Text draws from
 * atlas glyphs with proper advances; without one the built-in 8x8 font
 * is used. */
int KrySwSetAtlas(KrySw *sw, const unsigned char *data, unsigned len);

/* Convert the RGBA8 target to dst (w*h uint16, RGB565). */
void KrySwToRGB565(const KrySw *sw, unsigned short *dst);

#endif
