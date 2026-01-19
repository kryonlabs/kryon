/**
 * Plan 9 Backend - Font Management
 *
 * Handles font loading, caching, and text measurement
 * Uses Plan 9's bitmap font system
 */

#include "plan9_internal.h"
#include "../desktop/abstract/desktop_fonts.h"
#include "../../ir/include/ir_log.h"

/* Global font cache (shared across all renderer instances) */
static Plan9FontCacheEntry g_font_cache[PLAN9_MAX_FONT_CACHE];
static int g_font_cache_count = 0;

/* Map TrueType font requests to Plan 9 bitmap fonts */
static const char*
map_font_to_plan9(const char* path, int size)
{
    static char fontpath[256];

    /* If it's already a Plan 9 font path, use it directly */
    if (strstr(path, "/lib/font/bit/")) {
        return path;
    }

    /* Map common font families to Plan 9 fonts */
    if (strstr(path, "sans") || strstr(path, "Sans") ||
        strstr(path, "arial") || strstr(path, "Arial")) {
        /* LucidaSans Unicode font */
        if (size <= 8)
            snprint(fontpath, sizeof(fontpath),
                   "/lib/font/bit/lucsans/unicode.8.font");
        else if (size <= 10)
            snprint(fontpath, sizeof(fontpath),
                   "/lib/font/bit/lucsans/unicode.10.font");
        else
            snprint(fontpath, sizeof(fontpath),
                   "/lib/font/bit/lucsans/unicode.13.font");
    } else if (strstr(path, "mono") || strstr(path, "Mono") ||
               strstr(path, "courier") || strstr(path, "Courier")) {
        /* Lucida Mono font */
        snprint(fontpath, sizeof(fontpath),
               "/lib/font/bit/lucm/unicode.9.font");
    } else {
        /* Default to LucidaSans */
        snprint(fontpath, sizeof(fontpath),
               "/lib/font/bit/lucsans/unicode.13.font");
    }

    return fontpath;
}

/* Load font from path and size */
static DesktopFont*
plan9_load_font(const char* path, int size)
{
    Font* font;
    const char* fontpath;
    int i;

    if (!path || !display)
        return nil;

    /* Check cache first */
    for (i = 0; i < g_font_cache_count; i++) {
        if (strcmp(g_font_cache[i].path, path) == 0 &&
            g_font_cache[i].size == size) {
            IR_LOG_DEBUG("PLAN9", "Font cache hit: %s size %d", path, size);
            return (DesktopFont*)g_font_cache[i].font;
        }
    }

    /* Map font path to Plan 9 font */
    fontpath = map_font_to_plan9(path, size);

    /* Load font */
    font = openfont(display, (char*)fontpath);
    if (!font) {
        fprint(2, "plan9: openfont failed for %s: %r\n", fontpath);
        /* Fall back to default font */
        return (DesktopFont*)display->defaultfont;
    }

    /* Add to cache if space available */
    if (g_font_cache_count < PLAN9_MAX_FONT_CACHE) {
        g_font_cache[g_font_cache_count].path = strdup(path);
        g_font_cache[g_font_cache_count].size = size;
        g_font_cache[g_font_cache_count].font = font;
        g_font_cache_count++;
        IR_LOG_DEBUG("PLAN9", "Cached font: %s -> %s", path, fontpath);
    }

    return (DesktopFont*)font;
}

/* Unload font */
static void
plan9_unload_font(DesktopFont* font_handle)
{
    Font* font;
    int i;

    if (!font_handle)
        return;

    font = (Font*)font_handle;

    /* Remove from cache */
    for (i = 0; i < g_font_cache_count; i++) {
        if (g_font_cache[i].font == font) {
            free(g_font_cache[i].path);
            g_font_cache[i] = g_font_cache[g_font_cache_count - 1];
            g_font_cache_count--;
            break;
        }
    }

    /* Free font */
    freefont(font);
}

/* Measure text width */
static float
plan9_measure_text_width(DesktopFont* font_handle, const char* text)
{
    Font* font;
    int width;

    if (!font_handle || !text)
        return 0.0f;

    font = (Font*)font_handle;
    width = stringwidth(font, (char*)text);

    return (float)width;
}

/* Get font height */
static float
plan9_get_font_height(DesktopFont* font_handle)
{
    Font* font;

    if (!font_handle)
        return 0.0f;

    font = (Font*)font_handle;
    return (float)font->height;
}

/* Get font ascent */
static float
plan9_get_font_ascent(DesktopFont* font_handle)
{
    Font* font;

    if (!font_handle)
        return 0.0f;

    font = (Font*)font_handle;
    return (float)font->ascent;
}

/* Get font descent */
static float
plan9_get_font_descent(DesktopFont* font_handle)
{
    Font* font;

    if (!font_handle)
        return 0.0f;

    font = (Font*)font_handle;
    return (float)(font->height - font->ascent);
}

/* Font operations table */
DesktopFontOps g_plan9_font_ops = {
    plan9_load_font,
    plan9_unload_font,
    plan9_measure_text_width,
    plan9_get_font_height,
    plan9_get_font_ascent,
    plan9_get_font_descent
};
