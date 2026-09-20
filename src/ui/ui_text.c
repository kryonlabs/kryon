#include "ui/text_rows.h"
#include "ui/selectable_text.h"
#include "ui/text.h"
#include "ui_text.h"
#include "ui_text_backend.h"
#include "ui_clip.h"
#include "ui_internal.h"
#include "embedded_assets.h"
#include "ui_scaling.h"
#include "ui_style_internal.h"
#include "theme.h"
#include "runtime/paragraph.h"
#include "runtime/text.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "kryon_mem.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Font kryon_zero_font;

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#define TEXT_FONT_MAX_REGISTERED 16
#define TEXT_FONT_DEFAULT_NAME "default"

/* Text whose physical size is at least this multiple of the base raster gets
 * a dedicated large rasterization so headings stay crisp. One large slot per
 * font holds the largest size seen; smaller large-tier sizes scale down. */

typedef struct TextFontEntry {
    char name[32];
    char file_type_buf[8];
    Font font;
    Font small_font;
    const char *file_type;
    const unsigned char *font_data;
    unsigned int font_data_size;
    int owns_font_data;
    int *codepoints;
    int codepoint_count;
    int codepoint_cap;
    /* Source fonts are rasterized per requested physical size into a small
     * tier cache: single-tier scaling made every non-base size (12, 14,
     * 17px...) bilinear-resampled from one 16px atlas, which reads as blurry
     * on dense UI text. The cap keeps memory bounded versus the former
     * unbounded per-size cache; font_size_scale still applies at all
     * measure/draw sites for tier-miss fallbacks. Apps that draw more
     * distinct physical sizes than slots thrash: every frame evicts a tier
     * and re-rasterizes the full codepoint set (~100ms/frame for a ~1000
     * glyph seed on a low-end phone), so keep headroom over the realistic
     * per-screen type scale (a phone UI in one dynamic font easily reaches
     * six sizes once widget-internal label sizes are counted). */
#define TEXT_FONT_MAX_RASTER_TIERS 16
    Font tier_font[TEXT_FONT_MAX_RASTER_TIERS];
    int tier_size[TEXT_FONT_MAX_RASTER_TIERS]; /* 0 = free slot */
} TextFontEntry;

static TextFontEntry g_ui_fonts[TEXT_FONT_MAX_REGISTERED];
static int g_ui_font_count = 0;
static int g_ui_active_font = -1;
static int g_ui_default_font_attempted = 0;
static Font g_ui_italic_font = {0};
static int g_ui_italic_font_attempted = 0;

/* Trace flag resolved once: the draw loops check it per glyph, and a
 * getenv() per glyph is a linear scan of the whole environment block. */
static int g_ui_text_trace = -1;
static int
ui_text_trace_enabled(void)
{
    if(g_ui_text_trace < 0)
        g_ui_text_trace = getenv("KRYON_TEXT_TRACE") != NULL;
    return g_ui_text_trace;
}

static void
ui_draw_text_strikethrough(int x, int y, int width, int height, Color color)
{
    if(text_strikethrough && width > 0)
        DrawRectangleRec(TextStrikethroughBounds((float)x, (float)y,
            (float)width, (float)height, (float)Scale(1000) / 1000.0f), color);
}

static int
font_valid(Font font)
{
    return TextFontReady(font);
}

int
TextFontHasGlyph(Font font, int codepoint)
{
    return TextFontHasGlyphValue(font, codepoint);
}

static int
font_entry_index(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : TEXT_FONT_DEFAULT_NAME;

    for(int i = 0; i < g_ui_font_count; i++) {
        if(strcmp(g_ui_fonts[i].name, key) == 0)
            return i;
    }

    return -1;
}

static int
font_entry_alloc(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : TEXT_FONT_DEFAULT_NAME;
    int index = font_entry_index(key);

    if(index >= 0)
        return index;
    if(g_ui_font_count >= TEXT_FONT_MAX_REGISTERED)
        return -1;

    index = g_ui_font_count++;
    memset(&g_ui_fonts[index], 0, sizeof(g_ui_fonts[index]));
    snprintf(g_ui_fonts[index].name, sizeof(g_ui_fonts[index].name), "%s", key);
    if(g_ui_active_font < 0)
        g_ui_active_font = index;
    return index;
}

static int
font_physical_size(int font_size)
{
    int size = font_size;

    if(size <= 0)
        size = font_size > 0 ? font_size : TextBaseSize;
    if(size <= 0)
        size = TextBaseSize;
    return size;
}

static void
ui_font_trim_heap(void)
{
#if defined(__GLIBC__)
    /* Font rasterization is the biggest transient heap churn in the text
     * path; return the freed pages instead of holding them in the arena. */
    malloc_trim(0);
#endif
}

static void
clear_font_cache(TextFontEntry *entry)
{
    if(entry == NULL)
        return;
    for(int i = 0; i < TEXT_FONT_MAX_RASTER_TIERS; i++) {
        if(font_valid(entry->tier_font[i]))
            UnloadFont(entry->tier_font[i]);
        entry->tier_font[i] = kryon_zero_font;
        entry->tier_size[i] = 0;
    }
}

static void
clear_font_entry(TextFontEntry *entry)
{
    if(entry == NULL)
        return;
    clear_font_cache(entry);
    if(entry->owns_font_data)
        UnloadFileData((unsigned char *)entry->font_data);
    free(entry->codepoints);
    entry->font = kryon_zero_font;
    entry->small_font = kryon_zero_font;
    entry->file_type = NULL;
    entry->file_type_buf[0] = '\0';
    entry->font_data = NULL;
    entry->font_data_size = 0;
    entry->owns_font_data = 0;
    entry->codepoints = NULL;
    entry->codepoint_count = 0;
    entry->codepoint_cap = 0;
}

static int
font_entry_has_codepoint(TextFontEntry *entry, int codepoint)
{
    if(entry == NULL || codepoint <= 0)
        return 0;
    if(entry->font_data == NULL)
        return TextFontHasGlyphValue(entry->font, codepoint) ||
               TextFontHasGlyphValue(entry->small_font, codepoint);
    for(int i = 0; i < entry->codepoint_count; i++) {
        if(entry->codepoints[i] == codepoint)
            return 1;
    }
    return 0;
}

static Font
load_font_source_size(TextFontEntry *entry, int physical_size)
{
    Font font;

#if defined(KRYON_NATIVE_PLAN9)
    if(entry == NULL)
        return kryon_zero_font;
#else
    if(entry == NULL || entry->font_data == NULL || entry->font_data_size == 0)
        return kryon_zero_font;
#endif

    font = LoadFontFromMemory(
        entry->file_type != NULL && entry->file_type[0] != '\0' ? entry->file_type : ".ttf",
        entry->font_data, (int)entry->font_data_size, physical_size,
        entry->codepoints, entry->codepoint_count);

    /* raylib returns the global default font (a 95-glyph ASCII bitmap) when a
     * source cannot be loaded -- unsupported format such as .ttc, no matching
     * glyphs, or corrupt data. Treat that as failure instead of letting the
     * default font masquerade as a valid entry and pollute the per-codepoint
     * fallback chain. A freshly loaded font always owns its own texture. */
    if(font.texture.id == GetFontDefault().texture.id)
        return kryon_zero_font;

    if(font_valid(font))
        SetTextureFilter(TextFontAtlasTexture(font), TEXTURE_FILTER_BILINEAR);
    return font;
}

static Font
entry_source_font_for_size(TextFontEntry *entry, int font_size)
{
    int physical_size = font_physical_size(font_size);

    if(entry == NULL || entry->font_data == NULL || entry->font_data_size == 0)
        return kryon_zero_font;

    /* Exact tier hit: rasterized at this exact physical size, no resampling. */
    for(int i = 0; i < TEXT_FONT_MAX_RASTER_TIERS; i++) {
        if(entry->tier_size[i] == physical_size && font_valid(entry->tier_font[i]))
            return entry->tier_font[i];
    }

    /* Failed earlier at this exact size: never re-attempt. A source that
     * parses at registration but cannot rasterize (unsupported tables,
     * no matching glyphs) would otherwise re-run the full parse for
     * every glyph of every string, pinning a core and freezing the app
     * mid-frame. */
    for(int i = 0; i < TEXT_FONT_MAX_RASTER_TIERS; i++) {
        if(entry->tier_size[i] == -physical_size)
            return kryon_zero_font;
    }

    /* Free slot: rasterize this size. */
    for(int i = 0; i < TEXT_FONT_MAX_RASTER_TIERS; i++) {
        if(entry->tier_size[i] == 0) {
            entry->tier_font[i] = load_font_source_size(entry, physical_size);
            if(font_valid(entry->tier_font[i])) {
                /* Record the size the rasterizer actually produced. TTF
                 * tiers come back at exactly physical_size; the plan9
                 * bitmap buckets come back at their nearest available
                 * height, so recording the real base size lets nearby
                 * requests share one bucket tier. */
                int raster_size = TextFontBaseSize(entry->tier_font[i]);

                entry->tier_size[i] =
                    raster_size > 0 ? raster_size : physical_size;
                ui_font_trim_heap();
                return entry->tier_font[i];
            }
            /* Latch the failure so this size is never rasterized again. */
            entry->tier_font[i] = kryon_zero_font;
            entry->tier_size[i] = -physical_size;
            return kryon_zero_font;
        }
    }

    /* A draw must never unload an atlas that earlier glyphs in the same
     * render batch still reference. When all exact-size slots are occupied,
     * reuse the nearest raster and scale it. New tiers are created only in
     * free slots; dirty atlases rebuild at the next frame boundary. */
    {
        int victim = 0;
        int nearest = 0x7fffffff;

        for(int i = 0; i < TEXT_FONT_MAX_RASTER_TIERS; i++) {
            int dist = entry->tier_size[i] > physical_size
                       ? entry->tier_size[i] - physical_size
                       : physical_size - entry->tier_size[i];

            if(dist < nearest && font_valid(entry->tier_font[i])) {
                nearest = dist;
                victim = i;
            }
        }
        if(nearest != 0x7fffffff)
            return entry->tier_font[victim];
        return kryon_zero_font;
    }
}

void
ui_text_begin_frame(void)
{
    /* Font atlases are immutable after registration.  Rebuilding an atlas
     * while a user types stalls the only UI thread and invalidates textures
     * referenced by the current frame. */
}

static Font
entry_font_for_size(TextFontEntry *entry, int font_size)
{
    Font font;

    if(entry == NULL)
        return kryon_zero_font;

    font = entry_source_font_for_size(entry, font_size);
    if(font_valid(font))
        return font;
    if(font_size == Text8 && font_valid(entry->small_font))
        return entry->small_font;
    if(font_valid(entry->font))
        return entry->font;
    return kryon_zero_font;
}

static Font
entry_font_for_codepoint(TextFontEntry *entry, int codepoint, int font_size)
{
    Font font;

    if(entry == NULL)
        return kryon_zero_font;
    /* Coverage is declared when a source is registered.  Never add a glyph
     * here: text drawing runs on the interaction path, and changing the
     * codepoint set would require recreating every cached GPU atlas. */
    if(codepoint > 0 && codepoint != ' ' && codepoint != '\t' &&
       !font_entry_has_codepoint(entry, codepoint))
        return kryon_zero_font;

    font = entry_font_for_size(entry, font_size);
    if(!font_valid(font))
        return kryon_zero_font;
    if(codepoint <= 0 || codepoint == ' ' || codepoint == '\t' ||
       TextFontHasGlyphValue(font, codepoint))
        return font;
    return kryon_zero_font;
}

static Font
active_font(void)
{
    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count) {
        Font font = entry_font_for_size(&g_ui_fonts[g_ui_active_font],
                                        TextBaseSize);

        if(font_valid(font))
            return font;
    }

    return GetFontDefault();
}

static Font
active_font_for_size(int font_size)
{
    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count) {
        Font font = entry_font_for_size(&g_ui_fonts[g_ui_active_font], font_size);

        if(font_valid(font))
            return font;
    }

    return active_font();
}

static Font
font_for_codepoint(int codepoint, int font_size)
{
    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count) {
        Font font = entry_font_for_codepoint(&g_ui_fonts[g_ui_active_font],
                                             codepoint, font_size);

        if(font_valid(font))
            return font;
    }

    for(int i = 0; i < g_ui_font_count; i++) {
        Font font;

        if(i == g_ui_active_font)
            continue;
        font = entry_font_for_codepoint(&g_ui_fonts[i], codepoint, font_size);
        if(font_valid(font))
            return font;
    }

    return active_font_for_size(font_size);
}

static Font
font_for_scaled_codepoint(int codepoint)
{
    return font_for_codepoint(codepoint, TextBaseSize);
}

static float
font_size_scale(Font font, int font_size)
{
    int target_size = font_physical_size(font_size);
    int base = TextFontBaseSize(font);
    int base_size = base > 0 ? base : TextBaseSize;

    return (float)target_size / (float)base_size;
}

Font
GetTextFont(void)
{
    return active_font();
}

int
EnsureDefaultFont(void)
{
    static const char *paths[] = {
        "fonts/noto/NotoSans-Regular.ttf",
        "../fonts/noto/NotoSans-Regular.ttf",
        "vendor/kryon/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    char system_font_path[512];
    static const char *semibold_paths[] = {
        "fonts/noto/NotoSans-SemiBold.ttf",
        "../fonts/noto/NotoSans-SemiBold.ttf",
        "vendor/kryon/fonts/noto/NotoSans-SemiBold.ttf"
    };

    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count &&
       font_valid(entry_font_for_size(&g_ui_fonts[g_ui_active_font],
                                      TextBaseSize)))
        return 1;
    if(!IsWindowReady())
        return 0;
    if(g_ui_default_font_attempted)
        return 0;

    g_ui_default_font_attempted = 1;
    for(int i = 0; paths[i] != NULL; i++) {
        if(RegisterTextFontFileSource(TEXT_FONT_DEFAULT_NAME, paths[i], NULL, 0) &&
           UseTextFont(TEXT_FONT_DEFAULT_NAME)) {
            if(font_entry_index("semibold") < 0)
                RegisterTextFontFileSource("semibold", semibold_paths[i], NULL, 0);
            TraceLog(LOG_INFO, "TEXTFONT: default font resolved from %s", paths[i]);
            return 1;
        }
    }

    if(GetSystemTextFontFile(system_font_path, sizeof(system_font_path)) &&
       RegisterTextFontFileSource(TEXT_FONT_DEFAULT_NAME, system_font_path, NULL, 0) &&
       UseTextFont(TEXT_FONT_DEFAULT_NAME)) {
        TraceLog(LOG_INFO, "TEXTFONT: bundled face unavailable; default font "
                          "resolved from system: %s", system_font_path);
        return 1;
    }

    TraceLog(LOG_WARNING,
             "TEXTFONT: no font source resolved; text will use the built-in "
             "bitmap font (embed a face via the FONT_FILES build variable)");
    return 0;
}

static int
ensure_ui_italic_font(void)
{
    static const char *paths[] = {
        "fonts/noto/NotoSans-Italic.ttf",
        "../fonts/noto/NotoSans-Italic.ttf",
        "vendor/kryon/fonts/noto/NotoSans-Italic.ttf",
        "/usr/local/share/fonts/noto/NotoSans-Italic.ttf",
        "/usr/local/share/fonts/dejavu/DejaVuSans-Oblique.ttf",
        NULL
    };

    if(font_valid(g_ui_italic_font))
        return 1;
    if(!IsWindowReady())
        return 0;
    if(g_ui_italic_font_attempted)
        return 0;

    g_ui_italic_font_attempted = 1;
    for(int i = 0; paths[i] != NULL; i++) {
        g_ui_italic_font = LoadTextFontAsset(paths[i], TextBaseSize);
        if(font_valid(g_ui_italic_font))
            return 1;
    }

    return 0;
}

static Font
italic_font_for_codepoint(int codepoint, int font_size)
{
    (void)font_size;
    if(ensure_ui_italic_font() && TextFontHasGlyph(g_ui_italic_font, codepoint))
        return g_ui_italic_font;
    return font_for_codepoint(codepoint, font_size);
}

int
RegisterTextFont(const char *name, Font font)
{
    int index;

    if(!font_valid(font))
        return 0;

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_entry(&g_ui_fonts[index]);
    g_ui_fonts[index].font = font;
    return 1;
}

int
RegisterSmallTextFont(const char *name, Font font)
{
    int index;

    if(!font_valid(font))
        return 0;

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_entry(&g_ui_fonts[index]);
    g_ui_fonts[index].small_font = font;
    return 1;
}

static int *ui_font_codepoints(int *out_count);
static int *ui_font_codepoints_for_text(const char *text, int *out_count);

static int
register_ui_font_source(const char *name, const char *file_type,
                        const unsigned char *font_data, unsigned int font_size,
                        const int *codepoints, int codepoint_count)
{
    int index;

#if defined(KRYON_NATIVE_PLAN9)
    if(font_size == 0)
        font_size = 1;
#else
    if(font_data == NULL || font_size == 0)
        return 0;
#endif

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_entry(&g_ui_fonts[index]);

    if(codepoints != NULL && codepoint_count > 0) {
        g_ui_fonts[index].codepoints = calloc((size_t)codepoint_count, sizeof(*codepoints));
        if(g_ui_fonts[index].codepoints == NULL) {
            clear_font_entry(&g_ui_fonts[index]);
            return 0;
        }
        memcpy(g_ui_fonts[index].codepoints, codepoints,
               (size_t)codepoint_count * sizeof(*codepoints));
        g_ui_fonts[index].codepoint_count = codepoint_count;
        g_ui_fonts[index].codepoint_cap = codepoint_count;
    } else {
        /* No codepoint set given: use the standard UI coverage (ASCII, Latin
         * Extended, punctuation, currency, Greek and Cyrillic). */
        int seed_count = 0;
        int *seed = ui_font_codepoints(&seed_count);
        if(seed == NULL || seed_count == 0) {
            free(seed);
            clear_font_entry(&g_ui_fonts[index]);
            return 0;
        }
        g_ui_fonts[index].codepoints = seed;
        g_ui_fonts[index].codepoint_count = seed_count;
        g_ui_fonts[index].codepoint_cap = seed_count;
    }

    /* Copy the type string: callers may free it once this call returns (the
     * Go binding does), and later tier re-rasterizations re-read it. */
    snprintf(g_ui_fonts[index].file_type_buf,
             sizeof(g_ui_fonts[index].file_type_buf), "%s",
             file_type != NULL && file_type[0] != '\0' ? file_type : ".ttf");
    g_ui_fonts[index].file_type = g_ui_fonts[index].file_type_buf;
    g_ui_fonts[index].font_data = font_data;
    g_ui_fonts[index].font_data_size = font_size;
    g_ui_fonts[index].font = kryon_zero_font;
    g_ui_fonts[index].small_font = kryon_zero_font;
    if(!font_valid(entry_source_font_for_size(&g_ui_fonts[index], TextBaseSize))) {
        clear_font_entry(&g_ui_fonts[index]);
        return 0;
    }
    return 1;
}

int
RegisterTextFontSource(const char *name, const char *file_type,
                     const unsigned char *font_data, unsigned int font_size,
                     const int *codepoints, int codepoint_count)
{
    return register_ui_font_source(name, file_type, font_data, font_size,
                                   codepoints, codepoint_count);
}

int
RegisterTextFontSourceForText(const char *name, const char *file_type,
                            const unsigned char *font_data,
                            unsigned int font_size, const char *text)
{
    int codepoint_count = 0;
    int *codepoints = ui_font_codepoints_for_text(text, &codepoint_count);
    int ok;

    if(codepoints == NULL || codepoint_count <= 0)
        return 0;
    ok = register_ui_font_source(name, file_type, font_data, font_size,
                                 codepoints, codepoint_count);
    free(codepoints);
    return ok;
}

int
RegisterFixedTextFontSource(const char *name, const char *file_type,
                          const unsigned char *font_data,
                          unsigned int font_size,
                          const int *codepoints, int codepoint_count)
{
    if(codepoints == NULL || codepoint_count <= 0)
        return 0;
    return register_ui_font_source(name, file_type, font_data, font_size,
                                   codepoints, codepoint_count);
}

int
RegisterTextFontFileSource(const char *name, const char *path,
                         const int *codepoints, int codepoint_count)
{
    const EmbeddedAsset *asset;
    const char *dot;
    unsigned char *data;
    int data_size = 0;
    int owns_data;
    int ok;

    if(path == NULL || path[0] == '\0')
        return 0;

    /* Prefer an embedded asset so font sources resolve with no filesystem
     * (e.g. on Android, where the .ttf is baked into libkryon.a). The embedded
     * blob is static storage and must not be freed. */
    asset = GetEmbeddedAsset(path);
    if(asset != NULL) {
        data = (unsigned char *)asset->data;
        data_size = (int)asset->size;
        owns_data = 0;
    } else {
        data = LoadFileData(path, &data_size);
        if(data == NULL || data_size <= 0) {
#if defined(KRYON_NATIVE_PLAN9)
            data = NULL;
            data_size = 1;
            owns_data = 0;
#else
            return 0;
#endif
        } else {
            owns_data = 1;
        }
    }

    dot = strrchr(path, '.');
    if(dot == NULL || dot[0] == '\0')
        dot = ".ttf";

    ok = RegisterTextFontSource(name, dot, data, (unsigned int)data_size,
                              codepoints, codepoint_count);
    if(!ok) {
        if(owns_data)
            UnloadFileData(data);
        return 0;
    }

    int index = font_entry_index(name);
    if(index < 0) {
        if(owns_data)
            UnloadFileData(data);
        return 0;
    }
    snprintf(g_ui_fonts[index].file_type_buf, sizeof(g_ui_fonts[index].file_type_buf),
             "%s", dot);
    g_ui_fonts[index].file_type = g_ui_fonts[index].file_type_buf;
    g_ui_fonts[index].owns_font_data = owns_data;
    return 1;
}

int
RegisterTextFontFileSourceForText(const char *name, const char *path,
                                const char *text)
{
    int codepoint_count = 0;
    int *codepoints = ui_font_codepoints_for_text(text, &codepoint_count);
    int ok;

    if(codepoints == NULL || codepoint_count <= 0)
        return 0;
    ok = RegisterTextFontFileSource(name, path, codepoints, codepoint_count);
    free(codepoints);
    return ok;
}

int
UseTextFont(const char *name)
{
    int index = font_entry_index(name);

    if(index < 0)
        return 0;
    if(!font_valid(entry_source_font_for_size(&g_ui_fonts[index], TextBaseSize)) &&
       !font_valid(g_ui_fonts[index].font))
        return 0;

    g_ui_active_font = index;
    return 1;
}

int
PushTextFont(const char *name)
{
    int token = g_ui_active_font;

    if(name != NULL && name[0] != '\0')
        (void)UseTextFont(name);
    return token;
}

void
PopTextFont(int token)
{
    if(token >= 0 && token < g_ui_font_count)
        g_ui_active_font = token;
}

int
ui_active_font_token(void)
{
    return g_ui_active_font;
}

void
ui_draw_text_with_font_token(const char *text, int x, int y, int font_size,
                             Color color, int token)
{
    int previous = g_ui_active_font;

    if(token >= 0 && token < g_ui_font_count)
        g_ui_active_font = token;
    RenderText(text, x, y, font_size, color);
    g_ui_active_font = previous;
}

static int
append_codepoint_range(int *codepoints, int index, int first, int last)
{
    for(int codepoint = first; codepoint <= last; codepoint++)
        codepoints[index++] = codepoint;
    return index;
}

static int *
ui_font_codepoints(int *out_count)
{
    int count = 0;
    int *codepoints;

    count += 0x7E - 0x20 + 1;
    count += 0x024F - 0x00A0 + 1;
    count += 0x206F - 0x2000 + 1;
    count += 0x20CF - 0x20A0 + 1;
    count += 0x03FF - 0x0370 + 1;
    count += 0x04FF - 0x0400 + 1;

    codepoints = calloc((size_t)count, sizeof(*codepoints));
    if(codepoints == NULL) {
        *out_count = 0;
        return NULL;
    }

    count = 0;
    count = append_codepoint_range(codepoints, count, 0x20, 0x7E);
    count = append_codepoint_range(codepoints, count, 0x00A0, 0x024F);
    count = append_codepoint_range(codepoints, count, 0x2000, 0x206F);
    count = append_codepoint_range(codepoints, count, 0x20A0, 0x20CF);
    count = append_codepoint_range(codepoints, count, 0x0370, 0x03FF);
    count = append_codepoint_range(codepoints, count, 0x0400, 0x04FF);
    *out_count = count;
    return codepoints;
}

static int
ui_font_codepoint_contains(const int *codepoints, int count, int codepoint)
{
    for(int i = 0; i < count; i++) {
        if(codepoints[i] == codepoint)
            return 1;
    }
    return 0;
}

static int
ui_font_codepoint_append_unique(int **codepoints, int *count, int *cap,
                                int codepoint)
{
    int *next;

    if(codepoint <= 0)
        return 1;
    if(ui_font_codepoint_contains(*codepoints, *count, codepoint))
        return 1;
    if(*count >= *cap) {
        int next_cap = *cap > 0 ? *cap * 2 : 64;
        next = realloc(*codepoints, (size_t)next_cap * sizeof(**codepoints));
        if(next == NULL)
            return 0;
        *codepoints = next;
        *cap = next_cap;
    }
    (*codepoints)[(*count)++] = codepoint;
    return 1;
}

static int *
ui_font_codepoints_for_text(const char *text, int *out_count)
{
    int count = 0;
    int cap = 0;
    int *codepoints = ui_font_codepoints(&count);

    if(out_count != NULL)
        *out_count = 0;
    if(codepoints == NULL || count <= 0)
        return NULL;
    cap = count;

    for(int offset = 0; text != NULL && text[offset] != '\0';) {
        int bytes = 0;
        int codepoint = GetCodepointNext(text + offset, &bytes);
        if(bytes <= 0)
            bytes = 1;
        if(!ui_font_codepoint_append_unique(&codepoints, &count, &cap, codepoint)) {
            free(codepoints);
            return NULL;
        }
        offset += bytes;
    }

    if(out_count != NULL)
        *out_count = count;
    return codepoints;
}

Font
LoadTextFontFromMemory(const char *file_type, const unsigned char *font_data,
                     unsigned int font_size, int base_size)
{
    Font font = {0};
    int codepoint_count = 0;
    int *codepoints;

    if(font_data == NULL || font_size == 0)
        return font;

    codepoints = ui_font_codepoints(&codepoint_count);
    if(codepoints == NULL || codepoint_count == 0)
        return font;

    font = LoadFontFromMemory(
        file_type != NULL && file_type[0] != '\0' ? file_type : ".ttf",
        font_data, (int)font_size,
        base_size > 0 ? base_size : TextBaseSize,
        codepoints, codepoint_count);
    free(codepoints);

    if(font_valid(font))
        SetTextureFilter(TextFontAtlasTexture(font), TEXTURE_FILTER_BILINEAR);
    return font;
}

Font
LoadTextFontAsset(const char *path, int base_size)
{
    const EmbeddedAsset *asset;
    const char *file_type;
    unsigned char *data;
    int data_size = 0;
    Font font;

    if(path == NULL || path[0] == '\0')
        return kryon_zero_font;

    file_type = GetEmbeddedAssetExtension(path);
    asset = GetEmbeddedAsset(path);
    if(asset != NULL)
        return LoadTextFontFromMemory(file_type, asset->data, asset->size, base_size);

    data = LoadFileData(path, &data_size);
    if(data == NULL || data_size <= 0)
        return kryon_zero_font;

    font = LoadTextFontFromMemory(file_type, data, (unsigned int)data_size, base_size);
    UnloadFileData(data);
    return font;
}

void
UnloadTextFont(Font *font)
{
    if(font == NULL || font->texture.id == 0)
        return;

    UnloadFont(*font);
    *font = kryon_zero_font;
}

void
ClearTextFonts(void)
{
    for(int i = 0; i < g_ui_font_count; i++) {
        clear_font_entry(&g_ui_fonts[i]);
    }
    if(font_valid(g_ui_italic_font))
        UnloadFont(g_ui_italic_font);
    g_ui_italic_font = kryon_zero_font;
    g_ui_italic_font_attempted = 0;
    memset(g_ui_fonts, 0, sizeof(g_ui_fonts));
    g_ui_font_count = 0;
    g_ui_active_font = -1;
    g_ui_default_font_attempted = 0;
    ui_font_trim_heap();
}

void
TextFontMemoryReport(const char *tag)
{
    if(!KryonMemDebugEnabled())
        return;

    fprintf(stderr, "[kryon-mem] --- ui fonts (%s) ---\n",
            tag != NULL ? tag : "-");
    for(int i = 0; i < g_ui_font_count; i++) {
        TextFontEntry *entry = &g_ui_fonts[i];

        fprintf(stderr,
                "[kryon-mem] font '%s' active=%d codepoints=%d tiers=",
                entry->name, i == g_ui_active_font ? 1 : 0,
                entry->codepoint_count);
        for(int t = 0; t < TEXT_FONT_MAX_RASTER_TIERS; t++) {
            if(entry->tier_size[t] > 0)
                fprintf(stderr, "%s%dpx/%dglyphs", t ? "," : "",
                        entry->tier_size[t], TextFontGlyphCount(entry->tier_font[t]));
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}

static int
ui_text_normalize_token_size(int font_size)
{
    /* Text sizes reaching this layer are already physical pixels. A value
     * such as 24 may be Scale(Text16), so treating it as the Text24 token
     * applies scaling again and clips text inside scaled controls. */
    return font_size;
}

int
MeasureTextWidth(const char *text, int font_size, const char *typeface)
{
    int token = PushTextFont(typeface);
    int width = TextWidth(text, font_size);
    PopTextFont(token);
    return width;
}

int
TextWidth(const char *text, int font_size)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    int width = 0;

    if(text == NULL || !TextFontReady(font))
        return 0;

    if(TextFontHasNativeText(font) && g_ui_text_letter_spacing == 0) {
        int byte_len = 0;

        while(text[byte_len] != '\0' && text[byte_len] != '\n')
            byte_len++;
        return TextFontNativeTextWidth(font, text, byte_len);
    }

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;

        if(codepoint == '\n')
            break;
        glyph_font = font_for_codepoint(codepoint, normalized_font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "TEXTFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, normalized_font_size, glyph_font.baseSize,
                     (double)TextFontGlyph(glyph_font, codepoint).advanceX);
        }
        if(i > 0)
            width += g_ui_text_letter_spacing;
        if(TextFontHasNativeText(glyph_font))
            width += TextFontNativeTextWidth(glyph_font, &text[i], codepoint_byte_count);
        else
            width += (int)((float)TextFontAdvance(glyph_font, codepoint) *
                           font_size_scale(glyph_font, normalized_font_size) + 0.5f);
        i += codepoint_byte_count;
    }
    return width;
}

static int
ui_text_line_byte_len(const char *text)
{
    int len = 0;

    if(text == NULL)
        return 0;
    while(text[len] != '\0' && text[len] != '\n')
        len++;
    return len;
}

static int
ui_text_width_bytes(const char *text, int byte_len, int font_size)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    int width = 0;

    if(text == NULL || byte_len <= 0 || !TextFontReady(font))
        return 0;

    if(TextFontHasNativeText(font) && g_ui_text_letter_spacing == 0)
        return TextFontNativeTextWidth(font, text, byte_len);

    for(int i = 0; i < byte_len && text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;

        if(codepoint == '\n')
            break;
        if(codepoint_byte_count <= 0)
            codepoint_byte_count = 1;
        if(i + codepoint_byte_count > byte_len)
            break;
        glyph_font = font_for_codepoint(codepoint, normalized_font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "TEXTFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, normalized_font_size, glyph_font.baseSize,
                     (double)TextFontGlyph(glyph_font, codepoint).advanceX);
        }
        if(i > 0)
            width += g_ui_text_letter_spacing;
        if(TextFontHasNativeText(glyph_font))
            width += TextFontNativeTextWidth(glyph_font, &text[i], codepoint_byte_count);
        else
            width += (int)((float)TextFontAdvance(glyph_font, codepoint) *
                           font_size_scale(glyph_font, normalized_font_size) + 0.5f);
        i += codepoint_byte_count;
    }

    return width;
}

int
ui_text_byte_offset_at_x(const char *text, int font_size, int target_x)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    int byte_len = ui_text_line_byte_len(text);
    int cursor_x = 0;

    if(text == NULL || target_x <= 0)
        return 0;

    for(int i = 0; i < byte_len;) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        int advance;

        if(codepoint_byte_count <= 0)
            codepoint_byte_count = 1;
        if(i + codepoint_byte_count > byte_len)
            return i;
        glyph_font = font_for_codepoint(codepoint, normalized_font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "TEXTFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, normalized_font_size, glyph_font.baseSize,
                     (double)TextFontGlyph(glyph_font, codepoint).advanceX);
        }
        advance = (int)((float)TextFontAdvance(glyph_font, codepoint) *
                        font_size_scale(glyph_font, normalized_font_size) + 0.5f);
        if(TextFontHasNativeText(glyph_font))
            advance = TextFontNativeTextWidth(glyph_font, &text[i], codepoint_byte_count);
        if(i + codepoint_byte_count < byte_len)
            advance += g_ui_text_letter_spacing;
        if(target_x < cursor_x + advance / 2)
            return i;
        cursor_x += advance;
        i += codepoint_byte_count;
    }

    return byte_len;
}

void
ui_text_draw_selection(const char *text, int x, int y, int font_size,
                       Color color, int start, int end)
{
    int line_h;
    int start_x;
    int end_x;

    if(text == NULL || end <= start)
        return;

    line_h = TextLineHeight(font_size);
    start_x = x + ui_text_width_bytes(text, start, font_size);
    end_x = x + ui_text_width_bytes(text, end, font_size);
    if(start > 0 && text[start] != '\0' && text[start] != '\n')
        start_x += g_ui_text_letter_spacing;
    if(end > 0 && text[end] != '\0' && text[end] != '\n')
        end_x += g_ui_text_letter_spacing;
    if(end_x <= start_x)
        return;

    DrawRectangle(start_x, y, end_x - start_x, line_h, color);
}

int
TextHeight(const char *text, int font_size)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || !TextFontReady(font))
        return normalized_font_size;

    if(TextFontHasNativeText(font)) {
        int native_h = TextFontNativeTextHeight(font);

        return native_h > 0 ? native_h : normalized_font_size;
    }

    scale = font_size_scale(font, normalized_font_size);
    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;
        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, normalized_font_size);
            GlyphInfo glyph = TextFontGlyph(glyph_font, codepoint);
            Rectangle rec = TextFontAtlasRec(glyph_font, codepoint);
            float glyph_scale = font_size_scale(glyph_font, normalized_font_size);
            int padding = TextFontGlyphPadding(glyph_font);
            float glyph_top = (float)glyph.offsetY * glyph_scale - (float)padding * glyph_scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)padding) * glyph_scale;

            if(!seen_glyph) {
                min_top = glyph_top;
                max_bottom = glyph_bottom;
                seen_glyph = 1;
            } else {
                if(glyph_top < min_top)
                    min_top = glyph_top;
                if(glyph_bottom > max_bottom)
                    max_bottom = glyph_bottom;
            }
        }
        i += codepoint_byte_count;
    }

    return TextHeightFor((int)((float)TextFontBaseSize(font) * scale + 0.5f),
                         seen_glyph != 0, min_top, max_bottom);
}

int
TextLineHeight(int font_size)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    float scale = font_size_scale(font, normalized_font_size);
    int base = TextFontBaseSize(font);

    if(TextFontHasNativeText(font)) {
        int native_h = TextFontNativeTextHeight(font);

        if(native_h > 0)
            return native_h;
    }

    return TextLineHeightFor(base, TextBaseSize, scale);
}

Font
GetTextFontForCodepoint(int codepoint, int font_size)
{
    return font_for_codepoint(codepoint, ui_text_normalize_token_size(font_size));
}

int
RenderTextGlyph(unsigned int codepoint, int x, int y, int font_size,
                Color color)
{
    int size = ui_text_normalize_token_size(font_size);
    Font glyph_font;
    float scale;
    GlyphInfo glyph;
    Rectangle src;

    if(codepoint == 0 || codepoint == ' ' || codepoint == '\t')
        return 0;
    glyph_font = font_for_codepoint((int)codepoint, size);
    if(!TextFontReady(glyph_font))
        return 0;
    scale = font_size_scale(glyph_font, size);
    glyph = TextFontGlyph(glyph_font, (int)codepoint);
    src = TextFontAtlasRec(glyph_font, (int)codepoint);
    if(src.width <= 0.0f || src.height <= 0.0f)
        return 0;
    {
        Rectangle dst = {
            .x = (float)x + (float)glyph.offsetX * scale,
            .y = (float)y + (float)glyph.offsetY * scale,
            .width = src.width * scale,
            .height = src.height * scale
        };

        DrawTexturePro(TextFontAtlasTexture(glyph_font), src, dst,
                       (Vector2){0.0f, 0.0f}, 0.0f, color);
    }
    return 1;
}

float
GetTextFontScale(Font font, int font_size)
{
    return font_size_scale(font, ui_text_normalize_token_size(font_size));
}

static int
text_quad_inside_clip(Rectangle quad, Rectangle clip)
{
    float left = g_ui_camera.offset.x + quad.x * g_ui_camera.zoom;
    float top = g_ui_camera.offset.y + quad.y * g_ui_camera.zoom;
    return left >= clip.x && top >= clip.y &&
        left + quad.width * g_ui_camera.zoom <= clip.x + clip.width &&
        top + quad.height * g_ui_camera.zoom <= clip.y + clip.height;
}

int
ui_text_fits_bounds(const char *text, int x, int y, int font_size,
                    Rectangle bounds)
{
    int cursor_x = x;
    int size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(size);
    Rectangle clip;

    if(text == NULL || text[0] == '\0')
        return 1;
    if(!TextFontReady(font) || TextFontHasNativeText(font) ||
       g_ui_camera.zoom <= 0 || g_ui_camera.rotation != 0 ||
       g_ui_camera.target.x != 0 || g_ui_camera.target.y != 0)
        return 0;
    /* Match the integer pixel bounds used by ui_begin_world_clip. Advances
     * alone are insufficient: fallback glyphs and italics can overhang. */
    clip = (Rectangle){
        (int)(g_ui_camera.offset.x + bounds.x * g_ui_camera.zoom),
        (int)(g_ui_camera.offset.y + bounds.y * g_ui_camera.zoom),
        (int)(bounds.width * g_ui_camera.zoom),
        (int)(bounds.height * g_ui_camera.zoom)
    };
    if(ui_inline_text_selection_active(text, x, y, size)) {
        Rectangle selection = {(float)x, (float)y,
            (float)TextWidth(text, size), (float)TextLineHeight(size)};
        if(!text_quad_inside_clip(selection, clip))
            return 0;
    }
    for(int i = 0; text[i] != '\0';) {
        int bytes = 0;
        int codepoint = GetCodepointNext(text + i, &bytes);
        if(codepoint == '\n')
            break;
        Font glyph_font = font_for_codepoint(codepoint, size);
        if(!TextFontReady(glyph_font) || TextFontHasNativeText(glyph_font))
            return 0;
        float scale = font_size_scale(glyph_font, size);
        GlyphInfo glyph = TextFontGlyph(glyph_font, codepoint);
        Rectangle source = TextFontAtlasRec(glyph_font, codepoint);
        Rectangle quad = {cursor_x + glyph.offsetX * scale,
            y + glyph.offsetY * scale, source.width * scale, source.height * scale};
        if(source.width > 0 && source.height > 0 &&
           !text_quad_inside_clip(quad, clip))
            return 0;
        cursor_x += (int)(glyph.advanceX * scale + 0.5f) + g_ui_text_letter_spacing;
        i += bytes;
    }
    return 1;
}

void
RenderTextEx(const char *text, int x, int y, int font_size, Color color,
             int selectable_arg)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    int cursor_x = x;
    int byte_len;
    int text_w;
    int line_h;

    if(text == NULL || !TextFontReady(font))
        return;

    font_size = normalized_font_size;
    byte_len = ui_text_line_byte_len(text);
    text_w = TextWidth(text, font_size);
    line_h = TextLineHeight(font_size);

    ui_inline_text_before_render(text, x, y, font_size, color,
                                 selectable_arg, byte_len, text_w, line_h);

    if(TextFontHasNativeText(font) && g_ui_text_letter_spacing == 0) {
        (void)TextFontDrawNativeText(font, text, byte_len, x, y, font_size, color);
        ui_draw_text_strikethrough(x, y, text_w, line_h, color);
        return;
    }

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        float scale;
        GlyphInfo glyph;
        Rectangle src;

        if(codepoint == '\n')
            break;

        glyph_font = font_for_codepoint(codepoint, font_size);
        if(TextFontHasNativeText(glyph_font)) {
            (void)TextFontDrawNativeText(glyph_font, &text[i], codepoint_byte_count,
                                      cursor_x, y, font_size, color);
            cursor_x += TextFontNativeTextWidth(glyph_font, &text[i], codepoint_byte_count)
                        + g_ui_text_letter_spacing;
            i += codepoint_byte_count;
            continue;
        }
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "TEXTFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, font_size, glyph_font.baseSize,
                     (double)TextFontGlyph(glyph_font, codepoint).advanceX);
        }
        scale = font_size_scale(glyph_font, font_size);
        glyph = TextFontGlyph(glyph_font, codepoint);
        src = TextFontAtlasRec(glyph_font, codepoint);

        if(ui_text_trace_enabled() && i == 0) {
            TraceLog(LOG_WARNING, "TEXT: txt=%.12s fs=%d base=%d sc=%.3f x=%d y=%d off=(%d,%d) adv=%.2f w=%.0f",
                     text, font_size, glyph_font.baseSize, scale, cursor_x, y,
                     glyph.offsetX, glyph.offsetY, (double)glyph.advanceX,
                     (double)src.width);
        }
        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)cursor_x + (float)glyph.offsetX * scale,
                .y = (float)y + (float)glyph.offsetY * scale,
                .width = src.width * scale,
                .height = src.height * scale
            };
            DrawTexturePro(TextFontAtlasTexture(glyph_font), src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += (int)((float)glyph.advanceX * scale + 0.5f) + g_ui_text_letter_spacing;
        i += codepoint_byte_count;
    }
    ui_draw_text_strikethrough(x, y, text_w, line_h, color);
}

void
ui_render_italic_text(const char *text, int x, int y, int font_size, Color color)
{
    int cursor_x = x;

    if(text == NULL)
        return;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        float scale;
        GlyphInfo glyph;
        Rectangle src;

        if(codepoint == '\n')
            break;
        if(codepoint_byte_count <= 0)
            codepoint_byte_count = 1;

        glyph_font = italic_font_for_codepoint(codepoint, font_size);
        if(!TextFontReady(glyph_font)) {
            i += codepoint_byte_count;
            continue;
        }

        scale = font_size_scale(glyph_font, font_size);
        glyph = TextFontGlyph(glyph_font, codepoint);
        src = TextFontAtlasRec(glyph_font, codepoint);

        if(ui_text_trace_enabled() && i == 0) {
            TraceLog(LOG_WARNING, "TEXT: txt=%.12s fs=%d base=%d sc=%.3f x=%d y=%d off=(%d,%d) adv=%.2f w=%.0f",
                     text, font_size, glyph_font.baseSize, scale, cursor_x, y,
                     glyph.offsetX, glyph.offsetY, (double)glyph.advanceX,
                     (double)src.width);
        }
        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)cursor_x + (float)glyph.offsetX * scale,
                .y = (float)y + (float)glyph.offsetY * scale,
                .width = src.width * scale,
                .height = src.height * scale
            };
            DrawTexturePro(TextFontAtlasTexture(glyph_font), src, dst,
                           (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += (int)((float)TextFontAdvance(glyph_font, codepoint) * scale + 0.5f);
        i += codepoint_byte_count;
    }
}

void
DrawScaledText(const char *text, int x, int y, int scale, Color color)
{
    Font font = active_font();
    int cursor_x = x;

    if(text == NULL || !TextFontReady(font))
        return;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        Font glyph_font = font_for_scaled_codepoint(codepoint);
        GlyphInfo glyph = TextFontGlyph(glyph_font, codepoint);
        Rectangle src = TextFontAtlasRec(glyph_font, codepoint);

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)(cursor_x + glyph.offsetX * scale),
                .y = (float)(y + glyph.offsetY * scale),
                .width = src.width * (float)scale,
                .height = src.height * (float)scale
            };
            DrawTexturePro(TextFontAtlasTexture(glyph_font), src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += glyph.advanceX * scale;
        i += codepoint_byte_count;
    }
}

int
TextBaselineY(const char *text, int box_y, int box_h, int font_size)
{
    int normalized_font_size = ui_text_normalize_token_size(font_size);
    Font font = active_font_for_size(normalized_font_size);
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || !TextFontReady(font))
        return TextBaselineYFor(box_y, box_h,
                                TextLineHeight(normalized_font_size),
                                false, 0.0f, 0.0f);

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, normalized_font_size);
            GlyphInfo glyph = TextFontGlyph(glyph_font, codepoint);
            Rectangle rec = TextFontAtlasRec(glyph_font, codepoint);
            float glyph_scale = font_size_scale(glyph_font, normalized_font_size);
            int padding = TextFontGlyphPadding(glyph_font);
            float glyph_top = (float)glyph.offsetY * glyph_scale - (float)padding * glyph_scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)padding) * glyph_scale;

            if(!seen_glyph) {
                min_top = glyph_top;
                max_bottom = glyph_bottom;
                seen_glyph = 1;
            } else {
                if(glyph_top < min_top)
                    min_top = glyph_top;
                if(glyph_bottom > max_bottom)
                    max_bottom = glyph_bottom;
            }
        }

        i += codepoint_byte_count;
    }

    if(!seen_glyph)
        return TextBaselineYFor(box_y, box_h, normalized_font_size,
                                false, 0.0f, 0.0f);

    return TextBaselineYFor(box_y, box_h, normalized_font_size,
                            true, min_top, max_bottom);
}
