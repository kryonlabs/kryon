/**
 * Plan 9 Backend - Internal Structures
 *
 * Internal data structures for the Plan 9/9front rendering backend
 * Uses libdraw for graphics and libevent for input handling
 */

#ifndef PLAN9_INTERNAL_H
#define PLAN9_INTERNAL_H

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include "../../ir/include/ir_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum cached color images */
#define PLAN9_MAX_COLOR_CACHE 256

/* Maximum cached fonts */
#define PLAN9_MAX_FONT_CACHE 32

/* Color cache entry */
typedef struct {
    ulong rgba;        /* RGBA color value */
    Image* image;      /* Cached 1x1 replicated image */
} Plan9ColorCacheEntry;

/* Font cache entry */
typedef struct {
    char* path;        /* Font path */
    int size;          /* Font size */
    Font* font;        /* Loaded font */
} Plan9FontCacheEntry;

/* Plan 9 renderer backend data */
typedef struct {
    /* Display and window state */
    Display* display;           /* libdraw display connection */
    Image* screen;              /* Screen image */
    Font* default_font;         /* Default font */

    /* Resource caches */
    Plan9ColorCacheEntry color_cache[PLAN9_MAX_COLOR_CACHE];
    int color_cache_count;

    Plan9FontCacheEntry font_cache[PLAN9_MAX_FONT_CACHE];
    int font_cache_count;

    /* Rendering state */
    int running;                /* Main loop running flag */
    int needs_redraw;           /* Redraw requested flag */
    Rectangle clip_rect;        /* Current clipping rectangle */

    /* Performance tracking */
    vlong frame_count;          /* Total frames rendered */
    vlong last_frame_time;      /* Last frame timestamp (nsec) */
} Plan9RendererData;

/* Helper function declarations */

/* Get or create cached color image */
Image* plan9_get_color_image(Plan9RendererData* data, ulong rgba);

/* Free all cached color images */
void plan9_free_color_cache(Plan9RendererData* data);

/* Execute command buffer */
void plan9_execute_commands(Plan9RendererData* data, void* cmd_buf);

/* Draw rounded rectangle (approximated with arcs) */
void plan9_draw_rounded_rect(Plan9RendererData* data, int x, int y,
                              int w, int h, int radius, ulong color);

/* Convert RGBA to Plan 9 color format */
ulong plan9_rgba_to_plan9(uint32_t rgba);

#ifdef __cplusplus
}
#endif

#endif /* PLAN9_INTERNAL_H */
