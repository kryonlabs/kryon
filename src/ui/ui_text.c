#include "ui_text.h"
#include "ui_text_backend.h"
#include "ui_clip.h"
#include "ui_internal.h"
#include "ui_tk.h"
#include "ui_widget.h"
#include "embedded_assets.h"
#include "ui_scaling.h"
#include "theme.h"

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

#define UI_FONT_MAX_REGISTERED 16
#define UI_FONT_DEFAULT_NAME "default"

/* Text whose physical size is at least this multiple of the base raster gets
 * a dedicated large rasterization so headings stay crisp. One large slot per
 * font holds the largest size seen; smaller large-tier sizes scale down. */

typedef struct UIFontEntry {
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
#define UI_FONT_MAX_RASTER_TIERS 16
    Font tier_font[UI_FONT_MAX_RASTER_TIERS];
    int tier_size[UI_FONT_MAX_RASTER_TIERS]; /* 0 = free slot */
} UIFontEntry;

static UIFontEntry g_ui_fonts[UI_FONT_MAX_REGISTERED];
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
        g_ui_text_trace = getenv("INBE_TEXT_TRACE") != NULL;
    return g_ui_text_trace;
}

static int g_ui_text_selectable_stack[16];
static int g_ui_text_selectable_stack_count = 0;
static int g_ui_text_selectable = 1;

typedef struct UITextSelectionState {
    int id;
    int anchor;
    int cursor;
    int dragging;
} UITextSelectionState;

static UITextSelectionState g_ui_text_selection = {0};
static UITextSelectionState g_ui_text_block_selection = {0};
static int g_ui_text_block_last_click_id = 0;
static int g_ui_text_block_last_click_line = -1;
static Vector2 g_ui_text_block_last_click_position = {0};
static double g_ui_text_block_last_click_time = -1.0;

typedef struct UITextBlockLine {
    int start;
    int end;
} UITextBlockLine;

static Rectangle
text_world_rect_to_screen(Rectangle rect)
{
    return (Rectangle){
        g_ui_camera.offset.x + rect.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + rect.y * g_ui_camera.zoom,
        rect.width * g_ui_camera.zoom,
        rect.height * g_ui_camera.zoom
    };
}

static int
font_valid(Font font)
{
    return UIFontReady(font);
}

int
UIFontHasGlyph(Font font, int codepoint)
{
    return UIFontHasGlyphValue(font, codepoint);
}

static int
font_entry_index(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : UI_FONT_DEFAULT_NAME;

    for(int i = 0; i < g_ui_font_count; i++) {
        if(strcmp(g_ui_fonts[i].name, key) == 0)
            return i;
    }

    return -1;
}

static int
font_entry_alloc(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : UI_FONT_DEFAULT_NAME;
    int index = font_entry_index(key);

    if(index >= 0)
        return index;
    if(g_ui_font_count >= UI_FONT_MAX_REGISTERED)
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
    int size = ScaleUIPx(font_size);

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
clear_font_cache(UIFontEntry *entry)
{
    if(entry == NULL)
        return;
    for(int i = 0; i < UI_FONT_MAX_RASTER_TIERS; i++) {
        if(font_valid(entry->tier_font[i]))
            UnloadFont(entry->tier_font[i]);
        entry->tier_font[i] = kryon_zero_font;
        entry->tier_size[i] = 0;
    }
}

static void
clear_font_entry(UIFontEntry *entry)
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
font_entry_has_codepoint(UIFontEntry *entry, int codepoint)
{
    if(entry == NULL || codepoint <= 0)
        return 0;
    for(int i = 0; i < entry->codepoint_count; i++) {
        if(entry->codepoints[i] == codepoint)
            return 1;
    }
    return 0;
}

static Font
load_font_source_size(UIFontEntry *entry, int physical_size)
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
        SetTextureFilter(UIFontAtlasTexture(font), TEXTURE_FILTER_BILINEAR);
    return font;
}

static Font
entry_source_font_for_size(UIFontEntry *entry, int font_size)
{
    int physical_size = font_physical_size(font_size);

    if(entry == NULL || entry->font_data == NULL || entry->font_data_size == 0)
        return kryon_zero_font;

    /* Exact tier hit: rasterized at this exact physical size, no resampling. */
    for(int i = 0; i < UI_FONT_MAX_RASTER_TIERS; i++) {
        if(entry->tier_size[i] == physical_size && font_valid(entry->tier_font[i]))
            return entry->tier_font[i];
    }

    /* Free slot: rasterize this size. */
    for(int i = 0; i < UI_FONT_MAX_RASTER_TIERS; i++) {
        if(entry->tier_size[i] == 0) {
            entry->tier_font[i] = load_font_source_size(entry, physical_size);
            if(font_valid(entry->tier_font[i])) {
                /* Record the size the rasterizer actually produced. TTF
                 * tiers come back at exactly physical_size; the plan9
                 * bitmap buckets come back at their nearest available
                 * height, so recording the real base size lets nearby
                 * requests share one bucket tier. */
                int raster_size = UIFontBaseSize(entry->tier_font[i]);

                entry->tier_size[i] =
                    raster_size > 0 ? raster_size : physical_size;
                ui_font_trim_heap();
                return entry->tier_font[i];
            }
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

        for(int i = 0; i < UI_FONT_MAX_RASTER_TIERS; i++) {
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
entry_font_for_size(UIFontEntry *entry, int font_size)
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
entry_font_for_codepoint(UIFontEntry *entry, int codepoint, int font_size)
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
       UIFontHasGlyphValue(font, codepoint))
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
    int base = UIFontBaseSize(font);
    int base_size = base > 0 ? base : TextBaseSize;

    return (float)target_size / (float)base_size;
}

Font
GetUIFont(void)
{
    return active_font();
}

int
EnsureUIDefaultFont(void)
{
    static const char *paths[] = {
        "fonts/noto/NotoSans-Regular.ttf",
        "../fonts/noto/NotoSans-Regular.ttf",
        "vendor/kryon/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    char system_font_path[512];

    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count &&
       font_valid(entry_font_for_size(&g_ui_fonts[g_ui_active_font],
                                      TextBaseSize)))
        return 1;
    if(!IsWindowReady())
        return 0;
    if(g_ui_default_font_attempted)
        return 0;

    g_ui_default_font_attempted = 1;
    if(GetSystemUIFontFile(system_font_path, sizeof(system_font_path)) &&
       RegisterUIFontFileSource(UI_FONT_DEFAULT_NAME, system_font_path, NULL, 0) &&
       UseUIFont(UI_FONT_DEFAULT_NAME))
        return 1;

    for(int i = 0; paths[i] != NULL; i++) {
        if(RegisterUIFontFileSource(UI_FONT_DEFAULT_NAME, paths[i], NULL, 0) &&
           UseUIFont(UI_FONT_DEFAULT_NAME))
            return 1;
    }

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
        g_ui_italic_font = LoadUIFontAsset(paths[i], TextBaseSize);
        if(font_valid(g_ui_italic_font))
            return 1;
    }

    return 0;
}

static Font
italic_font_for_codepoint(int codepoint, int font_size)
{
    (void)font_size;
    if(ensure_ui_italic_font() && UIFontHasGlyph(g_ui_italic_font, codepoint))
        return g_ui_italic_font;
    return font_for_codepoint(codepoint, font_size);
}

int
RegisterUIFont(const char *name, Font font)
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
RegisterUISmallFont(const char *name, Font font)
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
RegisterUIFontSource(const char *name, const char *file_type,
                     const unsigned char *font_data, unsigned int font_size,
                     const int *codepoints, int codepoint_count)
{
    return register_ui_font_source(name, file_type, font_data, font_size,
                                   codepoints, codepoint_count);
}

int
RegisterUIFixedFontSource(const char *name, const char *file_type,
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
RegisterUIFontFileSource(const char *name, const char *path,
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

    ok = RegisterUIFontSource(name, dot, data, (unsigned int)data_size,
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
UseUIFont(const char *name)
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
PushUIFont(const char *name)
{
    int token = g_ui_active_font;

    if(name != NULL && name[0] != '\0')
        (void)UseUIFont(name);
    return token;
}

void
PopUIFont(int token)
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
    DrawUIText(text, x, y, font_size, color);
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

Font
LoadUIFontFromMemory(const char *file_type, const unsigned char *font_data,
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
        SetTextureFilter(UIFontAtlasTexture(font), TEXTURE_FILTER_BILINEAR);
    return font;
}

Font
LoadUIFontAsset(const char *path, int base_size)
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
        return LoadUIFontFromMemory(file_type, asset->data, asset->size, base_size);

    data = LoadFileData(path, &data_size);
    if(data == NULL || data_size <= 0)
        return kryon_zero_font;

    font = LoadUIFontFromMemory(file_type, data, (unsigned int)data_size, base_size);
    UnloadFileData(data);
    return font;
}

void
UnloadUIFont(Font *font)
{
    if(font == NULL || font->texture.id == 0)
        return;

    UnloadFont(*font);
    *font = kryon_zero_font;
}

void
ClearUIFonts(void)
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
UIFontMemoryReport(const char *tag)
{
    if(!KryonMemDebugEnabled())
        return;

    fprintf(stderr, "[kryon-mem] --- ui fonts (%s) ---\n",
            tag != NULL ? tag : "-");
    for(int i = 0; i < g_ui_font_count; i++) {
        UIFontEntry *entry = &g_ui_fonts[i];

        fprintf(stderr,
                "[kryon-mem] font '%s' active=%d codepoints=%d tiers=",
                entry->name, i == g_ui_active_font ? 1 : 0,
                entry->codepoint_count);
        for(int t = 0; t < UI_FONT_MAX_RASTER_TIERS; t++) {
            if(entry->tier_size[t] > 0)
                fprintf(stderr, "%s%dpx/%dglyphs", t ? "," : "",
                        entry->tier_size[t], UIFontGlyphCount(entry->tier_font[t]));
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}

int
TextWidth(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);
    int width = 0;

    if(text == NULL || !UIFontReady(font))
        return 0;

    if(UIFontHasNativeText(font)) {
        int byte_len = 0;

        while(text[byte_len] != '\0' && text[byte_len] != '\n')
            byte_len++;
        return UIFontNativeTextWidth(font, text, byte_len);
    }

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;

        if(codepoint == '\n')
            break;
        glyph_font = font_for_codepoint(codepoint, font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "UIFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, font_size, glyph_font.baseSize,
                     (double)UIFontGlyph(glyph_font, codepoint).advanceX);
        }
        width += (int)((float)UIFontAdvance(glyph_font, codepoint) *
                       font_size_scale(glyph_font, font_size) + 0.5f);
        i += codepoint_byte_count;
    }
    return width;
}

static int
ui_text_hash_int(int hash, int value)
{
    unsigned int h = (unsigned int)hash;

    h ^= (unsigned int)value + 0x9e3779b9u + (h << 6) + (h >> 2);
    return (int)(h & 0x7fffffff);
}

static int
ui_text_id(const char *text, int x, int y, int font_size)
{
    uintptr_t ptr = (uintptr_t)text;
    int hash = 5381;

    hash = ui_text_hash_int(hash, (int)(ptr & 0xffffffffu));
#if UINTPTR_MAX > 0xffffffffu
    hash = ui_text_hash_int(hash, (int)(ptr >> 32));
#endif
    hash = ui_text_hash_int(hash, x);
    hash = ui_text_hash_int(hash, y);
    hash = ui_text_hash_int(hash, font_size);
    if(hash == 0)
        hash = 1;
    return hash;
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
    Font font = active_font_for_size(font_size);
    int width = 0;

    if(text == NULL || byte_len <= 0 || !UIFontReady(font))
        return 0;

    if(UIFontHasNativeText(font))
        return UIFontNativeTextWidth(font, text, byte_len);

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
        glyph_font = font_for_codepoint(codepoint, font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "UIFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, font_size, glyph_font.baseSize,
                     (double)UIFontGlyph(glyph_font, codepoint).advanceX);
        }
        width += (int)((float)UIFontAdvance(glyph_font, codepoint) *
                       font_size_scale(glyph_font, font_size) + 0.5f);
        i += codepoint_byte_count;
    }

    return width;
}

static int
ui_text_byte_offset_at_x(const char *text, int font_size, int target_x)
{
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
        glyph_font = font_for_codepoint(codepoint, font_size);
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "UIFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, font_size, glyph_font.baseSize,
                     (double)UIFontGlyph(glyph_font, codepoint).advanceX);
        }
        advance = (int)((float)UIFontAdvance(glyph_font, codepoint) *
                        font_size_scale(glyph_font, font_size) + 0.5f);
        if(target_x < cursor_x + advance / 2)
            return i;
        cursor_x += advance;
        i += codepoint_byte_count;
    }

    return byte_len;
}

static Color
ui_text_default_selection_color(Color text_color)
{
    Color color = c_link.a != 0 ? c_link : text_color;

    color.a = 88;
    return color;
}

static void
ui_text_copy_selection(const char *text, int start, int end)
{
    char *copy;
    int len;

    if(text == NULL || end <= start)
        return;

    len = end - start;
    copy = (char *)malloc((size_t)len + 1);
    if(copy == NULL)
        return;
    memcpy(copy, text + start, (size_t)len);
    copy[len] = '\0';
    SetUIClipboardTextValue(copy);
    free(copy);
}

static void
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
    if(end_x <= start_x)
        return;

    DrawRectangle(start_x, y, end_x - start_x, line_h, color);
}

static int
ui_text_mod_key_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

int
TextHeight(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || !UIFontReady(font))
        return font_size;

    if(UIFontHasNativeText(font)) {
        int native_h = UIFontNativeTextHeight(font);

        return native_h > 0 ? native_h : font_size;
    }

    scale = font_size_scale(font, font_size);
    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;
        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, font_size);
            GlyphInfo glyph = UIFontGlyph(glyph_font, codepoint);
            Rectangle rec = UIFontAtlasRec(glyph_font, codepoint);
            float glyph_scale = font_size_scale(glyph_font, font_size);
            int padding = UIFontGlyphPadding(glyph_font);
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
        return (int)((float)UIFontBaseSize(font) * scale + 0.5f);
    return (int)(max_bottom - min_top + 0.5f);
}

int
TextLineHeight(int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale = font_size_scale(font, font_size);
    int base = UIFontBaseSize(font);

    if(UIFontHasNativeText(font)) {
        int native_h = UIFontNativeTextHeight(font);

        if(native_h > 0)
            return native_h;
    }

    return base > 0 ? (int)((float)base * scale + 0.5f) :
        (int)((float)TextBaseSize * scale + 0.5f);
}

int
ScaledTextWidth(const char *text, int scale)
{
    Font font = active_font();
    int width = 0;

    if(text == NULL || !UIFontReady(font))
        return 0;
    if(scale < 1)
        scale = 1;

    if(UIFontHasNativeText(font))
        return UIFontNativeTextWidth(font, text, -1) * scale;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        Font glyph_font = font_for_scaled_codepoint(codepoint);
        width += UIFontAdvance(glyph_font, codepoint) * scale;
        i += codepoint_byte_count;
    }

    return width;
}

Font
GetUIFontForCodepoint(int codepoint, int font_size)
{
    return font_for_codepoint(codepoint, font_size);
}

float
GetUIFontScale(Font font, int font_size)
{
    return font_size_scale(font, font_size);
}

void
DrawUITextEx(const char *text, int x, int y, int font_size, Color color,
             int selectable_arg)
{
    Font font = active_font_for_size(font_size);
    int cursor_x = x;
    int selectable;
    int id;
    int byte_len;
    int text_w;
    int line_h;
    int selected_start = 0;
    int selected_end = 0;

    if(text == NULL || !UIFontReady(font))
        return;

    selectable = selectable_arg && g_ui_text_selectable && text[0] != '\0';
    byte_len = ui_text_line_byte_len(text);
    text_w = TextWidth(text, font_size);
    line_h = TextLineHeight(font_size);
    id = selectable ? ui_text_id(text, x, y, font_size) : 0;

    if(text[0] != '\0' && text_w > 0 && line_h > 0) {
        char inspect_id[96];
        Rectangle bounds = {(float)x, (float)y, (float)text_w, (float)line_h};
        int inspect_hash = ui_text_id(text, x, y, font_size);
        UIWidget widget;

        snprintf(inspect_id, sizeof(inspect_id), "tmp:text:%d", inspect_hash);
        widget = BeginUIWidget("text", inspect_id, bounds, UI_WIDGET_READONLY);
        UIWidgetSetAction(&widget, text);
        EndUIWidget(&widget);
    }

    if(selectable) {
        Rectangle bounds = {(float)x, (float)y, (float)text_w, (float)line_h};
        Vector2 mouse = ui_mouse_world();
        int inside = CheckCollisionPointRec(mouse, bounds);
        int captured = UIInputCapturesClick(mouse);

        if(inside && !captured)
            MarkUICursor(MOUSE_CURSOR_IBEAM);

        if(inside && !captured && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int offset = ui_text_byte_offset_at_x(text, font_size, (int)mouse.x - x);

            g_ui_text_selection.id = id;
            g_ui_text_selection.anchor = offset;
            g_ui_text_selection.cursor = offset;
            g_ui_text_selection.dragging = 1;
            g_ui_pointer_owner = UI_POINTER_OWNER_TEXT_SELECTION;
        }
        if(g_ui_text_selection.id == id && g_ui_text_selection.dragging) {
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                g_ui_text_selection.cursor =
                    ui_text_byte_offset_at_x(text, font_size, (int)mouse.x - x);
            } else {
                g_ui_text_selection.dragging = 0;
            }
        }
        if(g_ui_text_selection.id == id && ui_text_mod_key_down() &&
           IsKeyPressed(KEY_C)) {
            int start = g_ui_text_selection.anchor;
            int end = g_ui_text_selection.cursor;

            if(start > end) {
                int tmp = start;
                start = end;
                end = tmp;
            }
            start = ui_clampi(start, 0, byte_len);
            end = ui_clampi(end, 0, byte_len);
            ui_text_copy_selection(text, start, end);
        }
        if(g_ui_text_selection.id == id) {
            selected_start = g_ui_text_selection.anchor;
            selected_end = g_ui_text_selection.cursor;
            if(selected_start > selected_end) {
                int tmp = selected_start;
                selected_start = selected_end;
                selected_end = tmp;
            }
            selected_start = ui_clampi(selected_start, 0, byte_len);
            selected_end = ui_clampi(selected_end, 0, byte_len);
            ui_text_draw_selection(text, x, y, font_size,
                                   ui_text_default_selection_color(color),
                                   selected_start, selected_end);
        }
    }

    if(UIFontHasNativeText(font)) {
        (void)UIFontDrawNativeText(font, text, byte_len, x, y, font_size, color);
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
        if(ui_text_trace_enabled()) {
            /* which font entry actually serves this glyph + its advance */
            TraceLog(LOG_WARNING, "UIFONT: cp=%d fs=%d entry_base=%d adv=%.4f",
                     codepoint, font_size, glyph_font.baseSize,
                     (double)UIFontGlyph(glyph_font, codepoint).advanceX);
        }
        scale = font_size_scale(glyph_font, font_size);
        glyph = UIFontGlyph(glyph_font, codepoint);
        src = UIFontAtlasRec(glyph_font, codepoint);

        if(ui_text_trace_enabled() && i == 0) {
            TraceLog(LOG_WARNING, "UITEXT: txt=%.12s fs=%d base=%d sc=%.3f x=%d y=%d off=(%d,%d) adv=%.2f w=%.0f",
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
            DrawTexturePro(UIFontAtlasTexture(glyph_font), src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += (int)((float)glyph.advanceX * scale + 0.5f);
        i += codepoint_byte_count;
    }
}

static char *
ui_text_slice(const char *text, int start, int end)
{
    char *slice;
    int len = end - start;

    if(text == NULL || len < 0)
        return NULL;
    slice = (char *)malloc((size_t)len + 1);
    if(slice == NULL)
        return NULL;
    memcpy(slice, text + start, (size_t)len);
    slice[len] = '\0';
    return slice;
}

static int
ui_text_block_lines(const char *text, int width, int font_size,
                    UITextBlockLine **out_lines)
{
    UITextBlockLine *lines;
    int count = 0;
    int cap = 8;
    int len;
    int start = 0;

    if(out_lines == NULL)
        return 0;
    *out_lines = NULL;
    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    lines = (UITextBlockLine *)malloc((size_t)cap * sizeof(*lines));
    if(lines == NULL)
        return 0;

    while(start < len || (len == 0 && count == 0)) {
        int end = start;
        int last_space = -1;
        int chosen = start;

        if(text[start] == '\n') {
            chosen = start;
            end = start + 1;
        } else {
            while(end < len && text[end] != '\n') {
                int cp_bytes = 0;
                int next;
                char *slice;
                int measured;

                (void)GetCodepointNext(text + end, &cp_bytes);
                if(cp_bytes <= 0)
                    cp_bytes = 1;
                next = end + cp_bytes;
                if(text[end] == ' ' || text[end] == '\t')
                    last_space = end;
                slice = ui_text_slice(text, start, next);
                measured = slice != NULL ? TextWidth(slice, font_size) : 0;
                free(slice);
                if(width > 0 && measured > width) {
                    if(last_space >= start)
                        chosen = last_space;
                    else if(end > start)
                        chosen = end;
                    else
                        chosen = next;
                    break;
                }
                end = next;
                chosen = end;
            }
            if(end < len && text[end] == '\n' && chosen == end)
                end++;
        }
        if(count == cap) {
            UITextBlockLine *grown;
            cap *= 2;
            grown = (UITextBlockLine *)realloc(lines, (size_t)cap * sizeof(*lines));
            if(grown == NULL) {
                free(lines);
                return 0;
            }
            lines = grown;
        }
        lines[count++] = (UITextBlockLine){start, chosen};
        start = chosen;
        while(start < len && (text[start] == ' ' || text[start] == '\t'))
            start++;
        if(start < len && text[start] == '\n')
            start++;
        if(len == 0)
            break;
    }
    *out_lines = lines;
    return count;
}

int
MeasureUISelectableTextBlock(const char *text, int width, int font_size,
                             int line_gap)
{
    UITextBlockLine *lines = NULL;
    int count = ui_text_block_lines(text, width, font_size, &lines);
    int line_h = TextLineHeight(font_size);
    int height = count > 0 ? count * line_h + (count - 1) * line_gap : 0;

    free(lines);
    return height;
}

int
DrawUISelectableTextBlock(SelectableTextBlock block)
{
    UITextBlockLine *lines = NULL;
    Vector2 mouse = ui_mouse_world();
    int count;
    int line_h;
    int height;
    int captured;
    int selected_start = 0;
    int selected_end = 0;

    if(block.text == NULL || block.id <= 0 || block.font_size <= 0)
        return 0;
    count = ui_text_block_lines(block.text, (int)block.bounds.width,
                                block.font_size, &lines);
    line_h = TextLineHeight(block.font_size);
    height = count > 0 ? count * line_h + (count - 1) * block.line_gap : 0;
    captured = UIInputCapturesClick(mouse);

    for(int i = 0; i < count; i++) {
        int y = (int)block.bounds.y + i * (line_h + block.line_gap);
        char *line = ui_text_slice(block.text, lines[i].start, lines[i].end);
        int line_w = line != NULL ? TextWidth(line, block.font_size) : 0;
        Rectangle hit = {block.bounds.x, (float)y, (float)line_w, (float)line_h};
        int inside = CheckCollisionPointRec(mouse, hit);

        if(inside && !captured)
            MarkUICursor(MOUSE_CURSOR_IBEAM);
        if(inside && !captured && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int local = ui_text_byte_offset_at_x(line, block.font_size,
                                                 (int)(mouse.x - block.bounds.x));
            double now = GetTime();
            float dx = mouse.x - g_ui_text_block_last_click_position.x;
            float dy = mouse.y - g_ui_text_block_last_click_position.y;
            int slop = ScaleUIPx(6);
            int double_click = g_ui_text_block_last_click_id == block.id &&
                g_ui_text_block_last_click_line == i &&
                g_ui_text_block_last_click_time >= 0.0 &&
                now - g_ui_text_block_last_click_time <= 0.40 &&
                dx >= -slop && dx <= slop && dy >= -slop && dy <= slop;

            if(double_click) {
                /* A wrapped visual line is the useful unit here. Keep the
                 * completed range stable after the second button release. */
                g_ui_text_block_selection = (UITextSelectionState){
                    block.id, lines[i].start, lines[i].end, 0
                };
                g_ui_text_block_last_click_id = 0;
                g_ui_text_block_last_click_line = -1;
                g_ui_text_block_last_click_time = -1.0;
            } else {
                g_ui_text_block_selection = (UITextSelectionState){
                    block.id, lines[i].start + local,
                    lines[i].start + local, 1
                };
                g_ui_text_block_last_click_id = block.id;
                g_ui_text_block_last_click_line = i;
                g_ui_text_block_last_click_position = mouse;
                g_ui_text_block_last_click_time = now;
            }
            g_ui_pointer_owner = UI_POINTER_OWNER_TEXT_SELECTION;
        }
        free(line);
    }

    if(g_ui_text_block_selection.id == block.id &&
       g_ui_text_block_selection.dragging) {
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int line_index = (int)(mouse.y - block.bounds.y) /
                             (line_h + block.line_gap);
            char *line;
            int local;

            line_index = ui_clampi(line_index, 0, count - 1);
            line = ui_text_slice(block.text, lines[line_index].start,
                                 lines[line_index].end);
            local = mouse.y < block.bounds.y ? 0 :
                    mouse.y > block.bounds.y + height ?
                        lines[line_index].end - lines[line_index].start :
                        ui_text_byte_offset_at_x(line, block.font_size,
                                                 (int)(mouse.x - block.bounds.x));
            free(line);
            g_ui_text_block_selection.cursor = lines[line_index].start + local;
            g_ui_pointer_owner = UI_POINTER_OWNER_TEXT_SELECTION;
        } else {
            g_ui_text_block_selection.dragging = 0;
        }
    }

    if(g_ui_text_block_selection.id == block.id) {
        selected_start = g_ui_text_block_selection.anchor;
        selected_end = g_ui_text_block_selection.cursor;
        if(selected_start > selected_end) {
            int tmp = selected_start;
            selected_start = selected_end;
            selected_end = tmp;
        }
        if(ui_text_mod_key_down() && IsKeyPressed(KEY_C))
            ui_text_copy_selection(block.text, selected_start, selected_end);
    }

    for(int i = 0; i < count; i++) {
        int y = (int)block.bounds.y + i * (line_h + block.line_gap);
        int start = selected_start > lines[i].start ? selected_start : lines[i].start;
        int end = selected_end < lines[i].end ? selected_end : lines[i].end;
        char *line = ui_text_slice(block.text, lines[i].start, lines[i].end);

        if(end > start && line != NULL)
            ui_text_draw_selection(line, (int)block.bounds.x, y, block.font_size,
                                   ui_text_default_selection_color(block.color),
                                   start - lines[i].start, end - lines[i].start);
        if(line != NULL)
            DrawUITextEx(line, (int)block.bounds.x, y, block.font_size,
                         block.color, 0);
        free(line);
    }
    free(lines);
    return height;
}

void
DrawUIText(const char *text, int x, int y, int font_size, Color color)
{
    DrawUITextEx(text, x, y, font_size, color, 1);
}

static void
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
        if(!UIFontReady(glyph_font)) {
            i += codepoint_byte_count;
            continue;
        }

        scale = font_size_scale(glyph_font, font_size);
        glyph = UIFontGlyph(glyph_font, codepoint);
        src = UIFontAtlasRec(glyph_font, codepoint);

        if(ui_text_trace_enabled() && i == 0) {
            TraceLog(LOG_WARNING, "UITEXT: txt=%.12s fs=%d base=%d sc=%.3f x=%d y=%d off=(%d,%d) adv=%.2f w=%.0f",
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
            DrawTexturePro(UIFontAtlasTexture(glyph_font), src, dst,
                           (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += (int)((float)UIFontAdvance(glyph_font, codepoint) * scale + 0.5f);
        i += codepoint_byte_count;
    }
}

void
DrawUITextStyled(const char *text, int x, int y, TextStyle style)
{
    int font_size = style.font_size > 0 ? style.font_size : TextBaseSize;

    if(!style.italic) {
        DrawUITextEx(text, x, y, font_size, style.color, style.selectable);
        return;
    }

    ui_render_italic_text(text, x, y, font_size, style.color);
}

void
DrawUITextItalic(const char *text, int x, int y, int font_size, Color color)
{
    DrawUITextStyled(text, x, y, (TextStyle){font_size, color, 1, 1});
}

void
DrawUINonSelectableText(const char *text, int x, int y, int font_size, Color color)
{
    DrawUITextEx(text, x, y, font_size, color, 0);
}

int
PushTextSelectable(int selectable)
{
    int token = g_ui_text_selectable;

    if(g_ui_text_selectable_stack_count <
       (int)(sizeof(g_ui_text_selectable_stack) / sizeof(g_ui_text_selectable_stack[0])))
        g_ui_text_selectable_stack[g_ui_text_selectable_stack_count++] = token;
    g_ui_text_selectable = selectable != 0;
    return token;
}

void
PopTextSelectable(int token)
{
    if(g_ui_text_selectable_stack_count > 0)
        g_ui_text_selectable = g_ui_text_selectable_stack[--g_ui_text_selectable_stack_count];
    else
        g_ui_text_selectable = token != 0;
}

void
DrawScaledUIText(const char *text, int x, int y, int scale, Color color)
{
    Font font = active_font();
    int cursor_x = x;

    if(text == NULL || !UIFontReady(font))
        return;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        Font glyph_font = font_for_scaled_codepoint(codepoint);
        GlyphInfo glyph = UIFontGlyph(glyph_font, codepoint);
        Rectangle src = UIFontAtlasRec(glyph_font, codepoint);

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)(cursor_x + glyph.offsetX * scale),
                .y = (float)(y + glyph.offsetY * scale),
                .width = src.width * (float)scale,
                .height = src.height * (float)scale
            };
            DrawTexturePro(UIFontAtlasTexture(glyph_font), src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += glyph.advanceX * scale;
        i += codepoint_byte_count;
    }
}

void
DrawCenteredUIText(const char *text, int center_x, int center_y, int font_size, Color color)
{
    int text_w = TextWidth(text, font_size);
    int line_h = TextLineHeight(font_size);
    int y = TextBaselineY("Hg", center_y - line_h / 2, line_h, font_size);

    DrawUIText(text, center_x - text_w / 2, y, font_size, color);
}

void
DrawUITextInRect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int text_w = TextWidth(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = TextBaselineY(value, (int)rect.y, (int)rect.height, font_size);
    int clip_guard = 1;

    Rectangle clip = text_world_rect_to_screen((Rectangle){
        rect.x, rect.y - clip_guard, rect.width, rect.height + clip_guard * 2
    });
    BeginUIClip((int)clip.x, (int)clip.y, (int)clip.width, (int)clip.height);
    DrawUIText(value, x, y, font_size, color);
    EndUIClip();
}

int
ScaledTextBaselineY(const char *text, int box_y, int box_h, int scale)
{
    Font font = active_font();
    int font_size;
    int base;

    if(scale < 1)
        scale = 1;
    base = UIFontBaseSize(font);
    font_size = base > 0 ? base * scale : 16 * scale;
    return TextBaselineY(text, box_y, box_h, font_size);
}

int
TextBaselineY(const char *text, int box_y, int box_h, int font_size)
{
    Font font = active_font_for_size(font_size);
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || !UIFontReady(font))
        return box_y + (int)(((float)box_h - (float)TextLineHeight(font_size)) * 0.5f + 0.5f);

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, font_size);
            GlyphInfo glyph = UIFontGlyph(glyph_font, codepoint);
            Rectangle rec = UIFontAtlasRec(glyph_font, codepoint);
            float glyph_scale = font_size_scale(glyph_font, font_size);
            int padding = UIFontGlyphPadding(glyph_font);
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
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    return box_y + (int)(((float)box_h - (max_bottom - min_top)) * 0.5f - min_top + 0.5f);
}
