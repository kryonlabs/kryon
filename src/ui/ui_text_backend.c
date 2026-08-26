#include "ui_text_backend.h"

#if defined(KRYON_BACKEND_LIBDRAW)
int kry_libdraw_font_height(unsigned id);
int kry_libdraw_font_text_width(unsigned id, const char *text, int byte_len);
void kry_libdraw_queue_text(unsigned font_id, const char *text, int byte_len,
                            int x, int y, Color color);
#endif

#if defined(KRYON_BACKEND_TERMI)
#include "termi.h"
static unsigned
ui_text_pack_color(Color color)
{
    return ((unsigned)color.r << 24) | ((unsigned)color.g << 16) |
           ((unsigned)color.b << 8) | (unsigned)color.a;
}
#endif

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const GlyphInfo kryon_zero_glyphinfo;
static const Rectangle kryon_zero_rectangle;


#define UI_GLYPH_INDEX_CACHE_SIZE 256

typedef struct UIGlyphIndexCacheEntry {
    const GlyphInfo *glyphs;
    unsigned int texture_id;
    int glyph_count;
    int codepoint;
    int index;
} UIGlyphIndexCacheEntry;

static UIGlyphIndexCacheEntry g_glyph_index_cache[UI_GLYPH_INDEX_CACHE_SIZE];

static int
ui_font_glyph_index(Font font, int codepoint)
{
    unsigned int slot;
    UIGlyphIndexCacheEntry *cached;
    int fallback = 0;

    if(!UIFontReady(font))
        return 0;

    slot = ((unsigned int)codepoint * 2654435761u ^ font.texture.id) %
           UI_GLYPH_INDEX_CACHE_SIZE;
    cached = &g_glyph_index_cache[slot];
    if(cached->glyphs == font.glyphs && cached->texture_id == font.texture.id &&
       cached->glyph_count == font.glyphCount && cached->codepoint == codepoint)
        return cached->index;

    for(int i = 0; i < font.glyphCount; i++) {
        if(font.glyphs[i].value == '?')
            fallback = i;
        if(font.glyphs[i].value == codepoint) {
            fallback = i;
            break;
        }
    }

    *cached = (UIGlyphIndexCacheEntry){font.glyphs, font.texture.id,
                                      font.glyphCount, codepoint, fallback};
    return fallback;
}

/*
 * Backend-neutral implementation of the text-backend seam (declared in
 * ui_text_backend.h). Every function forwards to a public-surface accessor
 * (GetGlyphInfo, GetGlyphAtlasRec, GetGlyphIndex) that the active graphics
 * backend provides, so this single source file works for all backends.
 *
 * The two validity/base-size helpers read Font struct fields because raylib
 * exposes no accessor for them; that is the one struct coupling that remains,
 * confined to these helpers rather than scattered through ui_text.c.
 */

int
UIFontReady(Font font)
{
    return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL &&
           font.glyphCount > 0 && font.baseSize > 0;
}

int
UIFontBaseSize(Font font)
{
    return font.baseSize;
}

int
UIFontGlyphPadding(Font font)
{
    return font.glyphPadding;
}

GlyphInfo
UIFontGlyph(Font font, int codepoint)
{
    if(!UIFontReady(font))
        return kryon_zero_glyphinfo;
    return font.glyphs[ui_font_glyph_index(font, codepoint)];
}

Rectangle
UIFontAtlasRec(Font font, int codepoint)
{
    if(!UIFontReady(font))
        return kryon_zero_rectangle;
    return font.recs[ui_font_glyph_index(font, codepoint)];
}

Texture2D
UIFontAtlasTexture(Font font)
{
    return font.texture;
}

int
UIFontAdvance(Font font, int codepoint)
{
    if(!UIFontReady(font))
        return 0;
    return font.glyphs[ui_font_glyph_index(font, codepoint)].advanceX;
}

int
UIFontHasGlyphValue(Font font, int codepoint)
{
    GlyphInfo glyph;

    if(!UIFontReady(font))
        return 0;

    glyph = font.glyphs[ui_font_glyph_index(font, codepoint)];
    return glyph.value == codepoint;
}

int
UIFontGlyphCount(Font font)
{
    return font.glyphCount;
}

int
UIFontHasNativeText(Font font)
{
#if defined(KRYON_BACKEND_LIBDRAW)
    return kry_libdraw_font_height(font.texture.id) > 0;
#elif defined(KRYON_BACKEND_TERMI)
    return termi_font_height(font.texture.id) > 0;
#else
    (void)font;
    return 0;
#endif
}

int
UIFontNativeTextWidth(Font font, const char *text, int byte_len)
{
#if defined(KRYON_BACKEND_LIBDRAW)
    return kry_libdraw_font_text_width(font.texture.id, text, byte_len);
#elif defined(KRYON_BACKEND_TERMI)
    return termi_text_width(font.texture.id, text, byte_len);
#else
    (void)font;
    (void)text;
    (void)byte_len;
    return 0;
#endif
}

int
UIFontNativeTextHeight(Font font)
{
#if defined(KRYON_BACKEND_LIBDRAW)
    return kry_libdraw_font_height(font.texture.id);
#elif defined(KRYON_BACKEND_TERMI)
    return termi_font_height(font.texture.id);
#else
    (void)font;
    return 0;
#endif
}

int
UIFontDrawNativeText(Font font, const char *text, int byte_len, int x, int y,
                     int font_size, Color color)
{
#if defined(KRYON_BACKEND_LIBDRAW)
    (void)font_size;
    if(kry_libdraw_font_height(font.texture.id) <= 0)
        return 0;
    kry_libdraw_queue_text(font.texture.id, text, byte_len, x, y, color);
    return 1;
#elif defined(KRYON_BACKEND_TERMI)
    if(termi_font_height(font.texture.id) <= 0)
        return 0;
    termi_queue_text(font.texture.id, text, byte_len, x, y, font_size,
                     ui_text_pack_color(color));
    return 1;
#else
    (void)font;
    (void)text;
    (void)byte_len;
    (void)x;
    (void)y;
    (void)font_size;
    (void)color;
    return 0;
#endif
}
